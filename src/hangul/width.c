/*
 * MnHangul 폭 계산 코드 (리소스 HgWd 0) — 3b 단계: 폭 함수 트랩 5개를 2바이트 한글 폭으로 교체
 *
 * v8 실측에서 메모 편집 때 불린 폭 함수(트랩). 원형은 SDK Font.h와 같다.
 *   0 FntCharsWidth(chars, len)                           문자열 폭
 *   1 FntLineWidth(chars, len)                            문자열 폭(탭 포함)
 *   2 FntCharsInWidth(str, *width, *length, *fit)         주어진 폭에 들어가는 길이
 *   3 FntWordWrap(chars, maxWidth)                        한 줄 길이(줄바꿈 위치)
 *   4 FntWidthToOffset(chars, len, px, *leading, *trunc)  위치 → 글자 오프셋(커서)
 * 안전장치: 처리 범위에 0xA1~0xFE 바이트가 없으면(영문만) 원래 함수를 그대로 부른다.
 * 한글 2바이트 글자 폭(좌표 단위)은 그리기(patch.c) 결과와 같게(Intl 폭 intl.c도 같은 규칙):
 *   모드 #22 = glyph이고 폭 잴 때 글꼴 높이 8~15(저해상도, 그릴 때 2배 고해상도 글꼴) → 18 ÷ 2 = 9 (v12)
 *   글꼴 높이 16~40 → 18픽셀 ÷ 좌표 배율('W' 픽셀 폭 ÷ 원래 FntCharWidth('W'))
 *   box 모드 또는 작은 글꼴 → "[]" 폭, off → 두 바이트의 라틴 폭
 * 1바이트 글자는 원래 FntCharsWidth로 잰다. 탭은 fntTabChrWidth(20) 배수 위치로.
 * 줄바꿈: 공백 뒤에서 끊는 것을 우선하고, 공백이 없으면 글자 경계(2바이트를 자르지 않음)에서 끊는다.
 *
 * v11 진단 기록(feature 'MnHg'): #50~#54 함수별 호출 수, #56~#60 한글 경로 호출 수,
 *   #62 마지막 한글 폭 계산 = 폭<<16 | 배율<<8 | 글꼴 높이, #63 마지막 한글 경로 = 함수<<24 | 길이<<12 | 결과(12비트)
 *
 * 리소스 배치: 오프셋 0 = 진입점 5개의 오프셋 표, 20 = 원래 함수 주소 5개(앱이 DmWrite).
 * 어느 앱에서든 불리므로 전역변수·A5·재배치·문자열 상수 금지(빌드 스크립트가 검사).
 */

#include <PalmOS.h>

#define kHgCreator 'MnHg'
#include "winlog.h"
#define kGlyphW    18
#define kTab       20

Int16  HgCharsWidth(const Char *chars, Int16 len);
Int16  HgLineWidth(const Char *chars, UInt16 len);
void   HgCharsInWidth(const Char *str, Int16 *widthP, Int16 *lengthP, Boolean *fitP);
UInt16 HgWordWrap(const Char *chars, UInt16 maxWidth);
Int16  HgWidthToOffset(const Char *chars, UInt16 len, Int16 px, Boolean *leadingP, Int16 *truncP);

asm(".text\n.even\nWidthBase:\n"
    " .long HgCharsWidth-WidthBase, HgLineWidth-WidthBase, HgCharsInWidth-WidthBase\n"
    " .long HgWordWrap-WidthBase, HgWidthToOffset-WidthBase\n"
    "WidthOrig:\n .long 0,0,0,0,0\n");

typedef Int16  (*CharsWidthFn)(const Char *, Int16);
typedef Int16  (*LineWidthFn)(const Char *, UInt16);
typedef void   (*CharsInWidthFn)(const Char *, Int16 *, Int16 *, Boolean *);
typedef UInt16 (*WordWrapFn)(const Char *, UInt16);
typedef Int16  (*WidthToOffsetFn)(const Char *, UInt16, Int16, Boolean *, Int16 *);

static inline UInt32 Orig(UInt16 i)
{
    const UInt32 *p;

    asm("lea WidthOrig(%%pc),%0" : "=a" (p));
    return p[i];
}

static inline Boolean Hi(UInt8 b)
{
    return b >= 0xA1 && b <= 0xFE;
}

static inline Boolean HasHi(const UInt8 *t, UInt32 n)
{
    UInt32 i;

    for (i = 0; i < n; i++)
        if (Hi(t[i]))
            return true;
    return false;
}

static inline UInt32 Ftr(UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(kHgCreator, n, &v) != errNone)
        v = 0;
    return v;
}

static inline void Bump(UInt16 n)
{
    FtrSet(kHgCreator, n, Ftr(n) + 1);
}

static inline void Last(UInt16 fn, UInt32 len, Int16 res)
{
    FtrSet(kHgCreator, 63, ((UInt32)fn << 24) | ((len & 0xFFF) << 12) | ((UInt16)res & 0xFFF));
}

/* 1바이트 글자 폭(원래 FntCharsWidth) */
static inline Int16 W1(UInt8 c)
{
    Char b = (Char)c;

    return ((CharsWidthFn)Orig(0))(&b, 1);
}

/* 2바이트 글자 폭. 호출마다 한 번만 계산해 쓴다 */
static Int16 HangulW(UInt8 lead, UInt8 trail)
{
    const Int16 *f;
    UInt32       mode = Ftr(22);
    Char         b[2];

    if (mode == 0) {
        b[0] = (Char)lead;
        b[1] = (Char)trail;
        return ((CharsWidthFn)Orig(0))(b, 2);
    }
    f = (const Int16 *)FntGetFontPtr();
    /* v12: 폭을 잴 때는 저해상도 글꼴(높이 8~15)인데 그릴 때 Sony가 2배 고해상도 글꼴로 바꿔 넘긴다(v11 실측:
     * 폭 잴 때 11, 그릴 때 22 ×2). 그러면 그리기는 16×16 한글을 배율 2로 그리므로 폭 = 18 ÷ 2 */
    if (mode == 2 && f != NULL && f[7] >= 8 && f[7] < 16 && Ftr(23) != 0) {
        WinLog(kWinSeenW);   /* v16 진단: 이 창이 그릴 때 ×1인지 ×2인지 비교 */
        if (WinOffscreen() && Ftr(24) != 0) {   /* v17: offscreen 창 = ×1, 12×12 칸 */
            FtrSet(kHgCreator, 62, ((UInt32)kCell12W << 16) | (1 << 8) | (UInt8)f[7]);
            return kCell12W;
        }
        FtrSet(kHgCreator, 62, ((UInt32)(kGlyphW / 2) << 16) | (2 << 8) | (UInt8)f[7]);
        return kGlyphW / 2;
    }
    if (mode == 2 && f != NULL && f[7] >= 16 && f[7] <= 40 && Ftr(23) != 0) {
        const UInt16 *owt = (const UInt16 *)((const UInt8 *)f + 16 + (UInt16)f[8] * 2);
        Int16         first = f[1], last = f[2], coord = W1('W');
        UInt16        px = 0, ratio = 1;

        if ('W' >= first && 'W' <= last && owt['W' - first] != 0xFFFF)
            px = owt['W' - first] & 0xFF;
        if (coord > 0 && px >= (UInt16)coord * 2 - 1)
            ratio = 2;
        FtrSet(kHgCreator, 62, ((UInt32)((kGlyphW + ratio - 1) / ratio) << 16) | (ratio << 8) | (UInt8)f[7]);
        return (kGlyphW + ratio - 1) / ratio;
    }
    b[0] = '[';
    b[1] = ']';
    FtrSet(kHgCreator, 62, ((UInt32)((CharsWidthFn)Orig(0))(b, 2) << 16) | (f != NULL ? (UInt8)f[7] : 0));
    return ((CharsWidthFn)Orig(0))(b, 2);
}

/* t[i]부터 한 글자: 크기 반환, *w = 폭(x = 줄 안 현재 위치, 탭 계산용). hw < 0이면 2바이트 폭을 계산해 채움 */
static inline UInt16 Step(const UInt8 *t, UInt32 i, UInt32 n, Int16 x, Int16 *hw, Int16 *w)
{
    UInt8 c = t[i];

    if (Hi(c) && i + 1 < n && Hi(t[i + 1])) {
        if (*hw < 0)
            *hw = HangulW(c, t[i + 1]);
        *w = *hw;
        return 2;
    }
    *w = (c == '\t') ? (kTab - (x % kTab)) : W1(c);
    return 1;
}

/* 2바이트 글자를 고려한 폭. tabs = 탭을 칸 맞춤으로 셈(FntLineWidth) */
static Int16 Measure(const UInt8 *t, UInt32 n, Boolean tabs)
{
    Int16  x = 0, hw = -1, w;
    UInt32 i = 0, run = 0;

    while (i < n) {
        if (!Hi(t[i]) && !(tabs && t[i] == '\t')) {   /* 1바이트 구간은 묶어서 원래 함수로 */
            run = i;
            while (i < n && !Hi(t[i]) && !(tabs && t[i] == '\t'))
                i++;
            x += ((CharsWidthFn)Orig(0))((const Char *)t + run, (Int16)(i - run));
            continue;
        }
        i += Step(t, i, n, x, &hw, &w);
        x += w;
    }
    return x;
}

Int16 HgCharsWidth(const Char *chars, Int16 len)
{
    Int16 r;

    Bump(50);
    if (chars == NULL || len <= 0 || !HasHi((const UInt8 *)chars, len))
        return ((CharsWidthFn)Orig(0))(chars, len);
    Bump(56);
    r = Measure((const UInt8 *)chars, len, false);
    Last(0, len, r);
    return r;
}

Int16 HgLineWidth(const Char *chars, UInt16 len)
{
    Int16 r;

    Bump(51);
    if (chars == NULL || len == 0 || !HasHi((const UInt8 *)chars, len))
        return ((LineWidthFn)Orig(1))(chars, len);
    Bump(57);
    r = Measure((const UInt8 *)chars, len, true);
    Last(1, len, r);
    return r;
}

void HgCharsInWidth(const Char *str, Int16 *widthP, Int16 *lengthP, Boolean *fitP)
{
    const UInt8 *t = (const UInt8 *)str;
    Int16        maxW, x = 0, hw = -1, w;
    UInt32       n, i = 0;
    Boolean      fit = true;

    Bump(52);
    if (str == NULL || widthP == NULL || lengthP == NULL || *lengthP <= 0 ||
        !HasHi(t, (UInt32)*lengthP)) {
        ((CharsInWidthFn)Orig(2))(str, widthP, lengthP, fitP);
        return;
    }
    Bump(58);
    maxW = *widthP;
    n = (UInt32)*lengthP;
    while (i < n) {
        UInt16 sz;

        if (t[i] == '\n' || t[i] == 0) {
            fit = false;
            break;
        }
        sz = Step(t, i, n, x, &hw, &w);
        if (x + w > maxW) {
            fit = false;
            break;
        }
        x += w;
        i += sz;
    }
    while (i > 0 && t[i - 1] == ' ') {   /* 끝 공백은 길이·폭에서 뺀다 */
        i--;
        x -= W1(' ');
    }
    *widthP = x;
    *lengthP = (Int16)i;
    Last(2, i, x);
    if (fitP != NULL)
        *fitP = fit;
}

UInt16 HgWordWrap(const Char *chars, UInt16 maxWidth)
{
    const UInt8 *t = (const UInt8 *)chars;
    UInt16       l;
    Int16        x = 0, hw = -1, w;
    UInt32       i = 0, brk = 0;

    Bump(53);
    if (chars == NULL)
        return 0;
    l = ((WordWrapFn)Orig(3))(chars, maxWidth);
    if (!HasHi(t, (UInt32)l + 1))   /* 이 줄과 다음 한 바이트에 한글이 없으면 원래 결과 */
        return l;
    Bump(59);
    while (t[i] != 0) {
        UInt16 sz;

        if (t[i] == '\n')
            return (UInt16)(i + 1);
        sz = Step(t, i, 0xFFFFFFFFUL, x, &hw, &w);
        if (x + w > (Int16)maxWidth) {
            if (t[i] == ' ') {   /* 넘치는 공백은 이 줄에 붙인다 */
                while (t[i] == ' ')
                    i++;
                if (t[i] == '\n')
                    i++;
                return (UInt16)i;
            }
            if (brk > 0)
                return (UInt16)brk;
            return (UInt16)((i > 0) ? i : sz);   /* 한 단어가 줄보다 길면 글자 경계에서 */
        }
        x += w;
        i += sz;
        if (t[i - 1] == ' ' && sz == 1)
            brk = i;
    }
    return (UInt16)i;
}

Int16 HgWidthToOffset(const Char *chars, UInt16 len, Int16 px, Boolean *leadingP, Int16 *truncP)
{
    const UInt8 *t = (const UInt8 *)chars;
    Int16        x = 0, hw = -1, w;
    UInt32       i = 0;

    Bump(54);
    if (chars == NULL || len == 0 || !HasHi(t, len))
        return ((WidthToOffsetFn)Orig(4))(chars, len, px, leadingP, truncP);
    Bump(60);
    while (i < len) {
        UInt16 sz = Step(t, i, len, x, &hw, &w);

        if (px < x + w) {
            if (leadingP != NULL)
                *leadingP = (px - x) < (w + 1) / 2;
            if (truncP != NULL)
                *truncP = x;
            return (Int16)i;
        }
        x += w;
        i += sz;
    }
    if (leadingP != NULL)
        *leadingP = false;
    if (truncP != NULL)
        *truncP = x;
    return (Int16)len;
}
