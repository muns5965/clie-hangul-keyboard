/*
 * MnHangul5 v10 — Palm OS 5(Sony TG50·NX70V) 한글 표시·입력 설정 앱 (크리에이터 MnH5)
 *
 * 켜기: ARM 코드(armc 1001)·글꼴 묶음(HgJp 1000 = 16×16, 1001 = 24×24)·KS X 1001 표(Ksct 1000)를 동적 힙(소유자 0)에 복사하고,
 *   표 8의 칸(H5Tb 1000 = src/os5/slots.txt)을 그 코드로 바꾼다. 원래 값은 ARM 코드의 ctx에 둔다.
 *   OS 5는 68k SysSetTrapAddress를 거부하므로(sysErrNotAllowed) 표 8을 직접 바꾼다(docs/OS5_RESEARCH.md).
 * 기기 확인: Sony('sony')·TG50('vrna') 또는 NX70V('crdb')이고, 바꿀 칸의 지금 값이 모두 알려진 원래 값(slots.txt)일 때만 켠다.
 *   v4: NX70V — 칸 번호·키 이벤트가 TG50과 같다(NX70V ROM의 PACE 연결 함수로 확인), 칸의 값만 다르다.
 * v5: ON 때 빠진 칸(다른 확장이 원래 값으로 되돌린 칸)을 다시 잇고, 화면 Now:에 빠진 칸 수를 보인다.
 * v6: 배경을 칠하는 그리기 함수(WinDrawChars 등) 대신 한글을 그릴 때 칸을 먼저 지운다 — 줄 중간을 고치면 겹쳐 보이던 문제.
 * v7: 자동 대문자 칸(Address 이름 등)의 첫 글자가 된소리가 되던 문제 — 실제 Shift(원시 keyCode 0x84xx)일 때만 된소리.
 * v8: 찾기(TxtFindString, 표 8 0x9ac)가 한글 낱말 가운데를 앞 글자 끝 바이트에 따라 맞추던 문제 — 원래 결과를 거른다(text.c).
 * v9: 켤 때 주소 기록(Feature)을 칸 교체 전에 하고 실패를 확인(실패하면 칸을 건드리지 않고 메모리를 푼다).
 * v10: 찾기 — 원문에 한글이 있으면 글자 경계마다 부분 검색(가나다에서 나). 한글 없는 레코드는 Palm 규칙(낱말 첫머리) 그대로.
 * 리셋: 표는 RAM이라 리셋하면 풀린다. Start at reset(앱 설정 0, 기본 ON)이면 리셋 뒤 다시 켠다.
 *   리셋 실행(sysAppLaunchCmdSystemReset)에서는 리셋 완료 알림(sysNotifyResetFinishedEvent, 모든 앱의 리셋 실행 뒤)만
 *   등록하고, 그 알림에서 켠다. 이때 그리기 3칸은 아직 ROM 함수다(실측: WinDrawChars 0x20a521f8). Sony 모듈이 나중에
 *   자기 함수(0x20c12800 등, 원래 값을 저장해 부르는 연결)로 바꾸면 그 아래에서 계속 불린다. slots.txt가 두 값을 모두 허용한다.
 *   리셋·알림 실행에서는 앱 전역변수를 쓸 수 없다 → On·Check·CopyRes·Table8Addr·PrefGet·SelfDB는 전역 없음(빌드 스크립트가 검사).
 * v2: 한글을 글자마다 그림 한 번으로 그림(src/os5/draw.c, 약 8배 빠름).
 * v3: 큰 글꼴은 24×24 한글(HgJp 1001), 굵은 글꼴은 한글도 굵게.
 * 코드는 복사본으로 돌기 때문에 켜진 채로 PRC를 지우거나 덮어써도 리셋 전까지 그대로 동작한다.
 *
 * Feature(MnH5): 0 코드 블록, 1 글꼴 16, 2 KSC 표, 3 글꼴 24, 9 리셋 때 켜기 결과(0x10000 | 결과)
 * 앱 설정(MnH5, 저장): 0 Start at reset(기본 1), 2 Debug 화면
 */
#include <PalmOS.h>

#define kVer "v10"
#define kCr 'MnH5'
#ifndef MN_DIAG
#define MN_DIAG 1
#endif

UInt32 PceNativeCall(void *nativeFuncP, void *userDataP) SYS_TRAP(0xA45A);   /* OS 5 (SDK 4에는 선언 없음) */

#define SW(x) ((((x) & 0xffUL) << 24) | (((x) & 0xff00UL) << 8) | (((x) >> 8) & 0xff00UL) | (((x) >> 24) & 0xffUL))

/* struct Ctx(src/os5/os5.h) 오프셋 */
#define kCtxPack 96
#define kCtxKsc  100
#define kCtxHits 104
#define kCtxSt   112
#define kCtxRing 144
#define kCtxRpos 240
#define kCtxIdle 244
#define kCtxPack24 248

/* 켜기 결과 */
#define kOk         0
#define kOnAlready  1
#define kNoRes      2
#define kNotSupported    3
#define kSlotDiffer 4   /* + 칸 순번<<8 */
#define kNoMem      5
#define kRelinked   6   /* + 다시 이은 칸 수<<8 (v5) */

#include "../common/app_ui.h"

static Char gMsg[40];

static UInt16 PrefGet(UInt16 id, UInt16 def)
{
    UInt16 v = def, size = sizeof v;

    if (PrefGetAppPreferences(kCr, id, &v, &size, true) == noPreferenceFound || size != sizeof v)
        v = def;
    return v;
}

static void PrefPut(UInt16 id, UInt16 v)
{
    PrefSetAppPreferences(kCr, id, 1, &v, sizeof v, true);
}

static UInt32 GetF(UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(kCr, n, &v) != errNone)
        v = 0;
    return v;
}

static MemPtr CopyRes(UInt32 type, UInt16 id)
{
    MemHandle h = DmGetResource(type, id);
    MemPtr    src, dst;
    UInt32    size;

    if (h == NULL)
        return NULL;
    size = MemHandleSize(h);
    dst = MemPtrNew(size);
    if (dst != NULL) {
        MemPtrSetOwner(dst, 0);
        src = MemHandleLock(h);
        MemMove(dst, src, size);
        MemHandleUnlock(h);
    }
    DmReleaseResource(h);
    return dst;
}

/* 표 8 주소(armc 1000: ldr r0,[r9,#-8]) */
static UInt32 Table8Addr(void)
{
    UInt32   *a, r;
    MemHandle h = DmGetResource('armc', 1000);

    if (h == NULL)
        return 0;
    a = (UInt32 *)MemPtrNew(4);   /* ARM 인자는 4바이트 정렬(68k 스택은 2바이트 정렬) */
    if (a == NULL) {
        DmReleaseResource(h);
        return 0;
    }
    a[0] = 0;
    PceNativeCall(MemHandleLock(h), a);
    MemHandleUnlock(h);
    DmReleaseResource(h);
    r = SW(a[0]);
    MemPtrFree(a);
    return r;
}

/* H5Tb 1000: n, ctx 오프셋, 칸마다 (칸, 함수 오프셋, O_ 번호, 0, 허용하는 원래 값 32비트 4개) — 모두 빅엔디언 */
#define kTbW               12   /* 칸 하나의 16비트 워드 수 */
#define TbSlot(tb, i)      ((tb)[2 + (i) * kTbW])
#define TbFunc(tb, i)      ((tb)[2 + (i) * kTbW + 1])
#define TbIdx(tb, i)       ((tb)[2 + (i) * kTbW + 2])
#define TbExpect(tb, i, k) ((((UInt32)(tb)[2 + (i) * kTbW + 4 + 2 * (k)]) << 16) | (tb)[2 + (i) * kTbW + 5 + 2 * (k)])

/* 지원 기기 + 칸의 지금 값이 TG50의 알려진 원래 값인지(slots.txt) */
static UInt16 Check(const UInt16 *tb, UInt32 t8)
{
    UInt32 co = 0, dev = 0;
    UInt16 i;

    FtrGet(sysFtrCreator, sysFtrNumOEMCompanyID, &co);
    FtrGet(sysFtrCreator, sysFtrNumOEMDeviceID, &dev);
    if (co != 'sony' || (dev != 'vrna' && dev != 'crdb') || t8 == 0)
        return kNotSupported;
    for (i = 0; i < tb[0]; i++) {
        UInt32 v = SW(*(UInt32 *)(t8 + TbSlot(tb, i)));
        UInt16 k;

        for (k = 0; k < 4; k++)
            if (v == TbExpect(tb, i, k))
                break;
        if (k == 4)
            return kSlotDiffer | (i << 8);
    }
    return kOk;
}

/* v5: 켜져 있는데 칸이 빠졌는지(다른 확장이 칸을 원래 값으로 되돌림). 칸마다 지금 값이
 *   우리 코드 → 정상, 우리가 저장한 원래 값 → 빠짐, 그 밖 → 위에 다른 패치(Sony 모듈 등, 연결 유지로 본다).
 *   빠진 칸 수를 돌려주고, fix면 그 칸만 우리 코드로 다시 잇는다(메모리는 그대로 — 해제 위험 없음). */
static UInt16 Detached(const UInt16 *tb, UInt32 block, UInt32 t8, Boolean fix)
{
    UInt16 i, d = 0;

    if (block == 0 || t8 == 0)
        return 0;
    for (i = 0; i < tb[0]; i++) {
        UInt32 cur = *(UInt32 *)(t8 + TbSlot(tb, i));   /* 그대로(리틀엔디언) 비교 */

        if (cur == SW(block + TbFunc(tb, i)))
            continue;
        if (cur == *(UInt32 *)(block + tb[1] + 4 * TbIdx(tb, i))) {
            d++;
            if (fix)
                *(UInt32 *)(t8 + TbSlot(tb, i)) = SW(block + TbFunc(tb, i));
        }
    }
    return d;
}
#define Relink(tb, block, t8) Detached(tb, block, t8, true)

static UInt16 On(void)   /* 전역 없음(리셋 알림에서도 부름) */
{
    UInt32    block, pack, pack24, ksc, t8, v = 0;
    MemHandle th;
    UInt16   *tb, n, ctx, i, r;

    th = DmGetResource('H5Tb', 1000);
    if (th == NULL)
        return kNoRes;
    tb = (UInt16 *)MemHandleLock(th);
    n = tb[0];
    ctx = tb[1];
    if (FtrGet(kCr, 0, &v) == errNone && v != 0) {   /* 이미 있음: 쉬는 중이면 깨우고, 빠진 칸은 다시 잇는다 */
        UInt16 back = Relink(tb, v, Table8Addr());

        r = back != 0 ? (kRelinked | (back << 8)) : *(UInt32 *)(v + ctx + kCtxIdle) ? kOk : kOnAlready;
        *(UInt32 *)(v + ctx + kCtxIdle) = 0;
        MemHandleUnlock(th);
        DmReleaseResource(th);
        return r;
    }
    t8 = Table8Addr();
    r = Check(tb, t8);
    if (r == kOk) {
        block = (UInt32)CopyRes('armc', 1001);
        pack = (UInt32)CopyRes('HgJp', 1000);
        ksc = (UInt32)CopyRes('Ksct', 1000);
        pack24 = (UInt32)CopyRes('HgJp', 1001);
        if (block == 0 || pack == 0 || ksc == 0 || pack24 == 0) {
            if (block) MemPtrFree((MemPtr)block);
            if (pack) MemPtrFree((MemPtr)pack);
            if (ksc) MemPtrFree((MemPtr)ksc);
            if (pack24) MemPtrFree((MemPtr)pack24);
            r = kNoMem;
        } else {
            *(UInt32 *)(block + ctx + kCtxPack) = SW(pack);   /* ARM이 읽는 값은 리틀엔디언 */
            *(UInt32 *)(block + ctx + kCtxKsc) = SW(ksc);
            *(UInt32 *)(block + ctx + kCtxPack24) = SW(pack24);
            /* v9: 주소 기록(Feature)을 칸을 바꾸기 전에 하고 실패를 확인한다 — 칸만 바뀌고 기록이 없으면 OFF로 끌 수 없다 */
            if (FtrSet(kCr, 1, pack) != errNone || FtrSet(kCr, 2, ksc) != errNone ||
                FtrSet(kCr, 3, pack24) != errNone || FtrSet(kCr, 0, block) != errNone) {
                for (i = 0; i < 4; i++)
                    FtrUnregister(kCr, i);
                MemPtrFree((MemPtr)block);
                MemPtrFree((MemPtr)pack);
                MemPtrFree((MemPtr)ksc);
                MemPtrFree((MemPtr)pack24);
                r = kNoMem;
            } else {
                for (i = 0; i < n; i++)   /* 원래 값 먼저(그대로 복사) */
                    *(UInt32 *)(block + ctx + 4 * TbIdx(tb, i)) = *(UInt32 *)(t8 + TbSlot(tb, i));
                for (i = 0; i < n; i++)
                    *(UInt32 *)(t8 + TbSlot(tb, i)) = SW(block + TbFunc(tb, i));
            }
        }
    }
    MemHandleUnlock(th);
    DmReleaseResource(th);
    return r;
}

/* 끄기: 칸이 모두 아직 우리 코드를 가리키면 되돌리고 메모리를 푼다(1).
 *   다른 것이 위에 있으면(리셋 뒤 Sony 모듈이 그리기 3칸 위에 올라옴) 되돌릴 수 없으므로 쉬게만 한다(2):
 *   코드는 남고 모든 함수가 원래 함수로 넘긴다. 완전히 빠지는 것은 다음 리셋(Start at reset: off면 다시 켜지지 않음). */
static UInt16 Off(void)
{
    UInt32    block = GetF(0), t8, v;
    MemHandle th;
    UInt16   *tb, n, i;

    if (block == 0)
        return 1;
    th = DmGetResource('H5Tb', 1000);
    if (th == NULL)
        return 0;
    t8 = Table8Addr();
    if (t8 == 0) {   /* v5: 리소스를 놓고 나감 */
        DmReleaseResource(th);
        return 0;
    }
    tb = (UInt16 *)MemHandleLock(th);
    n = tb[0];
    for (i = 0; i < n; i++)
        if (SW(*(UInt32 *)(t8 + TbSlot(tb, i))) != block + TbFunc(tb, i))
            break;
    if (i < n) {
        *(UInt32 *)(block + tb[1] + kCtxIdle) = SW(1UL);
        MemHandleUnlock(th);
        DmReleaseResource(th);
        return 2;
    }
    for (i = 0; i < n; i++)
        *(UInt32 *)(t8 + TbSlot(tb, i)) = *(UInt32 *)(block + tb[1] + 4 * TbIdx(tb, i));
    MemHandleUnlock(th);
    DmReleaseResource(th);
    for (i = 0; i < 4; i++) {
        v = GetF(i);
        if (v != 0)
            MemPtrFree((MemPtr)v);
        FtrUnregister(kCr, i);
    }
    return 1;
}

/* 이 앱 DB (리셋 실행에서 알림 등록용) */
static Boolean SelfDB(UInt16 *card, LocalID *id)
{
    *card = 0;
    *id = DmFindDatabase(0, "MnHangul5");
    return *id != 0;
}

static Boolean Idle(UInt32 block)
{
    MemHandle th = DmGetResource('H5Tb', 1000);
    Boolean   idle = false;

    if (th != NULL) {
        idle = *(UInt32 *)(block + ((UInt16 *)MemHandleLock(th))[1] + kCtxIdle) != 0;
        MemHandleUnlock(th);
        DmReleaseResource(th);
    }
    return idle;
}

static void OnMsg(UInt16 r)
{
    switch (r & 0xff) {
    case kOk:        StrCopy(gMsg, "on"); break;
    case kOnAlready: StrCopy(gMsg, "already on"); break;
    case kNotSupported:   StrCopy(gMsg, "not supported: TG50/NX70V only"); break;
    case kSlotDiffer: StrPrintF(gMsg, "not on: system table differs (#%u)", r >> 8); break;
    case kNoMem:     StrCopy(gMsg, "not on: out of memory"); break;
    case kRelinked:  StrPrintF(gMsg, "on: relinked %u detached", r >> 8); break;
    default:         StrCopy(gMsg, "not on: resource missing"); break;
    }
}

/* Debug 화면: ARM 쪽 기록(진단 빌드) */
static void DrawDiag(void)
{
    UInt32 block = GetF(0), v = GetF(9);
    Char   s[48];

    if (v != 0)
        StrPrintF(s, "reset: started (%lx)", v);
    else
        StrCopy(s, "reset: -");
    UiRow(0, s);
    if (block == 0) {
        UiRow(1, "off: no record");
        return;
    }
    {
        MemHandle th = DmGetResource('H5Tb', 1000);
        UInt16    ctx = th ? ((UInt16 *)MemHandleLock(th))[1] : 8;
        UInt32   *st = (UInt32 *)(block + ctx + kCtxSt);

        if (th) {
            MemHandleUnlock(th);
            DmReleaseResource(th);
        }
        StrPrintF(s, "block %lx glyphs %ld", block, SW(*(UInt32 *)(block + ctx + kCtxHits)));
        UiRow(1, s);
        StrPrintF(s, "han %ld sent %ld seen %ld", SW(st[2]), SW(st[4]), SW(st[5]));
        UiRow(2, s);
        StrPrintF(s, "events %ld", SW(st[7]));
        UiRow(3, s);
#if MN_DIAG
        {   /* 최근 키(오래된 것부터): 글자<<16 | mod<<8 | 한<<7 | 처리 (src/os5/ime.c) */
            UInt32 *ring = (UInt32 *)(block + ctx + kCtxRing);
            UInt32  pos = SW(*(UInt32 *)(block + ctx + kCtxRpos)), i, k;
            Int16   x = 2, row = 4;

            for (i = pos < 24 ? 0 : pos - 24; i < pos; i++) {
                k = SW(ring[i % 24]);
                StrPrintF(s, "%lx", k);
                WinDrawChars(s, StrLen(s), x, (row + 1) * kUiH);
                x += 52;
                if (x > 110) {
                    x = 2;
                    row++;
                }
            }
        }
#else
        UiRow(4, "release build: no key log");
#endif
    }
}

static void Draw(void)
{
    RectangleType r;
    UInt32        block = GetF(0);

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnHangul5 " kVer, sizeof("MnHangul5 " kVer) - 1, 0, 0);
    if (PrefGet(2, 0)) {
        DrawDiag();
        UiCheck(kUiRowDebug, "Debug", true);
        return;
    }
    UiRow(0, "Hangul display + kbd (TG50/NX70V)");
    UiMsg(gMsg);
    if (!block)
        UiRow(kUiRowNow, "Now: off");
    else if (Idle(block))
        UiRow(kUiRowNow, "Now: off (idle until reset)");
    else {
        MemHandle dh = DmGetResource('H5Tb', 1000);
        UInt16    d = 0;
        Char      s[40];

        if (dh != NULL) {
            d = Detached((UInt16 *)MemHandleLock(dh), block, Table8Addr(), false);
            MemHandleUnlock(dh);
            DmReleaseResource(dh);
        }
        if (d != 0) {   /* v5: 다른 확장이 칸을 되돌림 — ON으로 다시 잇는다 */
            StrPrintF(s, "Now: ON, %u detached (tap ON)", d);
            UiRow(kUiRowNow, s);
        } else
            UiRow(kUiRowNow, "Now: ON");
    }
    UiButton(kUiXOn, kUiRowBtn, "ON");
    UiButton(kUiXOff, kUiRowBtn, "OFF");
    UiButton(kUiXOn, kUiRowAuto, PrefGet(0, 1) ? "Start at reset: ON" : "Start at reset: off");
    UiButton(kUiXOn, kUiRowReset, "Soft Reset (hold 3s)");
    WinDrawGrayLine(2, (kUiRowMods + 1) * kUiH - 1, 157, (kUiRowMods + 1) * kUiH - 1);
    {
        UInt32 t8 = Table8Addr();
        MemHandle th = DmGetResource('H5Tb', 1000);
        UInt16 c = kNoRes;

        if (block)
            c = kOk;
        else if (th) {
            c = Check((UInt16 *)MemHandleLock(th), t8);
            MemHandleUnlock(th);
        }
        if (th)
            DmReleaseResource(th);
        if ((c & 0xff) == kOk) {
            UInt32 dev = 0;

            FtrGet(sysFtrCreator, sysFtrNumOEMDeviceID, &dev);
            UiPair(kUiRowMods, "Device", dev == 'crdb' ? "NX70V OK" : "TG50 OK");
        } else
            UiPair(kUiRowMods, "Device", (c & 0xff) == kNotSupported ? "not TG50/NX70V" : "table differs");
    }
    UiPair(kUiRowMods + 1, "Han/Eng", "Shift+Space");
    UiPair(kUiRowMods + 2, "Mark", "\"Han\" at lower right");
    UiPair(kUiRowMods + 3, "CJKOS", UiCjkos() ? "on: turn it off" : "-");
    UiRow(kUiRowUse, "Type in Hangul mode: two-set");
    UiCheck(kUiRowDebug, "Debug", false);
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    EventType e;

    if (cmd == sysAppLaunchCmdSystemReset) {   /* 전역 없음: 리셋 완료 알림을 기다림 */
        UInt16  card;
        LocalID id;

        if (PrefGet(0, 1) && SelfDB(&card, &id))
            SysNotifyRegister(card, id, sysNotifyResetFinishedEvent, NULL, sysNotifyNormalPriority, NULL);
        return 0;
    }
    if (cmd == sysAppLaunchCmdNotify) {   /* 전역 없음 */
        SysNotifyParamType *np = (SysNotifyParamType *)cmdPBP;
        UInt16              card;
        LocalID             id;

        if (np != NULL && np->notifyType == sysNotifyResetFinishedEvent) {
            FtrSet(kCr, 9, 0x10000UL | On());
            if (SelfDB(&card, &id))
                SysNotifyUnregister(card, id, sysNotifyResetFinishedEvent, sysNotifyNormalPriority);
        }
        return 0;
    }
    if (cmd != sysAppLaunchCmdNormalLaunch)
        return 0;
    gMsg[0] = 0;
    Draw();
    do {
        EvtGetEvent(&e, evtWaitForever);
        if (e.eType == penDownEvent && e.screenY >= 0 && e.screenY < 160) {
            Int16 row = e.screenY / kUiH - 1;

            gMsg[0] = 0;
            if (row == kUiRowDebug && e.screenX < 60)
                PrefPut(2, PrefGet(2, 0) ? 0 : 1);
            else if (PrefGet(2, 0))
                ;
            else if (row == kUiRowBtn) {
                UInt16 k = UiBtnHit(e.screenX);

                if (k == 0)
                    OnMsg(On());
                else if (k == 1)
                {
                    UInt16 o = Off();

                    StrCopy(gMsg, o == 1 ? "off" : o == 2 ? "off (idle: removed at next reset)" : "not off: resource missing");
                }
            } else if (row == kUiRowAuto && e.screenX < 100) {
                PrefPut(0, PrefGet(0, 1) ? 0 : 1);
                StrCopy(gMsg, PrefGet(0, 1) ? "start at reset: ON" : "start at reset: off");
            } else if (row == kUiRowReset && e.screenX < 100) {
                if (!UiHoldReset(kUiRowReset, "Soft Reset (hold 3s)", NULL))
                    StrCopy(gMsg, "reset cancelled");
            }
            Draw();
            continue;
        }
        SysHandleEvent(&e);
    } while (e.eType != appStopEvent);
    return 0;
}
