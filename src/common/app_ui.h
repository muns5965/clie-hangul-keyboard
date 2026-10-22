/*
 * 네 앱(MnHangul·MnSonyKbd·MnKbdHan·MnSkKbd) 공용 사용자 화면 틀 — 같은 줄·같은 자리(저해상도 좌표, 줄 높이 11).
 *
 *   제목  앱 이름 vN
 *   0     한 줄 설명
 *   1     알림(비어 있으면 그리지 않음)
 *   2     Now: …
 *   3     [ON] [OFF] [All OFF]   (x 2·40·80)
 *   4     [Start at reset: …]
 *   5     [Update (hold 3s)]
 *   6     [Soft Reset (hold 3s)]
 *   7~10  모듈 상태 4줄(네 앱 모두 같음): Display / Sony kbd / System kbd / Mn kbd
 *   11    사용법 한 줄
 *   12    [ ] Debug
 *
 * 앱 화면에서만 쓴다(리셋·알림 실행에서 부르지 않음).
 */
#ifndef APP_UI_H
#define APP_UI_H

#define kUiH        11
#define kUiRowNow   2
#define kUiRowBtn   3
#define kUiRowAuto  4
#define kUiRowUpd   5
#define kUiRowReset 6
#define kUiRowMods  7
#define kUiRowUse   11
#define kUiRowDebug 12
#define kUiXOn      2
#define kUiXOff     40
#define kUiXAll     80
#define kUiXVal     58
#define kUiHoldSec  3

#define kMnTypeOn   'SkWr'   /* Mn 키보드 로더 DB 타입: 켜짐(SilkExtn이 부팅 때 찾음) */
#define kMnTypeOff  'SkWo'   /* 꺼짐(부팅 때 찾지 않음 — MnSkKbd v1 실측) */

static void UiRow(UInt16 row, const Char *s)
{
    WinDrawChars(s, StrLen(s), 2, (row + 1) * kUiH);
}

static void UiPair(UInt16 row, const Char *k, const Char *v)
{
    WinDrawChars(k, StrLen(k), 2, (row + 1) * kUiH);
    WinDrawChars(v, StrLen(v), kUiXVal, (row + 1) * kUiH);
}

static void UiButton(UInt16 x, UInt16 row, const Char *s)
{
    RectangleType r;

    RctSetRectangle(&r, x, (row + 1) * kUiH + 1, FntCharsWidth(s, StrLen(s)) + 6, kUiH - 2);
    WinDrawChars(s, StrLen(s), x + 3, (row + 1) * kUiH);
    WinDrawRectangleFrame(roundFrame, &r);
}

/* 버튼 줄의 누른 자리: 0 ON, 1 OFF, 2 All OFF, 3 없음 */
static UInt16 UiBtnHit(Int16 x)
{
    if (x < kUiXOff - 4)
        return 0;
    if (x < kUiXAll - 4)
        return 1;
    return x < 130 ? 2 : 3;
}

static void UiCheck(UInt16 row, const Char *label, Boolean on)
{
    RectangleType r;

    RctSetRectangle(&r, 4, (row + 1) * kUiH + 2, 7, 7);
    WinDrawRectangleFrame(rectangleFrame, &r);
    if (on) {
        WinDrawLine(5, (row + 1) * kUiH + 5, 7, (row + 1) * kUiH + 8);
        WinDrawLine(7, (row + 1) * kUiH + 8, 10, (row + 1) * kUiH + 2);
    }
    WinDrawChars(label, StrLen(label), 15, (row + 1) * kUiH);
}

/* 알림 줄: 비어 있거나 "-"면 그리지 않음(빈 반전 토막이 남던 것) */
static void UiMsg(const Char *m)
{
    if (m[0] != 0 && !(m[0] == '-' && m[1] == 0))
        WinDrawInvertedChars(m, StrLen(m), 0, 2 * kUiH);
}

static Boolean UiApp(UInt32 cr)
{
    DmSearchStateType st;
    UInt16            card;
    LocalID           id;

    return DmGetNextDatabaseByTypeCreator(true, &st, sysFileTApplication, cr, true, &card, &id) == errNone;
}

static UInt32 UiFtr(UInt32 cr, UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(cr, n, &v) != errNone)
        v = 0;
    return v;
}

/* CJKOS 켜짐('dCJK' Feature, ClieDiag v24 실측) */
static Boolean UiCjkos(void)
{
    UInt32 v = 0;

    return FtrGet('dCJK', 0, &v) == errNone;
}

/* Mn 키보드 로더 DB: 1 켜짐 타입, 2 꺼짐 타입, 0 없음 */
static UInt16 UiMnDB(UInt16 *card, LocalID *id)
{
    DmSearchStateType st;

    *id = 0;
    if (DmGetNextDatabaseByTypeCreator(true, &st, kMnTypeOn, 'MnSk', false, card, id) == errNone && *id != 0)
        return 1;
    if (DmGetNextDatabaseByTypeCreator(true, &st, kMnTypeOff, 'MnSk', false, card, id) == errNone && *id != 0)
        return 2;
    return 0;
}

/* Mn 키보드: 0 설치 안 됨, 1 ON, 2 off, 3 ON after reset, 4 off after reset.
 *   등록됨 = 이번 부팅에 로더 Open 기록(Feature 'MnSk' #0 위 16비트 'SK') */
static UInt16 UiMnState(void)
{
    UInt16  card;
    LocalID id;
    UInt16  k = UiMnDB(&card, &id);
    Boolean reg = (UiFtr('MnSk', 0) >> 16) == 0x534B;

    if (k == 0)
        return 0;
    if (k == 1)
        return reg ? 1 : 3;
    return reg ? 4 : 2;
}

static void UiState(Char *o, const Char *name, Boolean inst, Boolean on)
{
    if (!inst)
        StrCopy(o, "none");
    else {
        StrCopy(o, name);
        StrCat(o, on ? " ON" : " off");
    }
}

/* 모듈 상태 4줄(7~10) */
static void UiMods(void)
{
    Char   s[40];
    UInt16 m;

    WinDrawGrayLine(2, (kUiRowMods + 1) * kUiH - 1, 157, (kUiRowMods + 1) * kUiH - 1);
    if (UiCjkos())
        StrCopy(s, UiFtr('MnHg', 21) ? "CJKOS + MnHangul" : "CJKOS");
    else
        UiState(s, "MnHangul", UiApp('MnHg'), UiFtr('MnHg', 21) != 0);
    UiPair(kUiRowMods, "Display", s);
    UiState(s, "MnSonyKbd", UiApp('MnSy'), UiFtr('MnSy', 1) != 0);
    UiPair(kUiRowMods + 1, "Sony kbd", s);
    UiState(s, "MnKbdHan", UiApp('MnKh'), UiFtr('MnKh', 2) != 0);
    UiPair(kUiRowMods + 2, "System kbd", s);
    m = UiMnState();
    UiState(s, "MnSkKbd", m != 0, m == 1 || m == 4);
    if (m == 3)
        StrCopy(s, "ON after reset");
    else if (m == 4)
        StrCopy(s, "off after reset");
    UiPair(kUiRowMods + 3, "Mn kbd", s);
}

/* Update: 설치되어 있는 다른 패치 모듈(MnHangul·MnSonyKbd·MnKbdHan)도 다음 리셋 한 번 자동 시작 건너뛰기(앱 설정 3번) */
static void UiSkipOthers(UInt32 self)
{
    UInt16 i, v = 1;

    for (i = 0; i < 3; i++) {
        UInt32 cr = i == 0 ? 'MnHg' : i == 1 ? 'MnSy' : 'MnKh';

        if (cr != self && UiApp(cr))
            PrefSetAppPreferences(cr, 3, 1, &v, sizeof v, true);
    }
}

/* 3초 누르면 before() 뒤 리셋, 중간에 떼면 false(취소) */
static Boolean UiHoldReset(UInt16 row, const Char *label, void (*before)(void))
{
    UInt32        t0 = TimGetTicks(), tps = SysTicksPerSecond();
    Int16         px, py, shown = -1, left;
    Boolean       down = true;
    RectangleType r, b;
    Char          s[24];
    UInt16        w = FntCharsWidth(label, StrLen(label)) + 6;

    RctSetRectangle(&r, kUiXOn, (row + 1) * kUiH, w + 2, kUiH);
    RctSetRectangle(&b, kUiXOn, (row + 1) * kUiH + 1, w, kUiH - 2);
    while (down) {
        left = (Int16)(kUiHoldSec - (TimGetTicks() - t0) / tps);
        if (left <= 0)
            StrCopy(s, "Resetting...");
        else
            StrPrintF(s, "Hold... %ds", left);
        if (left != shown || left <= 0) {
            WinEraseRectangle(&r, 0);
            WinDrawChars(s, StrLen(s), kUiXOn + 3, (row + 1) * kUiH);
            WinDrawRectangleFrame(roundFrame, &b);
            WinInvertRectangle(&b, 0);
            shown = left;
        }
        if (left <= 0) {
            if (before != NULL)
                before();
            SysReset();
            return true;
        }
        SysTaskDelay(tps / 20);
        EvtGetPen(&px, &py, &down);
    }
    return false;
}

#endif
