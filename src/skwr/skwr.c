/*
 * MnSk - NR70V SilkWare(SkWr) 플러그인 7단계: 4줄 자판(안 B) + 자모 단독 출력(조합 없음)
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
 *   한글:  ㅂ ㅈ ㄷ ㄱ ㅅ ㅗ ㅐ ㅔ   (Shift: ㅃ ㅉ ㄸ ㄲ ㅆ ㅗ ㅒ ㅖ)
 *          ㅁ ㄴ ㅇ ㄹ ㅎ ㅓ ㅏ ㅣ
 *          ^  ㅋ ㅌ ㅊ ㅍ ㅜ ㅡ BS
 *   영문:  q w e r t y u i o p / a s d f g h j k l / ^ z x c v b n m BS   (Shift: 대문자)
 *   숫자:  1 2 3 4 5 6 7 8 9 0 / - / : ; ( ) $ & @ " / # % * + = ' ? ! BS
 *   공통 아랫줄: [한/A] [123] [,] [  space  ] [.] [Ent]
 *   - 자모는 KSC5601 호환 자모(0xA4A1~)로 하나씩 보낸다. 조합은 다음 단계.
 *   - Shift는 글자 하나를 보내면 풀린다. Shift·123이 켜져 있으면 그 키를 반전 표시.
 *
 * 기록 (feature 'MnSk'):
 *   #0 Open 표시, #1~#16 이벤트 0~15 횟수, #17 마지막 이벤트, #18 번호≥16 횟수, #19 out≠NULL 횟수
 *   #20~#23 Open 앞 16바이트, #24~#27 마지막 Start 앞 16바이트,
 *   #29 그린 창 크기(31~16 너비, 15~0 높이), #31~#34 마지막 PenDown, #35~#38 마지막 PenUp,
 *   #39~#42 마지막 DispChange, #45 그리기 판정(1 그림, 2 핸들 이상, 3 크기 이상),
 *   #46 눌린 키(0 없음, 줄*16+칸+1), #47 보낸 키 횟수, #48 마지막 전송(31~16 키 번호, 15~0 코드),
 *   #49 자판 상태(비트0 영문, 비트1 숫자, 비트8 Shift)
 */

#include <PalmOS.h>

#define kSkCreator    'MnSk'
#define kSkMagic      0x534BUL
#define kSkBuild      7

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

/* 키 영역(테두리 포함 칸 전체) */
static inline void KeyCell(UInt16 st, UInt16 row, UInt16 col, Coord w, Coord h, RectangleType *r)
{
    Coord rh = h / kRows;
    Coord top = row * rh;
    Coord bot = (row == kRows - 1) ? h : top + rh;
    Coord l, rr;

    if (row == kRows - 1) {
        l = BottomEdge(col) * w / kBaseW;
        rr = BottomEdge(col + 1) * w / kBaseW;
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
            if (x < BottomEdge(col + 1) * w / kBaseW)
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
    case 6:  return sh ? J(0x3152) : J(0x3150);   /* ㅒ ㅐ */
    case 7:  return sh ? J(0x3156) : J(0x3154);   /* ㅖ ㅔ */
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
static inline void KeyInner(RectangleType *r)
{
    RctInsetRectangle(r, 2);
}

/* 키 번호 k의 안쪽 반전 (현재 그리기 창이 실크 창일 때) */
static inline void InvertCell(UInt16 st, UInt16 k, Coord w, Coord h)
{
    RectangleType r;

    if (k == 0)
        return;
    KeyCell(st, (k - 1) / 16, (k - 1) % 16, w, h, &r);
    KeyInner(&r);
    WinInvertRectangle(&r, 0);
}

static inline void DrawKeys(UInt32 hwin)
{
    WinHandle     prev;
    Coord         w = 0, h = 0;
    RectangleType r;
    FontID        oldFont;
    UInt16        st = (UInt16)GetFtr(kFtrState);
    UInt16        row, col;

    if (!WinOk(hwin))
        return;
    prev = WinSetDrawWindow((WinHandle)hwin);
    WinGetWindowExtent(&w, &h);

    RctSetRectangle(&r, 0, 0, w, h);
    WinEraseRectangle(&r, 0);

    oldFont = FntSetFont(stdFont);
    for (row = 0; row < kRows; row++) {
        UInt16 n = RowKeys(st, row);

        for (col = 0; col < n; col++) {
            UInt16 code = KeyCode(st, row, col);
            Char   lab[4];
            UInt16 len = KeyLabel(code, lab);

            KeyCell(st, row, col, w, h, &r);
            KeyInner(&r);
            WinDrawRectangleFrame(roundFrame, &r);
            if (len > 0)
                WinDrawChars(lab, len,
                             r.topLeft.x + (r.extent.x - FntCharsWidth(lab, len)) / 2,
                             r.topLeft.y + (r.extent.y - FntLineHeight()) / 2);
            if ((code == kKShift && (st & kStShift)) || (code == kKNum && (st & kStNum)))
                WinInvertRectangle(&r, 0);
        }
    }
    FntSetFont(oldFont);

    WinSetDrawWindow(prev);
    FtrSet(kSkCreator, kFtrStartWin, ((UInt32)(UInt16)w << 16) | (UInt16)h);
    FtrSet(kSkCreator, kFtrPressed, 0);
}

/* 눌린 키 반전 토글 */
static inline void InvertKey(UInt32 hwin, UInt16 k)
{
    WinHandle prev;
    Coord     w = 0, h = 0;

    if (k == 0 || !WinOk(hwin))
        return;
    prev = WinSetDrawWindow((WinHandle)hwin);
    WinGetWindowExtent(&w, &h);
    InvertCell((UInt16)GetFtr(kFtrState), k, w, h);
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
        nst = (st & kStEng) ^ kStEng;            /* 한↔영, 숫자·Shift 해제 */
        break;
    case kKNum:
        nst = (st ^ kStNum) & ~kStShift;
        break;
    default:
        EvtEnqueueKey(code, 0, 0);
        BumpFtr(kFtrKeyCount);
        FtrSet(kSkCreator, kFtrKeyLast, ((UInt32)k << 16) | code);
        if (code != chrBackspace)
            nst &= ~kStShift;                    /* 글자 하나 보내면 Shift 해제 */
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
            if (pressed != 0 && k == pressed && DoKey(k))
                DrawKeys((UInt32)slot);
        }
        break;

    case kEvtDisp:
        RawFtr(kFtrDispRaw, event);
        DrawKeys((UInt32)slot);
        break;
    }
    return 0;
}
