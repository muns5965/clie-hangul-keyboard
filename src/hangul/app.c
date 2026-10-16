/*
 * MnHangul v7 — CJKOS 대체 한글 모듈 시험 앱 (2a 통과 시험, 2b 자리표시, 2c 16×16 한글 그리기, 글꼴 덤프·크기 조사)
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
#define kFtrGlyphCnt  27
#define kFtrRatio     28
#define kGlyphRes     'HgFn'

#define kLineH        11
#define kRowOn        2
#define kRowOff       3
#define kRowBox       4
#define kRowClear     5

static Char   gMsg[40] = "-";
static UInt16 gPage = 0;   /* 0 Main, 1 Font, 2 Sizes */

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

    for (i = 0; i < 2; i++) {
        MemHandle h = DmGetResource(kGlyphRes, i);

        FtrSet(kHgCreator, kFtrGlyphH0 + i, (UInt32)h);
        FtrSet(kHgCreator, kFtrGlyph0 + i, (h != NULL) ? (UInt32)MemHandleLock(h) : 0);
    }
}

static void UnlockGlyphs(void)
{
    UInt16 i;

    for (i = 0; i < 2; i++) {
        UInt32 h = GetF(kFtrGlyphH0 + i);

        FtrSet(kHgCreator, kFtrGlyph0 + i, 0);
        FtrSet(kHgCreator, kFtrGlyphH0 + i, 0);
        if (h != 0) {
            MemHandleUnlock((MemHandle)h);
            DmReleaseResource((MemHandle)h);
        }
    }
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

static void ClearCounters(void)
{
    UInt16 i;

    for (i = 0; i <= kFtrMax; i++)
        FtrUnregister(kHgCreator, i);   /* #20~#26(핸들·진입점·모드·글자 데이터)은 지우지 않는다 */
    FtrUnregister(kHgCreator, kFtrGlyphCnt);
    FtrUnregister(kHgCreator, kFtrRatio);
    for (i = 30; i <= 37; i++)
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
    WinDrawInvertedChars("MnHangul v7 Font [tap: Sizes]", 29, 0, 0);
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
    WinDrawInvertedChars("MnHangul v7 Sizes [tap: Main]", 29, 0, 0);
    Row(0, "hangul calls by font h / rect h");
    for (i = 30; i <= 37; i++) {
        UInt32 v = GetF(i);
        UInt16 fh, rh;

        if (v == 0)
            continue;
        fh = (UInt16)(v >> 24);
        rh = (UInt16)((v >> 16) & 0xFF);
        StrPrintF(s, "font %u  rect %u  x%s  : %lu", fh, rh,
                  (fh == rh) ? "1" : (fh == rh * 2) ? "2" : "?", v & 0xFFFF);
        Row(row++, s);
    }
    if (row == 1)
        Row(1, "(none yet)");
    Row(10, "x1/x2 + font 16..40 = glyph");
    Row(11, "other = [] (box)");
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
    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    WinDrawInvertedChars("MnHangul v7 [tap: Font] CJKOS OFF", 33, 0, 0);

    ModeText(a, GetF(kFtrBox));
    StrPrintF(s, "patch: %s  mode: %s  A378=%lx", Installed() ? "ON" : "off", a,
              (UInt32)SysGetTrapAddress(kTrapBlt));
    Row(0, s);
    Button(kRowOn, "Patch ON");
    Button(kRowOff, "Patch OFF");
    StrPrintF(s, "Mode: %s", a);
    Button(kRowBox, s);
    Button(kRowClear, "Clear counters");
    /* 한글 견본: 한글 OK 가각 (EUC-KR) */
    WinDrawChars("\xC7\xD1\xB1\xDB OK \xB0\xA1\xB0\xA2", 12, 80, (kRowOn + 1) * kLineH);
    StrPrintF(s, "glyph %s/%s", GetF(kFtrGlyph0) ? "ok" : "-", GetF(kFtrGlyph1) ? "ok" : "-");
    WinDrawChars(s, StrLen(s), 80, (kRowOff + 1) * kLineH);
    StrPrintF(s, "2c calls %lu x%lu", GetF(kFtrGlyphCnt), GetF(kFtrRatio));
    WinDrawChars(s, StrLen(s), 80, (kRowBox + 1) * kLineH);

    StrPrintF(s, "calls %lu  hi %lu  drawn %lu", GetF(0), GetF(9), GetF(19));
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
                gPage = (gPage + 1) % 3;
            else if (gPage != 0)
                ;
            else if (row == kRowOn)
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
            (e.data.keyDown.chr == vchrPageUp || e.data.keyDown.chr == vchrPageDown)) {
            gPage = (e.data.keyDown.chr == vchrPageDown) ? (gPage + 1) % 3 : 0;
            Draw();
        }
    } while (e.eType != appStopEvent);
    StopHires();
    return 0;
}
