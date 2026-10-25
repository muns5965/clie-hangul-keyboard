/*
 * MnHangul 글자 경계 코드 (리소스 HgIt 0) — 3a 단계: Intl 루틴 8개를 EUC-KR(KS X 1001) 규칙으로 교체
 *
 * v8 실측(메모 입력·BS·커서 이동)에서 불린 Intl 루틴 중 글자 경계·속성·폭 6개를 바꾼다.
 * 앱이 IntlSetRoutineAddress로 끼운다. 원형은 SDK TextMgr.h와 같다(Intl 디스패처가 스택 그대로 넘김).
 *   0 TxtCharAttr(WChar)                      2바이트 → charAttr_XA(글자), 1바이트 → 원래 루틴
 *   1 TxtGetPreviousChar(text, offset, *ch)   앞 글자 크기(1·2)
 *   2 TxtGetNextChar(text, offset, *ch)       다음 글자 크기(1·2)
 *   3 TxtSetNextChar(text, offset, ch)        ch > 0xFF면 2바이트로 기록
 *   4 TxtCharBounds(text, offset, *s, *e)     offset이 든 글자의 시작·끝
 *   5 TxtCharWidth(WChar)                     2바이트 → 한글 폭(현재 글꼴·좌표 배율), 1바이트 → 원래 루틴
 *   6 TxtFindString(src, tgt, *pos, *len)     찾기(v71, MnHangul5 v10과 같은 규칙): 원문에 한글이 있으면 부분 검색,
 *                                             없으면 원래 결과 중 앞 바이트가 0xA1~0xFE인 것을 버림
 *   7 TxtWordBounds(text, len, off, *s, *e)  두 번 눌러 낱말 선택(v72, MnHangul5 v11과 같은 규칙): 한글 낱말 단위
 * 2바이트 글자: 앞·뒤 바이트 모두 0xA1~0xFE. 상위 바이트가 이어진 구간은 구간 시작부터 두 바이트씩 짝짓는다
 * (홀수 구간이면 마지막 바이트가 1바이트 글자).
 *
 * 리소스 배치: 오프셋 0 = 진입점 8개의 오프셋 표(.long, 리소스 기준), 32 = 원래 루틴 주소 8개(앱이 DmWrite).
 * 어느 앱에서든 불리므로 전역변수·A5·재배치·문자열 상수 금지(빌드 스크립트가 검사).
 * 한글 폭은 patch.c 그리기 결과와 같게(모드 #22): glyph 모드이고 글꼴 높이 8~15(저해상도)면 9(v12),
 * 16~40이면 18픽셀(좌표 배율로 나눔),
 * box 모드(또는 작은 글꼴)면 "[]" 폭, off면 두 바이트의 라틴 폭. 좌표 배율 = 'W'의 글꼴 픽셀 폭 ÷ FntCharWidth('W').
 */

#include <PalmOS.h>

#define kHgCreator 'MnHg'
#include "winlog.h"
#include "../common/diag.h"
#define kGlyphW    18

UInt16 HgCharAttr(WChar c);
UInt16 HgPrevChar(const Char *t, UInt32 o, WChar *ch);
UInt16 HgNextChar(const Char *t, UInt32 o, WChar *ch);
UInt16 HgSetNext(Char *t, UInt32 o, WChar c);
WChar  HgCharBounds(const Char *t, UInt32 o, UInt32 *s, UInt32 *e);
Int16  HgCharWidth(WChar c);
Boolean HgFindString(const Char *src, const Char *tgt, UInt32 *pos, UInt16 *len);
Boolean HgWordBounds(const Char *text, UInt32 n, UInt32 o, UInt32 *s, UInt32 *e);

asm(".text\n.even\nIntlBase:\n"
    " .long HgCharAttr-IntlBase, HgPrevChar-IntlBase, HgNextChar-IntlBase\n"
    " .long HgSetNext-IntlBase, HgCharBounds-IntlBase, HgCharWidth-IntlBase\n"
    " .long HgFindString-IntlBase, HgWordBounds-IntlBase\n"
    "IntlOrig:\n .long 0,0,0,0,0,0,0,0\n");

typedef UInt16 (*AttrFn)(WChar c);
typedef Int16  (*WidthFn)(WChar c);
typedef Boolean (*FindFn)(const Char *, const Char *, UInt32 *, UInt16 *);
typedef Boolean (*WordFn)(const Char *, UInt32, UInt32, UInt32 *, UInt32 *);

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

#if MN_DIAG
    FtrSet(kHgCreator, 55, Ftr(55) + 1);   /* v11 진단: 호출 수, 2바이트 호출 수 #61 */
    if (c > 0xFF)
        FtrSet(kHgCreator, 61, Ftr(61) + 1);
#endif
    if (c <= 0xFF)
        return Orig(5) ? ((WidthFn)Orig(5))(c) : FntCharWidth((Char)c);
    if (mode == 0) {   /* 그리기 패치가 꺼진 모드: 두 바이트가 라틴 글자 둘로 그려짐 */
        box[0] = (Char)(c >> 8);
        box[1] = (Char)c;
        return FntCharsWidth(box, 2);
    }
    f = (const Int16 *)FntGetFontPtr();
    if (f != NULL)
        MN_D(FidLog(kFidW, (UInt16)f[7]));   /* v21 진단 */
    if (mode == 2 && f != NULL && f[7] >= 8 && f[7] < 16 && Ftr(23) != 0) {
        MN_D(WinLog(kWinSeenW));   /* v16 진단 */
        if (WinOffscreen() && Ftr(24) != 0)
            return kCell12W;   /* v17: offscreen 창 = ×1, 12×12 칸(width.c와 같은 규칙) */
        if (f[7] >= 14 && Ftr(92) != 0)
            return kCell24W / 2;   /* v22: 큰 글자 → 그릴 때 30 ×2, 24×24 */
        return kGlyphW / 2;
    }   /* v12: 저해상도 글꼴 → 그릴 때 2배 고해상도(width.c와 같은 규칙) */
    if (mode == 2 && f != NULL && f[7] >= 16 && f[7] <= 40 && Ftr(23) != 0) {
        Int16  coord = FntCharWidth('W');
        UInt16 px = PixW(f, 'W'), ratio = 1;

        if (coord > 0 && px >= (UInt16)coord * 2 - 1)
            ratio = 2;
        if (f[7] >= 28 && Ftr(92) != 0)
            return (kCell24W + ratio - 1) / ratio;   /* v22 */
        return (kGlyphW + ratio - 1) / ratio;
    }
    box[0] = '[';
    box[1] = ']';
    return FntCharsWidth(box, 2);
}

/* TxtFindString(찾기, v71). 검색어는 TxtPrepFindString이 바이트마다 1:1로 바꾼 값이라(실측: 가 B0 A1 → 6C 4A)
 * 직접 비교하지 않고 원래 루틴의 비교를 쓴다. 원래 루틴은 낱말 첫머리를 앞 바이트 하나로 판단해
 * 한글 앞 글자의 끝 바이트가 Latin-1 기호면 낱말 가운데도 맞춘다(갚가에서 가).
 * - 원문에 한글 쌍이 있고 검색어가 64바이트 미만: 글자 경계마다 검색어 길이만큼 잘라 원래 루틴에 넘기고
 *   맨 앞(0)에서 맞을 때만 받아들인다(부분 검색: 가나다에서 나).
 * - 그 밖: 원래 결과 중 앞 바이트가 0xA1~0xFE인 것을 버리고 다음 바이트부터 다시 찾는다.
 * MnHangul5 v10(src/os5/text.c)과 같은 규칙. */
Boolean HgFindString(const Char *src, const Char *tgt, UInt32 *pos, UInt16 *len)
{
    const UInt8 *t = (const UInt8 *)src, *g = (const UInt8 *)tgt;
    FindFn       orig = (FindFn)Orig(6);
    Char         buf[64];
    UInt32       nt = 0, i = 0, k, p, base = 0;
    UInt16       l;
    Boolean      hi = false;

    if (orig == NULL)
        return false;
    if (src == NULL || tgt == NULL)
        return orig(src, tgt, pos, len);
    while (g[nt] && nt < 64)
        nt++;
    for (k = 0; t[k]; k++)
        if (Hi(t[k]) && Hi(t[k + 1])) {
            hi = true;
            break;
        }
    if (hi && nt > 0 && nt < 64) {
        while (t[i]) {
            for (k = 0; k < nt && t[i + k]; k++)
                buf[k] = (Char)t[i + k];
            if (k < nt)
                return false;
            buf[k] = 0;
            if (orig(buf, tgt, &p, &l) && p == 0) {
                if (pos) *pos = i;
                if (len) *len = l;
                return true;
            }
            i += (Hi(t[i]) && Hi(t[i + 1])) ? 2 : 1;
        }
        return false;
    }
    for (;;) {
        if (!orig(src + base, tgt, &p, &l))
            return false;
        p += base;
        if (p == 0 || !Hi(t[p - 1])) {
            if (pos) *pos = p;
            if (len) *len = l;
            return true;
        }
        base = p + 1;
    }
}

/* TxtWordBounds(두 번 눌러 낱말 선택, v72). 원래 루틴은 바이트를 Latin-1 글자로 분류해 한글 글자 가운데에서 낱말을 자른다
 * (가나다에서 나를 두 번 누르면 1바이트 선택 → 지우면 한글이 깨짐). 문자열에 한글 쌍이 있고 누른 글자가 낱말 글자
 * (2바이트 글자 또는 원래 TxtCharAttr로 영문자·숫자)면 좌우로 낱말 글자가 이어지는 데까지 넓힌다. 그 밖은 원래 루틴 그대로.
 * MnHangul5 v11(src/os5/text.c)과 같은 규칙. */
static Boolean Word1(UInt8 c)
{
    return Orig(0) ? ((((AttrFn)Orig(0))(c) & charAttrAlNum) != 0) : false;
}

Boolean HgWordBounds(const Char *text, UInt32 n, UInt32 o, UInt32 *s, UInt32 *e)
{
    const UInt8 *t = (const UInt8 *)text;
    WordFn       orig = (WordFn)Orig(7);
    UInt32       i, a, b, k;
    Boolean      two = false, any = false;

    if (orig == NULL)
        return false;
    if (text == NULL || o >= n)
        return orig(text, n, o, s, e);
    for (i = 0; i + 1 < n; i++)
        if (Hi(t[i]) && Hi(t[i + 1])) {
            any = true;
            break;
        }
    if (!any)
        return orig(text, n, o, s, e);
    a = o;
    if (Hi(t[o])) {   /* 상위 바이트 구간은 구간 시작부터 두 바이트씩(HgCharBounds와 같은 규칙) */
        k = RunBefore(t, o);
        a = o - (k & 1);
        if (a + 1 < n && Hi(t[a]) && Hi(t[a + 1]))
            two = true;
        else
            a = o;
    }
    if (!two && !Word1(t[a]))
        return orig(text, n, o, s, e);
    b = a + (two ? 2 : 1);
    while (a > 0) {   /* 왼쪽: 앞 글자가 2바이트(구간 안 짝수 자리)면 2, 아니면 1바이트 글자 */
        k = RunBefore(t, a);
        if (k >= 2 && !(k & 1)) {
            a -= 2;
            continue;
        }
        if (Word1(t[a - 1])) {
            a--;
            continue;
        }
        break;
    }
    while (b < n && t[b]) {   /* 오른쪽 */
        if (b + 1 < n && Hi(t[b]) && Hi(t[b + 1])) {
            b += 2;
            continue;
        }
        if (Word1(t[b])) {
            b++;
            continue;
        }
        break;
    }
    if (s) *s = a;
    if (e) *e = b;
    return true;
}
