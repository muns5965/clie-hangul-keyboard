/*
 * MnSk 본체(MnSkCore.prc) - 10단계: 4줄 자판 + 한글 조합 + 고해상도 그리기
 *
 * 10단계부터 SkWr 플러그인은 작은 로더(src/skwr/loader.c → MnSkPlugin.prc)이고, 이 파일은 본체다.
 * 로더가 Open을 처리하고, 자판이 켜질 때(Start 등) 본체 DB(타입 SkCo, 리소스 MnCo 0)의 코드를 잠가
 * 이 진입 함수를 부른다. 첫 인자는 SilkExtn이 넘긴 슬롯 값(실크 창) 그대로다.
 * 본체는 위치 독립 코드여야 한다(전역·A5·재배치 없음, 점프표·KSC 표는 PC 상대) — 빌드 스크립트가 검사.
 *
 * 확정 사항 (ROM SilkExtn 역분석 + 1~6단계 실측, 2026-10-16):
 *   - 호출: Err entry(UInt32 *slotOrValue, UInt8 *event, void *out)
 *       Open 때만 첫 인자가 슬롯 주소, 이후 이벤트는 슬롯에 넣어 둔 값이 그대로 온다.
 *   - 반환값 16비트 0. 호출 전 A5 미설정 → 전역변수·문자열 상수·초기화된 지역 배열 금지.
 *   - 이벤트 번호(event[0]): 1 Open, 3 Start, 4 Stop, 5 Draw, 6 DispChange, 7 PenDown, 8 PenMove, 9 PenUp
 *   - Open +2(32비트) = 실크 영역 창(WinHandle). 영역 160x65(저해상도 좌표).
 *   - PenDown/PenUp +2 x, +4 y (16비트). 실크 영역 기준 저해상도 좌표(0~159, 0~64).
 *   - 5단계: EvtEnqueueKey → 메모장 입력 성공.
 *   - 6단계: 한글은 KSC5601 2바이트 코드를 키 이벤트 하나로 보낸다(EvtEnqueueKey(0xB0A1) → 가).
 *     백스페이스 한 번에 한글 한 글자가 지워진다.
 *
 * 7단계 자판 (4줄, 한 줄 높이 16):
 *   한글:  ㅂ ㅈ ㄷ ㄱ ㅅ ㅗ ㅐ ㅔ   (Shift: ㅃ ㅉ ㄸ ㄲ ㅆ, 모음은 그대로)
 *          ㅁ ㄴ ㅇ ㄹ ㅎ ㅓ ㅏ ㅣ
 *          ^  ㅋ ㅌ ㅊ ㅍ ㅜ ㅡ BS
 *   영문:  q w e r t y u i o p / a s d f g h j k l / ^ z x c v b n m BS   (Shift: 대문자)
 *   숫자:  1 2 3 4 5 6 7 8 9 0 / - / : ; ( ) $ & @ " / # % * + = ' ? ! BS
 *   공통 아랫줄: [한/A] [123] [,] [  space  ] [.] [Ent]
 *   - Shift는 글자 하나를 보내면 풀린다. Shift·123이 켜져 있으면 그 키를 반전 표시.
 *   - 7단계 실측: 자판 그리기(자모 이름 포함)와 자모·영문·숫자 단독 출력 정상.
 *
 * 8단계 조합 (두벌식 규칙 + 단모음 연타):
 *   - 조합 중인 글자는 화면에 나간 글자 수(shown)만큼 BS로 지우고 새로 보낸다.
 *   - 연타: ㅏㅏ→ㅑ ㅓㅓ→ㅕ ㅗㅗ→ㅛ ㅜㅜ→ㅠ ㅐㅐ→ㅒ ㅔㅔ→ㅖ. 복합모음 ㅘㅙㅚㅝㅞㅟㅢ, 겹받침 11개.
 *   - 받침 넘김(닭+ㅏ → 달가). KSC5601에 없는 음절이 되면 앞 글자를 확정하고 새로 시작.
 *   - BS: 조합 중이면 자모 하나씩(각→가→ㄱ→없음), 아니면 보통 BS.
 *   - 조합 끝: 한글 자모 외의 키, 한/A·123, Start/Stop, 현재 앱이 바뀜.
 *   - 한계: 조합 중 본문을 탭해 커서를 옮기는 것은 알 수 없다.
 *
 * 기록 (feature 'MnSk'):
 *   #0 Open 표시, #1~#16 이벤트 0~15 횟수, #17 마지막 이벤트, #18 번호≥16 횟수, #19 out≠NULL 횟수
 *   #20~#23 Open 앞 16바이트, #24~#27 마지막 Start 앞 16바이트,
 *   #29 그린 창 크기(31~16 너비, 15~0 높이), #31~#34 마지막 PenDown, #35~#38 마지막 PenUp,
 *   #39~#42 마지막 DispChange, #45 그리기 판정(1 그림, 2 핸들 이상, 3 크기 이상),
 *   #46 눌린 키(0 없음, 줄*16+칸+1), #47 보낸 키 횟수, #48 마지막 전송(31~16 키 번호, 15~0 코드),
 *   #49 자판 상태(비트0 영문, 비트1 숫자, 비트8 Shift)
 *   #50 조합 상태(31~24 초성, 23~16 중성, 15~8 종성: 호환 자모 - 0x3130, 7~0 화면에 나간 글자 수)
 *   #51 조합 중인 앱 LocalID
 *   #52 HR 라이브러리 (31~16 SysLibFind 오류, 15~0 참조 번호), #53 HRWinGetWindowExtent 원래 값(참고용)
 *   #54 고해상도 표시(Start/DispChange +14), #55 본체 기록(31~16 'SK', 7~0 빌드)
 *
 * 9단계 실측: HRWinGetWindowExtent가 실크 창에서 160x65를 돌려줘 저해상도로 그려짐(선명해지지 않음).
 * 10단계: SilkExtn처럼(ROM 0x0178) 실크 창은 저해상도 크기 그대로 두고 좌표만 2배로 HR 함수에 넘긴다.
 *   고해상도 여부는 Start/DispChange +14 표시로 판단(런처 1, 저해상도 앱 0).
 * 10단계 실측: 로더로 본체 실행, 고해상도 그리기·한글 키 이름 정상. 아랫줄 오른쪽(space·.·Ent)이 안 그려짐
 *   → 11: 아랫줄 경계 계산의 16비트 곱셈 넘침 수정(Scale).
 */

#ifdef MNSK_HOST_TEST
#include "host_stub.h"      /* test/host_stub.h: PC에서 조합 로직만 시험 */
#else
#include <PalmOS.h>
#endif

#define kSkCreator    'MnSk'
#define kSkMagic      0x534BUL
#define kSkBuild      11

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
#define kFtrStartWin  29
#define kFtrDownRaw   31
#define kFtrUpRaw     35
#define kFtrDispRaw   39
#define kFtrDrawRes   45
#define kFtrPressed   46
#define kFtrKeyCount  47
#define kFtrKeyLast   48
#define kFtrState     49
#define kFtrComp      50
#define kFtrCompApp   51
#define kFtrHrRef     52
#define kFtrHrExt     53
#define kFtrHires     54
#define kFtrCoreBuild 55
#define kEvtStop      4

#define kStEng        0x0001
#define kStNum        0x0002
#define kStShift      0x0100

/* 특수 키 코드 (보내지 않음). 0x08 BS, 0x0A Enter, 0x20 이상은 그대로 보낸다 */
#define kKShift       1
#define kKHanEng      3
#define kKNum         4

#define kRows         4
#define kBaseW        160     /* 아랫줄 너비 비율 기준 */

#define kWinBoundsOff 10      /* WindowType.windowBounds 오프셋 (SDK 4 Window.h) */

/* 유니코드 호환 자모(U+3131~) → KSC5601 자모(0xA4A1~), 순서가 같다 */
#define J(u)          ((UInt16)(0xA4A1 + ((u) - 0x3131)))

static inline UInt32 GetFtr(UInt16 num)
{
    UInt32 v = 0;

    if (FtrGet(kSkCreator, num, &v) != errNone)
        v = 0;
    return v;
}

static inline void BumpFtr(UInt16 num)
{
    UInt32 v = GetFtr(num);

    if (v != 0xFFFFFFFFUL)
        v++;
    FtrSet(kSkCreator, num, v);
}

/* 이벤트 앞 16바이트를 기능 번호 4개에 빅엔디언 그대로 기록 */
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

/* 창 크기 */
static inline Boolean WinExt(UInt32 hwin, Coord *w, Coord *h)
{
    WinHandle prev;

    *w = 0;
    *h = 0;
    if (!WinOk(hwin))
        return false;
    prev = WinSetDrawWindow((WinHandle)hwin);
    WinGetWindowExtent(w, h);
    WinSetDrawWindow(prev);
    return (*w > 0 && *h > 0);
}

/* 줄별 키 개수 */
static inline UInt16 RowKeys(UInt16 st, UInt16 row)
{
    if (row == 3)
        return 6;
    if (st & kStNum)
        return (row == 2) ? 9 : 10;
    if (st & kStEng)
        return (row == 0) ? 10 : 9;
    return 8;
}

/* 아랫줄 칸 경계(기준 너비 160): 한/A 26, 123 22, ',' 16, space 48, '.' 16, Ent 32 */
static inline Coord BottomEdge(UInt16 col)
{
    switch (col) {
    case 0:  return 0;
    case 1:  return 26;
    case 2:  return 48;
    case 3:  return 64;
    case 4:  return 112;
    case 5:  return 128;
    default: return 160;
    }
}

/* 아랫줄 배율: 저해상도 160 → 1, 고해상도 320 → 2.
 * (BottomEdge * w / 160 은 w=320일 때 16비트 곱셈이 넘친다 — 10단계 실측에서 space·.·Ent 사라짐) */
static inline Coord Scale(Coord w)
{
    return (w >= 2 * kBaseW) ? 2 : 1;
}

/* 키 영역(테두리 포함 칸 전체) */
static inline void KeyCell(UInt16 st, UInt16 row, UInt16 col, Coord w, Coord h, RectangleType *r)
{
    Coord rh = h / kRows;
    Coord top = row * rh;
    Coord bot = (row == kRows - 1) ? h : top + rh;
    Coord l, rr;

    if (row == kRows - 1) {
        l = BottomEdge(col) * Scale(w);
        rr = BottomEdge(col + 1) * Scale(w);
    } else {
        UInt16 n = RowKeys(st, row);

        l = col * w / n;
        rr = (col + 1) * w / n;
    }
    RctSetRectangle(r, l, top, rr - l, bot - top);
}

/* 좌표 → 키 번호(줄*16+칸+1), 없으면 0 */
static inline UInt16 HitKey(UInt16 st, Coord x, Coord y, Coord w, Coord h)
{
    UInt16 row, col, n;

    if (x < 0 || y < 0 || x >= w || y >= h)
        return 0;
    row = y / (h / kRows);
    if (row >= kRows)
        row = kRows - 1;
    n = RowKeys(st, row);
    if (row == kRows - 1) {
        for (col = 0; col < n - 1; col++)
            if (x < BottomEdge(col + 1) * Scale(w))
                break;
    } else {
        col = x * n / w;
        if (col >= n)
            col = n - 1;
    }
    return row * 16 + col + 1;
}

/* 한글 줄 0~2 */
static inline UInt16 HanCode(Boolean sh, UInt16 row, UInt16 col)
{
    switch (row * 8 + col) {
    case 0:  return sh ? J(0x3143) : J(0x3142);   /* ㅃ ㅂ */
    case 1:  return sh ? J(0x3149) : J(0x3148);   /* ㅉ ㅈ */
    case 2:  return sh ? J(0x3138) : J(0x3137);   /* ㄸ ㄷ */
    case 3:  return sh ? J(0x3132) : J(0x3131);   /* ㄲ ㄱ */
    case 4:  return sh ? J(0x3146) : J(0x3145);   /* ㅆ ㅅ */
    case 5:  return J(0x3157);                    /* ㅗ */
    case 6:  return J(0x3150);                    /* ㅐ */
    case 7:  return J(0x3154);                    /* ㅔ */
    case 8:  return J(0x3141);                    /* ㅁ */
    case 9:  return J(0x3134);                    /* ㄴ */
    case 10: return J(0x3147);                    /* ㅇ */
    case 11: return J(0x3139);                    /* ㄹ */
    case 12: return J(0x314E);                    /* ㅎ */
    case 13: return J(0x3153);                    /* ㅓ */
    case 14: return J(0x314F);                    /* ㅏ */
    case 15: return J(0x3163);                    /* ㅣ */
    case 16: return kKShift;
    case 17: return J(0x314B);                    /* ㅋ */
    case 18: return J(0x314C);                    /* ㅌ */
    case 19: return J(0x314A);                    /* ㅊ */
    case 20: return J(0x314D);                    /* ㅍ */
    case 21: return J(0x315C);                    /* ㅜ */
    case 22: return J(0x3161);                    /* ㅡ */
    default: return chrBackspace;
    }
}

/* 영문 줄 0~2 (소문자) */
static inline UInt16 EngCode(UInt16 row, UInt16 col)
{
    switch (row * 10 + col) {
    case 0:  return 'q';
    case 1:  return 'w';
    case 2:  return 'e';
    case 3:  return 'r';
    case 4:  return 't';
    case 5:  return 'y';
    case 6:  return 'u';
    case 7:  return 'i';
    case 8:  return 'o';
    case 9:  return 'p';
    case 10: return 'a';
    case 11: return 's';
    case 12: return 'd';
    case 13: return 'f';
    case 14: return 'g';
    case 15: return 'h';
    case 16: return 'j';
    case 17: return 'k';
    case 18: return 'l';
    case 20: return kKShift;
    case 21: return 'z';
    case 22: return 'x';
    case 23: return 'c';
    case 24: return 'v';
    case 25: return 'b';
    case 26: return 'n';
    case 27: return 'm';
    default: return chrBackspace;
    }
}

/* 숫자·기호 줄 0~2 */
static inline UInt16 NumCode(UInt16 row, UInt16 col)
{
    switch (row * 10 + col) {
    case 0:  return '1';
    case 1:  return '2';
    case 2:  return '3';
    case 3:  return '4';
    case 4:  return '5';
    case 5:  return '6';
    case 6:  return '7';
    case 7:  return '8';
    case 8:  return '9';
    case 9:  return '0';
    case 10: return '-';
    case 11: return '/';
    case 12: return ':';
    case 13: return ';';
    case 14: return '(';
    case 15: return ')';
    case 16: return '$';
    case 17: return '&';
    case 18: return '@';
    case 19: return '"';
    case 20: return '#';
    case 21: return '%';
    case 22: return '*';
    case 23: return '+';
    case 24: return '=';
    case 25: return '\'';
    case 26: return '?';
    case 27: return '!';
    default: return chrBackspace;
    }
}

static inline UInt16 KeyCode(UInt16 st, UInt16 row, UInt16 col)
{
    Boolean sh = (st & kStShift) != 0;

    if (row == kRows - 1) {
        switch (col) {
        case 0:  return kKHanEng;
        case 1:  return kKNum;
        case 2:  return ',';
        case 3:  return ' ';
        case 4:  return '.';
        default: return chrLineFeed;
        }
    }
    if (st & kStNum)
        return NumCode(row, col);
    if (st & kStEng) {
        UInt16 c = EngCode(row, col);

        return (sh && c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
    }
    return HanCode(sh, row, col);
}

/* 키 이름 (문자열 상수 대신 한 글자씩). 길이 반환 */
static inline UInt16 KeyLabel(UInt16 code, Char *s)
{
    switch (code) {
    case kKShift:     s[0] = '^'; return 1;
    case kKHanEng:    s[0] = (Char)0xC7; s[1] = (Char)0xD1; s[2] = '/'; s[3] = 'A'; return 4;  /* 한/A */
    case kKNum:       s[0] = '1'; s[1] = '2'; s[2] = '3'; return 3;
    case chrBackspace: s[0] = 'B'; s[1] = 'S'; return 2;
    case chrLineFeed: s[0] = 'E'; s[1] = 'n'; s[2] = 't'; return 3;
    case ' ':         return 0;
    }
    if (code > 0xFF) {
        s[0] = (Char)(code >> 8);
        s[1] = (Char)(code & 0xFF);
        return 2;
    }
    s[0] = (Char)code;
    return 1;
}

/* 키 안쪽(반전 영역) */
/* ---- 소니 HR 라이브러리 (고해상도 320x130 그리기) ----
 * SilkExtn이 부팅 때 SysLibFind("Sony HR Library") → HROpen 하고 열어 둔 채로 상태바를 그린다(ROM 0x693C).
 * 그래서 플러그인은 SysLibFind로 참조 번호만 얻어 바로 쓴다(HROpen/HRClose 하지 않음).
 * 트랩 번호 = sysLibTrapCustom + 순번 (SDK 3.0 SonyHRLib.h 순서, ROM 호출로 확인: PaintChars 0xA82C, FntSetFont 0xA83E).
 * 글꼴 인자는 1바이트(SilkExtn도 moveb로 넘김). 소니 헤더는 저장소에 넣지 않으므로 필요한 것만 직접 선언한다. */
#define HRT(n)        SYS_TRAP(sysLibTrapCustom + (n))
#define kHrStdFont    8
void   HRWinDrawChars(UInt16 ref, const Char *s, Int16 len, Coord x, Coord y) HRT(9);
void   HRWinDrawRectangleFrame(UInt16 ref, FrameType f, RectangleType *r) HRT(16);
void   HRWinEraseRectangle(UInt16 ref, RectangleType *r, UInt16 corner) HRT(21);
void   HRWinGetWindowExtent(UInt16 ref, Coord *w, Coord *h) HRT(30);
void   HRWinInvertRectangle(UInt16 ref, RectangleType *r, UInt16 corner) HRT(35);
FontID HRFntSetFont(UInt16 ref, FontID f) HRT(57);

/* HR 라이브러리 참조 번호. 못 찾으면 0xFFFF. #52 = (오류 << 16) | 참조 번호 */
static inline UInt16 HrRef(void)
{
    Char   nm[16];
    UInt16 ref = 0xFFFF;
    Err    err;

    nm[0] = 'S'; nm[1] = 'o'; nm[2] = 'n'; nm[3] = 'y'; nm[4] = ' ';
    nm[5] = 'H'; nm[6] = 'R'; nm[7] = ' ';
    nm[8] = 'L'; nm[9] = 'i'; nm[10] = 'b'; nm[11] = 'r'; nm[12] = 'a'; nm[13] = 'r'; nm[14] = 'y';
    nm[15] = 0;
    err = SysLibFind(nm, &ref);
    FtrSet(kSkCreator, kFtrHrRef, ((UInt32)err << 16) | ref);
    return err ? 0xFFFF : ref;
}

/* 그리기 준비(현재 그리기 창이 실크 창일 때). HR로 그리면 참조 번호, 아니면 0xFFFF.
 * w,h = 배치 계산 크기: 저해상도 창 크기, HR이면 그 2배(고해상도 좌표) */
static inline UInt16 DrawSetup(Coord *w, Coord *h)
{
    UInt16 hr = 0xFFFF;

    *w = 0;
    *h = 0;
    WinGetWindowExtent(w, h);
    if (GetFtr(kFtrHires) != 0) {
        hr = HrRef();
        if (hr != 0xFFFF) {
            Coord hw = 0, hh = 0;

            HRWinGetWindowExtent(hr, &hw, &hh);
            FtrSet(kSkCreator, kFtrHrExt, ((UInt32)(UInt16)hw << 16) | (UInt16)hh);
            *w *= 2;
            *h *= 2;
        }
    }
    return hr;
}

static inline void KeyInner(UInt16 hr, RectangleType *r)
{
    RctInsetRectangle(r, (hr != 0xFFFF) ? 3 : 2);
}

static inline void InvRect(UInt16 hr, RectangleType *r)
{
    if (hr != 0xFFFF)
        HRWinInvertRectangle(hr, r, 0);
    else
        WinInvertRectangle(r, 0);
}

/* 키 번호 k의 안쪽 반전 (현재 그리기 창이 실크 창일 때) */
static inline void InvertCell(UInt16 hr, UInt16 st, UInt16 k, Coord w, Coord h)
{
    RectangleType r;

    if (k == 0)
        return;
    KeyCell(st, (k - 1) / 16, (k - 1) % 16, w, h, &r);
    KeyInner(hr, &r);
    InvRect(hr, &r);
}

static inline void DrawKeys(UInt32 hwin)
{
    WinHandle     prev;
    Coord         w = 0, h = 0;
    RectangleType r;
    FontID        oldFont;
    UInt16        st = (UInt16)GetFtr(kFtrState);
    UInt16        row, col, hr;

    if (!WinOk(hwin))
        return;
    prev = WinSetDrawWindow((WinHandle)hwin);
    hr = DrawSetup(&w, &h);

    RctSetRectangle(&r, 0, 0, w, h);
    if (hr != 0xFFFF) {
        HRWinEraseRectangle(hr, &r, 0);
        oldFont = HRFntSetFont(hr, kHrStdFont);
    } else {
        WinEraseRectangle(&r, 0);
        oldFont = FntSetFont(stdFont);
    }
    for (row = 0; row < kRows; row++) {
        UInt16 n = RowKeys(st, row);

        for (col = 0; col < n; col++) {
            UInt16 code = KeyCode(st, row, col);
            Char   lab[4];
            UInt16 len = KeyLabel(code, lab);

            KeyCell(st, row, col, w, h, &r);
            KeyInner(hr, &r);
            if (hr != 0xFFFF)
                HRWinDrawRectangleFrame(hr, roundFrame, &r);
            else
                WinDrawRectangleFrame(roundFrame, &r);
            if (len > 0) {
                Coord tx = r.topLeft.x + (r.extent.x - FntCharsWidth(lab, len)) / 2;
                Coord ty = r.topLeft.y + (r.extent.y - FntLineHeight()) / 2;

                if (hr != 0xFFFF)
                    HRWinDrawChars(hr, lab, len, tx, ty);
                else
                    WinDrawChars(lab, len, tx, ty);
            }
            if ((code == kKShift && (st & kStShift)) || (code == kKNum && (st & kStNum)))
                InvRect(hr, &r);
        }
    }
    if (hr != 0xFFFF)
        HRFntSetFont(hr, oldFont);
    else
        FntSetFont(oldFont);

    WinSetDrawWindow(prev);
    FtrSet(kSkCreator, kFtrStartWin, ((UInt32)(UInt16)w << 16) | (UInt16)h);
    FtrSet(kSkCreator, kFtrPressed, 0);
}

/* 눌린 키 반전 토글 (DrawKeys와 같은 방식으로) */
static inline void InvertKey(UInt32 hwin, UInt16 k)
{
    WinHandle prev;
    Coord     w = 0, h = 0;
    UInt16    hr;

    if (k == 0 || !WinOk(hwin))
        return;
    prev = WinSetDrawWindow((WinHandle)hwin);
    hr = DrawSetup(&w, &h);
    InvertCell(hr, (UInt16)GetFtr(kFtrState), k, w, h);
    WinSetDrawWindow(prev);
}

/* 펜 좌표가 들어간 키 번호 */
static inline UInt16 PenKey(UInt32 hwin, const UInt8 *event)
{
    Coord w, h;

    if (!WinExt(hwin, &w, &h))
        return 0;
    return HitKey((UInt16)GetFtr(kFtrState),
                  (Coord)Be16(event + 2), (Coord)Be16(event + 4), w, h);
}

/* ---------------- 한글 조합 ---------------- */
/* 자모는 호환 자모 번호 o = 유니코드 - 0x3130 (1~51) 로 다룬다 */
#define O(u)          ((UInt8)((u) - 0x3130))
#define P(a, b)       (((UInt16)O(a) << 8) | O(b))

static inline UInt16 KscJamo(UInt8 o)
{
    return (UInt16)(0xA4A1 + o - 1);
}

static inline Boolean IsVowel(UInt8 o)
{
    return o >= O(0x314F);
}

/* 초성 번호 0~18, 초성 불가면 -1 */
static inline Int16 ChoIdx(UInt8 o)
{
    switch (o) {
    case O(0x3131): return 0;   case O(0x3132): return 1;   case O(0x3134): return 2;
    case O(0x3137): return 3;   case O(0x3138): return 4;   case O(0x3139): return 5;
    case O(0x3141): return 6;   case O(0x3142): return 7;   case O(0x3143): return 8;
    case O(0x3145): return 9;   case O(0x3146): return 10;  case O(0x3147): return 11;
    case O(0x3148): return 12;  case O(0x3149): return 13;  case O(0x314A): return 14;
    case O(0x314B): return 15;  case O(0x314C): return 16;  case O(0x314D): return 17;
    case O(0x314E): return 18;
    default:        return -1;
    }
}

/* 종성 번호 1~27, 종성 불가면 0 (ㄸ ㅃ ㅉ 은 종성 불가) */
static inline UInt16 JongIdx(UInt8 o)
{
    if (o == 0 || o > O(0x314E))
        return 0;
    switch (o) {
    case O(0x3138): case O(0x3143): case O(0x3149):
        return 0;
    }
    /* 호환 자모 순서에서 ㄸ ㅃ ㅉ 만 빼면 종성 순서(ㄱ=1 ... ㅎ=27)와 같다 */
    {
        UInt16 n = o;

        if (o > O(0x3138)) n--;
        if (o > O(0x3143)) n--;
        if (o > O(0x3149)) n--;
        return n;
    }
}

/* 모음 + 모음 (복합모음, 단모음 연타). 안 되면 0 */
static inline UInt8 VowelPlus(UInt8 a, UInt8 b)
{
    switch (((UInt16)a << 8) | b) {
    case P(0x3157, 0x314F): return O(0x3158);   /* ㅗ+ㅏ ㅘ */
    case P(0x3157, 0x3150): return O(0x3159);   /* ㅗ+ㅐ ㅙ */
    case P(0x3157, 0x3163): return O(0x315A);   /* ㅗ+ㅣ ㅚ */
    case P(0x315C, 0x3153): return O(0x315D);   /* ㅜ+ㅓ ㅝ */
    case P(0x315C, 0x3154): return O(0x315E);   /* ㅜ+ㅔ ㅞ */
    case P(0x315C, 0x3163): return O(0x315F);   /* ㅜ+ㅣ ㅟ */
    case P(0x3161, 0x3163): return O(0x3162);   /* ㅡ+ㅣ ㅢ */
    case P(0x314F, 0x314F): return O(0x3151);   /* ㅏㅏ ㅑ */
    case P(0x3153, 0x3153): return O(0x3155);   /* ㅓㅓ ㅕ */
    case P(0x3157, 0x3157): return O(0x315B);   /* ㅗㅗ ㅛ */
    case P(0x315C, 0x315C): return O(0x3160);   /* ㅜㅜ ㅠ */
    case P(0x3150, 0x3150): return O(0x3152);   /* ㅐㅐ ㅒ */
    case P(0x3154, 0x3154): return O(0x3156);   /* ㅔㅔ ㅖ */
    default:                return 0;
    }
}

/* 모음 되돌리기 (BS). 기본 모음이면 0 */
static inline UInt8 VowelBase(UInt8 v)
{
    switch (v) {
    case O(0x3158): case O(0x3159): case O(0x315A): case O(0x315B): return O(0x3157);
    case O(0x315D): case O(0x315E): case O(0x315F): case O(0x3160): return O(0x315C);
    case O(0x3162): return O(0x3161);
    case O(0x3151): return O(0x314F);
    case O(0x3155): return O(0x3153);
    case O(0x3152): return O(0x3150);
    case O(0x3156): return O(0x3154);
    default:        return 0;
    }
}

/* 겹받침 만들기. 안 되면 0 */
static inline UInt8 JongPlus(UInt8 a, UInt8 b)
{
    switch (((UInt16)a << 8) | b) {
    case P(0x3131, 0x3145): return O(0x3133);   /* ㄳ */
    case P(0x3134, 0x3148): return O(0x3135);   /* ㄵ */
    case P(0x3134, 0x314E): return O(0x3136);   /* ㄶ */
    case P(0x3139, 0x3131): return O(0x313A);   /* ㄺ */
    case P(0x3139, 0x3141): return O(0x313B);   /* ㄻ */
    case P(0x3139, 0x3142): return O(0x313C);   /* ㄼ */
    case P(0x3139, 0x3145): return O(0x313D);   /* ㄽ */
    case P(0x3139, 0x314C): return O(0x313E);   /* ㄾ */
    case P(0x3139, 0x314D): return O(0x313F);   /* ㄿ */
    case P(0x3139, 0x314E): return O(0x3140);   /* ㅀ */
    case P(0x3142, 0x3145): return O(0x3144);   /* ㅄ */
    default:                return 0;
    }
}

/* 겹받침의 앞 자음(홑받침이면 0), 뒤 자음(홑받침이면 자기 자신) */
static inline UInt8 JongFirst(UInt8 t)
{
    switch (t) {
    case O(0x3133): return O(0x3131);
    case O(0x3135): case O(0x3136): return O(0x3134);
    case O(0x313A): case O(0x313B): case O(0x313C): case O(0x313D):
    case O(0x313E): case O(0x313F): case O(0x3140): return O(0x3139);
    case O(0x3144): return O(0x3142);
    default:        return 0;
    }
}

static inline UInt8 JongSecond(UInt8 t)
{
    switch (t) {
    case O(0x3133): case O(0x313D): case O(0x3144): return O(0x3145);
    case O(0x3135): return O(0x3148);
    case O(0x3136): case O(0x3140): return O(0x314E);
    case O(0x313A): return O(0x3131);
    case O(0x313B): return O(0x3141);
    case O(0x313C): return O(0x3142);
    case O(0x313E): return O(0x314C);
    case O(0x313F): return O(0x314D);
    default:        return t;
    }
}

#ifndef MNSK_HOST_TEST
/* 표(KscBits, KscCum)는 파일 끝에서 .text에 넣는다(진입 함수가 code 0 맨 앞이어야 함) */
static inline const UInt8 *KscBitsPtr(void)
{
    const UInt8 *p;

    asm("lea KscBits(%%pc),%0" : "=a" (p));
    return p;
}

static inline const UInt8 *KscCumPtr(void)
{
    const UInt8 *p;

    asm("lea KscCum(%%pc),%0" : "=a" (p));
    return p;
}
#endif

static inline UInt16 Pop8(UInt8 b)
{
    UInt16 n = 0;

    while (b) {
        n += b & 1;
        b >>= 1;
    }
    return n;
}

/* 초성·중성·종성 → KSC5601 코드, 완성형에 없으면 0 */
static inline UInt16 SylKsc(UInt8 cho, UInt8 jung, UInt8 jong)
{
    Int16        ci = ChoIdx(cho);
    UInt16       ti = 0, i, r, b;
    const UInt8 *tb, *cum;

    if (ci < 0 || jung == 0 || !IsVowel(jung))
        return 0;
    if (jong != 0) {
        ti = JongIdx(jong);
        if (ti == 0)
            return 0;
    }
    i = ((UInt16)ci * 21 + (jung - O(0x314F))) * 28 + ti;
    tb = KscBitsPtr();
    if (!(tb[i >> 3] & (0x80 >> (i & 7))))
        return 0;
    cum = KscCumPtr();
    r = ((UInt16)cum[(i >> 8) * 2] << 8) | cum[(i >> 8) * 2 + 1];
    for (b = (i >> 8) << 5; b < (i >> 3); b++)
        r += Pop8(tb[b]);
    r += Pop8(tb[i >> 3] & (UInt8)~(0xFF >> (i & 7)));
    return ((UInt16)(0xB0 + r / 94) << 8) | (0xA1 + r % 94);
}

static inline void SendKey(UInt16 c)
{
    EvtEnqueueKey(c, 0, 0);
    BumpFtr(kFtrKeyCount);
}

/* 조합 상태를 화면에 보낸다. 보낸 글자 수 반환 */
static inline UInt16 Emit(UInt8 cho, UInt8 jung, UInt8 jong)
{
    UInt16 n = 0;

    if (cho && jung) {
        UInt16 k = SylKsc(cho, jung, jong);

        if (k) {
            SendKey(k);
            return 1;
        }
    }
    if (cho)  { SendKey(KscJamo(cho));  n++; }
    if (jung) { SendKey(KscJamo(jung)); n++; }
    if (jong) { SendKey(KscJamo(jong)); n++; }
    return n;
}

static inline LocalID CurApp(void)
{
    UInt16  card = 0;
    LocalID id = 0;

    SysCurAppDatabase(&card, &id);
    return id;
}

/* 현재 조합 상태. 다른 앱에서 시작된 조합이면 버린다 */
static inline UInt32 GetComp(void)
{
    UInt32 s = GetFtr(kFtrComp);

    if (s != 0 && GetFtr(kFtrCompApp) != (UInt32)CurApp())
        s = 0;
    return s;
}

static inline void PutComp(UInt8 c, UInt8 v, UInt8 t, UInt16 shown)
{
    FtrSet(kSkCreator, kFtrComp,
           ((UInt32)c << 24) | ((UInt32)v << 16) | ((UInt32)t << 8) | (UInt8)shown);
    FtrSet(kSkCreator, kFtrCompApp, (UInt32)CurApp());
}

static inline void Commit(void)
{
    FtrSet(kSkCreator, kFtrComp, 0);
}

/* 자모 하나 입력 */
static inline void Compose(UInt8 o)
{
    UInt32  s = GetComp();
    UInt8   c0 = (UInt8)(s >> 24), v0 = (UInt8)(s >> 16), t0 = (UInt8)(s >> 8);
    UInt16  shown = (UInt8)s;
    UInt8   c = c0, v = v0, t = t0, nc = 0, nv = 0, x;
    Boolean split = false;

    if (shown == 0)
        c = v = t = 0;

    if (!IsVowel(o)) {
        if (c && v && !t && SylKsc(c, v, o))
            t = o;
        else if (t && (x = JongPlus(t, o)) != 0 && SylKsc(c, v, x))
            t = x;
        else {
            split = true;
            nc = o;
        }
    } else {
        if (c && !v) {
            if (SylKsc(c, o, 0))
                v = o;
            else {
                split = true;
                nv = o;
            }
        } else if (v && !t) {
            x = VowelPlus(v, o);
            if (x && (!c || SylKsc(c, x, 0)))
                v = x;
            else {
                split = true;
                nv = o;
            }
        } else if (t) {
            UInt8 keep = JongFirst(t), mv = JongSecond(t);

            split = true;
            if (SylKsc(mv, o, 0) && (keep == 0 || SylKsc(c, v, keep))) {
                t = keep;
                nc = mv;
            }
            nv = o;
        } else {
            split = true;
            nv = o;
        }
    }

    if (shown != 0 && (c != c0 || v != v0 || t != t0)) {
        UInt16 i;

        for (i = 0; i < shown; i++)
            SendKey(chrBackspace);
        shown = Emit(c, v, t);
    }
    if (split) {
        shown = Emit(nc, nv, 0);
        PutComp(nc, nv, 0, shown);
    } else
        PutComp(c, v, t, shown);
}

/* 조합 중 BS: 자모 하나 되돌리기. 조합 중이 아니면 보통 BS */
static inline void ComposeBack(void)
{
    UInt32 s = GetComp();
    UInt8  c = (UInt8)(s >> 24), v = (UInt8)(s >> 16), t = (UInt8)(s >> 8);
    UInt16 shown = (UInt8)s, i;

    if (shown == 0) {
        Commit();
        SendKey(chrBackspace);
        return;
    }
    if (t)
        t = JongFirst(t);
    else if (v)
        v = VowelBase(v);
    else
        c = 0;
    for (i = 0; i < shown; i++)
        SendKey(chrBackspace);
    if (c || v || t) {
        shown = Emit(c, v, t);
        PutComp(c, v, t, shown);
    } else
        Commit();
}

/* 키 동작. 다시 그려야 하면 true */
static inline Boolean DoKey(UInt16 k)
{
    UInt16 st = (UInt16)GetFtr(kFtrState);
    UInt16 code = KeyCode(st, (k - 1) / 16, (k - 1) % 16);
    UInt16 nst = st;

    switch (code) {
    case kKShift:
        nst ^= kStShift;
        break;
    case kKHanEng:
        Commit();
        nst = (st & kStEng) ^ kStEng;            /* 한↔영, 숫자·Shift 해제 */
        break;
    case kKNum:
        Commit();
        nst = (st ^ kStNum) & ~kStShift;
        break;
    case chrBackspace:
        ComposeBack();
        FtrSet(kSkCreator, kFtrKeyLast, ((UInt32)k << 16) | code);
        break;
    default:
        if ((code >> 8) == 0xA4) {               /* 한글 자모 → 조합 */
            Compose((UInt8)(code - 0xA4A1 + 1));
        } else {
            Commit();
            SendKey(code);
        }
        FtrSet(kSkCreator, kFtrKeyLast, ((UInt32)k << 16) | code);
        nst &= ~kStShift;                        /* 글자 하나 보내면 Shift 해제 */
        break;
    }
    if (nst == st)
        return false;
    FtrSet(kSkCreator, kFtrState, nst);
    return true;
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
        /* event+2 = 실크 영역 창. 슬롯에 넣어 두면 이후 첫 인자로 돌아온다. */
        if (slot != NULL)
            *slot = ((UInt32)event[2] << 24) | ((UInt32)event[3] << 16) |
                    ((UInt32)event[4] << 8) | event[5];
        break;

    case kEvtStart:
        RawFtr(kFtrStartRaw, event);
        FtrSet(kSkCreator, kFtrCoreBuild, (kSkMagic << 16) | kSkBuild);
        FtrSet(kSkCreator, kFtrHires, event[14]);
        Commit();
        DrawKeys((UInt32)slot);
        break;

    case kEvtStop:
        Commit();
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
            if (pressed != 0 && k == pressed && DoKey(k))
                DrawKeys((UInt32)slot);
        }
        break;

    case kEvtDisp:
        RawFtr(kFtrDispRaw, event);
        FtrSet(kSkCreator, kFtrHires, event[14]);
        DrawKeys((UInt32)slot);
        break;
    }
    return 0;
}

#ifndef MNSK_HOST_TEST
#include "ksc_table.h"
#endif
