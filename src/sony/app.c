/*
 * MnSonyKbd v13 — Sony 가상 키보드 한글 (NR70V ROM SilkExtn 키보드)
 *
 * MnHangul v35~v53의 Sony 가상 키보드 부분을 따로 뗀 모듈(MnHangul v54와 같이 나옴).
 * 한글 표시(글꼴)는 하지 않으므로 CJKOS 등 한글 시스템이나 MnHangul(한글 표시)과 같이 쓴다.
 *   SyPt 0 (spatch.c) BltDrawChars: 가상 실크 창의 영문 키 글자를 두벌식 자모로(그 밖은 아래 함수로 넘김)
 *   SyIm 0 (ime.c)    EvtGetEvent: 영문 키 → 두벌식 조합 → 완성형 2바이트 입력 / WinDrawBitmap: ⇪ 그림 → '한A'
 *   SyIc 0·1 '한A' 그림(tools/gen-caps-icon.py), SyJm 0 자모 그림 16×16(tools/gen-sony-jamo.py)
 *
 * v21(진단): v20 실측 sym 0(탭 글자가 BltDrawChars로 오지 않음), 빈칸 누름 t17·펜 73,219 80,223 → 탭 줄은 그림으로 추정.
 *   가상 실크 그림 그리기 최근 4개(주소·크기·위치)를 진단 3~6줄에, 빈칸을 누르면 화면에 바로 '한'/'A'를 그려 자리 확인.
 * v20(진단): 한/영 단추를 ⇪ 대신 sym 탭 오른쪽 빈칸으로 옮기는 시험 — 펜 위치 기록, sym 탭 옆 '한'/'A' 표시, 빈칸 누름 = 시험 표시만
 *   전환(입력은 아직 ⇪ 그대로). 진단 화면 7·8줄. dubeol.h kHanTab 참고. 동기: 시스템 키보드(MnKbdHan)와 같이 띄우면
 *   시스템 cap이 Graffiti ⇪를 바꿔 Sony 쪽 한글 판정이 어긋남(실측 2026-10-21).
 * v19: 지우기를 거부했을 때 맨 위 패치가 누구인지 기록(#204, src/common/top_who.h)하고, 다음에 앱을 열면 첫 화면 메시지로
 *   'can't delete: MnHangul on top' 등(#205 = 아직 안 보여 준 거부). 진단 10줄 off 뒤에 누구(H/S/C/?).
 * v18: 런처 지우기 때 자동 끄기(MnHangul v66과 같은 방식) — 켤 때 '-pdb'(sysNotifyDeleteProtectedEvent) 등록,
 *   크리에이터 MnSy DB를 지우려 하면 입력·키 글자 둘 다 맨 위(또는 이미 빠짐)인지 먼저 확인하고 둘 다 끈다. 하나라도 위에
 *   다른 패치가 있으면 아무것도 끄지 않음(지우기 거부). 등록 해제는 OFF 때, 그리고 앱 지우기 직전 'gbye' 알림 때.
 *   기록 #200 '-pdb' 수, #201 그 DB 크리에이터, #202 결과(1 끔, 2 위에 다른 패치, 4 켜진 것 없음), #203 등록 결과. 진단 화면 10줄.
 * v17: 켜기 전 기종 확인 — Sony SysInfo(FtrGet('SoNy', 1), Sony SDK SonySystemFtr.h)의 소프트웨어 실크 비트가 없으면
 *   켜지 않음('no Sony virtual silk', 결과 0xEE). ClieDiag v25 실측: NR70V extn 0x7F(실크 0x10 있음).
 * v16: 글꼴 높이도 FntCharHeight(v15 실측 일치). 예전 방식 비교 진단(v8 창·글꼴, SilkWareLib GetScrWindow)은 할 일을 마쳐 뺌 —
 *   동작 경로에서 글꼴 구조를 읽는 곳은 임시 글꼴 만들기(MakeTmpFont)만 남음.
 * v15: 창·글꼴 공개 함수 전환(spatch.c) — 화면 밖 창 = WinGetBitmap 비교, 자모 가운데 = FntCharWidth, 세로 위치 = FntBaseLine
 *   (v8 실측 일치). 글꼴 높이 f[7]은 그대로, FntCharHeight와 비교만(진단 10줄 hbad). 임시 글꼴 만들기는 구조 그대로.
 * v14: 첫 화면 3줄 [All OFF] = 자기와 MnHangul을 순서대로 끄기(src/common/mod_off.h). MnHangul이 실행 코드 kCmdModOff로 부르면 자기를 끈다.
 * v13: 판 번호를 kSyVer 한 곳에. 첫 화면 한글 표시 줄: CJKOS와 MnHangul이 둘 다 켜져 있으면 'CJKOS (+MnHangul)'.
 * v12: CJKOS 감지는 'dCJK' Feature만(함수 4개 방식 뺌). 첫 화면 'CJKOS'.
 * v11: CJKOS 감지 = 'dCJK' Feature(MnHangul v61과 같음). 첫 화면은 Feature로 확인되면 'CJKOS', 함수 4개 방식만 맞으면 'CJKOS?'.
 * v10: 조합 중 커서 이동 감지(MnSkCore build 17과 같은 방식, src/common/cursor_track.h). 진단 화면 9줄 = fld 비교·다름 수·기대/실제.
 * v9: CJKOS를 끈 뒤 입력 연결도 ⇪ 그림 때 스스로 다시 잇는다(v8 실측: 키 글자만 복구, 입력은 ON 필요).
 *   진단 화면 7줄 = 입력 함수 불린 수·EvtGetEvent 맨 위·입력 아래 주소, 9줄 = BltDrawChars 맨 위(⇪ 그림 순간).
 * v8 (진단, 동작 변경 없음): 판정 기록 화면 8~10줄 = 창 판정 세 가지(windowFlags / WinGetBitmap 비교 / GetScrWindow)·글꼴 공개 함수 비교.
 * v7: 연결 복구 때 ⇪ 실제 상태(#150)를 모름으로, 모를 때 ⇪ 누름은 Graffiti ⇪ 상태의 반대.
 * v6: 가상 실크 펜 누름 때(Sony가 다시 그리기 전) 키 글자 연결 확인·복구, 복구 직후 ⇪ 그림은 실제 상태로 씀.
 * v5: Update는 설치되어 있는 MnHangul도 다음 리셋 한 번 자동 시작을 건너뛰게 한다.
 * v4: 첫 화면 6줄 = 한글 표시 모듈(MnHangul / CJKOS? / none).
 * v3: CJKOS를 끄면 BltDrawChars가 ROM으로 되돌아가 키 글자 패치가 빠진다(v2 실측). 'Now:'는 실제 연결 상태(chain.h),
 *   빠졌으면 ON = 다시 잇기. ⇪ 그림을 그릴 때(ime.c)도 키 글자 패치가 빠졌으면 스스로 다시 잇는다.
 * 화면(v2, 쓰는 것만): [ON] [OFF], Start at reset(앱 설정 0번, 기본 ON), Update(다음 리셋 한 번 자동 시작
 *   건너뛰기, 앱 설정 3번), 사용법('한A' 그림). 제목을 누르면 판정 기록 화면.
 * 기록(feature 'MnSy'): #1 패치 진입점, #2 패치 핸들, #3 자모 그림, #4 자모 핸들, #5 ime 코드, #6 ime 핸들,
 *   #7 ime 상태 버퍼, #8·#9 '한A' 그림 핸들, #10 리셋 자동 시작 결과, #106 이 앱 실행 중,
 *   그 밖(#108·#127·#128·#143~#152·#160~#177)은 spatch.c·ime.c·dubeol.h 참고.
 */

#include <PalmOS.h>

#define kSyVer "v21"   /* 판 번호는 여기 한 곳만 고친다(모든 화면 제목이 이 값을 씀) */
#define SyTitle(rest) WinDrawInvertedChars("MnSonyKbd " kSyVer rest, sizeof("MnSonyKbd " kSyVer rest) - 1, 0, 0)
#include "chain.h"

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
#define kRomBlt      0x1008359AUL


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

/* v18: 지우기 알림('-pdb') 등록/해제. #203 = 등록 결과(0 정상), 해제 후 FFFFFFFF. 리셋 때 등록이 지워지므로 자동 시작에서도 등록 */
#define kFtrNtPdb   200
#define kFtrNtPdbCr 201
#define kFtrNtOff   202
#define kFtrNtReg   203
static void NotifyReg(Boolean on)
{
    UInt16  card;
    LocalID id;

    if (!AppDB(&card, &id))
        return;
    if (on)
        FtrSet(kSyCr, kFtrNtReg, (UInt32)SysNotifyRegister(card, id, sysNotifyDeleteProtectedEvent, NULL,
                                                           sysNotifyNormalPriority, NULL));
    else {
        SysNotifyUnregister(card, id, sysNotifyDeleteProtectedEvent, sysNotifyNormalPriority);
        FtrSet(kSyCr, kFtrNtReg, 0xFFFFFFFFUL);
    }
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
    NotifyReg(true);   /* v18 */
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
/* v17: Sony 가상 실크가 있는 기종인가. Sony SysInfo(구조 +4 extn, 비트 0x10 = 소프트웨어 실크).
 *   Sony SDK 헤더는 저장소에 넣지 않아 칸 위치만 적는다. 리셋 알림에서도 부르므로 전역 없음 */
#define kSonySilkBit 0x10UL
static Boolean SilkOk(void)
{
    UInt32 v = 0;

    if (FtrGet('SoNy', 1, &v) != errNone || v == 0)
        return false;
    return (*(const UInt32 *)((const UInt8 *)v + 4) & kSonySilkBit) != 0;
}

#define kOnNoSilk 0xEE   /* SonyOn 결과: Sony 가상 실크 없음(아무것도 바꾸지 않음) */

static UInt16 SonyOn(void)
{
    UInt16 p;

    if (!SilkOk())
        return kOnNoSilk;
    p = PatchOn();

    return (UInt16)(p << 4) | ImOn();
}

/* MnSonyKbd v11·v12(MnHangul v61·v62와 같음): CJKOS 동작 감지 = CJKOS가 켜질 때 등록하고 끌 때 지우는 'dCJK' Feature
 *   (ClieDiag v24 실측). v12: 함수 4개 방식은 v11 시험에서 일치 확인 뒤 뺌. 앱 화면에서만 부름(첫 화면 한글 표시 줄) */
static inline Boolean CjkFtr(void)
{
    UInt32 v = 0;

    return FtrGet('dCJK', 0, &v) == errNone;
}

/* ---------- 앱 화면에서만 부르는 함수 ---------- */

/* 0 끔, 1 켜져 있지 않음, 2 다른 패치가 위에 있어 못 끔. 전역 없음(v18: 지우기 알림에서도 부름).
 *   unreg = 알림 등록 해제(지우기 알림 처리 중에는 false — 'gbye' 때 푼다) */
static UInt16 PatchOff(Boolean unreg)
{
    UInt32 e = GetF(kFtrEntry);

    if (e == 0)
        return 1;
    if ((UInt32)SysGetTrapAddress(kTrapBlt) == e)
        SysSetTrapAddress(kTrapBlt, (void *)*(const UInt32 *)(e - 4));
    else if (SyBltLinked(e) != 0)   /* v3: 다른 패치 아래에 있거나 알 수 없음 → 그대로. 0 = 연결에서 이미 빠짐 → 풀기만 */
        return 2;
    MemHandleUnlock((MemHandle)GetF(kFtrJamoH));
    DmReleaseResource((MemHandle)GetF(kFtrJamoH));
    MemHandleUnlock((MemHandle)GetF(kFtrPatchH));
    DmReleaseResource((MemHandle)GetF(kFtrPatchH));
    FtrSet(kSyCr, kFtrEntry, 0);
    FtrSet(kSyCr, kFtrPatchH, 0);
    FtrSet(kSyCr, kFtrJamo, 0);
    FtrSet(kSyCr, kFtrJamoH, 0);
    if (unreg)
        NotifyReg(false);   /* v18: 보호를 풀기 전에 해제 */
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
    for (k = 0; k < 2; k++)   /* v3: 맨 위(1)는 되돌리고, 이미 빠진 것(0)은 풀기만, 다른 패치가 위(2)면 그대로 */
        if (SyImLinked(k == 0 ? sysTrapEvtGetEvent : sysTrapWinDrawBitmap,
                       (UInt32)(code + ((UInt32 *)code)[k])) == 2)
            return 2;
    for (k = 0; k < 2; k++) {
        UInt16 trap = k == 0 ? sysTrapEvtGetEvent : sysTrapWinDrawBitmap;

        if (SyImLinked(trap, (UInt32)(code + ((UInt32 *)code)[k])) == 1)
            SysSetTrapAddress(trap, (void *)((UInt32 *)code)[2 + k]);
    }
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

/* v14: 설치 전 두 모듈 순서대로 끄기. 0 꺼짐, 2 위에 다른 패치. 전역 없음(다른 앱이 불러도 됨) */
static UInt16 SelfOff(void)
{
    UInt16 i = ImOff(), p = PatchOff(true);

    return (i == 2 || p == 2) ? 2 : 0;
}

/* v18: 지우기 알림에서 끄기(전역 없음). 먼저 입력·키 글자 모두 풀 수 있는지(맨 위 또는 이미 빠짐) 확인하고, 하나라도
 *   위에 다른 패치가 있으면 아무것도 끄지 않는다. 결과(#202): 1 끔, 2 위에 다른 패치(남김), 4 켜진 것 없음 */
#define kFtrNtWho 204   /* v19: 거부 때 맨 위 패치(top_who.h kWho*) */
#define kFtrNtNew 205   /* v19: 아직 화면에 안 보여 준 거부 */
#include "../common/top_who.h"
/* *topP = 거부(2)일 때 맨 위 주소 */
static UInt16 NotifyOffCheck(UInt32 *topP)
{
    UInt8  *code = (UInt8 *)GetF(kFtrImC);
    UInt32  e = GetF(kFtrEntry), t;
    UInt16  k;

    if (code == NULL && e == 0)
        return 4;
    if (e != 0 && (t = (UInt32)SysGetTrapAddress(kTrapBlt)) != e && SyBltLinked(e) != 0) {
        *topP = t;
        return 2;
    }
    if (code != NULL)
        for (k = 0; k < 2; k++) {
            UInt16 trap = k == 0 ? sysTrapEvtGetEvent : sysTrapWinDrawBitmap;

            if (SyImLinked(trap, (UInt32)(code + ((UInt32 *)code)[k])) == 2) {
                *topP = (UInt32)SysGetTrapAddress(trap);
                return 2;
            }
        }
    return 0;
}

static void NotifyOff(void)
{
    UInt32 top = 0;
    UInt16 r = NotifyOffCheck(&top);

    if (r == 0) {
        ImOff();
        PatchOff(false);
        r = 1;
    }
    if (r == 4 && GetF(kFtrNtOff) == 1)   /* 이미 끈 뒤의 알림이 결과를 덮지 않게 */
        return;
    FtrSet(kSyCr, kFtrNtOff, r);
    if (r == 2) {   /* v19 */
        FtrSet(kSyCr, kFtrNtWho, TopWho(top, kSyCr));
        FtrSet(kSyCr, kFtrNtNew, 1);
    }
}

/* 알림 처리(전역 없음) */
static void NotifyLog(const SysNotifyParamType *np)
{
    UInt32 v = 0;

    if (np == NULL)
        return;
    if (np->notifyType == sysNotifyDeleteProtectedEvent) {
        FtrGet(kSyCr, kFtrNtPdb, &v);
        FtrSet(kSyCr, kFtrNtPdb, v + 1);
        if (np->notifyDetailsP != NULL) {
            UInt32 cr = ((const SysNotifyDBInfoType *)np->notifyDetailsP)->creator;

            FtrSet(kSyCr, kFtrNtPdbCr, cr);
            if (cr == kSyCr)   /* 우리 앱을 지우려 함 → 끄고 보호 해제 */
                NotifyOff();
        }
    } else if (np->notifyType == 'gbye')   /* sysNotifyCleanupEvent: 앱이 지워지기 직전(앱에만 옴) → 등록 해제 */
        NotifyReg(false);
}

#include "../common/mod_off.h"

static void SonyOff(void)
{
    UInt16 i = ImOff(), p = PatchOff(true);

    if (i == 2 || p == 2)
        StrCopy(gMsg, "kept: use Update, then install");
    else
        StrCopy(gMsg, "off: ready to install");
}

/* v3: 실제 연결 상태. 0 꺼짐, 1 연결됨, 2 알 수 없음(위에 다른 패치), 3 빠짐(다시 이어야 함) */
static UInt16 Status(void)
{
    UInt8  *code = (UInt8 *)GetF(kFtrImC);
    UInt16  b, g, m;

    if (GetF(kFtrEntry) == 0 || code == NULL)
        return 0;
    b = SyBltLinked(GetF(kFtrEntry));
    g = SyImLinked(sysTrapEvtGetEvent, (UInt32)(code + ((UInt32 *)code)[0]));
    m = SyImLinked(sysTrapWinDrawBitmap, (UInt32)(code + ((UInt32 *)code)[1]));
    if (b == 0 || g == 0 || m == 0)
        return 3;
    return (b == 2 || g == 2 || m == 2) ? 2 : 1;
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

/* v57: Update = 설치되어 있는 다른 모듈(MnHangul)도 다음 리셋 한 번 자동 시작 건너뛰기(앱 설정 3번).
 *   설치되어 있지 않으면 아무것도 저장하지 않는다(하나만 쓰는 경우는 그대로) */
static void SkipOther(UInt32 cr)
{
    DmSearchStateType st;
    UInt16            card, v = 1;
    LocalID           id;

    if (DmGetNextDatabaseByTypeCreator(true, &st, sysFileTApplication, cr, true, &card, &id) == errNone)
        PrefSetAppPreferences(cr, 3, 1, &v, sizeof v, true);
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
            if (upd) {
                PrefPut(3, 1);
                SkipOther('MnHg');
            }
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
    SyTitle("");
    Row(0, "Hangul kbd on Sony virtual kbd");
    WinDrawInvertedChars(gMsg, StrLen(gMsg), 0, 2 * kLineH);
    {
        static const Char *nm[4] = { "off", "ON", "ON (other patch above)", "broken: tap ON" };
        UInt16             st = Status();

        StrPrintF(s, "Now: %s", nm[st]);
    }
    Row(2, s);
    ButtonAt(3, kRowOn, "ON");
    ButtonAt(40, kRowOn, "OFF");
    ButtonAt(80, kRowOn, "All OFF");   /* v14 */
    ButtonAt(3, kRowAuto, PrefGet(0, 1) ? "Start at reset: ON" : "Start at reset: off");
    ButtonAt(3, kRowUpd, "Update (hold 3s)");
    {   /* v4: 한글 표시 모듈(없으면 입력한 한글이 깨져 보임) */
        UInt32 e = 0;

        FtrGet('MnHg', 21, &e);
        /* v13: MnHangul을 켠 뒤 CJKOS를 켜면 실제 그리기는 CJKOS(10-18 실측) — 둘 다 켜져 있음을 숨기지 않는다 */
        Row(6, CjkFtr() ? (e != 0 ? "Hangul display: CJKOS (+MnHangul)" : "Hangul display: CJKOS") :
                e != 0 ? "Hangul display: MnHangul" : "Hangul display: none (broken)");
    }
    if (ih != NULL) {   /* 사용법: '한A' 그림(켜짐) = 한/영 */
        WinDrawBitmap((BitmapPtr)MemHandleLock(ih), 3, 8 * kLineH);   /* 22×18(저해상도 좌표로 2배) */
        MemHandleUnlock(ih);
        DmReleaseResource(ih);
    }
    WinDrawChars("key = Hangul / English", 22, 28, 8 * kLineH + 4);
    Row(9, "Shift in Hangul = double cons.");
    Row(10, "Update: All OFF or [Update], install, ON");
}

/* 판정 기록(MnHangul KDlg 7~11줄과 같음) */
static void DrawDebug(void)
{
    RectangleType r;
    Char          s[48];
    UInt32        w = GetF(128);
    UInt16        m;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    SyTitle(" Debug [tap: back]");
    StrPrintF(s, "blt below %lx  jamo %lx", GetF(kFtrEntry) ? *(const UInt32 *)(GetF(kFtrEntry) - 4) : 0UL,
              GetF(kFtrBlt));
    Row(0, s);
    StrPrintF(s, "draw %lu ovl %lu heal %lx b%lx", GetF(108), GetF(127), GetF(kSyFtrHeal), GetF(kFtrBoot) >> 28);
    Row(1, s);
    /* v21 진단: 3~6줄 = 가상 실크(화면 밖 창) 그림 그리기 최근 4개(오래된 것부터). v46 판정 기록 표시는 잠시 뺌(기록은 계속) */
    for (m = 0; m < 4; m++) {
        UInt16 k = (UInt16)((GetF(220) + m) & 3);
        UInt32 a = GetF(221 + 3 * k), wh = GetF(222 + 3 * k), xy = GetF(223 + 3 * k);

        StrPrintF(s, "bmp %lx %ux%u @%d,%d", a, (UInt16)(wh >> 16), (UInt16)wh, (Int16)(xy >> 16), (Int16)xy);
        Row(2 + m, s);
    }
    StrPrintF(s, "silk bmps %lu", GetF(233));
    Row(6, s);
    {   /* v16: 8·10줄의 v8 비교 진단은 뺌(창·글꼴은 공개 함수로 바꿈, KOREAN_SUPPORT v8·v15 실측) */
        {   /* v20 진단: sym 탭·한/영 빈칸 */
            UInt32 xy = GetF(217), wh = GetF(218);

            StrPrintF(s, "sym %lu @%d,%d %ux%u han %lu t%lu", GetF(216), (Int16)(xy >> 16), (Int16)xy,
                      (UInt16)(wh >> 16), (UInt16)wh, GetF(214), GetF(219));   /* #214 = dubeol.h kFtrHanTab */
            Row(8, s);
        }
        {   /* v20 진단: 가상 실크 펜 누름 3개(오래된 것부터), 빈칸 누른 뒤 다른 창 글자 그리기 수 차이 */
            UInt16 k;
            Char   t[16];

            StrCopy(s, "pen");
            for (k = 0; k < 3; k++) {
                UInt32 v = GetF(211 + (UInt16)((GetF(210) + k) % 3));

                StrPrintF(t, " %d,%d", (Int16)(v >> 16), (Int16)v);
                StrCat(s, t);
            }
            StrPrintF(t, " d%ld", (Int32)(GetF(108) - GetF(215)));
            StrCat(s, t);
            Row(7, s);
        }
        {   /* v10: 조합 중 커서 이동 감지 — 비교 수, 다름 수, 마지막 기대/실제 위치, X 다름, C 조합 중 (v9의 blt top 줄 자리) */
            UInt32 fl = GetF(194), la = GetF(193);

            StrPrintF(s, "fld %lu bad %lu e%u a%u%s%s", GetF(191), GetF(192), (UInt16)(la >> 16), (UInt16)la,
                      (fl & 1) ? " X" : "", (fl & 2) ? " C" : "");
            Row(9, s);
        }
        {   /* v18: 지우기 알림 — 등록(ok / - 해제 / 오류 코드), '-pdb' 수·마지막 DB, 자동 끄기 결과 */
            UInt32 cr = GetF(kFtrNtPdbCr), rg = GetF(kFtrNtReg);
            Char   a[8], b[12];

            a[0] = (Char)(cr >> 24); a[1] = (Char)(cr >> 16); a[2] = (Char)(cr >> 8); a[3] = (Char)cr; a[4] = 0;
            if (cr == 0)
                StrCopy(a, "-");
            if (rg == 0xFFFFFFFFUL)
                StrCopy(b, "-");
            else if (rg == 0)
                StrCopy(b, "ok");
            else
                StrPrintF(b, "%lx", rg);
            StrPrintF(s, "ntf %s -pdb %lu %s off%lu%s", b, GetF(kFtrNtPdb), a, GetF(kFtrNtOff),
                      GetF(kFtrNtOff) == 2 ? (GetF(kFtrNtWho) == kWhoHg ? "H" : GetF(kFtrNtWho) == kWhoSy ? "S" :
                                              GetF(kFtrNtWho) == kWhoCjk ? "C" : "?") : "");
            Row(10, s);
        }
    }
    StrPrintF(s, "why%u H%lu S%lx p%lu ic%lu@%lu", (UInt16)(w >> 24), GetF(143), GetF(150), GetF(144),
              GetF(147), GetF(146));
    Row(11, s);
    Row(12, "[tap here: clear diag]");
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
    if (cmd == sysAppLaunchCmdNotify) {   /* v18: 지우기 알림 */
        NotifyLog((const SysNotifyParamType *)cmdPBP);
        return 0;
    }
    if (cmd == kCmdModOff)   /* v14: MnHangul이 설치 전 끄기를 부탁 */
        return SelfOff();
    if (cmd != sysAppLaunchCmdNormalLaunch)
        return 0;

    FtrSet(kSyCr, kFtrRun, 1);
    if (GetF(kFtrNtNew) != 0) {   /* v19: 지우기 거부 안내(한 번) */
        WhoMsg(gMsg, (UInt16)GetF(kFtrNtWho));
        FtrSet(kSyCr, kFtrNtNew, 0);
    }
    StartHires();
    Draw();
    do {
        EvtGetEvent(&e, evtWaitForever);
        if (e.eType == penDownEvent && e.screenY >= 0 && e.screenY < 160) {
            Int16 row = e.screenY / kLineH - 1;

            if (e.screenY < kLineH)   /* 제목 = 판정 기록 화면 ↔ 첫 화면 */
                gDebug = !gDebug;
            else if (gDebug) {
                if (row == 12) {   /* v8: 진단 기록 지우기(v9: #188~#190, v10: #191~#194, v15: #198·#199 포함) */
                    UInt16 n;

                    for (n = 180; n <= 194; n++)
                        FtrSet(kSyCr, n, 0);
                    FtrSet(kSyCr, 198, 0);
                    FtrSet(kSyCr, 199, 0);
                }
            }
            else if (row == kRowOn && e.screenX >= 78)   /* v14: 두 모듈 순서대로 끄기 */
                ModOffMsg(gMsg, ModOffBoth(SelfOff, 'MnHg'), CjkFtr());
            else if (row == kRowOn && e.screenX >= 40)
                SonyOff();
            else if (row == kRowOn) {
                if (Status() == 0) {
                    UInt16 rr = SonyOn();

                    if (rr == kOnNoSilk)
                        StrCopy(gMsg, "no Sony virtual silk");   /* v17 */
                    else
                        StrPrintF(gMsg, rr == 0 || rr == 0x11 ? "ON" : "ON: result %x", rr);
                } else {   /* v3: 빠진 연결 다시 잇기 */
                    UInt16 n = (SyBltHeal() ? 1 : 0) + SyImHeal();

                    if (n != 0)
                        StrPrintF(gMsg, "re-attached %u", n);
                    else
                        StrCopy(gMsg, Status() == 3 ? "broken: use Update" : "already ON");
                }
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
