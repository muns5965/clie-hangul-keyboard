/*
 * MnHangul 패치 코드 (리소스 HgPt 0) — 2a·2b·2c: BltDrawChars(트랩 0xA378) 통과 시험·자리표시·한글 그리기
 *
 * BltDrawChars는 SDK에 원형이 없는 내부 함수다. ROM 호출부(0x50D8C, 0x2BC4B8) 역분석으로 본 인자(34바이트 중 26):
 *   p1 포인터(그리기 상태로 추정), p2 사각형 포인터, p3 점 포인터, p4 문자열, p5 길이(16비트),
 *   p6 글꼴 구조체 포인터(FntGetFontPtr 결과), p7 포인터(글꼴 보조로 추정)
 * 2a: 인자를 feature('MnHg')에 기록하고 원래 함수를 그대로 부른다. 화면이 바뀌지 않아야 정상.
 * 2b: 자리표시 켜짐(#22 != 0)이면 KS X 1001 2바이트 글자(앞·뒤 바이트 0xA1~0xFE)마다 "[]"로 바꾼
 *     사본을 원래 함수에 넘긴다. 바이트 수가 같아 위치·폭·좌표계(저해상도/고해상도)는 원래 함수가
 *     그대로 처리하므로 좌표계를 몰라도 안전하다. 2바이트를 한 글자 단위로 끊는지 확인하는 단계.
 * 2c: 모드 2(#22 = 2)이고 글꼴 높이가 22(고해상도 std)이면 한글을 16×16 글자로 그린다.
 *     문자열을 라틴 구간과 2바이트 글자로 나눠, 라틴 구간은 원래 글꼴로, 2바이트 글자는 p6 머리를 본뜬
 *     1글자 임시 글꼴(폭 18, 높이 22, 글자 'H' 하나)로 원래 함수를 부른다. 좌표 배율 = 글꼴 높이 ÷ 사각형 높이
 *     (1 또는 2, 실측 v3). 라틴 폭은 p6 폭 표(SDK FontType 구조, 실측 v4)에서 계산한다.
 *     완성형 한글(앞 0xB0~0xC8)은 글자 데이터(#23·#24 포인터, 앱이 HgFn 0·1을 잠가 둠), 그 밖의 2바이트는 네모.
 *     조건이 안 맞으면(저해상도 글꼴 등) 2b처럼 "[]"로 그린다.
 * v7: 글꼴 높이 22만이 아니라 16~40(배율 1·2)이면 그린다(메모장 큰 글꼴 등). 세로 위치는 ascent - 15.
 *     한글 호출마다 (글꼴 높이, 사각형 높이) 조합별 횟수를 #30~#37에 센다.
 *
 * 리소스 배치: 오프셋 0 = 원래 함수 주소 칸(4바이트, 앱이 DmWrite로 기록), 오프셋 4 = 진입점 PatchBlt.
 * 어느 앱에서든 불리므로 전역변수·A5·재배치 금지(빌드 스크립트가 검사).
 *
 * 기록 (feature 'MnHg'):
 *   #0 호출 수, #1 마지막 길이, #2 마지막 첫 4바이트, #3 p6, #4 p2 사각형 (x<<16|y), #5 (w<<16|h),
 *   #6 p3 점 (x<<16|y), #7 p1, #8 p7,
 *   #9 0x80 이상 바이트가 든 호출 수, #10~#13 그 마지막 호출의 길이·첫 4바이트·사각형 xy·wh
 *   #14 마지막 호출 글꼴 (fontType<<16 | fRectHeight), #15 hi 호출 글꼴, #16 hi 호출 p3, #17 hi 호출 p1, #18 hi 호출 p6
 *   #19 자리표시·한글로 그린 글자 수, #22 모드(0 끔, 1 box, 2 glyph; 앱이 설정, 패치는 읽기만)
 *   #23·#24 글자 데이터 HgFn 0·1 포인터(앱이 설정), #27 2c로 그린 호출 수, #28 마지막 2c 호출 배율
 */

#include <PalmOS.h>

#define kHgCreator 'MnHg'

typedef UInt32 (*BltFn)(void *p1, void *p2, void *p3, const UInt8 *chars, UInt16 len, void *p6, void *p7);

/* 오프셋 0: 원래 BltDrawChars 주소 (앱이 설치 때 기록) */
asm(".text\n.even\nPatchOrig:\n .long 0\n");

static inline UInt32 OrigAddr(void)
{
    const UInt32 *p;

    asm("lea PatchOrig(%%pc),%0" : "=a" (p));
    return *p;
}

static inline UInt32 GetF(UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(kHgCreator, n, &v) != errNone)
        v = 0;
    return v;
}

static inline UInt32 First4(const UInt8 *c, UInt16 len)
{
    UInt32 v = 0;
    UInt16 i;

    for (i = 0; i < 4; i++)
        v = (v << 8) | ((c != NULL && i < len) ? c[i] : 0);
    return v;
}

static inline UInt32 Pair(const Int16 *p)
{
    return (p == NULL) ? 0xFFFFFFFFUL : (((UInt32)(UInt16)p[0] << 16) | (UInt16)p[1]);
}

#define kGlyphW   18      /* 한글 한 글자 폭(고해상도 픽셀, CJKOS 고해상도 std와 같음) */
#define kTmpChar  0x48    /* 임시 글꼴의 유일한 글자 */
#define kSplit    1175    /* HgFn 0 = 순위 0~1174, HgFn 1 = 나머지 */

/* KS X 1001 2바이트 글자인지 (앞·뒤 0xA1~0xFE) */
static inline Boolean IsPair(const UInt8 *c, UInt16 i, UInt16 len)
{
    return i + 1 < len && c[i] >= 0xA1 && c[i] <= 0xFE && c[i + 1] >= 0xA1 && c[i + 1] <= 0xFE;
}

/* 글꼴 f(SDK FontType)에서 글자 c의 폭(픽셀). 없는 글자는 '없는 글자' 칸의 폭 */
static inline UInt16 CharW(const Int16 *f, UInt8 c)
{
    const UInt16 *owt = (const UInt16 *)((const UInt8 *)f + 16 + (UInt16)f[8] * 2);
    Int16         first = f[1], last = f[2];
    UInt16        e;

    if ((Int16)c < first || (Int16)c > last)
        e = owt[last - first + 1];
    else {
        e = owt[c - first];
        if (e == 0xFFFF)
            e = owt[last - first + 1];
    }
    return e & 0xFF;
}

/* 1글자 임시 글꼴 (h = 글꼴 높이, 워드 수 TmpWords(h)):
 *   워드 0~12 머리, 13~ 그림(h줄 × 2워드), 그 뒤 위치표 3워드(글자, 없는 글자, 끝), 폭 표 3워드(글자, 없는 글자, 끝)
 *   owTLoc(워드 8) = (폭 표 바이트 위치 - 16) / 2 */
#define TmpWords(h)  (13 + (h) * 2 + 6)

static inline void MakeTmpFont(UInt16 *t, const Int16 *src)
{
    UInt16 h = src[7], loc = 13 + h * 2, i;

    for (i = 0; i < TmpWords(h); i++)
        t[i] = 0;
    t[0] = src[0];
    t[1] = kTmpChar;
    t[2] = kTmpChar;
    t[3] = kGlyphW;
    t[6] = kGlyphW;
    t[7] = h;
    t[8] = ((loc + 3) * 2 - 16) / 2;
    t[9] = src[9];
    t[10] = src[10];
    t[12] = 2;
    t[loc + 1] = kGlyphW;
    t[loc + 2] = kGlyphW;
    t[loc + 3] = kGlyphW;
    t[loc + 5] = 0xFFFF;
}

/* 16×16 글자 g(32바이트, NULL이면 네모)를 임시 글꼴 그림 칸 (1, top)에 넣는다 */
static inline void SetGlyph(UInt16 *t, const UInt8 *g, UInt16 top)
{
    UInt16 y;

    for (y = 0; y < 16; y++) {
        UInt16 v;

        if (g != NULL)
            v = ((UInt16)g[y * 2] << 8) | g[y * 2 + 1];
        else
            v = (y == 0 || y == 15) ? 0xFFFF : 0x8001;
        t[13 + (y + top) * 2] = v >> 1;
        t[13 + (y + top) * 2 + 1] = (v & 1) << 15;
    }
}

/* 글자 세로 시작 줄: 한글(1~13줄)을 영문 대문자 높이에 맞춘다. ascent - 15 (std 18 → 3, v6과 같음), 칸 안으로 제한 */
static inline UInt16 GlyphTop(const Int16 *f)
{
    Int16 top = f[9] - 15;

    if (top < 0)
        top = 0;
    if (top > f[7] - 16)
        top = f[7] - 16;
    return top;
}

/* v7 크기 조사: (글꼴 높이, 사각형 높이) 조합별 한글 호출 수. #30~#37 = 높이<<24 | 사각형<<16 | 횟수 */
static inline void CountSize(Int16 fh, Int16 rh)
{
    UInt32 key = ((UInt32)(fh & 0xFF) << 24) | ((UInt32)(rh & 0xFF) << 16);
    UInt16 i;

    for (i = 30; i <= 37; i++) {
        UInt32 v = GetF(i);

        if (v == 0 || (v & 0xFFFF0000UL) == key) {
            if ((v & 0xFFFF) != 0xFFFF)
                FtrSet(kHgCreator, i, key | ((v & 0xFFFF) + 1));
            return;
        }
    }
}

/* 2c: 라틴 구간·2바이트 글자로 나눠 그린다 */
static inline UInt32 DrawGlyphs(BltFn orig, void *p1, const RectangleType *rc, void *p3,
                                const UInt8 *chars, UInt16 len, const Int16 *font, void *p7,
                                UInt16 ratio, UInt16 *tmp, UInt32 g0, UInt32 g1)
{
    RectangleType r = *rc;
    UInt16        i = 0, px = 0, n = 0;
    UInt32        res = 0;
    UInt8         one = kTmpChar;

    UInt16        top = GlyphTop(font);

    MakeTmpFont(tmp, font);
    while (i < len) {
        if (IsPair(chars, i, len)) {
            const UInt8 *g = NULL;

            if (chars[i] >= 0xB0 && chars[i] <= 0xC8) {
                UInt16 rank = (chars[i] - 0xB0) * 94 + (chars[i + 1] - 0xA1);

                g = (rank < kSplit) ? (const UInt8 *)g0 + rank * 32
                                    : (const UInt8 *)g1 + (rank - kSplit) * 32;
            }
            SetGlyph(tmp, g, top);
            r.topLeft.x = rc->topLeft.x + px / ratio;
            r.extent.x = kGlyphW / ratio;
            res = orig(p1, &r, p3, &one, 1, tmp, p7);
            px += kGlyphW;
            i += 2;
            n++;
        } else {
            UInt16 start = i, w = 0;

            while (i < len && !IsPair(chars, i, len)) {
                w += CharW(font, chars[i]);
                i++;
            }
            r.topLeft.x = rc->topLeft.x + px / ratio;
            r.extent.x = (w + ratio - 1) / ratio;
            res = orig(p1, &r, p3, chars + start, i - start, (void *)font, p7);
            px += w;
        }
    }
    FtrSet(kHgCreator, 19, GetF(19) + n);
    FtrSet(kHgCreator, 27, GetF(27) + 1);
    FtrSet(kHgCreator, 28, ratio);
    return res;
}

/* FontType: +0 fontType, +14 fRectHeight */
static inline UInt32 FontInfo(const void *f)
{
    const Int16 *w = (const Int16 *)f;

    return (w == NULL) ? 0xFFFFFFFFUL : (((UInt32)(UInt16)w[0] << 16) | (UInt16)w[7]);
}

UInt32 PatchBlt(void *p1, void *p2, void *p3, const UInt8 *chars, UInt16 len, void *p6, void *p7)
{
    UInt32 orig = OrigAddr();
    UInt16 i;
    Boolean hi = false;

    FtrSet(kHgCreator, 0, GetF(0) + 1);
    FtrSet(kHgCreator, 1, len);
    FtrSet(kHgCreator, 2, First4(chars, len));
    FtrSet(kHgCreator, 3, (UInt32)p6);
    FtrSet(kHgCreator, 4, Pair((const Int16 *)p2));
    FtrSet(kHgCreator, 5, (p2 == NULL) ? 0xFFFFFFFFUL : Pair((const Int16 *)p2 + 2));
    FtrSet(kHgCreator, 6, Pair((const Int16 *)p3));
    FtrSet(kHgCreator, 7, (UInt32)p1);
    FtrSet(kHgCreator, 8, (UInt32)p7);
    FtrSet(kHgCreator, 14, FontInfo(p6));
    for (i = 0; chars != NULL && i < len; i++)
        if (chars[i] >= 0x80) {
            hi = true;
            break;
        }
    if (hi) {
        FtrSet(kHgCreator, 9, GetF(9) + 1);
        FtrSet(kHgCreator, 10, len);
        FtrSet(kHgCreator, 11, First4(chars, len));
        FtrSet(kHgCreator, 12, Pair((const Int16 *)p2));
        FtrSet(kHgCreator, 13, (p2 == NULL) ? 0xFFFFFFFFUL : Pair((const Int16 *)p2 + 2));
        FtrSet(kHgCreator, 15, FontInfo(p6));
        FtrSet(kHgCreator, 16, Pair((const Int16 *)p3));
        FtrSet(kHgCreator, 17, (UInt32)p1);
        FtrSet(kHgCreator, 18, (UInt32)p6);
        if (p2 != NULL && p6 != NULL)
            CountSize(((const Int16 *)p6)[7], ((const Int16 *)p2)[3]);
    }
    if (orig == 0)
        return 0;
    if (hi && GetF(22) == 2 && p2 != NULL && p6 != NULL) {
        const Int16 *f = (const Int16 *)p6;
        Int16        rh = ((const Int16 *)p2)[3];
        UInt32       g0 = GetF(23), g1 = GetF(24);

        Int16        fh = f[7];

        /* v7: 글꼴 높이 16~40, 배율(글꼴 높이 ÷ 사각형 높이) 1 또는 2 */
        if (fh >= 16 && fh <= 40 && rh > 0 && (fh == rh || fh == rh * 2) && g0 != 0 && g1 != 0) {
            UInt16 *tmp = MemPtrNew(TmpWords(fh) * 2);

            if (tmp != NULL) {
                UInt32 r = DrawGlyphs((BltFn)orig, p1, (const RectangleType *)p2, p3, chars, len, f, p7,
                                      (UInt16)(fh / rh), tmp, g0, g1);

                MemPtrFree(tmp);
                return r;
            }
        }
    }
    if (hi && GetF(22) != 0) {
        UInt8 *buf = MemPtrNew(len);

        if (buf != NULL) {
            UInt32 r;
            UInt16 n = 0;

            MemMove(buf, chars, len);
            for (i = 0; i + 1 < len; ) {
                if (buf[i] >= 0xA1 && buf[i] <= 0xFE && buf[i + 1] >= 0xA1 && buf[i + 1] <= 0xFE) {
                    buf[i] = '[';
                    buf[i + 1] = ']';
                    i += 2;
                    n++;
                } else
                    i++;
            }
            FtrSet(kHgCreator, 19, GetF(19) + n);
            r = ((BltFn)orig)(p1, p2, p3, buf, len, p6, p7);
            MemPtrFree(buf);
            return r;
        }
    }
    return ((BltFn)orig)(p1, p2, p3, chars, len, p6, p7);
}
