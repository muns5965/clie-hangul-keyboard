/*
 * MnSk - NR70V SilkWare(SkWr) 플러그인 5단계: ABC 버튼 3개 → 탭하면 현재 앱에 글자 입력
 *
 * 확정 사항 (ROM SilkExtn 역분석 + 1~4단계 실측, 2026-10-16):
 *   - 호출: Err entry(UInt32 *slotOrValue, UInt8 *event, void *out)
 *       Open 때만 첫 인자가 슬롯 주소, 이후 이벤트는 슬롯에 넣어 둔 값이 그대로 온다.
 *   - 반환값 16비트 0. 호출 전 A5 미설정 → 전역변수·문자열 상수 금지.
 *   - 이벤트 번호(event[0]): 1 Open, 3 Start, 4 Stop, 5 Draw, 6 DispChange, 7 PenDown, 8 PenMove, 9 PenUp
 *   - Open +2(32비트) = 실크 영역 창(WinHandle, SilkExtn 전역 84). 실측 0x48AA, 영역 160x65(저해상도 좌표).
 *     4단계에서 이 창에 그리기 성공.
 *   - Start/DispChange +2 너비, +6 높이, +10 색 깊이(32비트), +14 고해상도 여부(1바이트).
 *   - PenDown/PenMove/PenUp +2 x, +4 y (16비트). 실크 영역 왼쪽 위 기준 저해상도 좌표(0~159, 0~64).
 *   - 활성화: 런처 상태바 5번째 버튼(외부 플러그인이 있을 때만 표시).
 *
 * 5단계 동작:
 *   - Start / Draw / DispChange: 실크 영역을 지우고 A·B·C 버튼 3개를 그린다.
 *   - PenDown이 버튼 안이면 그 버튼을 반전 표시, PenUp 때 원래대로.
 *   - PenUp이 누른 버튼과 같은 버튼 안이면 EvtEnqueueKey로 'A'/'B'/'C'를 보낸다.
 *   - 상태(눌린 버튼)는 전역변수를 못 쓰므로 feature #46 에 둔다.
 *
 * 기록 (feature 'MnSk'):
 *   #0 Open 표시, #1~#16 이벤트 0~15 횟수, #17 마지막 이벤트, #18 번호≥16 횟수, #19 out≠NULL 횟수
 *   #20~#23 Open 앞 16바이트, #24~#27 마지막 Start 앞 16바이트,
 *   #29 그린 창 크기(31~16 너비, 15~0 높이), #31~#34 마지막 PenDown, #35~#38 마지막 PenUp,
 *   #39~#42 마지막 DispChange, #45 그리기 판정(1 그림, 2 핸들 이상, 3 크기 이상),
 *   #46 눌린 버튼(0 없음, 1~3), #47 보낸 키 횟수, #48 마지막으로 보낸 키
 */

#include <PalmOS.h>

#define kSkCreator    'MnSk'
#define kSkMagic      0x534BUL
#define kSkBuild      5

#define kEvtOpen      1
#define kEvtStart     3
#define kEvtDraw      5
#define kEvtDisp      6
#define kEvtPenDown   7
#define kEvtPenUp     9

#define kFtrOpen      0
#define kFtrCount0    1
#define kFtrLast      17
#define kFtrOver      18
#define kFtrOut       19
#define kFtrOpenRaw   20
#define kFtrStartRaw  24
#define kFtrStartArg  28
#define kFtrStartWin  29
#define kFtrStartPrev 30
#define kFtrDownRaw   31
#define kFtrUpRaw     35
#define kFtrDispRaw   39
#define kFtrWinPos    43
#define kFtrWinExt    44
#define kFtrDrawRes   45
#define kFtrPressed   46
#define kFtrKeyCount  47
#define kFtrKeyLast   48

#define kKeyN         3
#define kKeyTop       8
#define kKeyGap       4

#define kWinBoundsOff 10      /* WindowType.windowBounds 오프셋 (SDK 4 Window.h) */

static inline void BumpFtr(UInt16 num)
{
    UInt32 v = 0;

    if (FtrGet(kSkCreator, num, &v) != errNone)
        v = 0;
    if (v != 0xFFFFFFFFUL)
        v++;
    FtrSet(kSkCreator, num, v);
}

/* 이벤트 앞 16바이트를 기능 번호 4개에 빅엔디언 그대로 기록 (정렬 무관하게 바이트 단위로 조립) */
static inline void RawFtr(UInt16 base, const UInt8 *p)
{
    UInt16 i;

    for (i = 0; i < 4; i++) {
        const UInt8 *q = p + i * 4;
        FtrSet(kSkCreator, base + i,
               ((UInt32)q[0] << 24) | ((UInt32)q[1] << 16) | ((UInt32)q[2] << 8) | q[3]);
    }
}

static inline UInt16 Be16(const UInt8 *p)
{
    return ((UInt16)p[0] << 8) | p[1];
}

/* 창 핸들 안전 검사: 0 아님, 짝수, 동적 힙 범위, windowBounds 크기 정상 */
static inline Boolean WinOk(UInt32 h)
{
    const UInt8 *wb;
    UInt16 ww, wh;

    if (h == 0 || (h & 1) || h >= 0x00400000UL) {
        FtrSet(kSkCreator, kFtrDrawRes, 2);
        return false;
    }
    wb = (const UInt8 *)h + kWinBoundsOff;
    ww = Be16(wb + 4);
    wh = Be16(wb + 6);
    if ((ww != 160 && ww != 320) || wh == 0 || wh > 240) {
        FtrSet(kSkCreator, kFtrDrawRes, 3);
        return false;
    }
    FtrSet(kSkCreator, kFtrDrawRes, 1);
    return true;
}

/* 버튼 i(0~2)의 영역. 현재 그리기 창의 크기 기준 */
static inline void KeyRect(UInt16 i, Coord w, Coord h, RectangleType *r)
{
    Coord kw = (w - kKeyGap * (kKeyN + 1)) / kKeyN;

    RctSetRectangle(r, kKeyGap + i * (kw + kKeyGap), kKeyTop, kw, h - kKeyTop * 2);
}

/* 좌표 (x,y)가 들어간 버튼 번호(1~3), 없으면 0 */
static inline UInt16 HitKey(Coord x, Coord y, Coord w, Coord h)
{
    UInt16 i;
    RectangleType r;

    for (i = 0; i < kKeyN; i++) {
        KeyRect(i, w, h, &r);
        if (RctPtInRectangle(x, y, &r))
            return i + 1;
    }
    return 0;
}

static inline void DrawKeys(UInt32 hwin)
{
    WinHandle     prev;
    Coord         w = 0, h = 0;
    RectangleType r;
    FontID        oldFont;
    UInt16        i;
    Char          c;

    if (!WinOk(hwin))
        return;
    prev = WinSetDrawWindow((WinHandle)hwin);
    WinGetWindowExtent(&w, &h);

    RctSetRectangle(&r, 0, 0, w, h);
    WinEraseRectangle(&r, 0);

    oldFont = FntSetFont(boldFont);
    for (i = 0; i < kKeyN; i++) {
        KeyRect(i, w, h, &r);
        WinDrawRectangleFrame(roundFrame, &r);
        c = (Char)('A' + i);
        WinDrawChars(&c, 1,
                     r.topLeft.x + (r.extent.x - FntCharWidth(c)) / 2,
                     r.topLeft.y + (r.extent.y - FntLineHeight()) / 2);
    }
    FntSetFont(oldFont);

    WinSetDrawWindow(prev);
    FtrSet(kSkCreator, kFtrStartWin, ((UInt32)(UInt16)w << 16) | (UInt16)h);
    FtrSet(kSkCreator, kFtrPressed, 0);
}

/* 버튼 k(1~3) 반전 표시 토글 */
static inline void InvertKey(UInt32 hwin, UInt16 k)
{
    WinHandle     prev;
    Coord         w = 0, h = 0;
    RectangleType r;

    if (k == 0 || !WinOk(hwin))
        return;
    prev = WinSetDrawWindow((WinHandle)hwin);
    WinGetWindowExtent(&w, &h);
    KeyRect(k - 1, w, h, &r);
    RctInsetRectangle(&r, 1);
    WinInvertRectangle(&r, 0);
    WinSetDrawWindow(prev);
}

/* 펜 좌표가 들어간 버튼 번호 */
static inline UInt16 PenKey(UInt32 hwin, const UInt8 *event)
{
    WinHandle prev;
    Coord     w = 0, h = 0;

    if (!WinOk(hwin))
        return 0;
    prev = WinSetDrawWindow((WinHandle)hwin);
    WinGetWindowExtent(&w, &h);
    WinSetDrawWindow(prev);
    return HitKey((Coord)Be16(event + 2), (Coord)Be16(event + 4), w, h);
}

static inline UInt32 GetFtr(UInt16 num)
{
    UInt32 v = 0;

    if (FtrGet(kSkCreator, num, &v) != errNone)
        v = 0;
    return v;
}

Err SkwrEntry(UInt32 *slot, UInt8 *event, void *out)
{
    UInt8 e;

    if (event == NULL)
        return 0;
    e = event[0];

    if (e < 16)
        BumpFtr(kFtrCount0 + e);
    else
        BumpFtr(kFtrOver);
    if (out != NULL)
        BumpFtr(kFtrOut);
    FtrSet(kSkCreator, kFtrLast, ((UInt32)(out != NULL) << 16) | e);

    switch (e) {
    case kEvtOpen:
        FtrSet(kSkCreator, kFtrOpen,
               (kSkMagic << 16) | ((UInt32)kEvtOpen << 8) | kSkBuild);
        RawFtr(kFtrOpenRaw, event);
        /* 가설: event+2 = 실크 영역 창(SilkExtn 전역 84). 슬롯에 넣어 두면 이후 첫 인자로 돌아온다. */
        if (slot != NULL)
            *slot = ((UInt32)event[2] << 24) | ((UInt32)event[3] << 16) |
                    ((UInt32)event[4] << 8) | event[5];
        break;

    case kEvtStart:
        RawFtr(kFtrStartRaw, event);
        DrawKeys((UInt32)slot);
        break;

    case kEvtDraw:
        DrawKeys((UInt32)slot);
        break;

    case kEvtPenDown:
        RawFtr(kFtrDownRaw, event);
        {
            UInt16 k = PenKey((UInt32)slot, event);

            FtrSet(kSkCreator, kFtrPressed, k);
            InvertKey((UInt32)slot, k);
        }
        break;

    case kEvtPenUp:
        RawFtr(kFtrUpRaw, event);
        {
            UInt16 pressed = (UInt16)GetFtr(kFtrPressed);
            UInt16 k = PenKey((UInt32)slot, event);

            InvertKey((UInt32)slot, pressed);
            FtrSet(kSkCreator, kFtrPressed, 0);
            if (pressed != 0 && k == pressed) {
                WChar c = (WChar)('A' + k - 1);

                EvtEnqueueKey(c, 0, 0);
                BumpFtr(kFtrKeyCount);
                FtrSet(kSkCreator, kFtrKeyLast, c);
            }
        }
        break;

    case kEvtDisp:
        RawFtr(kFtrDispRaw, event);
        DrawKeys((UInt32)slot);
        break;
    }
    return 0;
}
