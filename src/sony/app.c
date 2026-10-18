/*
 * MnSonyKbd v2 — Sony 가상 키보드 한글 (NR70V ROM SilkExtn 키보드)
 *
 * MnHangul v35~v53의 Sony 가상 키보드 부분을 따로 뗀 모듈(MnHangul v54와 같이 나옴).
 * 한글 표시(글꼴)는 하지 않으므로 CJKOS 등 한글 시스템이나 MnHangul(한글 표시)과 같이 쓴다.
 *   SyPt 0 (spatch.c) BltDrawChars: 가상 실크 창의 영문 키 글자를 두벌식 자모로(그 밖은 아래 함수로 넘김)
 *   SyIm 0 (ime.c)    EvtGetEvent: 영문 키 → 두벌식 조합 → 완성형 2바이트 입력 / WinDrawBitmap: ⇪ 그림 → '한A'
 *   SyIc 0·1 '한A' 그림(tools/gen-caps-icon.py), SyJm 0 자모 그림 16×16(tools/gen-sony-jamo.py)
 *
 * 화면(v2, 쓰는 것만): [ON] [OFF], Start at reset(앱 설정 0번, 기본 ON), Update(다음 리셋 한 번 자동 시작
 *   건너뛰기, 앱 설정 3번), 사용법('한A' 그림). 제목을 누르면 판정 기록 화면.
 * 기록(feature 'MnSy'): #1 패치 진입점, #2 패치 핸들, #3 자모 그림, #4 자모 핸들, #5 ime 코드, #6 ime 핸들,
 *   #7 ime 상태 버퍼, #8·#9 '한A' 그림 핸들, #10 리셋 자동 시작 결과, #106 이 앱 실행 중,
 *   그 밖(#108·#127·#128·#143~#152·#160~#177)은 spatch.c·ime.c·dubeol.h 참고.
 */

#include <PalmOS.h>

#define kSyCr        'MnSy'
#define kTrapBlt     0xA378
#define kPatchRes    'SyPt'
#define kImRes       'SyIm'
#define kIconRes     'SyIc'
#define kJamoRes     'SyJm'
#define kFtrEntry    1
#define kFtrPatchH   2
#define kFtrJamo     3
#define kFtrJamoH    4
#define kFtrImC      5
#define kFtrImH      6
#define kFtrImBuf    7
#define kFtrIcH0     8
#define kFtrBoot     10
#define kFtrRun      106
#define kFtrBlt      152
#define kImBufSz     16
#define kRomBlt      0x1008359AUL   /* NR70V ROM BltDrawChars(MnHangul v24 실측) */

#define kLineH        11
#define kRowOn        3
#define kRowAuto      4
#define kRowUpd       5
#define kResetHoldSec 3

static Char    gMsg[40] = "-";
static Boolean gDebug = false;

/* 고해상도 모드 (소니 헤더 없이 필요한 트랩만, MnHangul과 같음) */
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

/* ---------- 리셋 알림에서도 부르는 함수: 전역변수·문자열 상수 금지(빌드 스크립트 검사) ---------- */

static UInt32 GetF(UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(kSyCr, n, &v) != errNone)
        v = 0;
    return v;
}

static UInt16 PrefGet(UInt16 id, UInt16 def)
{
    UInt16 v = 0, size = sizeof v;

    if (PrefGetAppPreferences(kSyCr, id, &v, &size, true) == noPreferenceFound || size != sizeof v)
        v = def;
    return v;
}

static void PrefPut(UInt16 id, UInt16 v)
{
    PrefSetAppPreferences(kSyCr, id, 1, &v, sizeof v, true);
}

static Boolean AppDB(UInt16 *card, LocalID *id)
{
    DmSearchStateType st;

    return DmGetNextDatabaseByTypeCreator(true, &st, sysFileTApplication, kSyCr, true, card, id) == errNone;
}

static void Protect(Boolean on)
{
    UInt16  card;
    LocalID id;

    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, on);
}

/* 자모 그리기 함수: 아래 함수가 ROM 범위면 그것, 아니면 NR70V ROM 0x1008359A 시작 바이트가 맞을 때만(MnHangul v52) */
static UInt32 RomBlt(UInt32 below)
{
    const UInt16 *q = (const UInt16 *)kRomBlt;

    if (below >= 0x10000000UL && below < 0x10800000UL)
        return below;
    if (q[0] == 0x4E56 && q[1] == 0xFFF0 && q[2] == 0x48E7 && q[3] == 0x1E30)
        return kRomBlt;
    return 0;
}

/* 키 글자 오버레이 켜기. 0 켬, 1 이미, 2 리소스 없음 */
static UInt16 PatchOn(void)
{
    MemHandle h, jh;
    UInt8    *p;
    UInt32    below;

    if (GetF(kFtrEntry) != 0)
        return 1;
    h = DmGetResource(kPatchRes, 0);
    jh = DmGetResource(kJamoRes, 0);
    if (h == NULL || jh == NULL) {
        if (h != NULL)
            DmReleaseResource(h);
        if (jh != NULL)
            DmReleaseResource(jh);
        return 2;
    }
    p = MemHandleLock(h);
    below = (UInt32)SysGetTrapAddress(kTrapBlt);
    DmWrite(p, 0, &below, 4);
    FtrSet(kSyCr, kFtrBlt, RomBlt(below));
    FtrSet(kSyCr, kFtrJamoH, (UInt32)jh);
    FtrSet(kSyCr, kFtrJamo, (UInt32)MemHandleLock(jh));
    FtrSet(kSyCr, kFtrPatchH, (UInt32)h);
    FtrSet(kSyCr, kFtrEntry, (UInt32)(p + 4));
    Protect(true);
    SysSetTrapAddress(kTrapBlt, p + 4);
    return 0;
}

/* 입력·'한A' 켜기 (MnHangul v35~v43 ImOn). 0 켬, 1 이미, 2 리소스 없음 */
static UInt16 ImOn(void)
{
    MemHandle h;
    UInt8    *code;
    UInt32   *buf;
    void     *o;
    UInt32    a[4];
    UInt16    k;
    DmOpenRef sx;

    if (GetF(kFtrImC) != 0)
        return 1;
    h = DmGetResource(kImRes, 0);
    if (h == NULL)
        return 2;
    code = MemHandleLock(h);
    o = SysGetTrapAddress(sysTrapEvtGetEvent);
    DmWrite(code, 8, &o, 4);
    o = SysGetTrapAddress(sysTrapWinDrawBitmap);
    DmWrite(code, 12, &o, 4);
    buf = MemPtrNew(kImBufSz);
    if (buf != NULL) {
        MemSet(buf, kImBufSz, 0);
        MemPtrSetOwner(buf, 0);
    }
    DmWrite(code, 16, &buf, 4);
    /* ROM SilkExtn의 ⇪ 그림(Tbmp 2400 보통·2500 반전) 주소와 대신 그릴 '한A' 그림 */
    a[0] = a[1] = a[2] = a[3] = 0;
    sx = DmOpenDatabaseByTypeCreator('extn', 'SeSi', dmModeReadOnly);
    if (sx != NULL) {
        for (k = 0; k < 2; k++) {
            MemHandle bh = DmGet1Resource('Tbmp', 2400 + 100 * k);

            if (bh != NULL) {
                a[k] = (UInt32)MemHandleLock(bh);   /* ROM 자원: 주소는 그대로 유지 */
                MemHandleUnlock(bh);
                DmReleaseResource(bh);
            }
        }
        DmCloseDatabase(sx);
    }
    for (k = 0; k < 2; k++) {
        MemHandle ih = DmGetResource(kIconRes, k);

        FtrSet(kSyCr, kFtrIcH0 + k, (UInt32)ih);
        if (ih != NULL)
            a[2 + k] = (UInt32)MemHandleLock(ih);
    }
    if (a[0] == 0 || a[1] == 0)
        a[0] = a[1] = 0;
    DmWrite(code, 20, a, 16);
    Protect(true);
    FtrSet(kSyCr, kFtrImH, (UInt32)h);
    FtrSet(kSyCr, kFtrImC, (UInt32)code);
    FtrSet(kSyCr, kFtrImBuf, (UInt32)buf);
    SysSetTrapAddress(sysTrapEvtGetEvent, code + ((UInt32 *)code)[0]);
    SysSetTrapAddress(sysTrapWinDrawBitmap, code + ((UInt32 *)code)[1]);
    return 0;
}

/* 모두 켜기. 결과 = 패치<<4 | 입력 */
static UInt16 SonyOn(void)
{
    UInt16 p = PatchOn();

    return (UInt16)(p << 4) | ImOn();
}

/* ---------- 앱 화면에서만 부르는 함수 ---------- */

/* 0 끔, 1 켜져 있지 않음, 2 다른 패치가 위에 있어 못 끔 */
static UInt16 PatchOff(void)
{
    UInt32 e = GetF(kFtrEntry);

    if (e == 0)
        return 1;
    if ((UInt32)SysGetTrapAddress(kTrapBlt) != e)
        return 2;
    SysSetTrapAddress(kTrapBlt, (void *)*(const UInt32 *)(e - 4));
    MemHandleUnlock((MemHandle)GetF(kFtrJamoH));
    DmReleaseResource((MemHandle)GetF(kFtrJamoH));
    MemHandleUnlock((MemHandle)GetF(kFtrPatchH));
    DmReleaseResource((MemHandle)GetF(kFtrPatchH));
    FtrSet(kSyCr, kFtrEntry, 0);
    FtrSet(kSyCr, kFtrPatchH, 0);
    FtrSet(kSyCr, kFtrJamo, 0);
    FtrSet(kSyCr, kFtrJamoH, 0);
    Protect(false);
    return 0;
}

static UInt16 ImOff(void)
{
    UInt8  *code = (UInt8 *)GetF(kFtrImC);
    void   *buf = (void *)GetF(kFtrImBuf), *z = NULL;
    UInt16  k;

    if (code == NULL)
        return 1;
    if ((UInt8 *)SysGetTrapAddress(sysTrapEvtGetEvent) != code + ((UInt32 *)code)[0] ||
        (UInt8 *)SysGetTrapAddress(sysTrapWinDrawBitmap) != code + ((UInt32 *)code)[1])
        return 2;
    SysSetTrapAddress(sysTrapEvtGetEvent, (void *)((UInt32 *)code)[2]);
    SysSetTrapAddress(sysTrapWinDrawBitmap, (void *)((UInt32 *)code)[3]);
    DmWrite(code, 16, &z, 4);
    for (k = 0; k < 2; k++) {
        MemHandle ih = (MemHandle)GetF(kFtrIcH0 + k);

        if (ih != NULL) {
            MemHandleUnlock(ih);
            DmReleaseResource(ih);
        }
        FtrSet(kSyCr, kFtrIcH0 + k, 0);
    }
    if (buf != NULL)
        MemPtrFree(buf);
    MemHandleUnlock((MemHandle)GetF(kFtrImH));
    DmReleaseResource((MemHandle)GetF(kFtrImH));
    FtrSet(kSyCr, kFtrImH, 0);
    FtrSet(kSyCr, kFtrImC, 0);
    FtrSet(kSyCr, kFtrImBuf, 0);
    Protect(false);
    return 0;
}

static void SonyOff(void)
{
    UInt16 i = ImOff(), p = PatchOff();

    if (i == 2 || p == 2)
        StrCopy(gMsg, "kept: use Update, then install");
    else
        StrCopy(gMsg, "off: ready to install");
}

static Boolean IsOn(void)
{
    return GetF(kFtrEntry) != 0 && GetF(kFtrImC) != 0;
}

static void Row(UInt16 row, const Char *s)
{
    WinDrawChars(s, StrLen(s), 2, (row + 1) * kLineH);
}

static void ButtonAt(UInt16 x, UInt16 row, const Char *s)
{
    RectangleType r;

    RctSetRectangle(&r, x, (row + 1) * kLineH + 1, FntCharsWidth(s, StrLen(s)) + 6, kLineH - 2);
    WinDrawChars(s, StrLen(s), x + 3, (row + 1) * kLineH);
    WinDrawRectangleFrame(roundFrame, &r);
}

/* 3초 누르면 리셋(중간에 떼면 취소). upd = 다음 리셋 한 번 자동 시작 건너뛰기 */
static void HoldReset(UInt16 row, const Char *label, Boolean upd)
{
    UInt32        t0 = TimGetTicks(), tps = SysTicksPerSecond();
    Int16         x, y, shown = -1, left;
    Boolean       down = true;
    RectangleType r, b;
    Char          s[24];
    UInt16        w = FntCharsWidth(label, StrLen(label)) + 6;

    RctSetRectangle(&r, 0, (row + 1) * kLineH, w + 6, kLineH);
    RctSetRectangle(&b, 3, (row + 1) * kLineH + 1, w, kLineH - 2);
    while (down) {
        left = (Int16)(kResetHoldSec - (TimGetTicks() - t0) / tps);
        if (left <= 0)
            StrCopy(s, "Resetting...");
        else
            StrPrintF(s, "Hold... %ds", left);
        if (left != shown || left <= 0) {
            WinEraseRectangle(&r, 0);
            WinDrawChars(s, StrLen(s), 6, (row + 1) * kLineH);
            WinDrawRectangleFrame(roundFrame, &b);
            WinInvertRectangle(&b, 0);
            shown = left;
        }
        if (left <= 0) {
            if (upd)
                PrefPut(3, 1);
            SysReset();
            return;
        }
        SysTaskDelay(tps / 20);
        EvtGetPen(&x, &y, &down);
    }
    StrCopy(gMsg, "reset cancelled");
}

/* v2: 쓰는 것만 — 켜기/끄기, 리셋 뒤 자동 시작, Update, 사용법('한A' 그림). 판정 기록은 제목을 누르면 */
static void DrawMain(void)
{
    RectangleType r;
    Char          s[48];
    MemHandle     ih = DmGetResource(kIconRes, 1);

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnSonyKbd v2", 12, 0, 0);
    Row(0, "Hangul kbd on Sony virtual kbd");
    WinDrawInvertedChars(gMsg, StrLen(gMsg), 0, 2 * kLineH);
    StrPrintF(s, "Now: %s", IsOn() ? "ON" : "off");
    Row(2, s);
    ButtonAt(3, kRowOn, "ON");
    ButtonAt(40, kRowOn, "OFF");
    ButtonAt(3, kRowAuto, PrefGet(0, 1) ? "Start at reset: ON" : "Start at reset: off");
    ButtonAt(3, kRowUpd, "Update (hold 3s)");
    if (ih != NULL) {   /* 사용법: '한A' 그림(켜짐) = 한/영 */
        WinDrawBitmap((BitmapPtr)MemHandleLock(ih), 3, 8 * kLineH);   /* 22×18(저해상도 좌표로 2배) */
        MemHandleUnlock(ih);
        DmReleaseResource(ih);
    }
    WinDrawChars("key = Hangul / English", 22, 28, 8 * kLineH + 4);
    Row(9, "Shift in Hangul = double cons.");
    Row(10, "Update: [Update], install, ON");
}

/* 판정 기록(MnHangul KDlg 7~11줄과 같음) */
static void DrawDebug(void)
{
    RectangleType r;
    Char          s[48];
    UInt32        k16 = GetF(176), w = GetF(128);
    Int16         j;
    UInt16        m = 0;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnSonyKbd v2 Debug [tap: back]", 30, 0, 0);
    StrPrintF(s, "blt below %lx  jamo %lx", GetF(kFtrEntry) ? *(const UInt32 *)(GetF(kFtrEntry) - 4) : 0UL,
              GetF(kFtrBlt));
    Row(0, s);
    StrPrintF(s, "draw %lu  ovl %lu  boot %lx", GetF(108), GetF(127), GetF(kFtrBoot));
    Row(1, s);
    for (j = 15; j >= 0; j--) {   /* 최근 16개, 오래된 것부터 한 줄에 2개 */
        UInt32 v;
        Char   ch[2];

        if ((UInt32)j >= k16)
            continue;
        v = GetF(160 + (UInt16)((k16 - 1 - (UInt32)j) & 15));
        if ((v >> 30) == 1) {
            ch[0] = (Char)((v >> 10) & 0xFF);
            ch[1] = 0;
            StrPrintF(s, "L%s%u H%up%uc%ut%u", ch, (UInt16)((v >> 18) & 0xFFF), (UInt16)((v >> 9) & 1),
                      (UInt16)((v >> 7) & 3), (UInt16)((v >> 6) & 1), (UInt16)((v >> 2) & 0xF));
        } else
            StrPrintF(s, "I%u o%ua%up%u", (UInt16)((v >> 18) & 0xFFF), (UInt16)(v & 1), (UInt16)((v >> 1) & 1),
                      (UInt16)((v >> 7) & 3));
        WinDrawChars(s, StrLen(s), 2 + (m % 2) * 80, (3 + m / 2) * kLineH);
        m++;
    }
    StrPrintF(s, "why%u H%lu S%lx p%lu ic%lu@%lu", (UInt16)(w >> 24), GetF(143), GetF(150), GetF(144),
              GetF(147), GetF(146));
    Row(11, s);
}

static void Draw(void)
{
    if (gDebug)
        DrawDebug();
    else
        DrawMain();
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    EventType e;

    if (cmd == sysAppLaunchCmdSystemReset) {
        if (PrefGet(3, 0)) {   /* Update 리셋: 이번 한 번 켜지 않음(잠금 없음 → 바로 설치 가능) */
            PrefPut(3, 0);
            FtrSet(kSyCr, kFtrBoot, 0x40000000UL);
        } else if (PrefGet(0, 1))
            FtrSet(kSyCr, kFtrBoot, 0x80000000UL | SonyOn());
        return 0;
    }
    if (cmd != sysAppLaunchCmdNormalLaunch)
        return 0;

    FtrSet(kSyCr, kFtrRun, 1);
    StartHires();
    Draw();
    do {
        EvtGetEvent(&e, evtWaitForever);
        if (e.eType == penDownEvent && e.screenY >= 0 && e.screenY < 160) {
            Int16 row = e.screenY / kLineH - 1;

            if (e.screenY < kLineH)   /* 제목 = 판정 기록 화면 ↔ 첫 화면 */
                gDebug = !gDebug;
            else if (gDebug)
                ;
            else if (row == kRowOn && e.screenX >= 40)
                SonyOff();
            else if (row == kRowOn) {
                UInt16 rr = SonyOn();

                StrPrintF(gMsg, rr == 0 ? "ON" : rr == 0x11 ? "already ON" : "ON: result %x", rr);
            } else if (row == kRowAuto) {
                PrefPut(0, PrefGet(0, 1) ? 0 : 1);
                StrCopy(gMsg, PrefGet(0, 1) ? "start at reset: ON" : "start at reset: off");
            } else if (row == kRowUpd && e.screenX < 90)
                HoldReset(kRowUpd, "Update (hold 3s)", true);
            Draw();
            continue;
        }
        SysHandleEvent(&e);
    } while (e.eType != appStopEvent);
    StopHires();
    FtrSet(kSyCr, kFtrRun, 0);
    return 0;
}
