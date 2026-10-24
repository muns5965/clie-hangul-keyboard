/*
 * MnHangul5 글자 경계 — 표 8의 Text Manager 함수 10개를 바꾼다(MnHangul src/hangul/intl.c와 같은 규칙).
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
