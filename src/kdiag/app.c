/*
 * KbdScan v2 — 다른 기종의 화면 키보드 진단 (크리에이터 'MnKd', NR70V 외 기종 시험용)
 *
 * 목적: N700C(일본어 OS 3.5) 등에서 (1) 한글 표시·입력에 필요한 시스템 기능이 있는지, (2) 화면 키보드가 키 글자를
 *   그림 한 장으로 그리는지 글자로 그리는지, (3) 누른 키가 키 이벤트(EvtGetEvent)로 지나가는지 입력칸에 바로 들어가는지를 본다.
 * 안전: 시작 때는 Feature 값만 읽는다(공개 함수). 시스템 함수 기록(KdHk 0, src/kdiag/hook.c)은 [Hook ON]을 눌렀을 때만 걸고,
 *   기종에 없는 함수(SysUnimplemented)는 걸지 않는다. 고해상도·Sony 라이브러리·Intl 함수는 쓰지 않는다(OS 3.5에서도 돌게).
 * v2: 숫자를 앞의 0 없이(Palm StrPrintF %lx는 8자리로 채워 줄이 화면 밖으로 넘쳤음, N700C 실측). 고해상도는 일부러 쓰지 않는다.
 * 화면: 제목 탭 = 페이지 전환(1 상태·횟수·그림, 2 글자·키·입력). [Hook ON] [Hook OFF] [Clear].
 * 기록(feature 'MnKd'): #1 버퍼, #2 코드, #3 핸들, #4 건 함수 비트.
 */

#include <PalmOS.h>

#define kCr       'MnKd'
#define kHookRes  'KdHk'
#define kHookN    11
#define kBufN     72
#define kOrigOff  44
#define kBufOff   88
#define kLineH    11
#define kRowBtn   2

static Char   gMsg[40] = "tap title: next page";
static void   Hx(Char *o, UInt32 v);
static UInt16 gPage = 0;

static UInt16 HookTrap(UInt16 i)
{
    switch (i) {
    case 0:  return sysTrapSysKeyboardDialog;
    case 1:  return sysTrapSysKeyboardDialogV10;
    case 2:  return sysTrapWinDrawBitmap;
    case 3:  return sysTrapWinPaintBitmap;
    case 4:  return sysTrapWinDrawChars;
    case 5:  return sysTrapWinPaintChars;
    case 6:  return 0xA378;   /* BltDrawChars */
    case 7:  return sysTrapWinCopyRectangle;
    case 8:  return sysTrapEvtGetEvent;
    case 9:  return sysTrapFldInsert;
    default: return sysTrapEvtEnqueueKey;
    }
}

static UInt32 GetF(UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(kCr, n, &v) != errNone)
        v = 0;
    return v;
}

static Boolean Impl(UInt16 trap)
{
    return SysGetTrapAddress(trap) != SysGetTrapAddress(sysTrapSysUnimplemented);
}

static Boolean AppDB(UInt16 *card, LocalID *id)
{
    DmSearchStateType st;

    return DmGetNextDatabaseByTypeCreator(true, &st, sysFileTApplication, kCr, true, card, id) == errNone;
}

static void HookOn(void)
{
    MemHandle h;
    UInt8    *code;
    UInt32   *buf;
    UInt32    mask = 0;
    UInt16    i, card;
    LocalID   id;

    if (GetF(2) != 0) {
        StrCopy(gMsg, "already ON");
        return;
    }
    h = DmGetResource(kHookRes, 0);
    if (h == NULL) {
        StrCopy(gMsg, "hook res missing");
        return;
    }
    buf = MemPtrNew(kBufN * 4);
    if (buf == NULL) {
        DmReleaseResource(h);
        StrCopy(gMsg, "no memory");
        return;
    }
    MemSet(buf, kBufN * 4, 0);
    MemPtrSetOwner(buf, 0);   /* 앱이 끝나도 남긴다 */
    code = MemHandleLock(h);
    for (i = 0; i < kHookN; i++) {
        void *o = Impl(HookTrap(i)) ? SysGetTrapAddress(HookTrap(i)) : NULL;

        DmWrite(code, kOrigOff + 4 * i, &o, 4);
        if (o != NULL)
            mask |= 1UL << i;
    }
    DmWrite(code, kBufOff, &buf, 4);
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, true);
    FtrSet(kCr, 1, (UInt32)buf);
    FtrSet(kCr, 2, (UInt32)code);
    FtrSet(kCr, 3, (UInt32)h);
    FtrSet(kCr, 4, mask);
    for (i = 0; i < kHookN; i++)
        if (mask & (1UL << i))
            SysSetTrapAddress(HookTrap(i), code + ((UInt32 *)code)[i]);
    StrCopy(gMsg, "hook ON mask ");
    Hx(gMsg + StrLen(gMsg), mask);
}

static void HookOff(void)
{
    UInt8  *code = (UInt8 *)GetF(2);
    UInt32  mask = GetF(4);
    void   *buf = (void *)GetF(1), *z = NULL;
    UInt16  i, kept = 0, card;
    LocalID id;

    if (code == NULL) {
        StrCopy(gMsg, "not ON");
        return;
    }
    for (i = 0; i < kHookN; i++)
        if ((mask & (1UL << i)) && (UInt8 *)SysGetTrapAddress(HookTrap(i)) != code + ((UInt32 *)code)[i])
            kept++;
    if (kept != 0) {   /* 위에 다른 것이 걸림: 풀면 안 된다 */
        StrPrintF(gMsg, "kept %u: reset to remove", kept);
        return;
    }
    for (i = 0; i < kHookN; i++)
        if (mask & (1UL << i))
            SysSetTrapAddress(HookTrap(i), (void *)((UInt32 *)code)[kOrigOff / 4 + i]);
    DmWrite(code, kBufOff, &z, 4);
    if (buf != NULL)
        MemPtrFree(buf);
    MemHandleUnlock((MemHandle)GetF(3));
    DmReleaseResource((MemHandle)GetF(3));
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, false);
    FtrSet(kCr, 1, 0);
    FtrSet(kCr, 2, 0);
    FtrSet(kCr, 3, 0);
    FtrSet(kCr, 4, 0);
    StrCopy(gMsg, "hook OFF");
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

/* v2: 앞의 0 없는 16진수 */
static void Hx(Char *o, UInt32 v)
{
    Char   t[9];
    UInt16 n = 0;

    do {
        UInt16 d = (UInt16)(v & 15);

        t[n++] = (Char)(d < 10 ? '0' + d : 'a' + d - 10);
        v >>= 4;
    } while (v != 0);
    while (n > 0)
        *o++ = t[--n];
    *o = 0;
}

/* 시스템 Feature 값(없으면 '!') */
static void Sys(Char *o, UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(sysFtrCreator, n, &v) != errNone)
        StrCopy(o, "!");
    else
        Hx(o, v);
}

static void Cr4(Char *o, UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(sysFtrCreator, n, &v) != errNone || v == 0) {
        StrCopy(o, "-");
        return;
    }
    o[0] = (Char)(v >> 24); o[1] = (Char)(v >> 16); o[2] = (Char)(v >> 8); o[3] = (Char)v; o[4] = 0;
}

static void DrawPage0(const UInt32 *b)
{
    Char   s[60], a[12], c[12], d[12], e[12];
    UInt16 k;

    Sys(a, sysFtrNumROMVersion);
    Sys(c, sysFtrNumEncoding);
    Sys(d, sysFtrNumLanguage);
    Sys(e, sysFtrNumCountry);
    StrPrintF(s, "rom %s enc %s lang %s ctry %s", a, c, d, e);
    Row(3, s);
    Sys(a, sysFtrNumIntlMgr);
    Sys(c, sysFtrNumNotifyMgrVersion);
    Cr4(d, sysFtrNumOEMCompanyID);
    Cr4(e, sysFtrNumOEMDeviceID);
    StrPrintF(s, "intl %s ntf %s oem %s %s", a, c, d, e);
    Row(4, s);
    Hx(a, GetF(4));
    StrPrintF(s, "blt %s paint %s  hooked %s", Impl(0xA378) ? "yes" : "no", Impl(sysTrapWinPaintChars) ? "yes" : "no", a);
    Row(5, s);
    if (b == NULL) {
        Row(7, "Hook ON, open keyboard in Memo,");
        Row(8, "tap keys, close, come back here");
        return;
    }
    StrPrintF(s, "kbd %lu v10 %lu type %lu  pen %lu", b[1], b[2], b[3], b[70]);
    Row(7, s);
    StrPrintF(s, "bmp %lu/%lu chr %lu/%lu blt %lu cp %lu", b[6], b[7], b[8], b[9], b[10], b[11]);
    Row(8, s);
    StrPrintF(s, "ev %lu ins %lu enq %lu  (in dialog)", b[12], b[13], b[14]);
    Row(9, s);
    for (k = 0; k < 4; k++) {   /* 그림 4개(오래된 것부터), P = WinPaintBitmap */
        UInt16 j = (UInt16)((b[20] + k) & 3);
        UInt32 wh = b[21 + 2 * j], xy = b[22 + 2 * j];

        if (wh == 0 && xy == 0)
            StrCopy(s, "-");
        else
            StrPrintF(s, "%s%ux%u@%d,%d", (wh >> 31) ? "P" : "B", (UInt16)((wh >> 16) & 0x7FFF), (UInt16)wh,
                      (Int16)(xy >> 16), (Int16)xy);
        WinDrawChars(s, StrLen(s), 2 + (k % 2) * 80, (11 + k / 2) * kLineH);
    }
}

static void DrawPage1(const UInt32 *b)
{
    Char   s[40];
    UInt16 k;

    if (b == NULL) {
        Row(4, "no record (Hook ON first)");
        return;
    }
    Row(3, "chars (D draw P paint) len@x bytes");
    for (k = 0; k < 6; k++) {
        UInt16 j = (UInt16)((b[30] + k) % 6);
        UInt32 v = b[31 + 2 * j];

        if (v == 0 && b[32 + 2 * j] == 0)
            StrCopy(s, "-");
        else
            StrPrintF(s, "%s%u@%u %08lx", (v >> 28) == 5 ? "P" : "D", (UInt16)((v >> 16) & 0xFFF), (UInt16)v,
                      b[32 + 2 * j]);
        WinDrawChars(s, StrLen(s), 2 + (k % 2) * 80, (5 + k / 2) * kLineH);
    }
    Row(7, "keyDown chr.mod (oldest first)");
    for (k = 0; k < 8; k++) {
        UInt32 v = b[45 + ((b[44] + k) & 7)];
        Char   x[10], y[10];

        Hx(x, v >> 16);
        Hx(y, v & 0xFFFF);
        StrPrintF(s, "%s.%s", x, y);
        WinDrawChars(s, StrLen(s), 2 + (k % 4) * 40, (9 + k / 4) * kLineH);
    }
    Row(10, "FldInsert len:bytes / Enqueue");
    for (k = 0; k < 4; k++) {
        UInt16 j = (UInt16)((b[54] + k) & 3);

        StrPrintF(s, "%lu:%08lx", b[55 + 2 * j], b[56 + 2 * j]);
        WinDrawChars(s, StrLen(s), 2 + (k % 2) * 80, (12 + k / 2) * kLineH);
    }
    StrCopy(s, "enq");
    for (k = 0; k < 4; k++) {
        Char x[10];

        Hx(x, b[65 + ((b[64] + k) & 3)] >> 16);
        StrCat(s, " ");
        StrCat(s, x);
    }
    Row(13, s);
}

static void Draw(void)
{
    RectangleType r;
    const UInt32 *b = (const UInt32 *)GetF(1);

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars(gPage == 0 ? "KbdScan v2 [1/2: system]" : "KbdScan v2 [2/2: input]",
                         StrLen(gPage == 0 ? "KbdScan v2 [1/2: system]" : "KbdScan v2 [2/2: input]"), 0, 0);
    WinDrawInvertedChars(gMsg, StrLen(gMsg), 0, kLineH);
    ButtonAt(2, kRowBtn, "Hook ON");
    ButtonAt(52, kRowBtn, "Hook OFF");
    ButtonAt(108, kRowBtn, "Clear");
    if (gPage == 0)
        DrawPage0(b);
    else
        DrawPage1(b);
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    EventType e;

    if (cmd != sysAppLaunchCmdNormalLaunch)
        return 0;
    Draw();
    do {
        EvtGetEvent(&e, evtWaitForever);
        if (e.eType == penDownEvent && e.screenY >= 0 && e.screenY < 160) {
            Int16 row = e.screenY / kLineH - 1;

            if (e.screenY < kLineH)
                gPage = (UInt16)(1 - gPage);
            else if (row == kRowBtn && e.screenX < 50)
                HookOn();
            else if (row == kRowBtn && e.screenX < 106)
                HookOff();
            else if (row == kRowBtn) {
                UInt32 *b = (UInt32 *)GetF(1);

                if (b != NULL) {
                    UInt32 depth = b[0];

                    MemSet(b, kBufN * 4, 0);
                    b[0] = depth;
                }
                StrCopy(gMsg, "cleared");
            }
            Draw();
            continue;
        }
        SysHandleEvent(&e);
    } while (e.eType != appStopEvent);
    return 0;
}
