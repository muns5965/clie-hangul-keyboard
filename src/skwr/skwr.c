/*
 * MnSk - NR70V SilkWare(SkWr) 플러그인 3단계: 이벤트 내용 기록 + 사각형 그리기 시험
 *
 * 확정 사항 (ROM SilkExtn 역분석 + 1·2단계 실측):
 *   - 호출: Err entry(UInt32 *slotOrValue, UInt8 *event, void *out)
 *       Open 때만 첫 인자가 슬롯 주소, 이후 이벤트는 슬롯에 넣어 둔 값이 그대로 온다.
 *   - 이벤트 번호(event[0]): 1 Open, 3 Start, 4 Stop, 6 DispChange, 7 PenDown, 8 PenMove, 9 PenUp
 *   - 활성화: 상태바의 전환 버튼(외부 플러그인이 있을 때만 표시)
 *   - 반환값 16비트 0. 호출 전 A5 미설정 → 전역변수·문자열 상수 금지.
 *
 * 3단계 가설 (실측으로 확인할 것):
 *   - Open 이벤트의 +10 위치 32비트 값 = SilkExtn 전역의 실크 화면 창(WinHandle).
 *     (SilkExtn 0x2E5C, SilkWareLib GetScrWindow 0x34E 가 같은 전역 필드 172를 쓴다.)
 *   - 이 값을 Open 때 슬롯에 넣어 두면, Start 때 첫 인자로 돌려받는다.
 *
 * 기록 (feature 'MnSk'):
 *   #0       Open 표시 (0x534B 01 빌드)
 *   #1~#16   이벤트 0~15 수신 횟수, #17 마지막 이벤트, #18 번호≥16 횟수, #19 out≠NULL 횟수
 *   #20~#23  Open 이벤트 앞 16바이트
 *   #24~#27  마지막 Start 이벤트 앞 16바이트
 *   #28      Start 때 받은 첫 인자(슬롯 값)
 *   #29      Start 그리기 결과: 31~16 창 너비, 15~0 창 높이 (그리기 안 했으면 0)
 *   #30      Start 직전의 현재 그리기 창 핸들
 *   #31~#34  마지막 PenDown 이벤트 앞 16바이트
 *   #35~#38  마지막 PenUp 이벤트 앞 16바이트
 *   #39~#42  마지막 DispChange 이벤트 앞 16바이트
 *
 * 그리기 (Start 때만): 슬롯 값이 NULL이 아니면 그 창을 그리기 창으로 잠시 바꾸고
 * 창 전체를 지운 뒤 테두리와 왼쪽 위 채운 사각형을 그리고, 원래 그리기 창으로 되돌린다.
 */

#include <PalmOS.h>

#define kSkCreator    'MnSk'
#define kSkMagic      0x534BUL
#define kSkBuild      3

#define kEvtOpen      1
#define kEvtStart     3
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

static inline void DrawTest(WinHandle win)
{
    WinHandle     prev;
    Coord         w = 0, h = 0;
    RectangleType r;

    prev = WinSetDrawWindow(win);
    WinGetWindowExtent(&w, &h);

    RctSetRectangle(&r, 0, 0, w, h);
    WinEraseRectangle(&r, 0);
    RctSetRectangle(&r, 1, 1, w - 2, h - 2);
    WinDrawRectangleFrame(simpleFrame, &r);
    RctSetRectangle(&r, 4, 4, 12, 8);
    WinDrawRectangle(&r, 0);

    WinSetDrawWindow(prev);
    FtrSet(kSkCreator, kFtrStartWin, ((UInt32)(UInt16)w << 16) | (UInt16)h);
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
        /* 가설: event+10 = 실크 화면 창. 슬롯에 넣어 두면 이후 이벤트의 첫 인자로 돌아온다. */
        if (slot != NULL)
            *slot = ((UInt32)event[10] << 24) | ((UInt32)event[11] << 16) |
                    ((UInt32)event[12] << 8) | event[13];
        break;

    case kEvtStart:
        RawFtr(kFtrStartRaw, event);
        FtrSet(kSkCreator, kFtrStartArg, (UInt32)slot);
        FtrSet(kSkCreator, kFtrStartPrev, (UInt32)WinGetDrawWindow());
        FtrSet(kSkCreator, kFtrStartWin, 0);
        if (slot != NULL)
            DrawTest((WinHandle)slot);
        break;

    case kEvtPenDown:
        RawFtr(kFtrDownRaw, event);
        break;

    case kEvtPenUp:
        RawFtr(kFtrUpRaw, event);
        break;

    case kEvtDisp:
        RawFtr(kFtrDispRaw, event);
        break;
    }
    return 0;
}
