/*
 * MnKbdHan v1 (진단) — 시스템 키보드 화면 한글 (크리에이터 'MnKh', Sony 가상 실크가 없는 Palm OS 4 영문 기종용, T600C 등)
 *
 * v15(진단): 키보드 화면에서 앱이 이벤트 하나를 처리한 시간(틱, #40~#48: 영문 키·넣은 키·펜 별 평균·수, 최대)과
 *   시험 스위치 #49(키 글자를 자모 대신 영문 — MnHangul 그리기 비용 빼기). 진단 9~11줄. v14 실측: MnHangul이 켜진 한 모드에서
 *   0.04~0.1초 간격 입력의 펜 누름이 빠짐(MnHangul을 끄면 모두 받음), 키 하나에 키 글자 그리기 약 50번.
 * v14(진단): 키 큐 넣기(EvtEnqueueKey) 실패 기록 — #32 넣은 수, #30 실패 수, #31 마지막 오류. 진단 8줄. 동작은 그대로.
 * v13: 네 앱 공용 화면(src/common/app_ui.h) — [ON][OFF][All OFF]·Start at reset(앱 설정 0번, 기본 ON)·Update(hold 3s, 설정 3번 =
 *   다음 리셋 한 번 건너뛰기)·Soft Reset·모듈 상태 4줄. 다른 앱의 All OFF가 부르면(kCmdModOff) 스스로 끔.
 *   켜기·끄기는 결과 번호를 돌려주고 알림 글은 화면 쪽에서(리셋 실행에는 전역변수가 없음).
 * v12: 한/영 표시를 MnSonyKbd와 같은 그림으로(KhIc 0·1, tools/gen-kbd-mark.py) — HR 라이브러리가 있으면 고해상도, 없으면 글자.
 *   기록: #5·#6 그림 주소(한·A), #7·#8 핸들, #9 HR 참조 번호(0x10000 | 번호, 없으면 0).
 * v11: 앱 화면 정리 — 고해상도(Sony HR 320, 없는 기종은 그대로), MnSonyKbd와 같은 배치(제목·설명·알림·Now·ON/OFF·한글 표시·사용법),
 *   진단 줄은 [ ] Debug 체크(앱 설정 2번)일 때만.
 * v10: 쌍자음은 손으로 누른 shift(키 x 0~31, y 107~121)일 때만 — v9 자동 시험: 새 메모에서 A→한 직후 키보드 화면 자체 shift로
 *   키 글자가 쌍자음으로 보이고 첫 글자가 쌍자음으로 들어감(rk → 까).
 * v9: 두벌식 조합 입력(한글 모드, 키보드 화면 안) + 한글 모드에서 자동 대문자 풀기. 진단 줄 = keyDown 수·마지막 키·조합 상태.
 * v8: 키 글자 표를 키 단위로(같은 줄 x 4 이내 덮어씀). v7 실측: 전환 때 쌍자음이 보였다가 기본 자모로 바뀜.
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
#define kVer      "v15"
#define kHookRes  'KhHk'
#define kHookN    4
#define kBufN     104
#define kOrigOff  16
#define kBufOff   32

static Char    gMsg[40] = "";
static UInt16 gHrRef = 0xFFFF;

/* v11: 고해상도 모드(MnSonyKbd·MnHangul과 같음, 소니 헤더 없이 필요한 트랩만). HR 라이브러리가 없으면(T600C 등) 그대로 */
Err HROpen(UInt16 ref)  SYS_TRAP(sysLibTrapOpen);
Err HRClose(UInt16 ref) SYS_TRAP(sysLibTrapClose);
Err HRWinScreenMode(UInt16 ref, WinScreenModeOperation op, UInt32 *w, UInt32 *h,
                    UInt32 *depth, Boolean *color) SYS_TRAP(sysLibTrapCustom + 48);

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

/* v11: Debug 체크(앱 설정 2번, 저장형) */
static UInt16 DebugGet(void)
{
    UInt16 v = 0, size = sizeof v;

    if (PrefGetAppPreferences(kCr, 2, &v, &size, true) == noPreferenceFound || size != sizeof v)
        v = 0;
    return v;
}

static void DebugSet(UInt16 v)
{
    PrefSetAppPreferences(kCr, 2, 1, &v, sizeof v, true);
}

/* v13: 앱 설정 0번 = Start at reset(기본 ON), 3번 = 다음 리셋 한 번 건너뛰기(Update). 리셋 실행에서도 부름(전역 없음) */
static UInt16 PrefGet(UInt16 id, UInt16 def)
{
    UInt16 v = 0, size = sizeof v;

    if (PrefGetAppPreferences(kCr, id, &v, &size, true) == noPreferenceFound || size != sizeof v)
        v = def;
    return v;
}

static void PrefPut(UInt16 id, UInt16 v)
{
    PrefSetAppPreferences(kCr, id, 1, &v, sizeof v, true);
}

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

/* v13: 결과 0 켬, 1 이미 켜짐, 2 지원 안 함, 3 리소스 없음, 4 메모리 없음. 리셋 실행에서도 부름(전역 없음) */
static UInt16 HookOn(void)
{
    MemHandle h;
    UInt8    *code;
    UInt32   *buf;
    UInt32    mask = 0;
    UInt16    i, card;
    LocalID   id;

    if (!Supported())
        return 2;
    if (GetF(2) != 0)
        return 1;
    h = DmGetResource(kHookRes, 0);
    if (h == NULL)
        return 3;
    buf = MemPtrNew(kBufN * 4);
    if (buf == NULL) {
        DmReleaseResource(h);
        return 4;
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
    {   /* v12: 한/영 표시 그림·HR 라이브러리 */
        UInt16 k, hr = 0;

        for (k = 0; k < 2; k++) {
            MemHandle ih = DmGetResource('KhIc', k);

            FtrSet(kCr, 7 + k, (UInt32)ih);
            FtrSet(kCr, 5 + k, ih != NULL ? (UInt32)MemHandleLock(ih) : 0);
        }
        FtrSet(kCr, 9, SysLibFind("Sony HR Library", &hr) == errNone ? 0x10000UL | hr : 0);
    }
    for (i = 0; i < kHookN; i++)
        SysSetTrapAddress(HookTrap(i), code + ((UInt32 *)code)[i]);
    return 0;
}

/* v13: 결과 0 꺼짐(또는 원래 꺼짐), 2 위에 다른 패치(남김) — mod_off.h 자기 끄기와 같은 번호 */
static UInt16 HookOff(void)
{
    UInt8  *code = (UInt8 *)GetF(2);
    void   *buf = (void *)GetF(1), *z = NULL;
    UInt16  i, kept = 0, card;
    LocalID id;

    if (code == NULL)
        return 0;
    for (i = 0; i < kHookN; i++)
        if ((UInt8 *)SysGetTrapAddress(HookTrap(i)) != code + ((UInt32 *)code)[i])
            kept++;
    if (kept != 0)   /* 위에 다른 패치: 풀면 안 된다 */
        return 2;
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
    {   /* v12 */
        UInt16 k;

        FtrSet(kCr, 9, 0);
        for (k = 0; k < 2; k++) {
            MemHandle ih = (MemHandle)GetF(7 + k);

            FtrSet(kCr, 5 + k, 0);
            if (ih != NULL) {
                MemHandleUnlock(ih);
                DmReleaseResource(ih);
            }
            FtrSet(kCr, 7 + k, 0);
        }
    }
    return 0;
}

#include "../common/mod_off.h"
#include "../common/app_ui.h"

static void OnMsg(UInt16 r)
{
    static const Char *m[5] = { "ON", "already ON", "unsupported: OS 4 Latin only", "hook res missing", "no memory" };

    StrCopy(gMsg, m[r < 5 ? r : 4]);
}

static void BeforeUpdate(void)
{
    PrefPut(3, 1);
    UiSkipOthers(kCr);
}

/* v11: 진단 줄(v1~v10의 화면 4~13줄) — Debug 체크일 때만 */
static void DrawDiag(void)
{
    const UInt32 *b = (const UInt32 *)GetF(1);
    Char          s[48], ch[2];

    if (b == NULL) {
        UiRow(1, "hook off: no diag");
        return;
    }
    StrPrintF(s, "kbd opens %lu  letters %lu  jamo %lu", b[1], b[3], b[2]);
    UiRow(1, s);
    StrPrintF(s, "han %lu  han taps %lu  keys %lu", b[8], b[13], b[14]);
    UiRow(2, s);
    ch[0] = (Char)(b[4] >> 24);
    ch[1] = 0;
    if (ch[0] == 0)
        StrCopy(ch, "-");
    StrPrintF(s, "last %s caps %u temp %u auto %u why %lu", ch, (UInt16)((b[4] >> 16) & 1), (UInt16)((b[4] >> 8) & 0xFF),
              (UInt16)(b[4] & 1), b[6]);
    UiRow(3, s);
    StrPrintF(s, "at %d,%d  width jamo %u / abc %u", (Int16)(b[5] >> 16), (Int16)b[5], (UInt16)(b[7] >> 16),
              (UInt16)b[7]);
    UiRow(4, s);
    {   /* v2: 화면 안 펜 누름 3개(오래된 것부터) */
        UInt16 k;

        StrCopy(s, "pen");
        for (k = 0; k < 3; k++) {
            UInt32 v = b[10 + ((b[9] + k) % 3)];
            Char   t[16];

            StrPrintF(t, " %d,%d", (Int16)(v >> 16), (Int16)v);
            StrCat(s, t);
        }
        UiRow(5, s);
    }
    {   /* v9: 입력 */
        Char a[12], c[12];

        a[0] = (Char)(b[99] >> 16);
        a[1] = 0;
        if ((UInt8)a[0] < 0x20 || (UInt8)a[0] > 0x7E)
            StrPrintF(a, "%x", (UInt16)(b[99] >> 16));
        StrPrintF(c, "%lx", b[96]);
        StrPrintF(s, "keys %lu last %s mod %x comp %s", b[98], a, (UInt16)b[99], c);
        UiRow(6, s);
    }
    StrPrintF(s, "shift %lu  ct %lu bad %lu", b[101], GetF(20), GetF(21));   /* v10 손 shift, 커서 비교 */
    UiRow(7, s);
    StrPrintF(s, "enq %lu fail %lu e%lx  open %lu", GetF(32), GetF(30), GetF(31), b[0]);   /* v14 */
    UiRow(8, s);
}

/* v15 진단: 이벤트 처리 시간(틱, 평균은 소수 한 자리), 시험 스위치 */
static void Avg(Char *o, UInt16 k)
{
    UInt32 sum = GetF(40 + 2 * k), n = GetF(41 + 2 * k), a10 = n ? sum * 10 / n : 0;

    StrPrintF(o, "%lu.%lu", a10 / 10, a10 % 10);
}

static void DrawTiming(void)
{
    Char s[48], a[3][12];
    UInt16 k;

    for (k = 0; k < 3; k++)
        Avg(a[k], k);
    StrPrintF(s, "tk/evt key %s ins %s pen %s", a[0], a[1], a[2]);
    UiRow(9, s);
    StrPrintF(s, "n key %lu ins %lu pen %lu max %lu", GetF(41), GetF(43), GetF(45), GetF(46));
    UiRow(10, s);
    UiButton(kUiXOn, 11, GetF(49) ? "Test abc labels: ON" : "Test abc labels: off");
    UiButton(100, 11, "Clear");
}

/* v13: 네 앱 공용 화면(app_ui.h). Debug 체크면 진단 줄 */
static void Draw(void)
{
    RectangleType r;
    Boolean       dbg = DebugGet() != 0;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnKbdHan " kVer, sizeof("MnKbdHan " kVer) - 1, 0, 0);
    if (dbg) {
        DrawDiag();
        DrawTiming();   /* v15 */
        UiCheck(kUiRowDebug, "Debug", true);
        return;
    }
    UiRow(0, "Hangul on Edit > Keyboard");
    UiMsg(gMsg);
    UiRow(kUiRowNow, !Supported() ? "Now: unsupported (OS 4 Latin only)" : GetF(2) ? "Now: ON" : "Now: off");
    UiButton(kUiXOn, kUiRowBtn, "ON");
    UiButton(kUiXOff, kUiRowBtn, "OFF");
    UiButton(kUiXAll, kUiRowBtn, "All OFF");
    UiButton(kUiXOn, kUiRowAuto, PrefGet(0, 1) ? "Start at reset: ON" : "Start at reset: off");
    UiButton(kUiXOn, kUiRowUpd, "Update (hold 3s)");
    UiButton(kUiXOn, kUiRowReset, "Soft Reset (hold 3s)");
    UiMods();
    UiRow(kUiRowUse, "Tap right of Int'l: Hangul / English");
    UiCheck(kUiRowDebug, "Debug", false);
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    EventType e;

    if (cmd == sysAppLaunchCmdSystemReset) {   /* v13: 전역 없음 */
        if (PrefGet(3, 0))   /* Update 리셋: 이번 한 번 켜지 않음(잠금 없음 → 바로 설치 가능) */
            PrefPut(3, 0);
        else if (PrefGet(0, 1))
            HookOn();
        return 0;
    }
    if (cmd == kCmdModOff)   /* v13: 다른 앱의 All OFF */
        return HookOff();
    if (cmd != sysAppLaunchCmdNormalLaunch)
        return 0;
    StartHires();
    Draw();
    do {
        EvtGetEvent(&e, evtWaitForever);
        if (e.eType == penDownEvent && e.screenY >= 0 && e.screenY < 160) {
            Int16 row = e.screenY / kUiH - 1;

            gMsg[0] = 0;
            if (row == kUiRowDebug && e.screenX < 60)
                DebugSet(DebugGet() ? 0 : 1);
            else if (DebugGet()) {   /* v15 진단 화면: 시험 스위치·시간 기록 지우기 */
                if (row == 11 && e.screenX < 96)
                    FtrSet(kCr, 49, GetF(49) ? 0 : 1);
                else if (row == 11 && e.screenX >= 100) {
                    UInt16 n;

                    for (n = 40; n <= 48; n++)
                        FtrSet(kCr, n, 0);
                }
            }
            else if (row == kUiRowBtn) {
                UInt16 k = UiBtnHit(e.screenX);

                if (k == 0)
                    OnMsg(HookOn());
                else if (k == 1)
                    StrCopy(gMsg, HookOff() == 2 ? "kept: use Update, then install" : "off: ready to install");
                else if (k == 2)
                    ModOffMsg(gMsg, ModOffAll(HookOff, kCr), UiCjkos());
            } else if (row == kUiRowAuto && e.screenX < 100) {
                PrefPut(0, PrefGet(0, 1) ? 0 : 1);
                StrCopy(gMsg, PrefGet(0, 1) ? "start at reset: ON" : "start at reset: off");
            } else if (row == kUiRowUpd && e.screenX < 90) {
                if (!UiHoldReset(kUiRowUpd, "Update (hold 3s)", BeforeUpdate))
                    StrCopy(gMsg, "reset cancelled");
            } else if (row == kUiRowReset && e.screenX < 100) {
                if (!UiHoldReset(kUiRowReset, "Soft Reset (hold 3s)", NULL))
                    StrCopy(gMsg, "reset cancelled");
            }
            Draw();
            continue;
        }
        SysHandleEvent(&e);
    } while (e.eType != appStopEvent);
    StopHires();
    return 0;
}
