/*
 * ClieDiag - Sony CLIE NR70V (Palm OS 4.1S) 1단계 진단 프로그램
 *
 * Sony CLIE SDK 헤더 없이, 표준 Palm OS 4 API만으로 다음을 확인한다.
 *   1/3 Info   : ROM 버전, OEM/기기 ID, 화면 모드, 'SoNy' 기능 플래그,
 *                Sony HR / Silk 라이브러리 탐색(SysLibFind)·적재(SysLibLoad) 결과
 *   2/3 Libr   : 기기에 설치된 공유 라이브러리('libr') 목록 (크리에이터, 버전, 이름)
 *   3/3 Events : 수신한 이벤트 기록 (펜 좌표, 키 코드). 가상 Graffiti 영역을
 *                눌렀을 때 앱이 어떤 이벤트를 받는지 확인용.
 *
 * 조작: 맨 위 제목줄을 누르면 다음 페이지, 위/아래 버튼으로 목록 스크롤.
 * 종료 시 이 프로그램이 직접 적재한 라이브러리는 SysLibRemove로 되돌린다.
 */

#include <PalmOS.h>

#define kMaxLines   48
#define kLineLen    40
#define kLineH      11
#define kVisible    13
#define kPages      3
#define kScreenW    160

typedef struct {
    Char   t[kMaxLines][kLineLen];
    UInt16 n;
} LineBuf;

static LineBuf gInfo, gLibr, gLog;
static UInt16  gPage = 0;
static Int16   gScroll = 0;
static UInt16  gLoadedRef[8];
static UInt16  gLoadedN = 0;
static UInt32  gPenMoves = 0;

/* 버퍼에 새 줄을 할당한다. 가득 차면 가장 오래된 줄을 버린다. */
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

/* 4문자 코드(크리에이터 등)를 출력 가능한 문자열로 바꾼다. */
static void FourCC(UInt32 v, Char *o)
{
    Int16 i;

    for (i = 0; i < 4; i++) {
        Char c = (Char)((v >> (24 - 8 * i)) & 0xFF);
        o[i] = (c >= 0x20 && c < 0x7F) ? c : '.';
    }
    o[4] = 0;
}

/* 이름으로 탐색만 한다. (적재하지 않음) */
static void FindOnly(const Char *name)
{
    UInt16 ref = sysInvalidRefNum;
    Err    err = SysLibFind(name, &ref);

    StrPrintF(NextLine(&gInfo), "%s", name);
    StrPrintF(NextLine(&gInfo), "  find=%x ref=%u", (UInt16)err, ref);
}

/* 이름으로 탐색하고, 없으면 크리에이터로 적재를 시도한다. */
static void TryLib(const Char *name, UInt32 creator)
{
    UInt16 ref = sysInvalidRefNum;
    Err    ef, el;
    Char   cc[5];

    FourCC(creator, cc);
    ef = SysLibFind(name, &ref);
    StrPrintF(NextLine(&gInfo), "[%s] %s", cc, name);
    if (ef == errNone) {
        StrPrintF(NextLine(&gInfo), "  find=0 ref=%u (loaded)", ref);
        return;
    }
    el = SysLibLoad(sysFileTLibrary, creator, &ref);
    if (el == errNone && gLoadedN < 8)
        gLoadedRef[gLoadedN++] = ref;
    StrPrintF(NextLine(&gInfo), "  find=%x load=%x ref=%u",
              (UInt16)ef, (UInt16)el, ref);
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

    /* 'SoNy' 크리에이터로 등록된 기능 번호 0~15 중 존재하는 것 */
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

    /* 이름 추정치로 탐색만 (정확한 이름은 Libr 페이지에서 확인) */
    FindOnly("Sony HR Library");
    FindOnly("Sony Silk Library");
}

/* 설치된 'libr' 목록을 만들고, HR/Silk 관련 라이브러리는 적재를 시도한다. */
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
        if (StrStr(name, "HR") || StrStr(name, "Silk") || StrStr(name, "silk"))
            TryLib(name, crid);
    }
    if (gLibr.n == 0)
        StrCopy(NextLine(&gLibr), "(no libr found)");
}

/* 이벤트를 기록한다. 기록했으면 true. */
static Boolean LogEvent(EventType *e)
{
    switch (e->eType) {
    case nilEvent:
        return false;
    case penMoveEvent:
        gPenMoves++;
        return false;
    case penDownEvent:
        gPenMoves = 0;
        StrPrintF(NextLine(&gLog), "penDown %d,%d", e->screenX, e->screenY);
        break;
    case penUpEvent:
        StrPrintF(NextLine(&gLog), "penUp   %d,%d mv=%ld",
                  e->screenX, e->screenY, (Int32)gPenMoves);
        break;
    case keyDownEvent:
        StrPrintF(NextLine(&gLog), "key c=%x k=%x m=%x",
                  (UInt16)e->data.keyDown.chr, e->data.keyDown.keyCode,
                  e->data.keyDown.modifiers);
        break;
    default:
        StrPrintF(NextLine(&gLog), "evt %u", (UInt16)e->eType);
        break;
    }
    return true;
}

static LineBuf *CurBuf(void)
{
    if (gPage == 0) return &gInfo;
    if (gPage == 1) return &gLibr;
    return &gLog;
}

static Int16 MaxScroll(LineBuf *b)
{
    return (b->n > kVisible) ? (Int16)(b->n - kVisible) : 0;
}

static void Draw(void)
{
    RectangleType r;
    LineBuf *b = CurBuf();
    Int16    i, start;
    Char     title[kLineLen];
    const Char *pname;

    RctSetRectangle(&r, 0, 0, kScreenW, kScreenW);
    WinEraseRectangle(&r, 0);

    if (gPage == 0)      pname = "Info";
    else if (gPage == 1) pname = "Libr";
    else                 pname = "Events";

    /* 이벤트 페이지는 항상 최신 기록이 아래에 오도록 */
    start = (gPage == 2) ? MaxScroll(b) : gScroll;

    StrPrintF(title, "%u/3 %s (%u)  tap=next", gPage + 1, pname, b->n);
    WinDrawInvertedChars(title, StrLen(title), 0, 0);

    for (i = 0; i < kVisible && start + i < (Int16)b->n; i++)
        WinDrawChars(b->t[start + i], StrLen(b->t[start + i]),
                     0, (i + 1) * kLineH);
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    EventType e;
    Boolean   logged;
    UInt16    i;

    if (cmd != sysAppLaunchCmdNormalLaunch)
        return 0;

    CollectInfo();
    CollectLibr();
    Draw();

    do {
        EvtGetEvent(&e, evtWaitForever);
        logged = LogEvent(&e);

        /* 제목줄 탭: 다음 페이지 */
        if (e.eType == penDownEvent && e.screenY >= 0 && e.screenY < kLineH
            && e.screenX >= 0 && e.screenX < kScreenW) {
            gPage = (gPage + 1) % kPages;
            gScroll = 0;
            Draw();
            continue;
        }

        if (!SysHandleEvent(&e)) {
            if (e.eType == keyDownEvent && gPage != 2) {
                Int16 m = MaxScroll(CurBuf());
                if (e.data.keyDown.chr == vchrPageDown) {
                    gScroll += kVisible - 1;
                    if (gScroll > m) gScroll = m;
                    Draw();
                } else if (e.data.keyDown.chr == vchrPageUp) {
                    gScroll -= kVisible - 1;
                    if (gScroll < 0) gScroll = 0;
                    Draw();
                }
            }
        }

        if (logged && gPage == 2)
            Draw();
    } while (e.eType != appStopEvent);

    for (i = 0; i < gLoadedN; i++)
        SysLibRemove(gLoadedRef[i]);

    return 0;
}
