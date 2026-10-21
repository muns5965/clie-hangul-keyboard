/*
 * MnKbdHan v1 (진단) — 시스템 키보드 화면 한글 (크리에이터 'MnKh', Sony 가상 실크가 없는 Palm OS 4 영문 기종용, T600C 등)
 *
 * v1: 키 위 자모 표시만 시험한다(src/kbdhan/hook.c, 리소스 KhHk 0). 입력은 영문 그대로.
 *   키보드 화면에서 cap(쉬프트 고정)을 누르면 키 글자가 대문자로 다시 그려지는데, 그때 두벌식 자모로 바꿔 그린다
 *   (MnSonyKbd처럼 쉬프트 고정 = 한글 모드, cap + shift = 쌍자음). 2바이트 자모는 MnHangul이 그리므로 MnHangul이 켜져 있어야 한다.
 *   확인할 것: 자모 위치·크기, cap/shift 때 Graffiti 상태(caps·temp)가 어떻게 바뀌는지(진단 줄).
 * 지원 기종: Palm OS 4.0 이상 + 문자 인코딩 PalmLatin(MnHangul v68과 같은 조건). [ON]·[OFF]는 버튼을 눌렀을 때만.
 * 기록(feature 'MnKh'): #1 버퍼, #2 코드, #3 핸들, #4 건 함수 비트.
 */

#include <PalmOS.h>

#define kCr       'MnKh'
#define kVer      "v1"
#define kHookRes  'KhHk'
#define kHookN    3
#define kBufN     8
#define kOrigOff  12
#define kBufOff   24
#define kLineH    11
#define kRowBtn   2

static Char gMsg[40] = "diag: jamo labels only";

static UInt16 HookTrap(UInt16 i)
{
    switch (i) {
    case 0:  return sysTrapSysKeyboardDialog;
    case 1:  return sysTrapSysKeyboardDialogV10;
    default: return sysTrapWinDrawChars;
    }
}

static UInt32 GetF(UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(kCr, n, &v) != errNone)
        v = 0;
    return v;
}

static Boolean Supported(void)
{
    UInt32 rom = 0, enc = 0;

    if (FtrGet(sysFtrCreator, sysFtrNumROMVersion, &rom) != errNone || rom < 0x04000000UL)
        return false;
    return FtrGet(sysFtrCreator, sysFtrNumEncoding, &enc) == errNone && enc == 3;
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

    if (!Supported()) {
        StrCopy(gMsg, "unsupported: OS 4 Latin only");
        return;
    }
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
    MemPtrSetOwner(buf, 0);
    code = MemHandleLock(h);
    for (i = 0; i < kHookN; i++) {
        void *o = SysGetTrapAddress(HookTrap(i));

        DmWrite(code, kOrigOff + 4 * i, &o, 4);
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
        SysSetTrapAddress(HookTrap(i), code + ((UInt32 *)code)[i]);
    StrCopy(gMsg, "ON: open keyboard, tap cap");
}

static void HookOff(void)
{
    UInt8  *code = (UInt8 *)GetF(2);
    void   *buf = (void *)GetF(1), *z = NULL;
    UInt16  i, kept = 0, card;
    LocalID id;

    if (code == NULL) {
        StrCopy(gMsg, "not ON");
        return;
    }
    for (i = 0; i < kHookN; i++)
        if ((UInt8 *)SysGetTrapAddress(HookTrap(i)) != code + ((UInt32 *)code)[i])
            kept++;
    if (kept != 0) {   /* 위에 다른 패치: 풀면 안 된다 */
        StrPrintF(gMsg, "kept %u: other patch on top", kept);
        return;
    }
    for (i = 0; i < kHookN; i++)
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
    StrCopy(gMsg, "OFF: ready to install");
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

static void Draw(void)
{
    RectangleType r;
    const UInt32 *b = (const UInt32 *)GetF(1);
    UInt32        hg = 0;
    Char          s[48], ch[2];

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnKbdHan " kVer " (diag)", sizeof("MnKbdHan " kVer " (diag)") - 1, 0, 0);
    WinDrawInvertedChars(gMsg, StrLen(gMsg), 0, kLineH);
    ButtonAt(2, kRowBtn, "ON");
    ButtonAt(40, kRowBtn, "OFF");
    FtrGet('MnHg', 21, &hg);
    StrPrintF(s, "hook %s  MnHangul %s  %s", GetF(2) ? "ON" : "off", hg ? "ON" : "off",
              Supported() ? "OS ok" : "unsupported");
    Row(4, s);
    Row(5, "Memo > keyboard > tap cap:");
    Row(6, "keys show jamo (input stays ABC)");
    if (b == NULL)
        return;
    StrPrintF(s, "kbd opens %lu  letters %lu  jamo %lu", b[1], b[3], b[2]);
    Row(8, s);
    ch[0] = (Char)(b[4] >> 24);
    ch[1] = 0;
    if (ch[0] == 0)
        StrCopy(ch, "-");
    StrPrintF(s, "last %s caps %u temp %u auto %u why %lu", ch, (UInt16)((b[4] >> 16) & 1), (UInt16)((b[4] >> 8) & 0xFF),
              (UInt16)(b[4] & 1), b[6]);
    Row(9, s);
    StrPrintF(s, "at %d,%d  width jamo %u / abc %u", (Int16)(b[5] >> 16), (Int16)b[5], (UInt16)(b[7] >> 16),
              (UInt16)b[7]);
    Row(10, s);
    Row(12, "why: 1 jamo 2 no cap 3 MnHangul off");
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

            if (row == kRowBtn && e.screenX < 36)
                HookOn();
            else if (row == kRowBtn && e.screenX < 80)
                HookOff();
            Draw();
            continue;
        }
        SysHandleEvent(&e);
    } while (e.eType != appStopEvent);
    return 0;
}
