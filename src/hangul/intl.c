/*
 * MnHangul 글자 경계 코드 (리소스 HgIt 0) — 3a 단계: Intl 루틴 6개를 EUC-KR(KS X 1001) 규칙으로 교체
 *
 * v8 실측(메모 입력·BS·커서 이동)에서 불린 Intl 루틴 중 글자 경계·속성·폭 6개를 바꾼다.
 * 앱이 IntlSetRoutineAddress로 끼운다. 원형은 SDK TextMgr.h와 같다(Intl 디스패처가 스택 그대로 넘김).
 *   0 TxtCharAttr(WChar)                      2바이트 → charAttr_XA(글자), 1바이트 → 원래 루틴
 *   1 TxtGetPreviousChar(text, offset, *ch)   앞 글자 크기(1·2)
 *   2 TxtGetNextChar(text, offset, *ch)       다음 글자 크기(1·2)
 *   3 TxtSetNextChar(text, offset, ch)        ch > 0xFF면 2바이트로 기록
 *   4 TxtCharBounds(text, offset, *s, *e)     offset이 든 글자의 시작·끝
 *   5 TxtCharWidth(WChar)                     2바이트 → 한글 폭(현재 글꼴·좌표 배율), 1바이트 → 원래 루틴
 * 2바이트 글자: 앞·뒤 바이트 모두 0xA1~0xFE. 상위 바이트가 이어진 구간은 구간 시작부터 두 바이트씩 짝짓는다
 * (홀수 구간이면 마지막 바이트가 1바이트 글자).
 *
 * 리소스 배치: 오프셋 0 = 진입점 6개의 오프셋 표(.long, 리소스 기준), 24 = 원래 루틴 주소 6개(앱이 DmWrite).
 * 어느 앱에서든 불리므로 전역변수·A5·재배치·문자열 상수 금지(빌드 스크립트가 검사).
 * 한글 폭은 patch.c 그리기 결과와 같게(모드 #22): glyph 모드이고 글꼴 높이 8~15(저해상도)면 9(v12),
 * 16~40이면 18픽셀(좌표 배율로 나눔),
 * box 모드(또는 작은 글꼴)면 "[]" 폭, off면 두 바이트의 라틴 폭. 좌표 배율 = 'W'의 글꼴 픽셀 폭 ÷ FntCharWidth('W').
 */

#include <PalmOS.h>

#define kHgCreator 'MnHg'
#include "winlog.h"
#define kGlyphW    18

UInt16 HgCharAttr(WChar c);
UInt16 HgPrevChar(const Char *t, UInt32 o, WChar *ch);
UInt16 HgNextChar(const Char *t, UInt32 o, WChar *ch);
UInt16 HgSetNext(Char *t, UInt32 o, WChar c);
WChar  HgCharBounds(const Char *t, UInt32 o, UInt32 *s, UInt32 *e);
Int16  HgCharWidth(WChar c);

asm(".text\n.even\nIntlBase:\n"
    " .long HgCharAttr-IntlBase, HgPrevChar-IntlBase, HgNextChar-IntlBase\n"
    " .long HgSetNext-IntlBase, HgCharBounds-IntlBase, HgCharWidth-IntlBase\n"
    "IntlOrig:\n .long 0,0,0,0,0,0\n");

typedef UInt16 (*AttrFn)(WChar c);
typedef Int16  (*WidthFn)(WChar c);

static inline UInt32 Orig(UInt16 i)
{
    const UInt32 *p;

    asm("lea IntlOrig(%%pc),%0" : "=a" (p));
    return p[i];
}

static inline Boolean Hi(UInt8 b)
{
    return b >= 0xA1 && b <= 0xFE;
}

/* offset 이전으로 이어진 상위 바이트 수(offset 바로 앞 바이트부터 뒤로) */
static inline UInt32 RunBefore(const UInt8 *t, UInt32 o)
{
    UInt32 k = 0;

    while (o > 0 && Hi(t[o - 1])) {
        o--;
        k++;
    }
    return k;
}

UInt16 HgCharAttr(WChar c)
{
    if (c > 0xFF)
        return charAttr_XA;
    return Orig(0) ? ((AttrFn)Orig(0))(c) : 0;
}

UInt16 HgPrevChar(const Char *text, UInt32 o, WChar *ch)
{
    const UInt8 *t = (const UInt8 *)text;
    UInt16       n;

    if (o == 0) {
        if (ch != NULL)
            *ch = 0;
        return 0;
    }
    n = (o >= 2 && Hi(t[o - 1]) && (RunBefore(t, o) & 1) == 0) ? 2 : 1;
    if (ch != NULL)
        *ch = (n == 2) ? (((WChar)t[o - 2] << 8) | t[o - 1]) : t[o - 1];
    return n;
}

UInt16 HgNextChar(const Char *text, UInt32 o, WChar *ch)
{
    const UInt8 *t = (const UInt8 *)text;
    UInt16       n = (Hi(t[o]) && Hi(t[o + 1])) ? 2 : 1;

    if (ch != NULL)
        *ch = (n == 2) ? (((WChar)t[o] << 8) | t[o + 1]) : t[o];
    return n;
}

UInt16 HgSetNext(Char *text, UInt32 o, WChar c)
{
    UInt8 *t = (UInt8 *)text;

    if (c > 0xFF) {
        if (t != NULL) {
            t[o] = (UInt8)(c >> 8);
            t[o + 1] = (UInt8)c;
        }
        return 2;
    }
    if (t != NULL)
        t[o] = (UInt8)c;
    return 1;
}

WChar HgCharBounds(const Char *text, UInt32 o, UInt32 *s, UInt32 *e)
{
    const UInt8 *t = (const UInt8 *)text;
    UInt32       start = o, end = o + 1;
    WChar        c = t[o];

    if (Hi(t[o])) {
        UInt32 k = RunBefore(t, o);   /* 같은 구간에서 o 앞의 바이트 수 */

        start = o - (k & 1);
        if (Hi(t[start]) && Hi(t[start + 1])) {
            end = start + 2;
            c = ((WChar)t[start] << 8) | t[start + 1];
        } else {
            start = o;
            end = o + 1;
        }
    }
    if (s != NULL)
        *s = start;
    if (e != NULL)
        *e = end;
    return c;
}

/* 글꼴 f에서 1바이트 글자 c의 폭(픽셀), SDK FontType 구조(v4 실측) */
static inline UInt16 PixW(const Int16 *f, UInt8 c)
{
    const UInt16 *owt = (const UInt16 *)((const UInt8 *)f + 16 + (UInt16)f[8] * 2);
    Int16         first = f[1], last = f[2];
    UInt16        v;

    if ((Int16)c < first || (Int16)c > last)
        v = owt[last - first + 1];
    else {
        v = owt[c - first];
        if (v == 0xFFFF)
            v = owt[last - first + 1];
    }
    return v & 0xFF;
}

static inline UInt32 Ftr(UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(kHgCreator, n, &v) != errNone)
        v = 0;
    return v;
}

Int16 HgCharWidth(WChar c)
{
    const Int16 *f;
    UInt32       mode = Ftr(22);
    Char         box[2];

    FtrSet(kHgCreator, 55, Ftr(55) + 1);   /* v11 진단: 호출 수, 2바이트 호출 수 #61 */
    if (c > 0xFF)
        FtrSet(kHgCreator, 61, Ftr(61) + 1);
    if (c <= 0xFF)
        return Orig(5) ? ((WidthFn)Orig(5))(c) : FntCharWidth((Char)c);
    if (mode == 0) {   /* 그리기 패치가 꺼진 모드: 두 바이트가 라틴 글자 둘로 그려짐 */
        box[0] = (Char)(c >> 8);
        box[1] = (Char)c;
        return FntCharsWidth(box, 2);
    }
    f = (const Int16 *)FntGetFontPtr();
    if (f != NULL)
        FidLog(kFidW, (UInt16)f[7]);   /* v21 진단 */
    if (mode == 2 && f != NULL && f[7] >= 8 && f[7] < 16 && Ftr(23) != 0) {
        WinLog(kWinSeenW);   /* v16 진단 */
        if (WinOffscreen() && Ftr(24) != 0)
            return kCell12W;   /* v17: offscreen 창 = ×1, 12×12 칸(width.c와 같은 규칙) */
        return kGlyphW / 2;
    }   /* v12: 저해상도 글꼴 → 그릴 때 2배 고해상도(width.c와 같은 규칙) */
    if (mode == 2 && f != NULL && f[7] >= 16 && f[7] <= 40 && Ftr(23) != 0) {
        Int16  coord = FntCharWidth('W');
        UInt16 px = PixW(f, 'W'), ratio = 1;

        if (coord > 0 && px >= (UInt16)coord * 2 - 1)
            ratio = 2;
        return (kGlyphW + ratio - 1) / ratio;
    }
    box[0] = '[';
    box[1] = ']';
    return FntCharsWidth(box, 2);
}
