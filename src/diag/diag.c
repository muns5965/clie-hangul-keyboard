/*
 * ClieDiag v2 - Sony CLIE NR70V (Palm OS 4.1S) 진단 프로그램
 *
 * Sony CLIE SDK 헤더 없이, 표준 Palm OS 4 API만으로 다음을 확인한다.
 *   Info   : ROM 버전, OEM/기기 ID, 화면 모드, 'SoNy' 기능 번호,
 *            Sony HR / Silk 라이브러리 탐색(SysLibFind) 결과,
 *            현재 적재된 라이브러리 표(ref 번호, 등록 이름, 크리에이터)
 *   Libr   : 기기에 설치된 공유 라이브러리('libr') DB 목록
 *   Events : 수신 이벤트 기록. Graffiti 영역(y>=160) 펜 이벤트를
 *            시스템에 넘기지 않는 가로채기(INT) 모드 토글 포함.
 *
 * 조작
 *   - 위/아래 버튼: 이전/다음 페이지
 *   - Info/Libr 페이지: 앱 영역 아무 곳 탭 → 다음 페이지
 *   - Events 페이지: 위쪽 절반 탭 → 다음 페이지, 아래쪽 절반 탭 → INT 토글
 *   - 맨 아래 줄: 마지막으로 받은 이벤트
 *
 * v1과 달리 라이브러리를 적재(SysLibLoad)하지 않는다. (v1은 이미 다른 이름으로
 * 올라와 있던 HRLib를 중복 적재하는 문제가 있었다.)
 *
 * 주의: INT 모드에서는 Graffiti 영역의 아이콘(홈·메뉴 등)도 동작하지 않는다.
 *       빠져나오려면 INT를 끄거나 하드웨어 앱 버튼을 누른다.
 */

#include <PalmOS.h>

#define kMaxLines   48
#define kLineLen    40
#define kLineH      11
#define kVisible    12          /* 제목줄 1 + 내용 12 + 하단 상태줄 1 */
#define kScreenW    160
#define kSilkTopY   160         /* 160x160 좌표계에서 이 아래는 가상 Graffiti 영역 */
#define kMaxLibRef  64

typedef struct {
    Char   t[kMaxLines][kLineLen];
    UInt16 n;
} LineBuf;

static LineBuf gInfo, gLibr, gLog;
static Char    gLast[kLineLen] = "-";
static UInt16  gPage = 0;
static Boolean gIntercept = false;
static UInt32  gPenMoves = 0;

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

/* 버퍼를 kVisible 줄 단위 페이지로 나눴을 때의 페이지 수 (최소 1) */
static UInt16 PagesOf(LineBuf *b)
{
    return (b->n == 0) ? 1 : (UInt16)((b->n + kVisible - 1) / kVisible);
}

static UInt16 TotalPages(void)
{
    return PagesOf(&gInfo) + PagesOf(&gLibr) + 1;   /* + Events */
}

/* ---------------------------------------------------------------- 정보 수집 */

static void FindOnly(const Char *name)
{
    UInt16 ref = sysInvalidRefNum;
    Err    err = SysLibFind(name, &ref);

    StrPrintF(NextLine(&gInfo), "%s: e=%x ref=%u", name, (UInt16)err, ref);
}

/* 현재 적재된 라이브러리 표를 읽는다.
 * 68K 라이브러리의 디스패치 테이블 첫 워드는 테이블 시작부터 라이브러리
 * 이름 문자열까지의 오프셋이다. */
static void CollectLoadedLibs(void)
{
    UInt16  ref;
    UInt16  count = 0;

    StrCopy(NextLine(&gInfo), "-- loaded libs (ref name db) --");
    for (ref = 0; ref < kMaxLibRef; ref++) {
        SysLibTblEntryPtr e = SysLibTblEntry(ref);
        UInt16 *tbl;
        Char    name[24];
        Char    cc[5];
        UInt32  crid = 0;

        if (e == NULL)
            break;
        if (e->dispatchTblP == NULL)
            continue;

        tbl = (UInt16 *)e->dispatchTblP;
        StrNCopy(name, (Char *)tbl + tbl[0], sizeof(name) - 1);
        name[sizeof(name) - 1] = 0;

        cc[0] = 0;
        if (e->dbID) {
            DmDatabaseInfo(0, e->dbID, NULL, NULL, NULL, NULL, NULL, NULL,
                           NULL, NULL, NULL, NULL, &crid);
            FourCC(crid, cc);
        }
        StrPrintF(NextLine(&gInfo), "%u %s %s", ref, name, cc);
        count++;
    }
    StrPrintF(NextLine(&gInfo), "-- %u loaded, scanned to %u --", count, ref);
}

static void CollectInfo(void)
{
    UInt32  rom = 0, comp = 0, dev = 0, v = 0;
    UInt32  w = 0, h = 0, d = 0;
    Boolean color = false;
    Err     err;
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

    FindOnly("Sony HR Library");
    FindOnly("Sony Silk Library");

    CollectLoadedLibs();
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

/* ---------------------------------------------------------------- 이벤트 */

/* 이벤트 내용을 문자열로 만든다. 기록할 가치가 없으면 false. */
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

/* 현재 페이지가 가리키는 버퍼와 시작 줄을 구한다. Events면 true. */
static Boolean ResolvePage(LineBuf **bufP, Int16 *startP, UInt16 *subP, UInt16 *subNP)
{
    UInt16 pi = PagesOf(&gInfo);
    UInt16 pl = PagesOf(&gLibr);

    if (gPage < pi) {
        *bufP = &gInfo; *subP = gPage; *subNP = pi;
    } else if (gPage < pi + pl) {
        *bufP = &gLibr; *subP = gPage - pi; *subNP = pl;
    } else {
        *bufP = &gLog; *subP = 0; *subNP = 1;
        *startP = (gLog.n > kVisible) ? (Int16)(gLog.n - kVisible) : 0;
        return true;
    }
    *startP = (Int16)(*subP * kVisible);
    return false;
}

static void Draw(void)
{
    RectangleType r;
    LineBuf *b;
    Int16    i, start;
    UInt16   sub, subN;
    Boolean  isEvents;
    Char     title[kLineLen];

    isEvents = ResolvePage(&b, &start, &sub, &subN);

    RctSetRectangle(&r, 0, 0, kScreenW, kScreenW);
    WinEraseRectangle(&r, 0);

    if (isEvents)
        StrPrintF(title, "%u/%u Events INT:%s", gPage + 1, TotalPages(),
                  gIntercept ? "ON" : "off");
    else
        StrPrintF(title, "%u/%u %s %u/%u", gPage + 1, TotalPages(),
                  (b == &gInfo) ? "Info" : "Libr", sub + 1, subN);
    WinDrawInvertedChars(title, StrLen(title), 0, 0);

    for (i = 0; i < kVisible && start + i < (Int16)b->n; i++)
        WinDrawChars(b->t[start + i], StrLen(b->t[start + i]),
                     0, (i + 1) * kLineH);

    if (isEvents) {
        /* 탭 영역 구분선 */
        WinDrawGrayLine(0, kScreenW / 2, kScreenW - 1, kScreenW / 2);
    }

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
    Draw();

    do {
        EvtGetEvent(&e, evtWaitForever);

        blocked = gIntercept && IsSilkPen(&e);
        logged = FormatEvent(&e, blocked, line);
        if (logged) {
            StrCopy(NextLine(&gLog), line);
            StrCopy(gLast, line);
        }

        /* 앱 영역 탭: 페이지 넘김 / INT 토글 */
        if (e.eType == penDownEvent && e.screenY >= 0 && e.screenY < kSilkTopY
            && e.screenX >= 0 && e.screenX < kScreenW) {
            LineBuf *b; Int16 s; UInt16 sub, subN;
            if (ResolvePage(&b, &s, &sub, &subN) && e.screenY >= kScreenW / 2) {
                gIntercept = !gIntercept;
                StrPrintF(NextLine(&gLog), "== INT %s ==", gIntercept ? "ON" : "off");
                Draw();
            } else {
                GoPage(1);
            }
            continue;
        }

        /* 가로채기: Graffiti 영역 펜 이벤트를 시스템에 넘기지 않음 */
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

        if (logged)
            Draw();
    } while (e.eType != appStopEvent);

    return 0;
}
