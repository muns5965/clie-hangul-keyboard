/*
 * ClieDiag v17 - Sony CLIE NR70V (Palm OS 4.1S) MnSk 한글 키보드 상태·진단 프로그램
 *
 * v14: 기본 화면(Main) 한 페이지만 보인다. Main의 "[ ] debug" 줄을 탭하면 아래 진단 페이지가 나타난다
 *      (체크 상태는 앱 설정에 저장). 복구용 DiagRescue(To Do 대체)는 폐기 — 로더 11부터 DisS가 필요 없다.
 * v15: Events의 INT 전환은 맨 아래 "[INT ...]" 줄 탭으로만(아래 절반 탭으로 실수로 켜져 홈 아이콘이 막혔음).
 *      SkWr 페이지에 "[delete DiagRescue]" — 이름으로 그 DB만 지운다(런처 삭제는 같은 크리에이터 'todo'의
 *      To Do 데이터까지 지울 수 있어서).
 * v16: Main에 "[soft reset: hold 3s]" — 3초 동안 누르고 있으면 SysReset(), 중간에 떼면 취소.
 *      로더 업데이트(설치 → 바로 리셋)를 앱 안에서 끝내기 위해.
 * v17: 누를 수 있는 줄은 둥근 테두리 버튼, 디버그는 네모 체크박스로 그린다. 누르면 버튼이 반전된다.
 *
 *   Main   : 로더(SkWr) 설치·빌드·보호 해제, 본체(SkCo) 설치·빌드·불러오기 결과, 자판 켜짐, 업데이트 방법
 *
 * 진단 페이지 (debug 체크 시)
 *   Info   : ROM 버전, OEM/기기 ID, 화면 모드, 'SoNy' 기능 번호, HR/Silk 탐색 결과
 *   Libr   : 설치된 공유 라이브러리('libr') DB 목록
 *   SkWr   : SilkWare 플러그인 시험. 설치된 'SkWr' DB 목록, 플러그인 기록
 *            (feature 'MnSk' #0) 해석, 기록 초기화, 실크 비활성화 스위치(DisS) 켜기/끄기
 *   SkEv   : SkWr 플러그인 이벤트 카운터 (feature 'MnSk' #1~#19) 표시
 *   SkRaw  : SkWr 플러그인 3단계 기록 - 이벤트 앞 16바이트(Open/Start/PenDown/PenUp/DispChange),
 *            Start 때 받은 슬롯 값, 그리기 창 크기, 직전 그리기 창 (feature 'MnSk' #20~#42)
 *   Events : 이벤트 기록, Graffiti 영역(y>=160) 펜 이벤트 가로채기(INT) 토글
 *
 * 조작
 *   - 위/아래 버튼: 이전/다음 페이지
 *   - Info/Libr: 앱 영역 탭 → 다음 페이지
 *   - SkWr: "[clear record]" 줄 탭 → 기록·카운터 모두 삭제, "[DisS ...]" 줄 탭 → 스위치 전환,
 *           "[delete DiagRescue]" 줄 탭 → 옛 복구 앱 삭제,
 *           그 외 영역 탭 → 다음 페이지
 *   - Events: 맨 아래 "[INT ...]" 줄 탭 → INT 토글, 그 외 탭 → 다음 페이지
 *   - 맨 아래 줄: 마지막 이벤트 / 마지막 조작 결과
 *
 * 실크 비활성화 스위치: ROM SilkExtn(SilkStartup, 0x27EC)은 부팅 시 타입 'DisS',
 * 크리에이터 'SeSi' DB가 있으면 실크 초기화 전체(SkWr 플러그인 로딩·보호 포함)를
 * 건너뛴다. 플러그인 문제 복구용. 전환 후 리셋해야 적용된다.
 *
 * v3의 Silk(Vsk) 페이지는 NR70V에 없는 함수(VskGetCurrentSlkw)를 불러
 * Fatal Exception을 일으켜 제거했다. v2의 SysLibTblEntry 표 읽기도 제거 상태.
 */

#include <PalmOS.h>

/* ---------------------------------------------------------------- SkWr 시험 */

#define kSilkLibName   "Sony Silk Library"
#define kSkwrDBType    'SkWr'
#define kSkCreator     'MnSk'
#define kSkFtrNum      0
#define kSkFtrCount0   1      /* +이벤트 번호 0~15 */
#define kSkFtrLast     17
#define kSkFtrOver     18
#define kSkFtrOut      19
#define kSkFtrMax      56     /* 지울 플러그인 기록 마지막 번호. #57·#58(본체 핸들·주소)은 지우지 않는다 */
#define kSkMagic       0x534BUL
#define kDisSType      'DisS'
#define kDisSCreator   'SeSi'
#define kDisSName      "MnSilkDisable"

/* ---------------------------------------------------------------- 화면/버퍼 */

#define kMaxLines   48
#define kLineLen    40
#define kLineH      11
#define kVisible    12          /* 제목줄 1 + 내용 12 + 하단 상태줄 1 */
#define kScreenW    160
#define kSilkTopY   160
#define kMaxSkwr    8

/* SkWr 페이지 줄 배치 (내용 영역 기준 0부터) */
#define kSkRowClear   3
#define kSkRowDisS    5
#define kSkRowFirst   8
#define kSkRowRescue  4
#define kEvRowInt     11
#define kRescueName   "DiagRescue"

typedef struct {
    Char   t[kMaxLines][kLineLen];
    UInt16 n;
} LineBuf;

typedef struct {
    UInt32 creator;
    Char   name[dmDBNameLength];
} SkwrInfo;

enum { kPgInfo, kPgLibr, kPgSkwr, kPgSkev, kPgSkraw, kPgEvents, kPgMain };

#define kAppCreator    'MnDg'
#define kPrefDebug     0
#define kMainRowDebug  9
#define kMainRowReset  8
#define kResetHoldSec  3
#define kCoreType      'SkCo'

static LineBuf  gInfo, gLibr, gLog;
static Char     gLast[kLineLen] = "-";
static UInt16   gPage = 0;
static Boolean  gIntercept = false;
static UInt32   gPenMoves = 0;
static Boolean  gHoldLast = false;  /* 조작 결과를 바로 뒤 penUp이 덮어쓰지 않게 */
static Boolean  gDebug = false;     /* 진단 페이지 표시 (앱 설정 kPrefDebug) */
static Int16    gBtnW[kVisible];    /* 줄별 버튼 너비(0 = 버튼 없음). Draw 때마다 새로 정한다 */

static SkwrInfo gSkwr[kMaxSkwr];
static UInt16   gSkwrN = 0;

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

static void SetLast(const Char *s)
{
    gHoldLast = true;
    StrNCopy(gLast, s, kLineLen - 1);
    gLast[kLineLen - 1] = 0;
}

static UInt16 PagesOf(LineBuf *b)
{
    return (b->n == 0) ? 1 : (UInt16)((b->n + kVisible - 1) / kVisible);
}

static UInt16 TotalPages(void)
{
    if (!gDebug)
        return 1;                                    /* Main만 */
    return 1 + PagesOf(&gInfo) + PagesOf(&gLibr) + 4; /* Main + Info + Libr + SkWr + SkEv + SkRaw + Events */
}

/* 현재 페이지 종류와, Info/Libr이면 버퍼와 하위 페이지 번호 */
static UInt16 PageKind(LineBuf **bufP, UInt16 *subP, UInt16 *subNP)
{
    UInt16 pi = PagesOf(&gInfo);
    UInt16 pl = PagesOf(&gLibr);
    UInt16 pg;

    *bufP = NULL; *subP = 0; *subNP = 1;
    if (gPage == 0 || !gDebug)
        return kPgMain;
    pg = gPage - 1;
    if (pg < pi) {
        *bufP = &gInfo; *subP = pg; *subNP = pi;
        return kPgInfo;
    }
    if (pg < pi + pl) {
        *bufP = &gLibr; *subP = pg - pi; *subNP = pl;
        return kPgLibr;
    }
    if (pg == pi + pl)
        return kPgSkwr;
    if (pg == pi + pl + 1)
        return kPgSkev;
    if (pg == pi + pl + 2)
        return kPgSkraw;
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

/* 설치된 SilkWare 플러그인(SkWr) 목록 */
static void CollectSkwr(void)
{
    DmSearchStateType st;
    UInt16  card;
    LocalID id;
    Boolean first = true;
    UInt32  crid;

    gSkwrN = 0;
    while (gSkwrN < kMaxSkwr &&
           DmGetNextDatabaseByTypeCreator(first, &st, kSkwrDBType, 0,
                                          false, &card, &id) == errNone) {
        first = false;
        crid = 0;
        gSkwr[gSkwrN].name[0] = 0;
        DmDatabaseInfo(card, id, gSkwr[gSkwrN].name, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, &crid);
        gSkwr[gSkwrN].creator = crid;
        gSkwrN++;
    }
}

/* 실크 비활성화 스위치 DB가 있으면 그 LocalID, 없으면 0 */
static LocalID FindDisS(UInt16 *cardP)
{
    DmSearchStateType st;
    LocalID id = 0;

    if (DmGetNextDatabaseByTypeCreator(true, &st, kDisSType, kDisSCreator,
                                       false, cardP, &id) != errNone)
        return 0;
    return id;
}

/* 옛 복구 앱 DiagRescue만 이름으로 지운다 (To Do 데이터는 건드리지 않음) */
static void DeleteRescue(void)
{
    LocalID id = DmFindDatabase(0, kRescueName);
    Char    s[kLineLen];

    if (id == 0)
        StrCopy(s, "DiagRescue: not found");
    else
        StrPrintF(s, "DiagRescue delete e=%x", (UInt16)DmDeleteDatabase(0, id));
    SetLast(s);
    StrCopy(NextLine(&gLog), s);
}

static void ToggleDisS(void)
{
    UInt16  card = 0;
    LocalID id = FindDisS(&card);
    Err     err;
    Char    s[kLineLen];

    if (id) {
        err = DmDeleteDatabase(card, id);
        StrPrintF(s, "DisS delete e=%x (reset!)", (UInt16)err);
    } else {
        err = DmCreateDatabase(0, kDisSName, kDisSCreator, kDisSType, false);
        StrPrintF(s, "DisS create e=%x (reset!)", (UInt16)err);
    }
    SetLast(s);
    StrCopy(NextLine(&gLog), s);
}

static void ClearRecord(void)
{
    Err    err = FtrUnregister(kSkCreator, kSkFtrNum);
    UInt16 i;
    Char   s[kLineLen];

    for (i = kSkFtrCount0; i <= kSkFtrMax; i++)
        FtrUnregister(kSkCreator, i);
    StrPrintF(s, "clear record e=%x (+counters)", (UInt16)err);
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

static void Hex8(UInt32 v, Char *o);
static UInt32 SkFtr(UInt16 num);

static void DrawRow(UInt16 row, const Char *s)
{
    WinDrawChars(s, StrLen(s), 0, (row + 1) * kLineH);
}

/* 버튼 안쪽 영역 (테두리는 이 바깥 1픽셀, 한 줄 높이 안에 들어간다) */
static void BtnRect(UInt16 row, Int16 w, RectangleType *r)
{
    RctSetRectangle(r, 3, (row + 1) * kLineH + 1, w, kLineH - 2);
}

/* 둥근 테두리 버튼 */
static void DrawButton(UInt16 row, const Char *label)
{
    RectangleType r;
    Int16         w = FntCharsWidth(label, StrLen(label)) + 6;

    BtnRect(row, w, &r);
    WinDrawChars(label, StrLen(label), 6, (row + 1) * kLineH);
    WinDrawRectangleFrame(roundFrame, &r);
    if (row < kVisible)
        gBtnW[row] = w;
}

/* 버튼 반전(누름 표시) */
static void InvertButton(UInt16 row)
{
    RectangleType r;

    if (row >= kVisible || gBtnW[row] == 0)
        return;
    BtnRect(row, gBtnW[row], &r);
    WinInvertRectangle(&r, 0);
}

/* 네모 체크박스 + 이름 */
static void DrawCheck(UInt16 row, const Char *label, Boolean on)
{
    RectangleType r;
    Coord         y = (row + 1) * kLineH;

    RctSetRectangle(&r, 3, y + 2, 7, 7);
    WinDrawRectangleFrame(simpleFrame, &r);
    if (on) {
        WinDrawLine(4, y + 5, 5, y + 7);
        WinDrawLine(5, y + 7, 9, y + 2);
        WinDrawLine(4, y + 6, 5, y + 8);
        WinDrawLine(5, y + 8, 9, y + 3);
    }
    WinDrawChars(label, StrLen(label), 14, y);
}

static void DrawSkwrPage(void)
{
    Char    s[kLineLen], cc[5];
    UInt32  v = 0;
    Err     err;
    UInt16  i, card;

    err = FtrGet(kSkCreator, kSkFtrNum, &v);
    if (err != errNone) {
        StrPrintF(s, "record: none (e=%x)", (UInt16)err);
        DrawRow(0, s);
        DrawRow(1, "= not loaded / no Open / write fail");
    } else if ((v >> 16) != kSkMagic) {
        StrPrintF(s, "record: %lx (unknown fmt)", v);
        DrawRow(0, s);
    } else {
        StrPrintF(s, "record: %lx", v);
        DrawRow(0, s);
        /* build = 로더(SkWr) 빌드, core = 본체 빌드(자판을 한 번 켠 뒤 기록, #55) */
        StrPrintF(s, "OK evt=%u build=%u core=%u", (UInt16)((v >> 8) & 0xFF),
                  (UInt16)(v & 0xFF), (UInt16)(SkFtr(55) & 0xFF));
        DrawRow(1, s);
    }
    DrawButton(kSkRowClear, "Clear Record");
    if (DmFindDatabase(0, kRescueName))
        DrawButton(kSkRowRescue, "Delete DiagRescue");

    card = 0;
    if (FindDisS(&card))
        DrawButton(kSkRowDisS, "DisS: ON (silk off)");
    else
        DrawButton(kSkRowDisS, "DisS: off (normal)");
    DrawRow(kSkRowDisS + 1, "(switch needs reset)");

    StrPrintF(s, "-- SkWr installed: %u --", gSkwrN);
    DrawRow(kSkRowFirst - 1, s);
    for (i = 0; i < gSkwrN && kSkRowFirst + i < kVisible; i++) {
        FourCC(gSkwr[i].creator, cc);
        StrPrintF(s, "%s %s", cc, gSkwr[i].name);
        DrawRow(kSkRowFirst + i, s);
    }
}

static UInt32 SkFtr(UInt16 num)
{
    UInt32 v = 0;

    if (FtrGet(kSkCreator, num, &v) != errNone)
        v = 0;
    return v;
}

/* 이벤트 번호 0~15별 수신 횟수: 한 줄에 4개씩 */
static void DrawSkevPage(void)
{
    Char   s[kLineLen];
    UInt16 r, i;
    UInt32 last;

    DrawRow(0, "evt: count  (0~15)");
    for (r = 0; r < 4; r++) {
        i = r * 4;
        StrPrintF(s, "%u:%lu %u:%lu %u:%lu %u:%lu",
                  i, SkFtr(kSkFtrCount0 + i), i + 1, SkFtr(kSkFtrCount0 + i + 1),
                  i + 2, SkFtr(kSkFtrCount0 + i + 2), i + 3, SkFtr(kSkFtrCount0 + i + 3));
        DrawRow(1 + r, s);
    }
    StrPrintF(s, ">=16: %lu  out!=0: %lu", SkFtr(kSkFtrOver), SkFtr(kSkFtrOut));
    DrawRow(6, s);
    last = SkFtr(kSkFtrLast);
    StrPrintF(s, "last evt=%u out=%u", (UInt16)(last & 0xFF), (UInt16)(last >> 16));
    DrawRow(7, s);
    {
        /* 7단계: #48 = (키 번호 << 16) | 코드.  키 번호 = 줄*16+칸+1, st = #49 자판 상태 */
        UInt32 kl = SkFtr(48);
        Char   h4[12];

        Hex8(kl & 0xFFFF, h4);
        StrPrintF(s, "keys:%lu k=%u c=%s st=%x", SkFtr(47), (UInt16)(kl >> 16), h4 + 4,
                  (UInt16)SkFtr(49));
        DrawRow(9, s);
        /* 8단계: #50 조합 상태(초성·중성·종성 자모 번호, 화면 글자 수) */
        Hex8(SkFtr(50), h4);
        StrPrintF(s, "comp=%s", h4);
        DrawRow(8, s);
    }
    {
        /* #52 HR 라이브러리(오류<<16 | 참조 번호), #53 HRWinGetWindowExtent 원래 값, #54 고해상도 표시 */
        UInt32 hr = SkFtr(52), ex = SkFtr(53);

        StrPrintF(s, "hr ref=%u e=%x raw=%ux%u hi=%lu", (UInt16)hr, (UInt16)(hr >> 16),
                  (UInt16)(ex >> 16), (UInt16)ex, SkFtr(54));
        DrawRow(10, s);
    }
    {
        /* 10단계: 로더가 불러온 본체. #55 본체 빌드, #56 불러오기 결과(1 성공 2 DB없음 3 리소스없음 4 버전 5 잠금) */
        UInt32 cb = SkFtr(55);

        StrPrintF(s, "core build=%u load=%lu %s", (UInt16)(cb & 0xFF), SkFtr(56),
                  SkFtr(58) ? "on" : "off");
        DrawRow(11, s);
    }
}

/* 32비트 값을 항상 8자리 16진수로 (StrPrintF의 자릿수 지정에 의존하지 않음) */
static void Hex8(UInt32 v, Char *o)
{
    Int16 i;

    for (i = 7; i >= 0; i--) {
        UInt8 d = (UInt8)(v & 0xF);
        o[i] = (Char)(d < 10 ? '0' + d : 'A' + d - 10);
        v >>= 4;
    }
    o[8] = 0;
}

/* 이벤트 앞 16바이트(기능 번호 base~base+3)를 두 줄로 표시 */
static void DrawRaw(UInt16 row, const Char *tag, UInt16 base)
{
    Char s[kLineLen], a[9], b[9];

    Hex8(SkFtr(base), a);
    Hex8(SkFtr(base + 1), b);
    StrPrintF(s, "%s %s %s", tag, a, b);
    DrawRow(row, s);
    Hex8(SkFtr(base + 2), a);
    Hex8(SkFtr(base + 3), b);
    StrPrintF(s, "   %s %s", a, b);
    DrawRow(row + 1, s);
}

static void DrawSkrawPage(void)
{
    Char   s[kLineLen], a[9], b[9];
    UInt32 wh = SkFtr(29);

    DrawRaw(0, "Op", 20);
    DrawRaw(2, "St", 24);
    Hex8(SkFtr(28), a);
    Hex8(SkFtr(30), b);
    StrPrintF(s, "arg=%s prv=%s", a, b);
    DrawRow(4, s);
    StrPrintF(s, "win %u x %u  bnd %u,%u %ux%u r=%lu", (UInt16)(wh >> 16), (UInt16)(wh & 0xFFFF),
              (UInt16)(SkFtr(43) >> 16), (UInt16)(SkFtr(43) & 0xFFFF),
              (UInt16)(SkFtr(44) >> 16), (UInt16)(SkFtr(44) & 0xFFFF), SkFtr(45));
    DrawRow(5, s);
    DrawRaw(6, "Dn", 31);
    DrawRaw(8, "Up", 35);
    DrawRaw(10, "Dc", 39);
}

/* 본체 DB(SkCo/MnSk)가 있으면 true, 버전 */
static Boolean CoreDB(UInt16 *verP)
{
    DmSearchStateType st;
    UInt16  card = 0;
    LocalID id = 0;

    *verP = 0;
    if (DmGetNextDatabaseByTypeCreator(true, &st, kCoreType, kSkCreator, true, &card, &id) != errNone ||
        id == 0)
        return false;
    DmDatabaseInfo(card, id, NULL, NULL, verP, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    return true;
}

/* 줄 = 앞말 + 뒷말 (gcc 2.95가 문자열 주소를 변수에 골라 담는 코드에서 내부 오류를 내서, 바로 그린다) */
static void Row2(UInt16 row, const Char *a, const Char *b)
{
    Char s[kLineLen];

    StrCopy(s, a);
    StrCat(s, b);
    DrawRow(row, s);
}

static void DrawMainPage(void)
{
    Char    s[kLineLen];
    UInt32  rec = SkFtr(kSkFtrNum), pr = SkFtr(60), ld = SkFtr(56), cb = SkFtr(55);
    Boolean loaderDB = false;
    UInt16  i, ver;

    for (i = 0; i < gSkwrN; i++)
        if (gSkwr[i].creator == kSkCreator)
            loaderDB = true;

    if (!loaderDB)
        Row2(1, "loader: ", "not installed");
    else if ((rec >> 16) != kSkMagic)
        Row2(1, "loader: ", "installed, reset needed");
    else {
        StrPrintF(s, "loader: build %u", (UInt16)(rec & 0xFF));
        DrawRow(1, s);
        if ((pr >> 16) != 0x5052)
            Row2(2, "  protect: ", "on (old loader)");
        else if ((pr & 0xFFFF) == 0)
            Row2(2, "  protect: ", "released");
        else if ((pr & 0xFFFF) == 0xFFFF)
            Row2(2, "  protect: ", "? (db not found)");
        else
            Row2(2, "  protect: ", "error");
    }

    if (!CoreDB(&ver))
        Row2(4, "core: ", "not installed");
    else {
        if ((cb >> 16) == kSkMagic)
            StrPrintF(s, "core: build %u (abi %u)", (UInt16)(cb & 0xFF), ver);
        else
            StrPrintF(s, "core: installed (abi %u)", ver);
        DrawRow(4, s);
    }

    if (ld == 0)      Row2(5, "  load: ", "- (turn keyboard on)");
    else if (ld == 1) Row2(5, "  load: ", "ok");
    else if (ld == 2) Row2(5, "  load: ", "core not found");
    else if (ld == 3) Row2(5, "  load: ", "core resource missing");
    else if (ld == 4) Row2(5, "  load: ", "core version mismatch");
    else              Row2(5, "  load: ", "lock failed");

    Row2(6, "keyboard: ", SkFtr(58) ? "on" : "off");
    DrawButton(kMainRowReset, "Soft Reset (hold 3s)");
    DrawCheck(kMainRowDebug, "Debug", gDebug);
    DrawRow(10, "core: off > install > on");
    DrawRow(11, "loader: off > install > reset");
}

static void Draw(void)
{
    RectangleType r;
    LineBuf *b;
    UInt16   sub, subN, kind;
    Int16    i, start;
    Char     title[kLineLen];

    kind = PageKind(&b, &sub, &subN);
    MemSet(gBtnW, sizeof(gBtnW), 0);

    RctSetRectangle(&r, 0, 0, kScreenW, kScreenW);
    WinEraseRectangle(&r, 0);

    switch (kind) {
    case kPgMain:
        StrPrintF(title, "%u/%u MnSk keyboard", gPage + 1, TotalPages());
        DrawMainPage();
        break;
    case kPgInfo:
    case kPgLibr:
        StrPrintF(title, "%u/%u %s %u/%u", gPage + 1, TotalPages(),
                  (kind == kPgInfo) ? "Info" : "Libr", sub + 1, subN);
        start = (Int16)(sub * kVisible);
        for (i = 0; i < kVisible && start + i < (Int16)b->n; i++)
            DrawRow((UInt16)i, b->t[start + i]);
        break;
    case kPgSkwr:
        StrPrintF(title, "%u/%u SkWr", gPage + 1, TotalPages());
        DrawSkwrPage();
        break;
    case kPgSkev:
        StrPrintF(title, "%u/%u SkEv", gPage + 1, TotalPages());
        DrawSkevPage();
        break;
    case kPgSkraw:
        StrPrintF(title, "%u/%u SkRaw", gPage + 1, TotalPages());
        DrawSkrawPage();
        break;
    default:
        StrPrintF(title, "%u/%u Events INT:%s", gPage + 1, TotalPages(),
                  gIntercept ? "ON" : "off");
        start = (gLog.n > kEvRowInt) ? (Int16)(gLog.n - kEvRowInt) : 0;
        for (i = 0; i < kEvRowInt && start + i < (Int16)gLog.n; i++)
            DrawRow((UInt16)i, gLog.t[start + i]);
        DrawButton(kEvRowInt, gIntercept ? "INT: ON (silk blocked)" : "INT: off");
        break;
    }

    /* 제목줄과 하단 상태줄은 마지막에 그린다 */
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

/* "[soft reset]" 줄을 누르고 있는 동안 남은 초를 보여 주고, 다 채우면 소프트 리셋. 떼면 취소 */
static void HoldReset(void)
{
    UInt32        t0 = TimGetTicks(), tps = SysTicksPerSecond();
    Int16         x, y, shown = -1, left;
    Boolean       down = true;
    RectangleType r;
    Char          s[kLineLen];

    RctSetRectangle(&r, 0, (kMainRowReset + 1) * kLineH, kScreenW, kLineH);
    while (down) {
        left = (Int16)(kResetHoldSec - (TimGetTicks() - t0) / tps);
        if (left <= 0) {
            WinEraseRectangle(&r, 0);
            DrawButton(kMainRowReset, "Resetting...");
            InvertButton(kMainRowReset);
            SysReset();
            return;
        }
        if (left != shown) {
            WinEraseRectangle(&r, 0);
            StrPrintF(s, "Reset in %d (release: cancel)", left);
            DrawButton(kMainRowReset, s);
            InvertButton(kMainRowReset);
            shown = left;
        }
        SysTaskDelay(tps / 20);
        EvtGetPen(&x, &y, &down);
    }
    SetLast("reset cancelled");
    Draw();
}

/* 앱 영역 탭 처리 */
static void HandleAppTap(Int16 x, Int16 y)
{
    LineBuf *b;
    UInt16   sub, subN;
    UInt16   kind = PageKind(&b, &sub, &subN);
    Int16    row = y / kLineH - 1;     /* 내용 영역 줄 번호 */

    /* 버튼이면 잠깐 반전해 눌린 것을 보여 준다 (리셋 버튼은 HoldReset이 따로 표시) */
    if (row >= 0 && row < kVisible && gBtnW[row] != 0 && !(kind == kPgMain && row == kMainRowReset)) {
        InvertButton((UInt16)row);
        SysTaskDelay(SysTicksPerSecond() / 8);
    }

    if (kind == kPgMain) {
        if (row == kMainRowReset) {
            HoldReset();
            return;
        }
        if (row == kMainRowDebug) {
            gDebug = !gDebug;
            PrefSetAppPreferences(kAppCreator, kPrefDebug, 1, &gDebug, sizeof(gDebug), true);
            gPage = 0;
            Draw();
        } else if (gDebug) {
            GoPage(1);
        }
        return;
    }

    if (kind == kPgEvents) {
        if (row == kEvRowInt) {
            gIntercept = !gIntercept;
            StrPrintF(NextLine(&gLog), "== INT %s ==", gIntercept ? "ON" : "off");
            Draw();
        } else {
            GoPage(1);
        }
        return;
    }

    if (kind == kPgSkwr) {
        if (row == kSkRowClear) {
            ClearRecord();
            Draw();
            return;
        }
        if (row == kSkRowDisS) {
            ToggleDisS();
            Draw();
            return;
        }
        if (row == kSkRowRescue && DmFindDatabase(0, kRescueName)) {
            DeleteRescue();
            CollectSkwr();
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

    {
        UInt16 size = sizeof(gDebug);

        if (PrefGetAppPreferences(kAppCreator, kPrefDebug, &gDebug, &size, true) == noPreferenceFound)
            gDebug = false;
    }
    CollectInfo();
    CollectLibr();
    CollectSkwr();
    Draw();

    do {
        EvtGetEvent(&e, evtWaitForever);

        blocked = gIntercept && IsSilkPen(&e);
        logged = FormatEvent(&e, blocked, line);
        if (logged) {
            StrCopy(NextLine(&gLog), line);
            if (gHoldLast && (e.eType == penUpEvent || e.eType == penDownEvent)) {
                if (e.eType == penUpEvent)
                    gHoldLast = false;
            } else {
                StrNCopy(gLast, line, kLineLen - 1);
                gLast[kLineLen - 1] = 0;
                gHoldLast = false;
            }
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

        if (logged)
            Draw();
    } while (e.eType != appStopEvent);

    return 0;
}
