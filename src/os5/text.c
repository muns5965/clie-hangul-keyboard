/*
 * MnHangul5 글자 경계 — 표 8의 Text Manager 함수 11개를 바꾼다(MnHangul src/hangul/intl.c와 같은 규칙).
 * 칸 번호는 ROM IntlDispatch 분기표로 확인했다(docs/OS5_RESEARCH.md).
 * WChar 출력 인자는 16비트, TxtCharBounds의 시작·끝은 32비트.
 */
#include "os5.h"

typedef u16 (*U8Fn)(u8);
typedef u16 (*WCharFn)(WChar);
typedef u16 (*GetNextFn)(const char *, u32, WChar *);
typedef u16 (*SetNextFn)(char *, u32, WChar);
typedef WChar (*BoundsFn)(const char *, u32, u32 *, u32 *);
typedef WChar (*GetCharFn)(const char *, u32);
typedef Boolean (*FindFn)(const char *, const char *, u32 *, u16 *);

#define byteAttrSingle 0x01
#define byteAttrLast 0x40
#define byteAttrFirst 0x80
#define charAttr_XA 0x0200

static u32 RunBefore(const u8 *t, u32 o) {
    u32 k = 0;
    while (o > 0 && Hi(t[o - 1])) { o--; k++; }
    return k;
}

u16 HgByteAttr(u8 b) {
    if (!Idle() && Hi(b)) return byteAttrFirst | byteAttrLast | byteAttrSingle;
    return ((U8Fn)ORIG(O_ByteAttr))(b);
}

u16 HgCharSize(WChar c) {
    if (!Idle() && c > 0xFF) return 2;
    return ((WCharFn)ORIG(O_CharSize))(c);
}

u16 HgCharAttr(WChar c) {
    if (!Idle() && c > 0xFF) return charAttr_XA;
    return ((WCharFn)ORIG(O_CharAttr))(c);
}

u16 HgCharIsValid(WChar c) {
    if (!Idle() && c > 0xFF) return Hi(c >> 8) && Hi(c & 0xFF);
    return ((WCharFn)ORIG(O_IsValid))(c);
}

u16 HgCharWidth(WChar c) {
    if (!Idle() && c > 0xFF) return HangulCell();
    return ((WCharFn)ORIG(O_CharWidth))(c);
}

u16 HgGetPrev(const char *text, u32 o, WChar *ch) {
    const u8 *t = (const u8 *)text;
    u16 n;
    if (Idle()) return ((GetNextFn)ORIG(O_GetPrev))(text, o, ch);
    if (o == 0) {
        if (ch) *ch = 0;
        return 0;
    }
    n = (o >= 2 && Hi(t[o - 1]) && (RunBefore(t, o) & 1) == 0) ? 2 : 1;
    if (ch) *ch = (n == 2) ? (WChar)((t[o - 2] << 8) | t[o - 1]) : t[o - 1];
    return n;
}

u16 HgGetNext(const char *text, u32 o, WChar *ch) {
    const u8 *t = (const u8 *)text;
    u16 n;
    if (Idle()) return ((GetNextFn)ORIG(O_GetNext))(text, o, ch);
    n = (Hi(t[o]) && Hi(t[o + 1])) ? 2 : 1;
    if (ch) *ch = (n == 2) ? (WChar)((t[o] << 8) | t[o + 1]) : t[o];
    return n;
}

WChar HgGetChar(const char *text, u32 o) {
    WChar c;
    if (Idle()) return ((GetCharFn)ORIG(O_GetChar))(text, o);
    HgGetNext(text, o, &c);
    return c;
}

u16 HgSetNext(char *text, u32 o, WChar c) {
    u8 *t = (u8 *)text;
    if (Idle()) return ((SetNextFn)ORIG(O_SetNext))(text, o, c);
    if (c > 0xFF) {
        if (t) { t[o] = (u8)(c >> 8); t[o + 1] = (u8)c; }
        return 2;
    }
    if (t) t[o] = (u8)c;
    return 1;
}

WChar HgCharBounds(const char *text, u32 o, u32 *s, u32 *e) {
    const u8 *t = (const u8 *)text;
    u32 start = o, end = o + 1;
    WChar c;
    if (Idle()) return ((BoundsFn)ORIG(O_Bounds))(text, o, s, e);
    c = t[o];
    if (Hi(t[o])) {
        u32 k = RunBefore(t, o);
        start = o - (k & 1);
        if (Hi(t[start]) && Hi(t[start + 1])) {
            end = start + 2;
            c = (WChar)((t[start] << 8) | t[start + 1]);
        } else {
            start = o;
            end = o + 1;
        }
    }
    if (s) *s = start;
    if (e) *e = end;
    return c;
}

/* TxtFindString(찾기, v8): 원래 함수는 낱말 첫머리를 앞 바이트 하나로 판단해 한글 앞 글자의 끝 바이트가
 * Latin-1 기호(예: 0xB1 ±)면 낱말 가운데도 맞춘다(갚가에서 가). 원래 결과를 거르기만 한다:
 * 맞은 자리 바로 앞 바이트가 0xA1–0xFE면(앞이 2바이트 글자 = 낱말 글자, 또는 2바이트 글자 가운데) 버리고
 * 그 다음 바이트부터 다시 찾는다. 앞 바이트가 그 밖이면 원래 판단 그대로(영문 결과는 바뀌지 않음). */
Boolean HgFindString(const char *src, const char *tgt, u32 *pos, u16 *len) {
    const u8 *t = (const u8 *)src;
    u32 base = 0, p;
    u16 l;
    if (Idle() || !src || !tgt) return ((FindFn)ORIG(O_FindString))(src, tgt, pos, len);
    {   /* v10: 원문에 한글이 있으면 부분 검색 — 글자 경계마다 원문을 검색어 길이만큼 잘라 원래 함수에 넘기고,
         * 맨 앞(0)에서 맞을 때만 받아들인다. 검색어는 TxtPrepFindString이 바이트마다 1:1로 바꾼 값이라
         * 직접 비교하지 않고 원래 함수의 비교 규칙을 그대로 쓴다. */
        const u8 *g = (const u8 *)tgt;
        char buf[64];
        u32 nt = 0, i = 0, k, hi = 0;
        while (g[nt] && nt < 64) nt++;
        for (k = 0; t[k]; k++)
            if (Hi(t[k]) && Hi(t[k + 1])) { hi = 1; break; }
        if (hi && nt > 0 && nt < 64) {
            while (t[i]) {
                for (k = 0; k < nt && t[i + k]; k++) buf[k] = (char)t[i + k];
                if (k < nt) return 0;
                buf[k] = 0;
                if (((FindFn)ORIG(O_FindString))(buf, tgt, &p, &l) && p == 0) {
                    if (pos) *pos = i;
                    if (len) *len = l;
                    return 1;
                }
                i += (Hi(t[i]) && Hi(t[i + 1])) ? 2 : 1;
            }
            return 0;
        }
    }
    for (;;) {
        if (!((FindFn)ORIG(O_FindString))(src + base, tgt, &p, &l)) return 0;
        p += base;
        if (p == 0 || !Hi(t[p - 1])) {
            if (pos) *pos = p;
            if (len) *len = l;
            return 1;
        }
        base = p + 1;
    }
}
