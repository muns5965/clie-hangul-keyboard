/*
 * ClieDiag v3 - Sony CLIE NR70V (Palm OS 4.1S) 진단 프로그램
 *
 * 페이지
 *   Info   : ROM 버전, OEM/기기 ID, 화면 모드, 'SoNy' 기능 번호, HR/Silk 탐색 결과
 *   Libr   : 설치된 공유 라이브러리('libr') DB 목록
 *   Silk   : 현재 활성 실리클릿(Slkw) 조회, 설치된 실리클릿 목록, 탭하여 전환/복원
 *   Events : 이벤트 기록, Graffiti 영역(y>=160) 펜 이벤트 가로채기(INT) 토글
 *
 * 조작
 *   - 위/아래 버튼: 이전/다음 페이지
 *   - Info/Libr: 앱 영역 탭 → 다음 페이지
 *   - Silk: 목록 줄 탭 → 해당 실리클릿으로 전환, "restore" 줄 → 처음 상태로 복원,
 *           그 외 영역 탭 → 다음 페이지
 *   - Events: 위쪽 절반 탭 → 다음 페이지, 아래쪽 절반 탭 → INT 토글
 *   - 맨 아래 줄: 마지막 이벤트 / 마지막 Silk 호출 결과
 *
 * Sony Silk Library 확장(Vsk) 함수는 Sony 원본 헤더 SonySilkLib.h(Vsk 판)의
 * 트랩 번호와 원형을 따른다. 실제 사용 예: SilkletMgr(VskGetCurrentSlkw),
 * SlkClock(VskSetCurrentSlkw).
 *
 * v2의 적재 라이브러리 표 읽기(SysLibTblEntry)는 Fatal Exception을 일으켜 제거했다.
 * 소니 함수 호출 직전에는 화면 맨 위에 진행 단계를 찍는다(크래시 위치 확인용).
 */

#include <PalmOS.h>

/* ---------------------------------------------------------------- Sony Silk (Vsk) */

#define kSilkLibName        "Sony Silk Library"
#define silkLibErrNotOpen   (oemErrorClass | 0x700 | 2)

/* 트랩 번호: sysLibTrapCustom(0xA805)부터 순서대로
 *   A805 ResizeDispWin, A806 EnableResize, A807 DisableResize,
 *   A808 GetAPIVersion, A809 SetCurrentSlkw, A80A GetCurrentSlkw, ... */
Err    SilkLibOpen(UInt16 refNum)                     SYS_TRAP(sysLibTrapOpen);
Err    SilkLibClose(UInt16 refNum)                    SYS_TRAP(sysLibTrapClose);
UInt32 VskGetAPIVersion(UInt16 refNum)                SYS_TRAP(sysLibTrapCustom + 3);
Err    VskSetCurrentSlkw(UInt16 refNum, UInt16 slkwType, UInt32 creator)
                                                      SYS_TRAP(sysLibTrapCustom + 4);
Err    VskGetCurrentSlkw(UInt16 refNum, UInt16 slkwType, UInt32 *creatorP)
                                                      SYS_TRAP(sysLibTrapCustom + 5);

#define kSlkwDBType   'Slkw'

/* ---------------------------------------------------------------- 화면/버퍼 */

#define kMaxLines   48
#define kLineLen    40
#define kLineH      11
#define kVisible    12          /* 제목줄 1 + 내용 12 + 하단 상태줄 1 */
#define kScreenW    160
#define kSilkTopY   160
#define kMaxSlkw    8

/* Silk 페이지 줄 배치 (내용 영역 기준 0부터) */
#define kSilkRowRestore   3
#define kSilkRowFirst     5

typedef struct {
    Char   t[kMaxLines][kLineLen];
    UInt16 n;
} LineBuf;

typedef struct {
    UInt32 creator;
    Char   name[dmDBNameLength];
} SlkwInfo;

enum { kPgInfo, kPgLibr, kPgSilk, kPgEvents };

static LineBuf  gInfo, gLibr, gLog;
static Char     gLast[kLineLen] = "-";
static UInt16   gPage = 0;
static Boolean  gIntercept = false;
static UInt32   gPenMoves = 0;

static UInt16   gSilkRef = sysInvalidRefNum;
static Boolean  gSilkOpened = false;
static Err      gOrigErr = 0;
static UInt32   gOrig = 0;
static Boolean  gOrigValid = false;
static SlkwInfo gSlkw[kMaxSlkw];
static UInt16   gSlkwN = 0;

/* ---------------------------------------------------------------- 공통 */

static Char *NextLine(LineBuf *b)
{
    Char *p;

    if (b->n >= kMaxLines) {
        MemMove(b->t[0], b->t[1], (Int32)(kMaxLines - 1) * kLineLen);
        b->n = kMaxLines - 1;
    }
    p = b->t[b->n++];
    p[0] = 0;
    return p;
}

static void FourCC(UInt32 v, Char *o)
{
    Int16 i;

    for (i = 0; i < 4; i++) {
        Char c = (Char)((v >> (24 - 8 * i)) & 0xFF);
        o[i] = (c >= 0x20 && c < 0x7F) ? c : '.';
    }
    o[4] = 0;
}

/* 진행 단계 표시: 소니 함수 호출 직전에 화면 맨 위에 찍는다. */
static void Crumb(const Char *s)
{
    RectangleType r;

    RctSetRectangle(&r, 0, 0, kScreenW, kLineH);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars(s, StrLen(s), 0, 0);
}

static void SetLast(const Char *s)
{
    StrNCopy(gLast, s, kLineLen - 1);
    gLast[kLineLen - 1] = 0;
}

static UInt16 PagesOf(LineBuf *b)
{
    return (b->n == 0) ? 1 : (UInt16)((b->n + kVisible - 1) / kVisible);
}

static UInt16 TotalPages(void)
{
    return PagesOf(&gInfo) + PagesOf(&gLibr) + 2;   /* + Silk + Events */
}

/* 현재 페이지 종류와, Info/Libr이면 버퍼와 하위 페이지 번호 */
static UInt16 PageKind(LineBuf **bufP, UInt16 *subP, UInt16 *subNP)
{
    UInt16 pi = PagesOf(&gInfo);
    UInt16 pl = PagesOf(&gLibr);

    *bufP = NULL; *subP = 0; *subNP = 1;
    if (gPage < pi) {
        *bufP = &gInfo; *subP = gPage; *subNP = pi;
        return kPgInfo;
    }
    if (gPage < pi + pl) {
        *bufP = &gLibr; *subP = gPage - pi; *subNP = pl;
        return kPgLibr;
    }
    if (gPage == pi + pl)
        return kPgSilk;
    return kPgEvents;
}

/* ---------------------------------------------------------------- 정보 수집 */

static void CollectInfo(void)
{
    UInt32  rom = 0, comp = 0, dev = 0, v = 0;
    UInt32  w = 0, h = 0, d = 0;
    Boolean color = false;
    Err     err;
    UInt16  ref;
    Char    cc[5], dd[5], num[8];
    Char   *p;
    UInt16  i;

    FtrGet(sysFtrCreator, sysFtrNumROMVersion, &rom);
    StrPrintF(NextLine(&gInfo), "ROM ver: %lx", rom);

    FtrGet(sysFtrCreator, sysFtrNumOEMCompanyID, &comp);
    FtrGet(sysFtrCreator, sysFtrNumOEMDeviceID, &dev);
    FourCC(comp, cc);
    FourCC(dev, dd);
    StrPrintF(NextLine(&gInfo), "OEM: %s  Device: %s", cc, dd);

    err = WinScreenMode(winScreenModeGet, &w, &h, &d, &color);
    StrPrintF(NextLine(&gInfo), "Scr: %ldx%ld %ldbpp col=%u e=%x",
              (Int32)w, (Int32)h, (Int32)d, (UInt16)color, (UInt16)err);

    p = NextLine(&gInfo);
    StrCopy(p, "SoNy ftr:");
    for (i = 0; i < 16; i++) {
        if (FtrGet('SoNy', i, &v) == errNone) {
            StrIToA(num, i);
            StrCat(p, " ");
            StrCat(p, num);
        }
    }
    err = FtrGet('SoNy', 1, &v);
    StrPrintF(NextLine(&gInfo), "SoNy#1: e=%x v=%lx", (UInt16)err, v);

    ref = sysInvalidRefNum;
    err = SysLibFind("Sony HR Library", &ref);
    StrPrintF(NextLine(&gInfo), "HR lib: e=%x ref=%u", (UInt16)err, ref);

    ref = sysInvalidRefNum;
    err = SysLibFind(kSilkLibName, &ref);
    StrPrintF(NextLine(&gInfo), "Silk lib: e=%x ref=%u", (UInt16)err, ref);
    if (err == errNone)
        gSilkRef = ref;
}

static void CollectLibr(void)
{
    DmSearchStateType st;
    UInt16  card, ver;
    LocalID id;
    Boolean first = true;
    Char    name[dmDBNameLength];
    UInt32  type, crid;
    Char    cc[5];

    while (DmGetNextDatabaseByTypeCreator(first, &st, sysFileTLibrary, 0,
                                          false, &card, &id) == errNone) {
        first = false;
        name[0] = 0;
        ver = 0;
        type = 0;
        crid = 0;
        DmDatabaseInfo(card, id, name, NULL, &ver, NULL, NULL, NULL, NULL,
                       NULL, NULL, &type, &crid);
        FourCC(crid, cc);
        StrPrintF(NextLine(&gLibr), "%s v%u %s", cc, ver, name);
    }
    if (gLibr.n == 0)
        StrCopy(NextLine(&gLibr), "(no libr found)");
}

/* 설치된 실리클릿(Slkw) 목록 */
static void CollectSlkw(void)
{
    DmSearchStateType st;
    UInt16  card;
    LocalID id;
    Boolean first = true;
    UInt32  crid;

    gSlkwN = 0;
    while (gSlkwN < kMaxSlkw &&
           DmGetNextDatabaseByTypeCreator(first, &st, kSlkwDBType, 0,
                                          true, &card, &id) == errNone) {
        first = false;
        crid = 0;
        gSlkw[gSlkwN].name[0] = 0;
        DmDatabaseInfo(card, id, gSlkw[gSlkwN].name, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, &crid);
        gSlkw[gSlkwN].creator = crid;
        gSlkwN++;
    }
}

/* 처음 상태(현재 활성 실리클릿)를 읽어 둔다. 읽기 전용 호출. */
static void ReadOrigSilk(void)
{
    if (gSilkRef == sysInvalidRefNum)
        return;
    Crumb("> VskGetCurrentSlkw(0)");
    gOrig = 0;
    gOrigErr = VskGetCurrentSlkw(gSilkRef, 0, &gOrig);
    gOrigValid = (gOrigErr == errNone);
}

/* 실리클릿 전환. 라이브러리가 열려 있지 않다는 오류면 한 번 열고 재시도한다. */
static void SetSilk(UInt32 creator)
{
    Err  err;
    Char cc[5], s[kLineLen];

    if (gSilkRef == sysInvalidRefNum) {
        SetLast("Silk lib not found");
        return;
    }
    FourCC(creator, cc);

    StrPrintF(s, "> VskSetCurrentSlkw %s", cc);
    Crumb(s);
    err = VskSetCurrentSlkw(gSilkRef, 0, creator);

    if (err == silkLibErrNotOpen && !gSilkOpened) {
        Crumb("> SilkLibOpen");
        if (SilkLibOpen(gSilkRef) == errNone)
            gSilkOpened = true;
        StrPrintF(s, "> VskSetCurrentSlkw %s (2)", cc);
        Crumb(s);
        err = VskSetCurrentSlkw(gSilkRef, 0, creator);
    }

    StrPrintF(s, "set %s e=%x%s", cc, (UInt16)err, gSilkOpened ? " (open)" : "");
    SetLast(s);
    StrCopy(NextLine(&gLog), s);
}

/* ---------------------------------------------------------------- 이벤트 */

static Boolean FormatEvent(EventType *e, Boolean blocked, Char *out)
{
    const Char *tag = blocked ? "*" : " ";

    switch (e->eType) {
    case nilEvent:
        return false;
    case penMoveEvent:
        gPenMoves++;
        return false;
    case penDownEvent:
        gPenMoves = 0;
        StrPrintF(out, "%spenDown %d,%d", tag, e->screenX, e->screenY);
        break;
    case penUpEvent:
        StrPrintF(out, "%spenUp   %d,%d mv=%ld", tag,
                  e->screenX, e->screenY, (Int32)gPenMoves);
        break;
    case keyDownEvent:
        StrPrintF(out, "%skey c=%x k=%x m=%x", tag,
                  (UInt16)e->data.keyDown.chr, e->data.keyDown.keyCode,
                  e->data.keyDown.modifiers);
        break;
    default:
        StrPrintF(out, "%sevt %u", tag, (UInt16)e->eType);
        break;
    }
    return true;
}

static Boolean IsSilkPen(EventType *e)
{
    return (e->eType == penDownEvent || e->eType == penUpEvent ||
            e->eType == penMoveEvent) && e->screenY >= kSilkTopY;
}

/* ---------------------------------------------------------------- 화면 */

static void DrawRow(UInt16 row, const Char *s)
{
    WinDrawChars(s, StrLen(s), 0, (row + 1) * kLineH);
}

static void DrawSilkPage(void)
{
    Char   s[kLineLen], cc[5];
    UInt32 cur = 0, ver;
    Err    err;
    UInt16 i;

    if (gSilkRef == sysInvalidRefNum) {
        DrawRow(0, "Silk lib not found");
        return;
    }

    Crumb("> VskGetAPIVersion");
    ver = VskGetAPIVersion(gSilkRef);
    Crumb("> VskGetCurrentSlkw(0)");
    err = VskGetCurrentSlkw(gSilkRef, 0, &cur);

    StrPrintF(s, "ref=%u api=%lx", gSilkRef, ver);
    DrawRow(0, s);
    FourCC(cur, cc);
    StrPrintF(s, "now: e=%x %lx '%s'", (UInt16)err, cur, cc);
    DrawRow(1, s);
    FourCC(gOrig, cc);
    StrPrintF(s, "orig: e=%x %lx '%s'", (UInt16)gOrigErr, gOrig, cc);
    DrawRow(2, s);

    if (gOrigValid)
        DrawRow(kSilkRowRestore, "[restore orig]  <- tap");
    else
        DrawRow(kSilkRowRestore, "[restore n/a]");

    StrPrintF(s, "-- Slkw installed: %u (tap=set) --", gSlkwN);
    DrawRow(kSilkRowFirst - 1, s);
    for (i = 0; i < gSlkwN && kSilkRowFirst + i < kVisible; i++) {
        FourCC(gSlkw[i].creator, cc);
        StrPrintF(s, "%s %s%s", cc, gSlkw[i].name,
                  (gSlkw[i].creator == cur) ? " *" : "");
        DrawRow(kSilkRowFirst + i, s);
    }
}

static void Draw(void)
{
    RectangleType r;
    LineBuf *b;
    UInt16   sub, subN, kind;
    Int16    i, start;
    Char     title[kLineLen];

    kind = PageKind(&b, &sub, &subN);

    RctSetRectangle(&r, 0, 0, kScreenW, kScreenW);
    WinEraseRectangle(&r, 0);

    switch (kind) {
    case kPgInfo:
    case kPgLibr:
        StrPrintF(title, "%u/%u %s %u/%u", gPage + 1, TotalPages(),
                  (kind == kPgInfo) ? "Info" : "Libr", sub + 1, subN);
        start = (Int16)(sub * kVisible);
        for (i = 0; i < kVisible && start + i < (Int16)b->n; i++)
            DrawRow((UInt16)i, b->t[start + i]);
        break;
    case kPgSilk:
        StrPrintF(title, "%u/%u Silk", gPage + 1, TotalPages());
        DrawSilkPage();
        break;
    default:
        StrPrintF(title, "%u/%u Events INT:%s", gPage + 1, TotalPages(),
                  gIntercept ? "ON" : "off");
        start = (gLog.n > kVisible) ? (Int16)(gLog.n - kVisible) : 0;
        for (i = 0; i < kVisible && start + i < (Int16)gLog.n; i++)
            DrawRow((UInt16)i, gLog.t[start + i]);
        WinDrawGrayLine(0, kScreenW / 2, kScreenW - 1, kScreenW / 2);
        break;
    }

    /* 제목줄은 Crumb 표시를 덮어쓰도록 마지막에 그린다 */
    RctSetRectangle(&r, 0, 0, kScreenW, kLineH);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars(title, StrLen(title), 0, 0);
    WinDrawInvertedChars(gLast, StrLen(gLast), 0, (kVisible + 1) * kLineH);
}

static void GoPage(Int16 delta)
{
    Int16 n = (Int16)TotalPages();
    Int16 p = (Int16)gPage + delta;

    while (p < 0) p += n;
    gPage = (UInt16)(p % n);
    Draw();
}

/* 앱 영역 탭 처리 */
static void HandleAppTap(Int16 x, Int16 y)
{
    LineBuf *b;
    UInt16   sub, subN;
    UInt16   kind = PageKind(&b, &sub, &subN);
    Int16    row = y / kLineH - 1;     /* 내용 영역 줄 번호 */

    if (kind == kPgEvents) {
        if (y >= kScreenW / 2) {
            gIntercept = !gIntercept;
            StrPrintF(NextLine(&gLog), "== INT %s ==", gIntercept ? "ON" : "off");
            Draw();
        } else {
            GoPage(1);
        }
        return;
    }

    if (kind == kPgSilk) {
        if (row == kSilkRowRestore && gOrigValid) {
            SetSilk(gOrig);
            Draw();
            return;
        }
        if (row >= kSilkRowFirst && row - kSilkRowFirst < (Int16)gSlkwN) {
            SetSilk(gSlkw[row - kSilkRowFirst].creator);
            Draw();
            return;
        }
    }
    GoPage(1);
}

/* ---------------------------------------------------------------- 메인 */

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    EventType e;
    Boolean   blocked, logged;
    Char      line[kLineLen];

    if (cmd != sysAppLaunchCmdNormalLaunch)
        return 0;

    CollectInfo();
    CollectLibr();
    CollectSlkw();
    ReadOrigSilk();
    Draw();

    do {
        EvtGetEvent(&e, evtWaitForever);

        blocked = gIntercept && IsSilkPen(&e);
        logged = FormatEvent(&e, blocked, line);
        if (logged) {
            StrCopy(NextLine(&gLog), line);
            SetLast(line);
        }

        if (e.eType == penDownEvent && e.screenY >= 0 && e.screenY < kSilkTopY
            && e.screenX >= 0 && e.screenX < kScreenW) {
            HandleAppTap(e.screenX, e.screenY);
            continue;
        }

        if (blocked) {
            Draw();
            continue;
        }

        if (!SysHandleEvent(&e)) {
            if (e.eType == keyDownEvent) {
                if (e.data.keyDown.chr == vchrPageDown) { GoPage(1);  continue; }
                if (e.data.keyDown.chr == vchrPageUp)   { GoPage(-1); continue; }
            }
        }

        /* Silk 페이지는 다시 그릴 때마다 소니 함수를 부르므로, 이벤트로는 갱신하지 않는다 */
        if (logged) {
            LineBuf *b; UInt16 sub, subN;
            if (PageKind(&b, &sub, &subN) != kPgSilk)
                Draw();
        }
    } while (e.eType != appStopEvent);

    if (gSilkOpened)
        SilkLibClose(gSilkRef);

    return 0;
}
