/*
 * MnSk5 — Palm OS 5 Sony Silk Plug-in 한글 키보드 (bin/MnSk5.prc, 타입 Slkw, 크리에이터 MnS5)
 *
 * NX70V·VZ90의 가상 실크(Graffiti 자리, 320×130)에 OS 4 Mn 가상 키보드(src/skwr/skwr.c)와 같은 4줄 자판을 그린다.
 * 조사: docs/OS5_RESEARCH.md 9절. 상태바 4번째 아이콘 → "Silk Plug-In" 목록에서 고르면 켜진다(리셋 없음).
 *
 * build 1(1단계): 자판 그리기·한/영·123·Shift 전환, 영문·숫자·기호·BS·Enter 보내기.
 * build 2(2단계): OS 4와 같은 단모음 한글 조합(src/common/hangul_compose.h, 완성형 판정표 src/skwr/ksc_table.h),
 *   조합 중 커서 이동 감지(src/common/cursor_track.h), 한/A의 '한'을 16×16 그림으로(src/skwr5/data/Sk5Han.bin, MnHangul5 없이도 보임),
 *   EvtEnqueueKey 결과 확인: 성공·실패를 따로 세고, 조합 중 하나라도 큐에 못 넣으면 조합을 끝낸다(다음 BS가 엉뚱한 글자를 지우지 않게).
 * build 5: 펜을 뗄 때 다시 그리는 자판의 색을 Silk Manager가 부른 그리기(Start·Draw·DispChange)의 색과 같게 한다 — 전에는 앱의 그리기 색을
 *   그대로 써 VZ90에서 처음은 회색(실크 배경, 색 번호 D8), 키를 누른 뒤는 검정(앱 배경 FF)으로 바뀌었다. NX70V는 둘 다 흰색이라 차이 없었음.
 * build 4: 자판 전체(DrawKeys)를 화면 밖 창에 다 그린 뒤 실크 창으로 한 번에 옮긴다(더블 버퍼링) — 전에는 실크를 지우고 키를 하나씩 그려
 *   느린 CPU에서 빈 자판·그리다 만 자판이 보였다(CPU 3 MIPS 시험). 화면 밖 창은 그릴 때마다 만들고 지운다(VZ90 16비트 약 83KB).
 *   만들지 못하면(메모리 부족 등) 전처럼 실크 창에 바로 그린다. 키 누름 반전(InvertKey)은 그 키만이라 그대로.
 * build 3: MnHangul5 v23이 세는 앱 화면 펜 누름 수(Feature 'MnH5' #10 = ARM ctx의 u32 칸 주소)가 바뀌었으면 조합을 끝낸다.
 *   Address 편집 화면·To Do처럼 표 안의 입력칸은 cursor_track.h가 찾지 못해, 다른 칸을 누른 뒤 앞 글자를 고치는 BS가 새 칸에 갔다(9.7절).
 *   커서 비교(CtCheck)는 그대로 함께 쓴다(펜 없이 커서가 옮겨진 경우). MnHangul5가 없거나 v22 이하면 이 비교는 하지 않는다.
 *
 * Silk Manager 약속 (NX70V ROM 역분석 + 시험 플러그인 실측, 9절):
 *   - DB 타입 Slkw, 리소스 code 0의 맨 앞이 진입점: Err entry(UInt32 *slot, UInt8 *ev). Open 때만 slot은 슬롯 주소.
 *   - ev[0] 이벤트 번호: 1 Open, 2 Close, 3 Start, 4 Stop, 5 Draw, 6 DispChange, 7 PenDown, 8 PenMove, 9 PenUp.
 *   - 68k 이벤트는 2바이트 정렬 배치: +0 번호(1바이트), +2 depth(32비트, NX70V 0x80·VZ90 0x10), +6 reserved, +10 data.
 *     Open·Start +10/+14(32비트): 실크 영역 크기(네이티브 320, 130). Pen +10/+12(16비트): 실크 영역 기준 네이티브 x, y.
 *   - 불릴 때 그리기 창은 실크 창이 아니다 → Sony Silk Library VskSetDrawWindow(silk) → 그리기 → VskRestoreDrawWindow.
 *   - 고해상도: WinSetCoordinateSystem(kCoordinatesNative)로 320×130 좌표에 그린다(OS 4는 Sony HR 라이브러리).
 *   - 시작 코드·A5가 없다 → 전역변수·문자열 상수·초기화된 지역 배열 금지, 위치 독립 코드(빌드 스크립트가 검사).
 *
 * 시험 빌드(SK5_CFLAGS로 켬, 배포 PRC에는 없음): -DSK5_DEBUG 자판 맨 위에 #47·#52·#50·#54를 표시(펜을 뗄 때마다 다시 그림),
 *   -DSK5_TEST_QFAIL=1|2|3 큐 실패 흉내(1 키 7개마다, 2 BS만 3번째마다, 3 글자만 3번째마다 실패로 처리).
 *
 * 보내는 키에는 keyCode kSkMark('MS')를 붙인다. MnHangul5 v22는 이 키를 조합하지 않고 그대로 보낸다
 * (MnHangul5가 한글 모드여도 이 키보드의 영문이 두벌식으로 바뀌지 않게).
 *
 * 기록 (feature 'MnS5'):
 *   #0 Open 표시(31~16 'S5', 7~0 빌드), #1~#11 이벤트 1~11 횟수, #12 Silk Library(31~16 SysLibFind 오류, 15~0 참조 번호),
 *   #13 VskSetDrawWindow 결과, #14 그린 크기(31~16 너비, 15~0 높이), #15 Start 크기(같은 형식), #16 Start depth 칸,
 *   #46 눌린 키(0 없음, 줄*16+칸+1), #47 큐에 넣은 키 수, #48 마지막 키(31~16 키 번호, 15~0 코드), #49 자판 상태,
 *   #50 조합 상태(31~24 초성, 23~16 중성, 15~8 종성: 호환 자모 - 0x3130, 7~0 화면에 나간 글자 수), #51 조합 중인 앱 LocalID,
 *   #52 큐에 못 넣은 키 수, #53 이번 키 처리 중 큐 실패(0/1), #54 큐 실패로 조합을 끝낸 수, #56 마지막으로 본 앱 화면 펜 수,
 *   #57 앱 화면 펜으로 조합을 끝낸 수, #58 화면 밖 창으로 그린 수, #59 화면 밖 창을 못 만들어 바로 그린 수(31~16 오류, 15~0 수),
 *   #64 Silk Manager 그리기의 색(build 5), #81~#87 커서 비교(cursor_track.h)
 */

#include <PalmOS.h>

#define kCr           'MnS5'
#define kMagic        0x5335UL     /* 'S5' */
#define kBuild        5

#define kSkMark       0x4D53       /* 'MS' — MnHangul5 v22 ime.c kSkMark와 같아야 한다 */

#define kEvtOpen      1
#define kEvtStart     3
#define kEvtStop      4
#define kEvtDraw      5
#define kEvtDisp      6
#define kEvtPenDown   7
#define kEvtPenUp     9

#define kFtrOpen      0
#define kFtrCount0    0            /* #1~#11 = 이벤트 1~11 */
#define kFtrLib       12
#define kFtrSetWin    13
#define kFtrDrawn     14
#define kFtrSize      15
#define kFtrDepth     16
#define kFtrPressed   46
#define kFtrKeyCount  47
#define kFtrKeyLast   48
#define kFtrState     49
#define kFtrComp      50
#define kFtrCompApp   51
#define kFtrKeyFail   52
#define kFtrQFail     53
#define kFtrQEnd      54
#define kFtrPenSeen   56
#define kFtrPenEnd    57
#define kFtrOffUsed   58           /* build 4: 화면 밖 창으로 그린 수 */
#define kFtrOffFail   59           /* build 4: 화면 밖 창을 못 만들어 바로 그린 수(31~16 마지막 오류) */
#define kFtrColors    64           /* build 5: Silk Manager가 부른 그리기의 색 번호(24 있음, 23~16 배경, 15~8 전경, 7~0 글자) */
#define kHgCr         'MnH5'   /* MnHangul5 */
#define kHgFtrPen     10       /* MnHangul5 v23: 앱 화면 펜 수 칸 주소 */
#define kFtrFld       81   /* #81~#87 조합 중 커서 이동 감지 */

#define kStEng        0x0001
#define kStNum        0x0002
#define kStShift      0x0100

/* 특수 키 코드 (보내지 않음). 0x08 BS, 0x0A Enter, 0x20 이상은 그대로 보낸다 */
#define kKShift       1
#define kKHanEng      3
#define kKNum         4

#define kRows         4
#define kBaseW        160

#define J(u)          ((UInt16)(0xA4A1 + ((u) - 0x3131)))

/* Sony Silk Library (Sony CLIÉ SDK 5.0 SonySilkLib.h의 트랩 순서: sysLibTrapCustom + 12 = VskSetDrawWindow).
 * Sony 헤더는 저장소에 넣지 않으므로 필요한 것만 직접 선언한다. slkwType 0 = vskSlkwTypeSilk */
#define kSlkwSilk     0
Err VskSetDrawWindow(UInt16 ref, UInt16 slkwType) SYS_TRAP(sysLibTrapCustom + 12);
Err VskRestoreDrawWindow(UInt16 ref, UInt16 slkwType) SYS_TRAP(sysLibTrapCustom + 13);

static inline UInt32 GetFtr(UInt16 num)
{
    UInt32 v = 0;

    if (FtrGet(kCr, num, &v) != errNone)
        v = 0;
    return v;
}

static inline void BumpFtr(UInt16 num)
{
    UInt32 v = GetFtr(num);

    if (v != 0xFFFFFFFFUL)
        v++;
    FtrSet(kCr, num, v);
}

static inline UInt16 Be16(const UInt8 *p)
{
    return ((UInt16)p[0] << 8) | p[1];
}

static inline UInt32 Be32(const UInt8 *p)
{
    return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) | ((UInt32)p[2] << 8) | p[3];
}

/* ---------------- 자판 배치 (src/skwr/skwr.c와 같음) ---------------- */

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

static inline Coord Scale(Coord w)
{
    return (w >= 2 * kBaseW) ? 2 : 1;
}

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

/* 키 이름 (OS 4와 같음). 한/A는 '/A'만 글자로, '한'은 그림으로 그린다(DrawKeys) */
static inline UInt16 KeyLabel(UInt16 code, Char *s)
{
    switch (code) {
    case kKShift:     s[0] = '^'; return 1;
    case kKHanEng:    s[0] = '/'; s[1] = 'A'; return 2;
    case kKNum:       s[0] = '1'; s[1] = '2'; s[2] = '3'; return 3;
    case chrBackspace: s[0] = 'B'; s[1] = 'S'; return 2;
    case chrLineFeed: s[0] = 'E'; s[1] = 'n'; s[2] = 't'; return 3;
    case ' ':         return 0;
    }
    if (code > 0xFF)
        return 0;   /* 자모는 그림으로 */
    s[0] = (Char)code;
    return 1;
}

/* ---------------- 그리기 ---------------- */

static inline const UInt8 *Jamo16Ptr(void)
{
    const UInt8 *p;

    asm("lea Jamo16(%%pc),%0" : "=a" (p));
    return p;
}

static inline const UInt8 *Han16Ptr(void)
{
    const UInt8 *p;

    asm("lea Han16(%%pc),%0" : "=a" (p));
    return p;
}

/* 16×16 1비트 그림 g(줄마다 2바이트)를 글자 점 범위의 가운데가 (cx, cy)에 오게 그린다(네이티브 좌표).
 * 고밀도(kDensityDouble) V3로 감싸 네이티브 1:1로 그린다(저밀도 그대로면 2배로 커진다). 그린 점 너비를 *pw에(선택) */
static inline Boolean DrawGlyph16(const UInt8 *g, Coord cx, Coord cy, Coord *pw)
{
    BitmapPtr      bmp;
    BitmapTypeV3  *v3;
    UInt8         *px;
    Err            e = 0;
    UInt16         rb = 0, r, c, rmin = 16, rmax = 0, cmin = 16, cmax = 0;

    for (r = 0; r < 16; r++) {
        UInt16 v = ((UInt16)g[r * 2] << 8) | g[r * 2 + 1];

        if (v == 0)
            continue;
        if (r < rmin)
            rmin = r;
        rmax = r;
        for (c = 0; c < 16; c++)
            if (v & (0x8000 >> c)) {
                if (c < cmin)
                    cmin = c;
                if (c > cmax)
                    cmax = c;
            }
    }
    if (rmin > rmax)
        return false;
    bmp = BmpCreate(16, 16, 1, NULL, &e);
    if (bmp == NULL)
        return false;
    BmpGetDimensions(bmp, NULL, NULL, &rb);
    px = (UInt8 *)BmpGetBits(bmp);
    if (rb != 2 || px == NULL) {
        BmpDelete(bmp);
        return false;
    }
    for (r = 0; r < 32; r++)
        px[r] = g[r];
    v3 = BmpCreateBitmapV3(bmp, kDensityDouble, px, NULL);
    if (v3 == NULL) {
        BmpDelete(bmp);
        return false;
    }
    WinDrawBitmap((BitmapPtr)v3, cx - (Coord)((cmin + cmax + 1) / 2), cy - (Coord)((rmin + rmax + 1) / 2));
    BmpDelete((BitmapPtr)v3);
    BmpDelete(bmp);
    if (pw != NULL)
        *pw = (Coord)(cmax - cmin + 1);
    return true;
}

/* 자모(KS X 1001 0xA4A1부터 번호 o = 1~94) */
static inline Boolean DrawJamo(UInt16 o, Coord cx, Coord cy)
{
    if (o == 0 || o > 94)
        return false;
    return DrawGlyph16(Jamo16Ptr() + (o - 1) * 32, cx, cy, NULL);
}

static inline void EndDraw(UInt16 ref, UInt16 old);

/* 실크 창에 네이티브 좌표로 그리기 시작. 성공하면 true(끝나면 EndDraw), 실패하면 정리까지 하고 false */
static inline Boolean BeginDraw(UInt16 *ref, UInt16 *old, Coord *w, Coord *h)
{
    Char nm[18];
    Err  e;

    nm[0] = 'S'; nm[1] = 'o'; nm[2] = 'n'; nm[3] = 'y'; nm[4] = ' ';
    nm[5] = 'S'; nm[6] = 'i'; nm[7] = 'l'; nm[8] = 'k'; nm[9] = ' ';
    nm[10] = 'L'; nm[11] = 'i'; nm[12] = 'b'; nm[13] = 'r'; nm[14] = 'a'; nm[15] = 'r'; nm[16] = 'y'; nm[17] = 0;
    *ref = 0;
    e = SysLibFind(nm, ref);
    FtrSet(kCr, kFtrLib, ((UInt32)e << 16) | *ref);
    if (e != errNone)
        return false;
    e = VskSetDrawWindow(*ref, kSlkwSilk);
    FtrSet(kCr, kFtrSetWin, e);
    if (e != errNone)
        return false;
    *old = WinSetCoordinateSystem(kCoordinatesNative);
    *w = 0;
    *h = 0;
    WinGetWindowExtent(w, h);
    if (*w > 0 && *h > 0)
        return true;
    EndDraw(*ref, *old);
    return false;
}

static inline void EndDraw(UInt16 ref, UInt16 old)
{
    WinSetCoordinateSystem(old);
    VskRestoreDrawWindow(ref, kSlkwSilk);
}

static inline void KeyInner(RectangleType *r)
{
    RctInsetRectangle(r, 3);
}



/* 펜 좌표(+10/+12, 실크 기준 네이티브)가 들어간 키 번호. 크기는 마지막으로 그린 자판 크기(#14, 실크 창의 네이티브 크기).
 * build 3: 전에는 Start 이벤트의 크기(#15)를 썼는데, 실크를 최소화했다 다시 펼칠 때 온 Start의 값이 맞지 않아(시험: 펼친 뒤 키가 안 먹음)
 * 그린 크기로 바꿨다 */
static inline UInt16 PenKey(const UInt8 *ev)
{
    UInt32 sz = GetFtr(kFtrDrawn);
    Coord  w = (Coord)(sz >> 16), h = (Coord)(sz & 0xFFFF);

    if (w <= 0 || h <= 0)
        return 0;
    return HitKey((UInt16)GetFtr(kFtrState), (Coord)Be16(ev + 10), (Coord)Be16(ev + 12), w, h);
}

/* ---------------- 키 동작 ---------------- */

#include "../common/cursor_track.h"

/* 키 하나를 큐에 넣는다(keyCode kSkMark). 실패하면 세고 '이번 키 처리 중 실패' 표시 */
static inline void HcSend(UInt16 c)
{
    Err e;

#ifdef SK5_TEST_QFAIL
    /* 시험 빌드: 큐 실패 흉내(#55 순환 계수). SK5_TEST_QFAIL=1 넣으려는 키 7개마다 하나, =2 BS만 세어 3번째마다,
     * =3 BS 아닌 키(글자)만 세어 3번째마다 넣지 않고 실패로 처리 */
    if (SK5_TEST_QFAIL == 1 || (SK5_TEST_QFAIL == 2) == (c == chrBackspace)) {
        UInt32 n = GetFtr(55), lim = (SK5_TEST_QFAIL == 1) ? 6 : 2;

        FtrSet(kCr, 55, n >= lim ? 0 : n + 1);
    }
    if ((SK5_TEST_QFAIL == 1 || (SK5_TEST_QFAIL == 2) == (c == chrBackspace)) && GetFtr(55) == 0)
        e = 1;
    else
#endif
    e = EvtEnqueueKey(c, kSkMark, 0);
    if (e == errNone) {
        BumpFtr(kFtrKeyCount);
        CtDelta(kCr, kFtrFld, c);
    } else {
        BumpFtr(kFtrKeyFail);
        FtrSet(kCr, kFtrQFail, 1);
    }
}

#include "../common/hangul_compose.h"

static inline void SendKey(UInt16 c)
{
    HcSend(c);
}

static inline void Commit(void)
{
    FtrSet(kCr, kFtrComp, 0);
}

/* 조합 상태는 #50·#51 (공용 함수는 상태 두 칸을 받음) */
static inline void SkCompose(UInt8 o)
{
    UInt32 st[2];

    st[0] = GetFtr(kFtrComp);
    st[1] = GetFtr(kFtrCompApp);
    Compose(st, o);
    FtrSet(kCr, kFtrComp, st[0]);
    FtrSet(kCr, kFtrCompApp, st[1]);
}

/* 조합 중 BS: 자모 하나 되돌리기. 조합 중이 아니면 보통 BS */
static inline void SkComposeBack(void)
{
    UInt32  st[2];
    Boolean done;

    st[0] = GetFtr(kFtrComp);
    st[1] = GetFtr(kFtrCompApp);
    done = ComposeBack(st);
    FtrSet(kCr, kFtrComp, st[0]);
    FtrSet(kCr, kFtrCompApp, st[1]);
    if (!done) {
        CtUnknown(kCr, kFtrFld);   /* 지울 글자 크기를 모름 */
        SendKey(chrBackspace);
    }
}

/* MnHangul5 v23의 앱 화면 펜 누름 수. 없으면 0(바뀌지 않으므로 비교 안 함). ARM 리틀엔디언 값이지만 바뀌었는지만 본다 */
static inline UInt32 AppPenCount(void)
{
    UInt32 p = 0;

    if (FtrGet(kHgCr, kHgFtrPen, &p) != errNone || p == 0 || (p & 3))
        return 0;
    return *(volatile UInt32 *)p;
}

/* 이번 키 처리 중 큐에 못 넣은 키가 있으면: 화면 글자와 조합 상태가 어긋났을 수 있으므로 조합을 끝내고 커서 비교도 쉰다 */
static inline void QFailEnd(void)
{
    if (GetFtr(kFtrQFail) == 0)
        return;
    FtrSet(kCr, kFtrQFail, 0);
    if (GetFtr(kFtrComp) != 0)
        BumpFtr(kFtrQEnd);
    Commit();
    CtUnknown(kCr, kFtrFld);
}


/* 큰 함수는 inline으로 두면 Sk5Entry 한 함수가 너무 커져(switch 점프표 16비트 넘침) 따로 둔다.
 * 진입점이 .text 맨 앞이어야 하므로 선언만 앞에 두고 본문은 Sk5Entry 뒤에 둔다. */
static void    DrawKeys(Boolean silkCtx);
static void    InvertKey(UInt16 k);
static Boolean DoKey(UInt16 k);

Err Sk5Entry(UInt32 *slot, UInt8 *ev)
{
    UInt8 e;

    if (ev == NULL)
        return 0;
    e = ev[0];
    if (e >= 1 && e <= 11)
        BumpFtr(kFtrCount0 + e);

    switch (e) {
    case kEvtOpen:
        FtrSet(kCr, kFtrOpen, (kMagic << 16) | kBuild);
        if (slot != NULL)
            *slot = (kMagic << 16) | kBuild;
        break;

    case kEvtStart:
        FtrSet(kCr, kFtrSize, ((Be32(ev + 10) & 0xFFFF) << 16) | (Be32(ev + 14) & 0xFFFF));
        FtrSet(kCr, kFtrDepth, Be32(ev + 2));
        Commit();
        CtReset(kCr, kFtrFld);   /* 키보드를 새로 띄우면 비교 다시 시작 */
        FtrSet(kCr, kFtrPenSeen, AppPenCount());
#ifdef SK5_TEST_QFAIL
        FtrSet(kCr, 55, 0);      /* 시험 빌드: 실패 흉내 계수를 켤 때마다 처음부터 */
#endif
        DrawKeys(true);
        break;

    case kEvtStop:
        Commit();
        CtReset(kCr, kFtrFld);
        break;

    case kEvtDraw:
    case kEvtDisp:
        DrawKeys(true);
        break;

    case kEvtPenDown:
        {
            UInt16 k = PenKey(ev);

            FtrSet(kCr, kFtrPressed, k);
            InvertKey(k);
        }
        break;

    case kEvtPenUp:
        {
            UInt16 pressed = (UInt16)GetFtr(kFtrPressed);
            UInt16 k = PenKey(ev);

            InvertKey(pressed);
            FtrSet(kCr, kFtrPressed, 0);
            if (pressed != 0 && k == pressed && DoKey(k))
                DrawKeys(false);
#ifdef SK5_DEBUG
            else
                DrawKeys(false);
#endif
        }
        break;
    }
    return 0;
}

static void DrawKeys(Boolean silkCtx)
{
    UInt16        ref, old;
    Coord         w, h;
    RectangleType r;
    FontID        oldFont;
    UInt16        st = (UInt16)GetFtr(kFtrState);
    UInt16        row, col;
    WinHandle     silk, off;
    Err           oe = errNone;
    IndexedColorType obk, ofg, otx;

    if (!BeginDraw(&ref, &old, &w, &h))
        return;
    /* build 5: 색 — Silk Manager가 부른 그리기면 그 색을 기억하고, 펜을 뗄 때(앱의 그리기 색)는 기억한 색으로 그린 뒤 되돌린다 */
    obk = WinSetBackColor(0);
    ofg = WinSetForeColor(0);
    otx = WinSetTextColor(0);
    if (silkCtx)
        FtrSet(kCr, kFtrColors, 0x01000000UL | ((UInt32)obk << 16) | ((UInt32)ofg << 8) | otx);
    {
        UInt32 cv = GetFtr(kFtrColors);

        if (cv & 0x01000000UL) {
            WinSetBackColor((IndexedColorType)(cv >> 16));
            WinSetForeColor((IndexedColorType)(cv >> 8));
            WinSetTextColor((IndexedColorType)cv);
        } else {
            WinSetBackColor(obk);
            WinSetForeColor(ofg);
            WinSetTextColor(otx);
        }
    }
    /* build 4: 화면 밖 창(실크 창과 같은 기기 형식·크기)에 그린 뒤 한 번에 옮김. 못 만들면 실크 창에 바로 */
    silk = WinGetDrawWindow();
    off = WinCreateOffscreenWindow(w, h, nativeFormat, &oe);
#ifdef SK5_TEST_NOOFF   /* 시험 빌드: 화면 밖 창을 못 만든 경우 흉내 */
    if (off != NULL) {
        WinDeleteWindow(off, false);
        off = NULL;
        oe = memErrNotEnoughSpace;
    }
#endif
    if (off != NULL) {
        Coord ow = 0, oh = 0;

        WinSetDrawWindow(off);
        WinGetWindowExtent(&ow, &oh);
        if (ow != w || oh != h) {   /* 크기가 다르면(좌표계 해석이 다른 경우) 쓰지 않음 */
            WinSetDrawWindow(silk);
            WinDeleteWindow(off, false);
            off = NULL;
            oe = 0xFFFF;
        }
    }
    if (off != NULL)
        BumpFtr(kFtrOffUsed);
    else
        FtrSet(kCr, kFtrOffFail, ((UInt32)(UInt16)oe << 16) | ((GetFtr(kFtrOffFail) + 1) & 0xFFFF));
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
            if ((code >> 8) == 0xA4)
                DrawJamo(code - 0xA4A1 + 1, r.topLeft.x + r.extent.x / 2, r.topLeft.y + r.extent.y / 2);
            else if (code == kKHanEng) {   /* '한' 그림 + 간격 1 + '/A' 글자, 묶어서 가운데 */
                Coord tw = FntCharsWidth(lab, len), gw = 12;
                Coord x0 = r.topLeft.x + (r.extent.x - (gw + 1 + tw)) / 2;

                DrawGlyph16(Han16Ptr(), x0 + gw / 2, r.topLeft.y + r.extent.y / 2, NULL);
                WinDrawChars(lab, len, x0 + gw + 1, r.topLeft.y + (r.extent.y - FntLineHeight()) / 2);
            } else if (len > 0)
                WinDrawChars(lab, len, r.topLeft.x + (r.extent.x - FntCharsWidth(lab, len)) / 2,
                             r.topLeft.y + (r.extent.y - FntLineHeight()) / 2);
            if ((code == kKShift && (st & kStShift)) || (code == kKNum && (st & kStNum)))
                WinInvertRectangle(&r, 0);
        }
    }
#ifdef SK5_DEBUG
    {   /* 시험 빌드(SK5_CFLAGS=-DSK5_DEBUG): 맨 위에 큐에 넣은 수 #47, 못 넣은 수 #52, Start 크기 #15, 그린 크기 #14 */
        Char s[40]; UInt16 i, j;
        for (i = 0; i < 4; i++) {
            UInt32 v = (i == 0) ? GetFtr(kFtrKeyCount) : (i == 1) ? GetFtr(kFtrKeyFail) : (i == 2) ? GetFtr(kFtrSize) : GetFtr(kFtrDrawn);
            for (j = 0; j < 8; j++) { UInt8 d = (UInt8)((v >> (28 - 4 * j)) & 15); s[i * 9 + j] = d < 10 ? '0' + d : 'A' + d - 10; }
            s[i * 9 + 8] = ' ';
        }
        FntSetFont(stdFont);
        WinDrawChars(s, 35, 2, 2);
    }
#endif
    FntSetFont(oldFont);
    if (off != NULL) {   /* build 4: 실크 창으로 한 번에 옮기고 화면 밖 창을 지움 */
        RctSetRectangle(&r, 0, 0, w, h);
        WinSetDrawWindow(silk);
        WinCopyRectangle(off, silk, &r, 0, 0, winPaint);
        WinDeleteWindow(off, false);
    }
    WinSetBackColor(obk);   /* build 5 */
    WinSetForeColor(ofg);
    WinSetTextColor(otx);
    EndDraw(ref, old);
    FtrSet(kCr, kFtrDrawn, ((UInt32)(UInt16)w << 16) | (UInt16)h);
    FtrSet(kCr, kFtrPressed, 0);
}

/* 눌린 키 안쪽 반전 토글 */
static void InvertKey(UInt16 k)
{
    UInt16        ref, old;
    Coord         w, h;
    RectangleType r;

    if (k == 0)
        return;
    if (!BeginDraw(&ref, &old, &w, &h))
        return;
    KeyCell((UInt16)GetFtr(kFtrState), (k - 1) / 16, (k - 1) % 16, w, h, &r);
    KeyInner(&r);
    WinInvertRectangle(&r, 0);
    EndDraw(ref, old);
}

/* 키 동작. 다시 그려야 하면 true */
static Boolean DoKey(UInt16 k)
{
    UInt16 st = (UInt16)GetFtr(kFtrState);
    UInt16 code = KeyCode(st, (k - 1) / 16, (k - 1) % 16);
    UInt16 nst = st;

    FtrSet(kCr, kFtrQFail, 0);
    {   /* build 3: 지난 키 뒤 앱 화면을 눌렀으면(다른 칸·다른 항목일 수 있음) 조합을 끝냄. 이미 보낸 키는 그대로 */
        UInt32 pc = AppPenCount();

        if (pc != GetFtr(kFtrPenSeen)) {
            FtrSet(kCr, kFtrPenSeen, pc);
            if (GetFtr(kFtrComp) != 0)
                BumpFtr(kFtrPenEnd);
            Commit();
        }
    }
    if (CtCheck(kCr, kFtrFld, (GetFtr(kFtrComp) & 0xFF) != 0))
        Commit();   /* 조합 중 커서가 옮겨졌거나 입력칸이 바뀜 */
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
        SkComposeBack();
        FtrSet(kCr, kFtrKeyLast, ((UInt32)k << 16) | code);
        break;
    default:
        if ((code >> 8) == 0xA4) {               /* 한글 자모 → 조합 */
            SkCompose((UInt8)(code - 0xA4A1 + 1));
        } else {
            Commit();
            SendKey(code);
        }
        FtrSet(kCr, kFtrKeyLast, ((UInt32)k << 16) | code);
        nst &= ~kStShift;                        /* 글자 하나 보내면 Shift 해제 */
        break;
    }
    QFailEnd();
    if (nst == st)
        return false;
    FtrSet(kCr, kFtrState, nst);
    return true;
}

#include "../skwr/ksc_table.h"   /* 완성형 판정표 KscBits·KscCum (tools/gen-ksc-table.py) */
asm(".text\n.even\nJamo16:\n .incbin \"SyJm0000.bin\", 126, 3008\n.even\n");   /* 낱자모 94자 × 16줄 × 2바이트 (src/sony/data) */
asm(".text\n.even\nHan16:\n .incbin \"Sk5Han.bin\", 0, 32\n.even\n");           /* '한' 16×16 (tools/gen-sk5-han.py) */
