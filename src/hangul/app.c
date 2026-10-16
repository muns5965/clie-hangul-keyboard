/*
 * MnHangul v15 — CJKOS 대체 한글 모듈 시험 앱 (2a~2c 그리기, 글꼴 덤프·크기 조사, 3단계 호출 조사, 3a 글자 경계, 3b 폭)
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
 * v4: 아래 버튼(PageDown)으로 Font 페이지. 마지막 호출 p6(#3)과 마지막 한글 호출 p6(#18)의 글꼴 머리 13워드
 *     (fontType firstChar lastChar maxWidth kernMax nDescent fRectWidth fRectHeight owTLoc ascent descent
 *     leading rowWords)와 SDK 구조로 계산한 표 위치, 'A'·0xC7의 오프셋/폭 항목을 보인다(2c 임시 글꼴 설계용).
 *     위 버튼(PageUp)으로 Main 페이지.
 * v5: 메시지 줄이 맨 아래 기록 줄(12)과 겹치던 것 수정(빈 줄 1로 옮김). 맨 위 제목 줄을 탭해도 페이지 전환.
 * v6: Box 버튼 → Mode 버튼(off → box → glyph 순환, #22 = 0/1/2). Patch ON 때 글자 데이터 리소스 HgFn 0·1
 *     (Neo둥근모 16×16 완성형, 시험용)을 잠가 포인터를 #23·#24, 핸들을 #25·#26에 둔다(Patch OFF 때 해제).
 *     버튼 오른쪽에 한글 견본(기존 함수 WinDrawChars, 고해상도 모드 = 배율 2 경우)을 그린다.
 * v7: 패치가 글꼴 높이 16~40에서 한글을 그림. Sizes 페이지(#30~#37: 글꼴 높이·사각형 높이별 한글 호출 수).
 *     제목 줄 탭 = Main → Font → Sizes → Main, 아래 버튼 = 다음 페이지, 위 버튼 = Main.
 * v8: Calls 페이지(3단계 조사). 2번째 줄 탭 = Count ON/OFF. 켜면 폭 함수 트랩 8개와 Intl 루틴 16개에
 *     통과 코드(리소스 HgCt 0, src/hangul/count.s)를 끼워 호출 수만 센다(동작은 그대로).
 *     트랩은 SysSetTrapAddress, Intl 루틴은 IntlSetRoutineAddress(선택자 29)로 바꾼다.
 *     끄기: 아직 우리 코드를 가리키는 것만 원래대로. 기록 #40 핸들, #41 횟수 블록, #42 코드 주소.
 *     Main의 Clear counters가 횟수도 0으로 만든다. 페이지: Main → Font → Sizes → Calls.
 * v9: 3a 글자 경계. Main 5번째 줄 오른쪽 [Intl ON/OFF] = Intl 루틴 6개(CharAttr, PrevChar, NextChar, SetNext,
 *     CharBounds, CharWidth)를 리소스 HgIt 0(src/hangul/intl.c)의 EUC-KR 판으로 교체(IntlSetRoutineAddress).
 *     기록 #45 핸들, #46 코드 주소. 글자 데이터에 자모 행 HgFn 2 추가(#43 포인터, #44 핸들, Patch ON 때 잠금).
 * v10: 3b 폭. Main 4번째 줄 오른쪽 [Width ON/OFF] = 폭 함수 트랩 5개(FntCharsWidth, FntLineWidth, FntCharsInWidth,
 *     FntWordWrap, FntWidthToOffset)를 리소스 HgWd 0(src/hangul/width.c)으로 교체. 기록 #47 핸들, #48 코드 주소.
 *     첫 줄에 prot N = 지금 켜진 기능 수(각각 DB 삭제 보호 1개). 0이어야 MnHangul을 덮어써 설치할 수 있다.
 * v11: Width 페이지(폭 함수 진단: 함수별 호출 수·한글 경로 수, 마지막 한글 폭·배율·글꼴 높이, #50~#63).
 *     Main 3번째 줄 오른쪽 [All OFF] = Width → Intl → Count → Patch 순서로 모두 끈다(업데이트 설치 전).
 *     페이지: Main → Font → Sizes → Calls → Width.
 * v12: 한글 폭 수정(width.c·intl.c). 폭을 잴 때 저해상도 글꼴(높이 8~15)이면 9 — 그릴 때 2배 고해상도라서.
 * v13: 글자 데이터 다시 생성(tools/gen-hangul16.py: 글꼴 격자에서 직접 그림). 조각이 겹치는 곳의 두꺼운 획 수정.
 * v14: 완성형 글자 데이터(HgFn 0~2, 78KB)를 조합형 묶음으로 교체: HgJb 0 = Neo둥근모 16×16(14KB),
 *     HgJb 1 = 물마루 기반 12×12(23KB, 칸 높이 12~15에서 사용). tools/gen-johab.py가 11,172자 전부 원본과 같음을 확인.
 * v15: 11칸에도 12×12(한글은 위 11줄 안). Sizes 페이지에 크기별 마지막으로 그린 앱 크리에이터(#64~#71; #40~#48은 핸들이라 피함).
 */

#include <PalmOS.h>

#define kHgCreator    'MnHg'
#define kPatchRes     'HgPt'
#define kTrapBlt      0xA378
#define kFtrHandle    20
#define kFtrEntry     21
#define kFtrMax       19
#define kFtrBox       22      /* 모드: 0 off, 1 box, 2 glyph */
#define kFtrGlyph0    23
#define kFtrGlyph1    24
#define kFtrGlyphH0   25
#define kFtrGlyphH1   26
#define kGlyphN       2        /* v14: HgJb 0 = 조합형 16×16(Neo둥근모), HgJb 1 = 조합형 12×12 */
#define kFtrIntlH     45
#define kFtrIntlC     46
#define kIntlRes      'HgIt'
#define kIntlN        6
#define kFtrWidthH    47
#define kFtrWidthC    48
#define kWidthRes     'HgWd'
#define kWidthN       5
static const UInt16 kWidthTrap[kWidthN] = {
    sysTrapFntCharsWidth, sysTrapFntLineWidth, sysTrapFntCharsInWidth, sysTrapFntWordWrap, sysTrapFntWidthToOffset
};
static const UInt16 kGlyphPtrFtr[kGlyphN] = { 23, 24 };
static const UInt16 kGlyphHFtr[kGlyphN]   = { 25, 26 };
static const UInt16 kIntlSel[kIntlN] = {
    intlTxtCharAttr, intlTxtGetPreviousChar, intlTxtGetNextChar,
    intlTxtSetNextChar, intlTxtCharBounds, intlTxtCharWidth
};
#define kFtrGlyphCnt  27
#define kFtrRatio     28
#define kGlyphRes     'HgJb'

#define kLineH        11
#define kRowOn        2
#define kRowOff       3
#define kRowBox       4
#define kRowClear     5

static Char   gMsg[40] = "-";
static UInt16 gPage = 0;   /* 0 Main, 1 Font, 2 Sizes, 3 Calls, 4 Width */
#define kPages 5

/* 3단계 호출 조사 대상 (count.s의 StubN 순서와 같다): 0~7 트랩, 8~23 Intl 선택자 */
#define kCountN      24
#define kCountTraps  8
#define kCountRes    'HgCt'
#define kFtrCountH   40
#define kFtrCountP   41
#define kFtrCountC   42
static const UInt16 kCountId[kCountN] = {
    sysTrapFntCharWidth, sysTrapFntCharsWidth, sysTrapFntCharsInWidth, sysTrapFntLineWidth,
    sysTrapFntWordWrap, sysTrapFntWordWrapReverseNLines, sysTrapFntWidthToOffset, sysTrapFldWordWrap,
    intlTxtByteAttr, intlTxtCharAttr, intlTxtCharXAttr, intlTxtCharSize,
    intlTxtGetPreviousChar, intlTxtGetNextChar, intlTxtGetChar, intlTxtSetNextChar,
    intlTxtCharBounds, intlTxtWordBounds, intlTxtCharEncoding, intlTxtStrEncoding,
    intlTxtCharIsValid, intlTxtCharWidth, intlTxtGetTruncationOffset, intlTxtGetWordWrapOffset
};
static const Char *kCountName[kCountN] = {
    "CharW", "CharsW", "CharsInW", "LineW", "WordWrap", "WWrapRevN", "W2Offset", "FldWWrap",
    "ByteAttr", "CharAttr", "CharXAttr", "CharSize", "PrevChar", "NextChar", "GetChar", "SetNext",
    "CharBnds", "WordBnds", "CharEnc", "StrEnc", "IsValid", "CharWid", "TruncOfs", "WWrapOfs"
};

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

/* 글자 데이터 HgFn 0·1 잠금(앱이 끝나도 DB 보호 중이라 주소 유지) */
static void LockGlyphs(void)
{
    UInt16 i;

    for (i = 0; i < kGlyphN; i++) {
        MemHandle h = DmGetResource(kGlyphRes, i);

        FtrSet(kHgCreator, kGlyphHFtr[i], (UInt32)h);
        FtrSet(kHgCreator, kGlyphPtrFtr[i], (h != NULL) ? (UInt32)MemHandleLock(h) : 0);
    }
}

static void UnlockGlyphs(void)
{
    UInt16 i;

    for (i = 0; i < kGlyphN; i++) {
        UInt32 h = GetF(kGlyphHFtr[i]);

        FtrSet(kHgCreator, kGlyphPtrFtr[i], 0);
        FtrSet(kHgCreator, kGlyphHFtr[i], 0);
        if (h != 0) {
            MemHandleUnlock((MemHandle)h);
            DmReleaseResource((MemHandle)h);
        }
    }
}

/* 3a: Intl 루틴 6개 교체 */
static void IntlOn(void)
{
    MemHandle h;
    UInt8    *code;
    UInt16    i, card;
    LocalID   id;

    if (GetF(kFtrIntlC) != 0) {
        StrCopy(gMsg, "intl already ON");
        return;
    }
    h = DmGetResource(kIntlRes, 0);
    if (h == NULL) {
        StrCopy(gMsg, "intl resource missing");
        return;
    }
    code = MemHandleLock(h);
    for (i = 0; i < kIntlN; i++) {
        void *o = IntlGetRoutineAddress(kIntlSel[i]);

        DmWrite(code, 24 + 4 * i, &o, 4);
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, true);
    FtrSet(kHgCreator, kFtrIntlH, (UInt32)h);
    FtrSet(kHgCreator, kFtrIntlC, (UInt32)code);
    for (i = 0; i < kIntlN; i++)
        IntlSetRoutineAddress(kIntlSel[i], code + ((UInt32 *)code)[i]);
    StrCopy(gMsg, "intl ON (6)");
}

static void IntlOff(void)
{
    UInt8  *code = (UInt8 *)GetF(kFtrIntlC);
    UInt16  i, kept = 0, card;
    LocalID id;

    if (code == NULL) {
        StrCopy(gMsg, "intl not ON");
        return;
    }
    for (i = 0; i < kIntlN; i++) {
        UInt8 *entry = code + ((UInt32 *)code)[i];

        if ((UInt8 *)IntlGetRoutineAddress(kIntlSel[i]) == entry)
            IntlSetRoutineAddress(kIntlSel[i], (void *)((UInt32 *)code)[6 + i]);
        else
            kept++;
    }
    if (kept != 0) {
        StrPrintF(gMsg, "intl: %u changed by other, kept", kept);
        return;
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, false);
    MemHandleUnlock((MemHandle)GetF(kFtrIntlH));
    DmReleaseResource((MemHandle)GetF(kFtrIntlH));
    FtrSet(kHgCreator, kFtrIntlH, 0);
    FtrSet(kHgCreator, kFtrIntlC, 0);
    StrCopy(gMsg, "intl OFF");
}

/* 3b: 폭 함수 트랩 5개 교체 */
static void WidthOn(void)
{
    MemHandle h;
    UInt8    *code;
    UInt16    i, card;
    LocalID   id;

    if (GetF(kFtrWidthC) != 0) {
        StrCopy(gMsg, "width already ON");
        return;
    }
    h = DmGetResource(kWidthRes, 0);
    if (h == NULL) {
        StrCopy(gMsg, "width resource missing");
        return;
    }
    code = MemHandleLock(h);
    for (i = 0; i < kWidthN; i++) {
        void *o = SysGetTrapAddress(kWidthTrap[i]);

        DmWrite(code, 20 + 4 * i, &o, 4);
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, true);
    FtrSet(kHgCreator, kFtrWidthH, (UInt32)h);
    FtrSet(kHgCreator, kFtrWidthC, (UInt32)code);
    for (i = 0; i < kWidthN; i++)
        SysSetTrapAddress(kWidthTrap[i], code + ((UInt32 *)code)[i]);
    StrCopy(gMsg, "width ON (5)");
}

static void WidthOff(void)
{
    UInt8  *code = (UInt8 *)GetF(kFtrWidthC);
    UInt16  i, kept = 0, card;
    LocalID id;

    if (code == NULL) {
        StrCopy(gMsg, "width not ON");
        return;
    }
    for (i = 0; i < kWidthN; i++) {
        UInt8 *entry = code + ((UInt32 *)code)[i];

        if ((UInt8 *)SysGetTrapAddress(kWidthTrap[i]) == entry)
            SysSetTrapAddress(kWidthTrap[i], (void *)((UInt32 *)code)[5 + i]);
        else
            kept++;
    }
    if (kept != 0) {
        StrPrintF(gMsg, "width: %u changed by other, kept", kept);
        return;
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, false);
    MemHandleUnlock((MemHandle)GetF(kFtrWidthH));
    DmReleaseResource((MemHandle)GetF(kFtrWidthH));
    FtrSet(kHgCreator, kFtrWidthH, 0);
    FtrSet(kHgCreator, kFtrWidthC, 0);
    StrCopy(gMsg, "width OFF");
}

/* 켜진 기능 수 = 걸려 있는 DB 삭제 보호 수 */
static UInt16 ProtectCount(void)
{
    return (GetF(kFtrEntry) ? 1 : 0) + (GetF(kFtrCountC) ? 1 : 0) +
           (GetF(kFtrIntlC) ? 1 : 0) + (GetF(kFtrWidthC) ? 1 : 0);
}

static void PatchOff(void);
static void CountOff(void);

/* 모두 끄기: 켠 반대 순서 */
static void AllOff(void)
{
    if (GetF(kFtrWidthC) != 0)
        WidthOff();
    if (GetF(kFtrIntlC) != 0)
        IntlOff();
    if (GetF(kFtrCountC) != 0)
        CountOff();
    if (GetF(kFtrEntry) != 0)
        PatchOff();
    if (ProtectCount() == 0)
        StrCopy(gMsg, "all off: ready to install");
}

/* v11 폭 진단 */
static void DrawWidthPage(void)
{
    static const Char *name[6] = { "CharsW", "LineW", "CharsInW", "WordWrap", "W2Offset", "CharWid" };
    RectangleType r;
    Char          s[48];
    UInt32        v;
    UInt16        i;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnHangul v15 Width [tap: Main]", 30, 0, 0);
    StrPrintF(s, "width %s intl %s  (calls / hangul)", GetF(kFtrWidthC) ? "ON" : "off",
              GetF(kFtrIntlC) ? "ON" : "off");
    Row(0, s);
    for (i = 0; i < 6; i++) {
        StrPrintF(s, "%s  %lu / %lu", name[i], GetF(50 + i), GetF(56 + i));
        Row(1 + i, s);
    }
    v = GetF(62);
    StrPrintF(s, "last hangul w %u  x%u  font h %u", (UInt16)(v >> 16), (UInt16)((v >> 8) & 0xFF),
              (UInt16)(v & 0xFF));
    Row(8, s);
    v = GetF(63);
    StrPrintF(s, "last: %s len %u -> %u", name[(v >> 24) < 6 ? (v >> 24) : 0], (UInt16)((v >> 12) & 0xFFF),
              (UInt16)(v & 0xFFF));
    Row(9, s);
    Row(11, "Clear counters (Main) resets");
}

static void ButtonAt(UInt16 x, UInt16 row, const Char *s)
{
    RectangleType r;

    RctSetRectangle(&r, x, (row + 1) * kLineH + 1, FntCharsWidth(s, StrLen(s)) + 6, kLineH - 2);
    WinDrawChars(s, StrLen(s), x + 3, (row + 1) * kLineH);
    WinDrawRectangleFrame(roundFrame, &r);
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
    LockGlyphs();
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
    UnlockGlyphs();
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

static void ModeText(Char *o, UInt32 m)
{
    StrCopy(o, (m == 2) ? "glyph" : (m == 1) ? "box" : "off");
}

static void ToggleBox(void)
{
    UInt32 m = (GetF(kFtrBox) + 1) % 3;

    FtrSet(kHgCreator, kFtrBox, m);
    ModeText(gMsg, m);
    StrCat(gMsg, Installed() ? " mode" : " mode (needs Patch ON)");
}

static void *GetRoutine(UInt16 i)
{
    return (i < kCountTraps) ? SysGetTrapAddress(kCountId[i]) : IntlGetRoutineAddress(kCountId[i]);
}

static void SetRoutine(UInt16 i, void *a)
{
    if (i < kCountTraps)
        SysSetTrapAddress(kCountId[i], a);
    else
        IntlSetRoutineAddress(kCountId[i], a);
}

static void CountOn(void)
{
    MemHandle h;
    UInt8    *code;
    UInt32   *cnt;
    UInt16    i, card, n = 0;
    LocalID   id;

    if (GetF(kFtrCountC) != 0) {
        StrCopy(gMsg, "count already ON");
        return;
    }
    h = DmGetResource(kCountRes, 0);
    if (h == NULL) {
        StrCopy(gMsg, "count resource missing");
        return;
    }
    cnt = MemPtrNew(kCountN * 4);
    if (cnt == NULL) {
        DmReleaseResource(h);
        StrCopy(gMsg, "no memory");
        return;
    }
    MemSet(cnt, kCountN * 4, 0);
    MemPtrSetOwner(cnt, 0);   /* 앱이 끝나도 남긴다 */
    code = MemHandleLock(h);
    DmWrite(code, 0, &cnt, 4);
    for (i = 0; i < kCountN; i++) {
        void *o = GetRoutine(i);

        DmWrite(code, 4 + 4 * i, &o, 4);
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, true);
    FtrSet(kHgCreator, kFtrCountH, (UInt32)h);
    FtrSet(kHgCreator, kFtrCountP, (UInt32)cnt);
    FtrSet(kHgCreator, kFtrCountC, (UInt32)code);
    for (i = 0; i < kCountN; i++)
        if (*(UInt32 *)(code + 4 + 4 * i) != 0) {   /* 원래 주소가 없는 것은 건너뜀 */
            SetRoutine(i, code + 100 + 18 * i);
            n++;
        }
    StrPrintF(gMsg, "count ON (%u)", n);
}

static void CountOff(void)
{
    UInt8  *code = (UInt8 *)GetF(kFtrCountC);
    UInt16  i, kept = 0, card;
    LocalID id;

    if (code == NULL) {
        StrCopy(gMsg, "count not ON");
        return;
    }
    for (i = 0; i < kCountN; i++) {
        UInt8 *stub = code + 100 + 18 * i;

        if ((UInt8 *)GetRoutine(i) == stub)
            SetRoutine(i, (void *)*(UInt32 *)(code + 4 + 4 * i));
        else if (*(UInt32 *)(code + 4 + 4 * i) != 0)
            kept++;
    }
    if (kept != 0) {   /* 다른 것이 위에 끼워져 있음: 코드를 풀면 안 된다 */
        StrPrintF(gMsg, "count: %u changed by other, kept", kept);
        return;
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, false);
    MemHandleUnlock((MemHandle)GetF(kFtrCountH));
    DmReleaseResource((MemHandle)GetF(kFtrCountH));
    MemPtrFree((void *)GetF(kFtrCountP));
    FtrSet(kHgCreator, kFtrCountH, 0);
    FtrSet(kHgCreator, kFtrCountP, 0);
    FtrSet(kHgCreator, kFtrCountC, 0);
    StrCopy(gMsg, "count OFF");
}

static void DrawCallsPage(void)
{
    RectangleType r;
    Char          s[48];
    UInt32       *cnt = (UInt32 *)GetF(kFtrCountP);
    UInt16        i;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnHangul v15 Calls [tap: Width]", 31, 0, 0);
    StrPrintF(s, "[tap] count %s: %s", GetF(kFtrCountC) ? "ON" : "off", gMsg);
    Row(0, s);
    for (i = 0; i < kCountN; i++) {   /* 줄 1~12, 두 칸씩 */
        StrPrintF(s, "%s %lu", kCountName[i], (cnt != NULL) ? cnt[i] : 0UL);
        WinDrawChars(s, StrLen(s), (i & 1) ? 82 : 2, (1 + i / 2) * kLineH + kLineH);
    }
}

static void ClearCounters(void)
{
    UInt16 i;

    for (i = 0; i <= kFtrMax; i++)
        FtrUnregister(kHgCreator, i);   /* #20~#26(핸들·진입점·모드·글자 데이터)은 지우지 않는다 */
    FtrUnregister(kHgCreator, kFtrGlyphCnt);
    FtrUnregister(kHgCreator, kFtrRatio);
    for (i = 30; i <= 37; i++)
        FtrUnregister(kHgCreator, i);
    for (i = 64; i <= 71; i++)
        FtrUnregister(kHgCreator, i);
    if (GetF(kFtrCountP) != 0)
        MemSet((void *)GetF(kFtrCountP), kCountN * 4, 0);
    for (i = 50; i <= 63; i++)
        FtrUnregister(kHgCreator, i);
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

/* 읽어도 되는 주소인지: ROM(0x10xxxxxx) 또는 RAM(16MB 아래), 짝수 */
static Boolean Readable(UInt32 a)
{
    return a != 0 && (a & 1) == 0 && (a < 0x01000000UL || (a >> 24) == 0x10);
}

/* 글꼴 하나를 6줄로: 머리 13워드, SDK FontType 구조로 계산한 표 위치, 'A'·0xC7 항목 */
static void DumpFont(UInt16 row, const Char *name, UInt32 a)
{
    const Int16 *w = (const Int16 *)a;
    Char         s[48];
    UInt32       bmp, loc, owt, exp;
    Int16        first, last;
    UInt16       k;

    StrPrintF(s, "%s @%lx", name, a);
    Row(row, s);
    if (!Readable(a)) {
        Row(row + 1, "  (none)");
        return;
    }
    for (k = 0; k < 3; k++) {
        UInt16 i = k * 5;

        if (k < 2)
            StrPrintF(s, "%d: %x %x %x %x %x", i, w[i], w[i + 1], w[i + 2], w[i + 3], w[i + 4]);
        else
            StrPrintF(s, "%d: %x %x %x", i, w[i], w[i + 1], w[i + 2]);
        Row(row + 1 + k, s);
    }
    first = w[1];
    last = w[2];
    bmp = (UInt32)(UInt16)w[12] * 2 * (UInt16)w[7];
    loc = 26 + bmp;
    owt = 16 + (UInt32)(UInt16)w[8] * 2;
    exp = loc + (UInt32)(last - first + 2) * 2;
    StrPrintF(s, "bmp %lu loc %lu owt %lu/%lu", bmp, loc, owt, exp);
    Row(row + 4, s);
    if (owt < 0x8000UL && last >= first && last - first < 512) {
        UInt16 ea = 0xFFFF, ec = 0xFFFF;

        if (0x41 >= first && 0x41 <= last)
            ea = *(const UInt16 *)(a + owt + (0x41 - first) * 2);
        if (0xC7 >= first && 0xC7 <= last)
            ec = *(const UInt16 *)(a + owt + (0xC7 - first) * 2);
        StrPrintF(s, "ow A %x  C7 %x", ea, ec);
        Row(row + 5, s);
    }
}

static void DrawFontPage(void)
{
    RectangleType r;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnHangul v15 Font [tap: Sizes]", 30, 0, 0);
    DumpFont(0, "last p6", GetF(3));
    DumpFont(6, "hi p6", GetF(18));
}

/* v7 크기 조사 표 */
static void DrawSizesPage(void)
{
    RectangleType r;
    Char          s[48];
    UInt16        i, row = 1;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnHangul v15 Sizes [tap: Calls]", 31, 0, 0);
    Row(0, "hangul calls by font h / rect h");
    for (i = 30; i <= 37; i++) {
        UInt32 v = GetF(i);
        UInt16 fh, rh;

        if (v == 0)
            continue;
        fh = (UInt16)(v >> 24);
        rh = (UInt16)((v >> 16) & 0xFF);
        {
            UInt32 c = GetF(i + 34);   /* v15: 마지막으로 그린 앱 크리에이터 */
            Char   cr[5];

            cr[0] = (Char)(c >> 24); cr[1] = (Char)(c >> 16); cr[2] = (Char)(c >> 8); cr[3] = (Char)c; cr[4] = 0;
            if (c == 0)
                StrCopy(cr, "-");
            StrPrintF(s, "font %u rect %u x%s : %lu  %s", fh, rh,
                      (fh == rh) ? "1" : (fh == rh * 2) ? "2" : "?", v & 0xFFFF, cr);
        }
        Row(row++, s);
    }
    if (row == 1)
        Row(1, "(none yet)");
    Row(10, "font 16..40 = 16x16, 11..15 = 12x12");
    Row(11, "last col = app creator, other = []");
}

static void Draw(void)
{
    RectangleType r;
    Char          s[48], a[16], b[16], c[16];

    if (gPage == 1) {
        DrawFontPage();
        return;
    }
    if (gPage == 2) {
        DrawSizesPage();
        return;
    }
    if (gPage == 3) {
        DrawCallsPage();
        return;
    }
    if (gPage == 4) {
        DrawWidthPage();
        return;
    }
    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnHangul v15 [tap: Font] CJKOS OFF", 34, 0, 0);

    ModeText(a, GetF(kFtrBox));
    StrPrintF(s, "patch %s  %s  font %s  prot %u%s", Installed() ? "ON" : "off", a,
              (GetF(kFtrGlyph0) && GetF(kFtrGlyph1)) ? "ok" : "-", ProtectCount(),
              ProtectCount() ? "" : " ok");
    Row(0, s);
    Button(kRowOn, "Patch ON");
    Button(kRowOff, "Patch OFF");
    StrPrintF(s, "Mode: %s", a);
    Button(kRowBox, s);
    Button(kRowClear, "Clear counters");
    ButtonAt(84, kRowClear, GetF(kFtrIntlC) ? "Intl: ON" : "Intl: off");
    /* 한글 견본: 한글 OK 가각 (EUC-KR) */
    WinDrawChars("\xC7\xD1\xB1\xDB OK \xB0\xA1\xB0\xA2", 12, 80, (kRowOn + 1) * kLineH);
    ButtonAt(84, kRowOff, "All OFF");
    ButtonAt(84, kRowBox, GetF(kFtrWidthC) ? "Width: ON" : "Width: off");

    StrPrintF(s, "calls %lu hi %lu drawn %lu 2c %lu x%lu", GetF(0), GetF(9), GetF(19),
              GetF(kFtrGlyphCnt), GetF(kFtrRatio));
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

    WinDrawInvertedChars(gMsg, StrLen(gMsg), 0, 2 * kLineH);   /* 빈 줄 1 (맨 아래는 줄 12까지 참) */
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

            if (e.screenY < kLineH)
                gPage = (gPage + 1) % kPages;
            else if (gPage == 3) {
                if (row == 0) {
                    if (GetF(kFtrCountC) != 0)
                        CountOff();
                    else
                        CountOn();
                }
            } else if (gPage != 0)
                ;
            else if (row == kRowOn)
                PatchOn();
            else if (row == kRowOff && e.screenX >= 84)
                AllOff();
            else if (row == kRowOff)
                PatchOff();
            else if (row == kRowBox && e.screenX >= 84) {
                if (GetF(kFtrWidthC) != 0)
                    WidthOff();
                else
                    WidthOn();
            } else if (row == kRowBox)
                ToggleBox();
            else if (row == kRowClear && e.screenX >= 84) {
                if (GetF(kFtrIntlC) != 0)
                    IntlOff();
                else
                    IntlOn();
            } else if (row == kRowClear)
                ClearCounters();
            Draw();
            continue;
        }
        if (!SysHandleEvent(&e) && e.eType == keyDownEvent &&
            (e.data.keyDown.chr == vchrPageUp || e.data.keyDown.chr == vchrPageDown)) {
            gPage = (e.data.keyDown.chr == vchrPageDown) ? (gPage + 1) % kPages : 0;
            Draw();
        }
    } while (e.eType != appStopEvent);
    StopHires();
    return 0;
}
