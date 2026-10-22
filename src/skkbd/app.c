/*
 * MnSkKbd v1 (진단) — Mn 가상 키보드(MnSkPlugin + MnSkCore) 설정 앱 (크리에이터 'MnSw')
 *
 * 왜: 다른 두 키보드(MnSonyKbd·MnKbdHan)처럼 Mn 키보드도 켜기/끄기·상태 확인을 한 앱에서. ClieDiag는 진단 전용으로 돌린다.
 * v2: 네 앱 공용 화면(src/common/app_ui.h). v1 진단 결과(2026-10-22, NR70V 에뮬레이터): 로더 DB 타입을 'SkWo'로 바꾸고 리셋하면
 *   상태바 □→가 없어지고 Graffiti 판만 남음(Mn 키보드가 떠 있던 채 바꿔도 이상 없음), 'SkWr'로 되돌리고 리셋하면 □→·Mn 키보드가 돌아옴.
 *   → [ON] = 타입 SkWr, [OFF] = 타입 SkWo, 둘 다 리셋 뒤 적용(Soft Reset). [All OFF] = 세 패치 모듈 끄기(Mn 키보드는 리셋이 필요해
 *   넣지 않음). Start at reset은 해당 없음(켜져 있으면 부팅 때 늘 등록). [Update] = OFF + 다른 모듈 다음 리셋 한 번 건너뛰기 + 리셋 → 설치 → ON + Soft Reset.
 *   상태는 1초마다 확인해 바뀌면 다시 그림(□→로 자판을 켜고 끈 것 등).
 * v1: 사용자 화면 = 상태(로더·본체 빌드, 자판 켜짐)·Soft Reset. [ ] Debug 화면 = 끄기 방법 시험:
 *   로더 DB(SkWr/MnSk)의 타입을 'SkWo'로 바꾸고 리셋하면 SilkExtn이 부팅 때 찾지 않아 □→ 순환에서 빠지는지,
 *   'SkWr'로 되돌리고 리셋하면 다시 들어오는지. 타입 바꾸기는 버튼을 눌렀을 때만.
 * 기록 없음(Feature 'MnSk'는 로더·본체 것을 읽기만).
 */

#include <PalmOS.h>

#define kCr        'MnSw'
#define kVer       "v2"
#define kSkCr      'MnSk'
#define kTypeOn    'SkWr'
#define kTypeOff   'SkWo'
#define kCoreType  'SkCo'
#define kSkMagic   0x534BUL

static Char   gMsg[40] = "";
static UInt16 gHrRef = 0xFFFF;

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

static UInt32 SkF(UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(kSkCr, n, &v) != errNone)
        v = 0;
    return v;
}

/* 로더 DB 찾기: 켜짐 타입 → 1, 꺼짐 타입 → 2, 없음 → 0 */
static UInt16 LoaderDB(UInt16 *card, LocalID *id)
{
    DmSearchStateType st;

    if (DmGetNextDatabaseByTypeCreator(true, &st, kTypeOn, kSkCr, false, card, id) == errNone && *id != 0)
        return 1;
    if (DmGetNextDatabaseByTypeCreator(true, &st, kTypeOff, kSkCr, false, card, id) == errNone && *id != 0)
        return 2;
    return 0;
}

static Boolean CoreDB(UInt16 *ver)
{
    DmSearchStateType st;
    UInt16            card;
    LocalID           id = 0;

    *ver = 0;
    if (DmGetNextDatabaseByTypeCreator(true, &st, kCoreType, kSkCr, true, &card, &id) != errNone || id == 0)
        return false;
    DmDatabaseInfo(card, id, NULL, NULL, ver, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    return true;
}

/* 로더가 이번 부팅에 등록되었나(Open 기록) */
static Boolean Registered(void)
{
    return (SkF(0) >> 16) == kSkMagic;
}

#include "../common/mod_off.h"
#include "../common/app_ui.h"

/* 로더 DB 타입 바꾸기(on = SkWr, off = SkWo). 결과는 알림 줄 */
static void SetType(Boolean on)
{
    UInt16  card = 0;
    LocalID id = 0;
    UInt16  k = LoaderDB(&card, &id);
    UInt32  t = on ? kTypeOn : kTypeOff;
    Err     err;

    if (k == 0) {
        StrCopy(gMsg, "loader not installed");
        return;
    }
    if ((k == 1) == on)
        err = 0;
    else
        err = DmSetDatabaseInfo(card, id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &t, NULL);
    if (err != 0)
        StrPrintF(gMsg, "type change failed %x", err);
    else if (UiMnState() == (on ? 1 : 2))
        StrCopy(gMsg, on ? "already ON" : "already off");
    else
        StrCopy(gMsg, on ? "ON after reset: Soft Reset" : "off after reset: Soft Reset");
}

static void BeforeUpdate(void)
{
    UInt16  card = 0;
    LocalID id = 0;
    UInt32  t = kTypeOff;

    if (LoaderDB(&card, &id) == 1)
        DmSetDatabaseInfo(card, id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &t, NULL);
    UiSkipOthers(kCr);   /* 다른 패치 모듈도 다음 리셋 한 번 건너뜀 → 모두 바로 설치 가능 */
}

static void TypeStr(UInt32 t, Char *o)
{
    o[0] = (Char)(t >> 24);
    o[1] = (Char)(t >> 16);
    o[2] = (Char)(t >> 8);
    o[3] = (Char)t;
    o[4] = 0;
}

static void DrawDebug(void)
{
    UInt16  card = 0, attr = 0, ver = 0, k;
    LocalID id = 0;
    UInt32  type = 0;
    Char    s[48], ts[5];

    k = LoaderDB(&card, &id);
    if (k != 0)
        DmDatabaseInfo(card, id, NULL, &attr, &ver, NULL, NULL, NULL, NULL, NULL, NULL, &type, NULL);
    TypeStr(type, ts);
    StrPrintF(s, "loader db %s attr %x ver %u", k ? ts : "none", attr, ver);
    UiRow(1, s);
    StrPrintF(s, "open %lx n%lu prot %lx", SkF(0), SkF(2), SkF(60));
    UiRow(2, s);
    StrPrintF(s, "core %lx load %lu", SkF(55), SkF(56));
    UiRow(3, s);
    StrPrintF(s, "active %lu  code %lx", SkF(59), SkF(58));
    UiRow(4, s);
    if (CoreDB(&ver))
        StrPrintF(s, "core db abi %u", ver);
    else
        StrCopy(s, "core db none");
    UiRow(5, s);
    UiRow(7, "load: 1 ok 2 no db 3 no res 4 abi 5 lock");
    UiRow(8, "more: ClieDiag (SkWr, SkEv pages)");
    UiCheck(kUiRowDebug, "Debug", true);
}

static void DrawUser(void)
{
    static const Char *nm[5] = { "not installed", "ON", "off", "ON after reset", "off after reset" };
    UInt16 ver = 0, m = UiMnState();
    Char   s[48], t[16];
    UInt32 cb = SkF(55);

    UiRow(0, "Mn keyboard on silkscreen");
    UiMsg(gMsg);
    StrPrintF(s, "Now: %s", nm[m]);
    if (Registered()) {
        StrPrintF(t, "  loader %u", (UInt16)(SkF(0) & 0xFF));
        StrCat(s, t);
    }
    if (!CoreDB(&ver))
        StrCat(s, "  no core");
    else if ((cb >> 16) == kSkMagic) {
        StrPrintF(t, "  core %u", (UInt16)(cb & 0xFF));
        StrCat(s, t);
    }
    UiRow(kUiRowNow, s);
    UiButton(kUiXOn, kUiRowBtn, "ON");
    UiButton(kUiXOff, kUiRowBtn, "OFF");
    UiButton(kUiXAll, kUiRowBtn, "All OFF");
    UiRow(kUiRowAuto, " Start at reset: always (when ON)");
    UiButton(kUiXOn, kUiRowUpd, "Update (hold 3s)");
    UiButton(kUiXOn, kUiRowReset, "Soft Reset (hold 3s)");
    UiMods();
    UiRow(kUiRowUse, "Status bar []-> : silk / Mn kbd");
    UiCheck(kUiRowDebug, "Debug", false);
}

static void Draw(void)
{
    RectangleType r;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnSkKbd " kVer, sizeof("MnSkKbd " kVer) - 1, 0, 0);
    if (DebugGet())
        DrawDebug();
    else
        DrawUser();
}

/* 화면에 보이는 상태의 요약(바뀌면 다시 그림) */
static UInt32 Sig(void)
{
    return (SkF(58) != 0 ? 1UL : 0) | ((UInt32)UiMnState() << 1) | (UiFtr('MnHg', 21) ? 0x10UL : 0) |
           (UiFtr('MnSy', 1) ? 0x20UL : 0) | (UiFtr('MnKh', 2) ? 0x40UL : 0) | (SkF(56) << 8);
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    EventType e;
    UInt32    sig;

    if (cmd != sysAppLaunchCmdNormalLaunch)
        return 0;
    StartHires();
    Draw();
    sig = Sig();
    do {
        EvtGetEvent(&e, SysTicksPerSecond());
        if (e.eType == nilEvent) {
            if (Sig() != sig) {
                sig = Sig();
                Draw();
            }
            continue;
        }
        if (e.eType == penDownEvent && e.screenY >= 0 && e.screenY < 160) {
            Int16 row = e.screenY / kUiH - 1;

            gMsg[0] = 0;
            if (row == kUiRowDebug && e.screenX < 60)
                DebugSet(DebugGet() ? 0 : 1);
            else if (DebugGet())
                ;
            else if (row == kUiRowBtn) {
                UInt16 k = UiBtnHit(e.screenX);

                if (k == 0)
                    SetType(true);
                else if (k == 1)
                    SetType(false);
                else if (k == 2)
                    ModOffMsg(gMsg, ModOffAll(NULL, kCr), UiCjkos());
            } else if (row == kUiRowUpd && e.screenX < 90) {
                if (!UiHoldReset(kUiRowUpd, "Update (hold 3s)", BeforeUpdate))
                    StrCopy(gMsg, "reset cancelled");
            } else if (row == kUiRowReset && e.screenX < 100) {
                if (!UiHoldReset(kUiRowReset, "Soft Reset (hold 3s)", NULL))
                    StrCopy(gMsg, "reset cancelled");
            }
            Draw();
            sig = Sig();
            continue;
        }
        SysHandleEvent(&e);
    } while (e.eType != appStopEvent);
    StopHires();
    return 0;
}
