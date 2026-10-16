/*
 * MnHangul v3 — CJKOS 대체 한글 모듈 시험 앱 (2a 통과 시험 + 2b 한글 자리표시)
 *
 * 같은 PRC 안의 패치 코드 리소스(HgPt 0, src/hangul/patch.c)를 시스템 트랩 BltDrawChars(0xA378)에 끼운다.
 *   켜기: 리소스 잠금 → 원래 함수 주소를 리소스 오프셋 0에 기록(DmWrite) → DB 삭제 보호
 *         → SysSetTrapAddress(0xA378, 리소스+4)
 *   끄기: 트랩이 아직 우리 진입점이면 원래 주소로 되돌림 → 보호 해제 → 잠금 해제
 * 시험 기간에는 부팅 때 자동으로 켜지 않는다. 문제가 생기면 리셋 한 번으로 원래대로 돌아간다.
 * 반드시 CJKOS를 끈 상태(같은 함수를 CJKOS도 바꿈)의 CloudpilotEmu 백업 복사본에서 시험한다.
 *
 * 화면: 상태, [Patch ON] [Patch OFF] [Box ON/OFF] [Clear counters], 패치 기록(feature 'MnHg', patch.c 참고).
 * Box: 패치가 켜진 동안 2바이트 한글을 "[]"로 바꿔 그린다(#22). Patch ON은 기록만 한다.
 * 위/아래 버튼 또는 화면 탭으로 다시 그린다(기록 새로고침).
 * v2: 고해상도 모드(ClieDiag v19와 같은 방식: HR 라이브러리 HRWinScreenMode 320x320, 끝날 때 복귀).
 *     맨 아래 메시지 줄이 화면 밖으로 잘리던 것 수정.
 * v3: Box 버튼, 글꼴(fontType/fRectHeight)·p3·p1 기록 표시(저해상도/고해상도 좌표 구분 단서).
 */

#include <PalmOS.h>

#define kHgCreator    'MnHg'
#define kPatchRes     'HgPt'
#define kTrapBlt      0xA378
#define kFtrHandle    20
#define kFtrEntry     21
#define kFtrMax       19
#define kFtrBox       22

#define kLineH        11
#define kRowOn        2
#define kRowOff       3
#define kRowBox       4
#define kRowClear     5

static Char gMsg[40] = "-";

/* 고해상도 모드 (소니 헤더 없이 필요한 트랩만 선언, sysLibTrapCustom + 순번) */
Err HROpen(UInt16 ref)  SYS_TRAP(sysLibTrapOpen);
Err HRClose(UInt16 ref) SYS_TRAP(sysLibTrapClose);
Err HRWinScreenMode(UInt16 ref, WinScreenModeOperation op, UInt32 *w, UInt32 *h,
                    UInt32 *depth, Boolean *color) SYS_TRAP(sysLibTrapCustom + 48);
static UInt16 gHrRef = 0xFFFF;

static void StartHires(void)
{
    UInt16 ref;
    UInt32 w = 320, h = 320;

    if (SysLibFind("Sony HR Library", &ref) != errNone || HROpen(ref) != errNone)
        return;
    if (HRWinScreenMode(ref, winScreenModeSet, &w, &h, NULL, NULL) != errNone) {
        HRClose(ref);
        return;
    }
    gHrRef = ref;
}

static void StopHires(void)
{
    if (gHrRef == 0xFFFF)
        return;
    HRWinScreenMode(gHrRef, winScreenModeSetToDefaults, NULL, NULL, NULL, NULL);
    HRClose(gHrRef);
    gHrRef = 0xFFFF;
}

static UInt32 GetF(UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(kHgCreator, n, &v) != errNone)
        v = 0;
    return v;
}

static void Row(UInt16 row, const Char *s)
{
    WinDrawChars(s, StrLen(s), 2, (row + 1) * kLineH);
}

static void Button(UInt16 row, const Char *s)
{
    RectangleType r;

    RctSetRectangle(&r, 3, (row + 1) * kLineH + 1, FntCharsWidth(s, StrLen(s)) + 6, kLineH - 2);
    WinDrawChars(s, StrLen(s), 6, (row + 1) * kLineH);
    WinDrawRectangleFrame(roundFrame, &r);
}

/* 앱 DB(card, id) */
static Boolean AppDB(UInt16 *card, LocalID *id)
{
    DmSearchStateType st;

    return DmGetNextDatabaseByTypeCreator(true, &st, sysFileTApplication, kHgCreator, true, card, id) == errNone;
}

static Boolean Installed(void)
{
    UInt32 e = GetF(kFtrEntry);

    return e != 0 && (UInt32)SysGetTrapAddress(kTrapBlt) == e;
}

static void PatchOn(void)
{
    MemHandle h;
    UInt8    *p;
    UInt32    orig;
    UInt16    card;
    LocalID   id;
    Err       err;

    if (Installed()) {
        StrCopy(gMsg, "already ON");
        return;
    }
    h = DmGetResource(kPatchRes, 0);
    if (h == NULL) {
        StrCopy(gMsg, "patch resource missing");
        return;
    }
    p = MemHandleLock(h);
    orig = (UInt32)SysGetTrapAddress(kTrapBlt);
    err = DmWrite(p, 0, &orig, sizeof(orig));
    if (err != errNone) {
        MemHandleUnlock(h);
        DmReleaseResource(h);
        StrPrintF(gMsg, "DmWrite e=%x", err);
        return;
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, true);
    FtrSet(kHgCreator, kFtrHandle, (UInt32)h);
    FtrSet(kHgCreator, kFtrEntry, (UInt32)(p + 4));
    err = SysSetTrapAddress(kTrapBlt, p + 4);
    StrPrintF(gMsg, "ON e=%x orig=%lx", err, orig);
}

static void PatchOff(void)
{
    UInt32    e = GetF(kFtrEntry), h = GetF(kFtrHandle);
    UInt32    orig;
    UInt16    card;
    LocalID   id;

    if (e == 0) {
        StrCopy(gMsg, "not ON");
        return;
    }
    if ((UInt32)SysGetTrapAddress(kTrapBlt) != e) {
        StrCopy(gMsg, "trap changed by other: kept");
        return;
    }
    orig = *(const UInt32 *)(e - 4);
    SysSetTrapAddress(kTrapBlt, (void *)orig);
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, false);
    if (h != 0) {
        MemHandleUnlock((MemHandle)h);
        DmReleaseResource((MemHandle)h);
    }
    FtrSet(kHgCreator, kFtrEntry, 0);
    FtrSet(kHgCreator, kFtrHandle, 0);
    StrPrintF(gMsg, "OFF orig=%lx", orig);
}

static void ToggleBox(void)
{
    if (GetF(kFtrBox) != 0) {
        FtrSet(kHgCreator, kFtrBox, 0);
        StrCopy(gMsg, "box off");
    } else {
        FtrSet(kHgCreator, kFtrBox, 1);
        StrCopy(gMsg, Installed() ? "box on" : "box on (needs Patch ON)");
    }
}

static void ClearCounters(void)
{
    UInt16 i;

    for (i = 0; i <= kFtrMax; i++)
        FtrUnregister(kHgCreator, i);   /* #20~#22(핸들·진입점·Box)는 지우지 않는다 */
    StrCopy(gMsg, "counters cleared");
}

static void Hex(Char *o, UInt32 v)
{
    StrPrintF(o, "%lx", v);
}

/* x<<16|y → "x,y" (부호 있음) */
static void XY(Char *o, UInt32 v)
{
    if (v == 0xFFFFFFFFUL)
        StrCopy(o, "null");
    else
        StrPrintF(o, "%d,%d", (Int16)(v >> 16), (Int16)(v & 0xFFFF));
}

/* w<<16|h → "wxh" */
static void WH(Char *o, UInt32 v)
{
    if (v == 0xFFFFFFFFUL)
        StrCopy(o, "null");
    else
        StrPrintF(o, "%dx%d", (Int16)(v >> 16), (Int16)(v & 0xFFFF));
}

/* fontType<<16|fRectHeight → "type/h" */
static void Font(Char *o, UInt32 v)
{
    StrPrintF(o, "%x/%d", (UInt16)(v >> 16), (Int16)(v & 0xFFFF));
}

static void Draw(void)
{
    RectangleType r;
    Char          s[48], a[16], b[16], c[16];

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnHangul v3 (2b) CJKOS OFF only", 31, 0, 0);

    StrPrintF(s, "patch: %s  box: %s  A378=%lx", Installed() ? "ON" : "off",
              GetF(kFtrBox) ? "ON" : "off", (UInt32)SysGetTrapAddress(kTrapBlt));
    Row(0, s);
    Button(kRowOn, "Patch ON");
    Button(kRowOff, "Patch OFF");
    Button(kRowBox, GetF(kFtrBox) ? "Box OFF" : "Box ON");
    Button(kRowClear, "Clear counters");

    StrPrintF(s, "calls %lu  hi %lu  boxed %lu", GetF(0), GetF(9), GetF(19));
    Row(6, s);
    Hex(a, GetF(2));
    StrPrintF(s, "last len %lu  %s", GetF(1), a);
    Row(7, s);
    XY(a, GetF(4));
    WH(b, GetF(5));
    XY(c, GetF(6));
    StrPrintF(s, "rect %s %s  p3 %s", a, b, c);
    Row(8, s);
    Font(a, GetF(14));
    Hex(b, GetF(7));
    StrPrintF(s, "font %s  p1 %s", a, b);
    Row(9, s);
    Hex(b, GetF(11));
    StrPrintF(s, "hi: len %lu  %s", GetF(10), b);
    Row(10, s);
    XY(a, GetF(12));
    WH(b, GetF(13));
    XY(c, GetF(16));
    StrPrintF(s, "hi rect %s %s  p3 %s", a, b, c);
    Row(11, s);
    Font(a, GetF(15));
    Hex(b, GetF(17));
    StrPrintF(s, "hi font %s  p1 %s", a, b);
    Row(12, s);

    WinDrawInvertedChars(gMsg, StrLen(gMsg), 0, 13 * kLineH);   /* 내용 13줄(0~12) 아래 */
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    EventType e;

    if (cmd != sysAppLaunchCmdNormalLaunch)
        return 0;

    StartHires();
    Draw();
    do {
        EvtGetEvent(&e, evtWaitForever);
        if (e.eType == penDownEvent && e.screenY >= 0 && e.screenY < 160) {
            Int16 row = e.screenY / kLineH - 1;

            if (row == kRowOn)
                PatchOn();
            else if (row == kRowOff)
                PatchOff();
            else if (row == kRowBox)
                ToggleBox();
            else if (row == kRowClear)
                ClearCounters();
            Draw();
            continue;
        }
        if (!SysHandleEvent(&e) && e.eType == keyDownEvent &&
            (e.data.keyDown.chr == vchrPageUp || e.data.keyDown.chr == vchrPageDown))
            Draw();
    } while (e.eType != appStopEvent);
    StopHires();
    return 0;
}
