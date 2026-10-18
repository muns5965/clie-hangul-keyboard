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
 *     v14: 글자는 조합형 묶음(HgJb 0 = Neo둥근모 16×16, HgJb 1 = 12×12)에서 조합한다(#23·#24).
 *     칸 높이 16~40 → 16×16(칸 폭 18), 11~15 → 12×12(칸 폭 13; v15부터 11 포함, 한글은 위 11줄 안에 있음). 자모 행 0xA4는 묶음의 자모표, 그 밖의 2바이트는 네모.
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
#include "winlog.h"

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

#define kTmpChar  0x48    /* 임시 글꼴의 유일한 글자 */

/* v14 조합형 글꼴 묶음(tools/gen-johab.py 형식, 리소스 HgJb 0 = 16×16, HgJb 1 = 12×12).
 * #23 = 16 묶음 주소, #24 = 12 묶음 주소(앱이 Patch ON 때 잠가 둠). 글자 칸 폭 = 글자 폭 + 양옆 1 */
#define kCell16   18      /* 16 글자 칸 폭(고해상도 픽셀, CJKOS 고해상도 std와 같음) */
#define kCell12   13      /* 12 글자 칸 폭(글자 11 + 1 + 1) */
#define kCell24   24      /* v22: 24 글자 칸 폭(추가 글꼴 MnHgFont24, 묶음 판 2 = 한 줄 4바이트). v23: 26 → 24(칸 여백 없음, 글꼴 자체 여백만) */

static inline UInt16 RdW(const UInt8 *p)
{
    return ((UInt16)p[0] << 8) | p[1];
}

/* v22: 묶음 그림 한 줄을 32비트(왼쪽 = 비트 31)로. 판 1 = 2바이트 줄, 판 2 = 4바이트 줄 */
static inline UInt32 RdRow(const UInt8 *pk, const UInt8 *p)
{
    if (pk[2] >= 2)
        return ((UInt32)RdW(p) << 16) | RdW(p + 2);
    return (UInt32)RdW(p) << 16;
}

static inline UInt16 RowBytes(const UInt8 *pk)
{
    return pk[2] >= 2 ? 4 : 2;
}

/* 아래 세 함수는 진입점(오프셋 4)보다 뒤에 놓이도록 파일 끝에 정의한다 */
static UInt16  RankToSyl(UInt16 r);
static void    Compose(const UInt8 *pk, UInt16 sy, UInt32 *rows);
static Boolean GlyphRows(const UInt8 *pk, UInt8 lead, UInt8 trail, UInt32 *rows);

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

static inline void MakeTmpFont(UInt16 *t, const Int16 *src, UInt16 cw)
{
    UInt16 h = src[7], loc = 13 + h * 2, i;

    for (i = 0; i < TmpWords(h); i++)
        t[i] = 0;
    t[0] = src[0];
    t[1] = kTmpChar;
    t[2] = kTmpChar;
    t[3] = cw;
    t[6] = cw;
    t[7] = h;
    t[8] = ((loc + 3) * 2 - 16) / 2;
    t[9] = src[9];
    t[10] = src[10];
    t[12] = 2;
    t[loc + 1] = cw;
    t[loc + 2] = cw;
    t[loc + 3] = cw;
    t[loc + 5] = 0xFFFF;
}

/* 글자 그림 rows(H줄, NULL이면 H×(cw-2) 네모)를 임시 글꼴 그림 칸 (1, top)에 넣는다 */
static inline void SetGlyph(UInt16 *t, const UInt32 *rows, UInt16 H, UInt16 cw, UInt16 top, UInt16 fh)
{
    UInt16 y;
    UInt32 edge = 0xFFFFFFFFUL << (32 - (cw - 2));

    for (y = 0; y < H && y + top < fh; y++) {   /* 칸보다 긴 줄은 버림(12×12 글자를 11칸에: 12번째 줄은 한글에서 늘 빈 줄) */
        UInt32 v;

        if (rows != NULL)
            v = rows[y];
        else
            v = (y == 0 || y == H - 1) ? edge : (0x80000000UL | (0x80000000UL >> (cw - 3)));
        if (cw > H)
            v >>= 1;                             /* 왼쪽 1칸 띄움(v23: 칸이 글자 폭과 같은 24는 띄우지 않음). 줄은 32비트(왼쪽 = 비트 31) */
        t[13 + (y + top) * 2] = (UInt16)(v >> 16);
        t[13 + (y + top) * 2 + 1] = (UInt16)v;
    }
}

/* 글자 세로 시작 줄(칸 안으로 제한).
 * 16: 한글(1~13줄)을 영문 대문자 높이에 맞춤 = ascent − 15 (std 18 → 3, v6과 같음)
 * 12: 묶음의 기준선(위에서 10줄)을 글꼴 기준선에 맞춤 = ascent − 10
 * 24: 16의 1.5배 = ascent − 22 */
static inline UInt16 GlyphTop(const Int16 *f, UInt16 H)
{
    Int16 top = f[9] - (H == 24 ? 22 : H == 16 ? 15 : 10);   /* v22: 24는 16의 1.5배(대문자 높이 맞춤) */

    if (top < 0)
        top = 0;
    if (top > f[7] - (Int16)H)
        top = f[7] - (Int16)H;
    if (top < 0)
        top = 0;   /* v15: 칸이 글자보다 낮으면(11칸에 12×12) 맨 위부터 */
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
            UInt16  card;
            LocalID id;
            UInt32  crid = 0;

            if ((v & 0xFFFF) != 0xFFFF)
                FtrSet(kHgCreator, i, key | ((v & 0xFFFF) + 1));
            /* v15: 이 크기를 마지막으로 그린 앱의 크리에이터(#64~#71, 같은 순서) */
            if (SysCurAppDatabase(&card, &id) == errNone &&
                DmDatabaseInfo(card, id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &crid) == errNone)
                FtrSet(kHgCreator, i + 34, crid);
            return;
        }
    }
}

/* 2c: 라틴 구간·2바이트 글자로 나눠 그린다. pk = 이 칸 크기에 쓸 조합형 묶음 */
static inline UInt32 DrawGlyphs(BltFn orig, void *p1, const RectangleType *rc, void *p3,
                                const UInt8 *chars, UInt16 len, const Int16 *font, void *p7,
                                UInt16 ratio, UInt16 *tmp, const UInt8 *pk)
{
    RectangleType r = *rc;
    UInt16        i = 0, px = 0, n = 0;
    UInt16        H = pk[3], cw = (H == 24) ? kCell24 : (H == 16) ? kCell16 : kCell12, top = GlyphTop(font, H);
    UInt32        rows[24];
    UInt32        res = 0;
    UInt8         one = kTmpChar;

    MakeTmpFont(tmp, font, cw);
    while (i < len) {
        if (IsPair(chars, i, len)) {
            Boolean ok = GlyphRows(pk, chars[i], chars[i + 1], rows);

            SetGlyph(tmp, ok ? rows : NULL, H, cw, top, (UInt16)font[7]);
            r.topLeft.x = rc->topLeft.x + px / ratio;
            r.extent.x = cw / ratio;
            res = orig(p1, &r, p3, &one, 1, tmp, p7);
            px += cw;
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

/* v29 진단: 활성 창이 아닌 창(가상 실크 등)에 찍히는 글자. MnHangul 실행 중(#106)에는 기록하지 않는다.
 *   #108 횟수, #109 창 주소, #110 위치(x<<16|y), #111 길이<<16 | windowFlags, #112 글꼴(FontInfo)
 *   #113 글자 고리 위치, #114~#121 글자 고리(32바이트, 그린 글자열의 앞 4글자까지 이어 붙임)
 * v30: #126 오버레이 켜짐(1), #127 오버레이로 바꿔 그린 횟수. v31: #128 걸러진 이유, #129 창 flags·비트맵 폭 */
static inline void OtherLog(const UInt8 *chars, UInt16 len, const void *p2, const void *p6)
{
    const UInt8 *w = (const UInt8 *)WinGetDrawWindow();
    UInt32       pos, v;
    UInt16       i, n;

    if (w == NULL || (void *)w == (void *)WinGetActiveWindow() || GetF(106) != 0)
        return;
    FtrSet(kHgCreator, 108, GetF(108) + 1);
    FtrSet(kHgCreator, 109, (UInt32)w);
    FtrSet(kHgCreator, 110, Pair((const Int16 *)p2));
    FtrSet(kHgCreator, 111, ((UInt32)len << 16) | *(const UInt16 *)(w + 8));
    FtrSet(kHgCreator, 112, FontInfo(p6));
    pos = GetF(113);
    n = len < 4 ? len : 4;
    for (i = 0; chars != NULL && i < n; i++) {
        UInt16 k = (UInt16)((pos % 32) / 4), sh = (UInt16)(24 - 8 * (pos % 4));

        v = GetF(114 + k);
        v = (v & ~(0xFFUL << sh)) | ((UInt32)chars[i] << sh);
        FtrSet(kHgCreator, 114 + k, v);
        pos++;
    }
    FtrSet(kHgCreator, 113, pos);
}

/* v35: Sony 가상 키보드 두벌식 배치·한글 모드(⇪) 판정은 dubeol.h (입력 ime.c와 같은 표) */
#include "dubeol.h"

/* v36: 키 글자를 그리는 순간의 한글 판정. Graffiti 상태는 다시 그리기보다 한 박자 늦게 바뀌므로
 *   (v35 실측: ⇪를 켜면 대문자 키 글자를 먼저 그리고 그다음 ⇪ 상태가 켜짐) 키 글자 대·소문자와 함께 본다.
 *     대문자: ⇪ 켜짐 → 한글, ⇪ 꺼짐·일시 쉬프트 없음 → ⇪를 막 켬 → 한글, 일시 쉬프트 있음 → 영문(쉬프트·자동 쉬프트)
 *     소문자: ⇪ 켜짐 → 한글(⇪+쉬프트), ⇪ 꺼짐 → 영문
 *   진단: 판정 입력(대·소문자, g = caps num temp auto)이 바뀔 때마다 #134~#141 고리(#142 다음 칸)에 기록 */
static inline Boolean HanLabel(UInt8 c)
{
    Boolean caps = false, num = false, autoS = false;
    UInt16  temp = 0;
    UInt32  e, k;

    GrfGetState(&caps, &num, &temp, &autoS);
    e = ((UInt32)(c >= 'A' && c <= 'Z' ? 1 : 0) << 16) | ((caps ? 1 : 0) << 12) | ((num ? 1 : 0) << 8) |
        ((temp & 0xF) << 4) | (autoS ? 1 : 0);
    k = GetF(142);
    if (GetF(134 + (UInt16)((k + 7) & 7)) != (e | 0x80000000UL)) {
        FtrSet(kHgCreator, 134 + (UInt16)(k & 7), e | 0x80000000UL);
        FtrSet(kHgCreator, 142, k + 1);
    }
    /* v39: 한글 모드는 ⇪ 펜 누름으로 바뀐 #143. 상태가 ⇪ 켜짐이고 마지막 펜이 ⇪가 아니면 한글로 다시 맞춤
     *      (⇪를 막 눌렀을 때는 상태가 한 박자 늦어 맞추지 않는다) */
    if (caps && GetF(kFtrPenCaps) == 0 && GetF(kFtrHan) == 0)
        FtrSet(kHgCreator, kFtrHan, 1);
    return GetF(kFtrHan) != 0;
}

/* v31: 지금 그리는 곳이 화면 밖 창(0x4000, v29 실측 가상 실크 flags C000)인가. 비트맵 폭은 #129에 기록만 */
static inline Boolean SilkWin(void)
{
    const UInt8 *w = (const UInt8 *)WinGetDrawWindow();
    const UInt8 *bm;

    if (w == NULL)
        return false;
    bm = *(const UInt8 * const *)(w + 26);
    FtrSet(kHgCreator, 129, ((UInt32)*(const UInt16 *)(w + 8) << 16) | (bm != NULL ? *(const UInt16 *)bm : 0xFFFF));
    return (*(const UInt16 *)(w + 8) & 0x4000) != 0;
}

/* v31: 오버레이 후보(영문 한 글자, 활성 창 아닌 곳)가 걸러진 이유. #128 = 이유<<24 | 글꼴 높이<<8 | 사각형 높이
 *   1 오버레이 꺼짐, 2 16 글꼴 없음(Patch ON 전), 3 글꼴 높이 22 아님, 4 화면 밖 창 아님(#129 = flags<<16 | 비트맵 폭),
 *   5 임시 메모리 없음, 6 쉬프트 고정(⇪) 꺼짐 = 영문 모드(v35) */
static inline void OvlReject(UInt16 why, const Int16 *f, const Int16 *rc)
{
    FtrSet(kHgCreator, 128, ((UInt32)why << 24) | ((UInt32)(f[7] & 0xFF) << 8) | (UInt8)rc[3]);
}

UInt32 PatchBlt(void *p1, void *p2, void *p3, const UInt8 *chars, UInt16 len, void *p6, void *p7)
{
    UInt32 orig = OrigAddr();
    UInt16 i;
    Boolean hi = false;

    if (p2 != NULL)
        OtherLog(chars, len, p2, p6);   /* v29 진단 */
    /* v30: 오버레이(#126 = 1) — 가상 실크 창에 높이 22 글꼴로 찍는 영문 한 글자를 두벌식 자모로.
     * v31: 비트맵 폭 검사 뺌, 사각형 높이가 22·11이 아니면 배율 1, 걸러진 이유 #128 */
    if (orig != 0 && len == 1 && chars != NULL && p2 != NULL && p6 != NULL && Dubeol(chars[0]) != 0 &&
        (void *)WinGetDrawWindow() != (void *)WinGetActiveWindow() && GetF(106) == 0) {
        const Int16 *f = (const Int16 *)p6;
        Int16        rh = ((const Int16 *)p2)[3];
        UInt16      *tmp = NULL;

        if (GetF(126) != 1)
            OvlReject(1, f, (const Int16 *)p2);
        else if (GetF(23) == 0)
            OvlReject(2, f, (const Int16 *)p2);
        else if (f[7] != 22)
            OvlReject(3, f, (const Int16 *)p2);
        else if (!SilkWin())
            OvlReject(4, f, (const Int16 *)p2);
        else if (!HanLabel(chars[0]))
            OvlReject(6, f, (const Int16 *)p2);
        else if ((tmp = MemPtrNew(TmpWords(f[7]) * 2)) == NULL)
            OvlReject(5, f, (const Int16 *)p2);
        else {
            {
                UInt8         j[2];
                RectangleType r = *(const RectangleType *)p2;
                UInt16        ratio = (rh > 0 && f[7] == rh * 2) ? 2 : 1;
                Int16         wl = (Int16)CharW(f, chars[0]), wg = kCell16;
                UInt32        res;

                j[0] = 0xA4;
                j[1] = Dubeol(chars[0]);
                r.topLeft.x += (wl - wg) / 2 / (Int16)ratio;   /* 키 글자 가운데에 맞춤 */
                FtrSet(kHgCreator, 127, GetF(127) + 1);
                FtrSet(kHgCreator, 128, ((UInt32)(f[7] & 0xFF) << 8) | (UInt8)rh);   /* 이유 0 = 바꿔 그림 */
                res = DrawGlyphs((BltFn)orig, p1, &r, p3, j, 2, f, p7, ratio, tmp, (const UInt8 *)GetF(23));
                MemPtrFree(tmp);
                return res;
            }
        }
    }

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
        if (p2 != NULL && p6 != NULL) {
            Int16 fh0 = ((const Int16 *)p6)[7], rh0 = ((const Int16 *)p2)[3];

            CountSize(fh0, rh0);
            FidLog(kFidD, (UInt16)fh0);   /* v21 진단 */
            WinLog(fh0 == rh0 ? kWinSeen1 : fh0 == rh0 * 2 ? kWinSeen2 : kWinSeenX);   /* v16 진단 */
        }
    }
    if (orig == 0)
        return 0;
    if (hi && GetF(22) == 2 && p2 != NULL && p6 != NULL) {
        const Int16 *f = (const Int16 *)p6;
        Int16        rh = ((const Int16 *)p2)[3];
        const UInt8 *p16 = (const UInt8 *)GetF(23), *p12 = (const UInt8 *)GetF(24), *p24 = (const UInt8 *)GetF(92);

        Int16        fh = f[7];

        /* v7: 글꼴 높이 16~40, 배율(글꼴 높이 ÷ 사각형 높이) 1 또는 2. v14: 12~15는 12×12 묶음 */
        /* v15: 11칸도 12×12. v22: 28 이상(메모 큰 글자 30/15)은 추가 글꼴 24×24가 있으면 그것 */
        const UInt8 *pk = (fh >= 28 && p24 != NULL) ? p24 : (fh >= 16) ? p16 : (fh >= 11) ? p12 : NULL;

        if (fh <= 40 && pk != NULL && rh > 0 && (fh == rh || fh == rh * 2)) {
            UInt16 *tmp = MemPtrNew(TmpWords(fh) * 2);

            if (tmp != NULL) {
                UInt32 r = DrawGlyphs((BltFn)orig, p1, (const RectangleType *)p2, p3, chars, len, f, p7,
                                      (UInt16)(fh / rh), tmp, pk);

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

/* KS X 1001 완성형 순위(0~2349) → 유니코드 음절 번호(0~11171). 판정표 ksc_table.h(키보드와 같은 표) */
static UInt16 RankToSyl(UInt16 r)
{
    const UInt8  *bits;
    const UInt16 *cum;
    UInt16        b = 0, k, sy;

    asm("lea KscBits(%%pc),%0" : "=a" (bits));
    asm("lea KscCum(%%pc),%0" : "=a" (cum));
    while (b + 1 < 44 && cum[b + 1] <= r)
        b++;
    k = r - cum[b];
    for (sy = b * 256; sy < 11172; sy++)
        if (bits[sy >> 3] & (0x80 >> (sy & 7))) {
            if (k == 0)
                return sy;
            k--;
        }
    return 0;
}

/* 묶음 pk에서 음절 sy를 rows[]에 조합(특수 글자 → 예외표 → 벌 규칙). gen-johab.py compose()와 같은 방법 */
static void Compose(const UInt8 *pk, UInt16 sy, UInt32 *rows)
{
    UInt16 H = pk[3], Nc = pk[6], Nj = pk[7], w = RowBytes(pk), rb = H * w, i, k;
    UInt16 oP = RdW(pk + 112), oE = RdW(pk + 116), nE = RdW(pk + 118), oS = RdW(pk + 120), nS = RdW(pk + 122);
    UInt16 L = sy / 588, V = (sy % 588) / 28, T = sy % 28, hasT = T ? 1 : 0;
    UInt16 id[3];
    Int16  lo, hi;

    for (i = 0; i < H; i++)
        rows[i] = 0;
    lo = 0;
    hi = (Int16)nS - 1;
    while (lo <= hi) {                      /* 특수 글자(통째 그림) */
        Int16  mid = (lo + hi) / 2;
        UInt16 v = RdW(pk + oS + mid * (2 + rb));

        if (v == sy) {
            for (i = 0; i < H; i++)
                rows[i] = RdRow(pk, pk + oS + mid * (2 + rb) + 2 + i * w);
            return;
        }
        if (v < sy)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    id[0] = pk[10 + V * 2 + hasT] * 19 + L;
    id[1] = Nc * 19 + pk[52 + L * 2 + hasT] * 21 + V;
    id[2] = T ? Nc * 19 + Nj * 21 + pk[90 + V] * 27 + T - 1 : 0xFFFF;
    lo = 0;
    hi = (Int16)nE - 1;
    while (lo <= hi) {                      /* 예외표 */
        Int16  mid = (lo + hi) / 2;
        UInt16 v = RdW(pk + oE + mid * 8);

        if (v == sy) {
            for (k = 0; k < 3; k++)
                id[k] = RdW(pk + oE + mid * 8 + 2 + k * 2);
            break;
        }
        if (v < sy)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    for (k = 0; k < 3; k++)
        if (id[k] != 0xFFFF)
            for (i = 0; i < H; i++)
                rows[i] |= RdRow(pk, pk + oP + (UInt32)id[k] * rb + i * w);
}

/* 2바이트 글자 하나의 그림: 완성형 한글 → 조합, 자모 행(0xA4) → 묶음의 자모표, 그 밖 → NULL(네모) */
static Boolean GlyphRows(const UInt8 *pk, UInt8 lead, UInt8 trail, UInt32 *rows)
{
    UInt16 H = pk[3], w = RowBytes(pk), i;

    if (lead >= 0xB0 && lead <= 0xC8) {
        Compose(pk, RankToSyl((lead - 0xB0) * 94 + (trail - 0xA1)), rows);
        return true;
    }
    if (lead == 0xA4) {
        const UInt8 *j = pk + RdW(pk + 124) + (trail - 0xA1) * H * w;

        for (i = 0; i < H; i++)
            rows[i] = RdRow(pk, j + i * w);
        return true;
    }
    return false;
}

/* KS X 1001 판정표(키보드와 같은 표, tools/gen-ksc-table.py). .text 끝에 둔다(오프셋 0·4 배치 유지) */
#include "../skwr/ksc_table.h"
