/*
 * MnKbdHan v1 (진단) — 시스템 키보드 화면 한글 (크리에이터 'MnKh', Sony 가상 실크가 없는 Palm OS 4 영문 기종용, T600C 등)
 *
 * v7: 키 글자만 바꿈(키보드 화면 창 + 키 세 줄 y 60~100). v6 실측: 메뉴 명령 글자도 자모로 그려짐.
 * v6: 한/영 표시 자리 지우기가 space 키 아래 테두리를 끊던 것 수정.
 * v5: 한/영 표시('한'/'A')를 키 글자가 그려진 뒤에 그림(v4 실측: 표시가 덮여 안 보임).
 * v4: PC 두벌식 규칙(소문자 = 기본 자모, 대문자 = 쌍자음). 한/영은 Int'l 오른쪽 빈칸(키보드 화면 왼쪽 아래 '한'/'A' 표시).
 *   v3 실측: Palm 키보드는 cap 켜진 채 shift를 누르면 cap이 꺼져 cap 기반 한/영을 쓸 수 없음.
 * v3: cap 키 위치를 키보드 화면 창 기준으로 고침(x 0~21, y 89~105).
 * v2: 한글 모드를 펜이 cap 키를 누른 것으로 판단(v1 실측: 그리는 순간의 Graffiti caps가 한 박자 늦음). 진단 줄에 펜 위치 3개.
 * v1: 키 위 자모 표시만 시험한다(src/kbdhan/hook.c, 리소스 KhHk 0). 입력은 영문 그대로.
 *   키보드 화면에서 cap(쉬프트 고정)을 누르면 키 글자가 대문자로 다시 그려지는데, 그때 두벌식 자모로 바꿔 그린다
 *   (MnSonyKbd처럼 쉬프트 고정 = 한글 모드, cap + shift = 쌍자음). 2바이트 자모는 MnHangul이 그리므로 MnHangul이 켜져 있어야 한다.
 *   확인할 것: 자모 위치·크기, cap/shift 때 Graffiti 상태(caps·temp)가 어떻게 바뀌는지(진단 줄).
 * 지원 기종: Palm OS 4.0 이상 + 문자 인코딩 PalmLatin(MnHangul v68과 같은 조건). [ON]·[OFF]는 버튼을 눌렀을 때만.
 * 기록(feature 'MnKh'): #1 버퍼, #2 코드, #3 핸들, #4 건 함수 비트.
 */

#include <PalmOS.h>

#define kCr       'MnKh'
#define kVer      "v7"
#define kHookRes  'KhHk'
#define kHookN    4
#define kBufN     96
#define kOrigOff  16
#define kBufOff   32
#define kLineH    11
#define kRowBtn   2

static Char gMsg[40] = "diag: jamo labels only";

static UInt16 HookTrap(UInt16 i)
{
    switch (i) {
    case 0:  return sysTrapSysKeyboardDialog;
    case 1:  return sysTrapSysKeyboardDialogV10;
    case 2:  return sysTrapWinDrawChars;
    default: return sysTrapEvtGetEvent;
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
    StrCopy(gMsg, "ON: keyboard, tap right of Int'l");
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
    Row(5, "Memo > keyboard > tap right of Int'l:");
    Row(6, "keys show jamo (input stays ABC)");
    if (b == NULL)
        return;
    StrPrintF(s, "kbd opens %lu  letters %lu  jamo %lu", b[1], b[3], b[2]);
    Row(7, s);
    StrPrintF(s, "han %lu  han taps %lu  keys %lu", b[8], b[13], b[14]);
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
    {   /* v2: 화면 안 펜 누름 3개(오래된 것부터) */
        UInt16 k;

        StrCopy(s, "pen");
        for (k = 0; k < 3; k++) {
            UInt32 v = b[10 + ((b[9] + k) % 3)];
            Char   t[16];

            StrPrintF(t, " %d,%d", (Int16)(v >> 16), (Int16)v);
            StrCat(s, t);
        }
        Row(11, s);
    }
    Row(12, "why: 1 jamo 2 not han 3 MnHangul off");
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
