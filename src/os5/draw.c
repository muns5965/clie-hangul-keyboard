/*
 * MnHangul5 그리기·폭 — 표 8의 WinDrawChars·WinDrawChar·WinPaintChars·WinDrawTruncChars와
 * 폭 함수 5개(FntCharsWidth·FntLineWidth·FntCharsInWidth·FntWordWrap·FntWidthToOffset)를 바꾼다.
 * 규칙은 OS 4 MnHangul(src/hangul/width.c·patch.c)과 같다. 한글 한 칸 = 표준 좌표 9(HW).
 * 글자 모양: 조합형 글꼴 묶음(HgJ16, Neo둥근모 16×16 기반 OFL)을 GlyphRows로 합성해 점으로 찍는다.
 */
#include "os5.h"

#define kCoordinatesNative 0
#define TAB 20
#define chrEllipsis 0x85

typedef void (*DrawTruncFn)(const char *, s16, s16, s16, s16);
typedef void (*DrawCharFn)(WChar, s16, s16);
typedef s16 (*LineWidthFn)(const char *, u16);
typedef void (*CharsInWidthFn)(const char *, s16 *, s16 *, Boolean *);
typedef u16 (*WordWrapFn)(const char *, u16);
typedef s16 (*WidthToOffsetFn)(const char *, u16, s16, Boolean *, s16 *);
typedef s16 (*LineHeightFn)(void);
typedef u16 (*CoordSysFn)(u16);
typedef void (*PixelFn)(s16, s16);
typedef u8 (*ColorFn)(u8);

static int HasHi(const u8 *t, u32 n) {
    u32 i;
    for (i = 0; i < n; i++)
        if (Hi(t[i])) return 1;
    return 0;
}
static inline u16 RdW(const u8 *p) { return (u16)((p[0] << 8) | p[1]); }

/* ---- 글자 모양 (MnHangul patch.c) ---- */
/* 완성형 순위 r → 유니코드 음절 번호. 표: 비트(1398바이트) + 누계(빅엔디언, tools/gen-ksc-table.py) */
static u16 RankToSyl(const u8 *ksc, u16 r) {
    const u8 *cum = ksc + 1398;
    u16 b = 0, k, sy;
    while (b + 1 < 44 && RdW(cum + (b + 1) * 2) <= r) b++;
    k = r - RdW(cum + b * 2);
    for (sy = b * 256; sy < 11172; sy++)
        if (ksc[sy >> 3] & (0x80 >> (sy & 7))) {
            if (k == 0) return sy;
            k--;
        }
    return 0;
}

static void GlyphRows(const u8 *pk, u16 sy, u16 *rows) {
    u16 H = pk[3], Nc = pk[6], Nj = pk[7], rb = H * 2, i, k;
    u16 oP = RdW(pk + 112), oE = RdW(pk + 116), nE = RdW(pk + 118), oS = RdW(pk + 120),
        nS = RdW(pk + 122);
    u16 L = sy / 588, V = (sy % 588) / 28, T = sy % 28, hasT = T ? 1 : 0;
    u16 id[3];
    int lo, hi;

    for (i = 0; i < H; i++) rows[i] = 0;
    lo = 0;
    hi = (int)nS - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        u16 v = RdW(pk + oS + mid * (2 + rb));
        if (v == sy) {
            for (i = 0; i < H; i++) rows[i] = RdW(pk + oS + mid * (2 + rb) + 2 + i * 2);
            return;
        }
        if (v < sy) lo = mid + 1; else hi = mid - 1;
    }
    id[0] = pk[10 + V * 2 + hasT] * 19 + L;
    id[1] = Nc * 19 + pk[52 + L * 2 + hasT] * 21 + V;
    id[2] = T ? Nc * 19 + Nj * 21 + pk[90 + V] * 27 + T - 1 : 0xFFFF;
    lo = 0;
    hi = (int)nE - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        u16 v = RdW(pk + oE + mid * 8);
        if (v == sy) {
            for (k = 0; k < 3; k++) id[k] = RdW(pk + oE + mid * 8 + 2 + k * 2);
            break;
        }
        if (v < sy) lo = mid + 1; else hi = mid - 1;
    }
    for (k = 0; k < 3; k++)
        if (id[k] != 0xFFFF)
            for (i = 0; i < H; i++) rows[i] |= RdW(pk + oP + (u32)id[k] * rb + i * 2);
}

/* 2바이트 글자 하나를 표준 좌표(x, y = 글줄 위)에 그린다 */
static void DrawGlyph(u8 lead, u8 trail, s16 x, s16 y) {
    struct Ctx *c = CtxPtr();
    const u8 *pk = c->pack;
    u16 rows[16], H = pk[3], r, col, old;
    s16 lineH = ((LineHeightFn)T8(T8_FntLineHeight))();
    s16 nx = x * 2, ny = y * 2 + (lineH * 2 - (s16)H) / 2;
    PixelFn pixel = (PixelFn)T8(T8_WinDrawPixel);
    ColorFn setText = (ColorFn)T8(T8_WinSetTextColor), setFore = (ColorFn)T8(T8_WinSetForeColor);
    u8 text, fore;

    if (lead == 0xA4) {
        const u8 *j = pk + RdW(pk + 124) + (trail - 0xA1) * H * 2;
        for (r = 0; r < H; r++) rows[r] = RdW(j + r * 2);
    } else if (lead >= 0xB0 && lead <= 0xC8 && trail >= 0xA1) {
        GlyphRows(pk, RankToSyl(c->ksc, (lead - 0xB0) * 94 + (trail - 0xA1)), rows);
    } else {   /* 그 밖의 KS X 1001 행: 빈 네모 */
        for (r = 0; r < H; r++) rows[r] = (r == 2 || r == H - 3) ? 0x3FF8 : (r > 2 && r < H - 3 ? 0x2008 : 0);
    }
#if MN_DIAG
    c->hits++;
#endif

    text = setText(0);  /* 글자 색으로 그림 */
    setText(text);
    fore = setFore(text);
    old = ((CoordSysFn)T8(T8_WinSetCoordinateSystem))(kCoordinatesNative);
    for (r = 0; r < H; r++)
        if (rows[r])
            for (col = 0; col < 16; col++)
                if (rows[r] & (0x8000 >> col)) pixel(nx + 1 + col, ny + r);
    ((CoordSysFn)T8(T8_WinSetCoordinateSystem))(old);
    setFore(fore);
}

/* ---- 폭 (MnHangul width.c, 한글 칸 = HW) ---- */
static inline s16 W1(u8 ch) {
    char b = (char)ch;
    return ((CharsWidthFn)ORIG(O_CharsWidth))(&b, 1);
}

static u16 Step(const u8 *t, u32 i, u32 n, s16 x, s16 *w) {
    u8 ch = t[i];
    if (Hi(ch) && i + 1 < n && Hi(t[i + 1])) {
        *w = HW;
        return 2;
    }
    *w = (ch == '\t') ? (TAB - (x % TAB)) : W1(ch);
    return 1;
}

static s16 Measure(const u8 *t, u32 n, int tabs) {
    s16 x = 0, w;
    u32 i = 0, run;
    while (i < n) {
        if (!Hi(t[i]) && !(tabs && t[i] == '\t')) {
            run = i;
            while (i < n && !Hi(t[i]) && !(tabs && t[i] == '\t')) i++;
            x += ((CharsWidthFn)ORIG(O_CharsWidth))((const char *)t + run, (s16)(i - run));
            continue;
        }
        i += Step(t, i, n, x, &w);
        x += w;
    }
    return x;
}

s16 HgCharsWidth(const char *chars, s16 len) {
    if (Idle() || !chars || len <= 0 || !HasHi((const u8 *)chars, len))
        return ((CharsWidthFn)ORIG(O_CharsWidth))(chars, len);
    return Measure((const u8 *)chars, len, 0);
}

s16 HgLineWidth(const char *chars, u16 len) {
    if (Idle() || !chars || len == 0 || !HasHi((const u8 *)chars, len))
        return ((LineWidthFn)ORIG(O_LineWidth))(chars, len);
    return Measure((const u8 *)chars, len, 1);
}

void HgCharsInWidth(const char *str, s16 *widthP, s16 *lengthP, Boolean *fitP) {
    const u8 *t = (const u8 *)str;
    s16 maxW, x = 0, w;
    u32 n, i = 0;
    Boolean fit = 1;
    if (Idle() || !str || !widthP || !lengthP || *lengthP <= 0 || !HasHi(t, (u32)*lengthP)) {
        ((CharsInWidthFn)ORIG(O_CharsInWidth))(str, widthP, lengthP, fitP);
        return;
    }
    maxW = *widthP;
    n = (u32)*lengthP;
    while (i < n) {
        u16 sz;
        if (t[i] == '\n' || t[i] == 0) { fit = 0; break; }
        sz = Step(t, i, n, x, &w);
        if (x + w > maxW) { fit = 0; break; }
        x += w;
        i += sz;
    }
    while (i > 0 && t[i - 1] == ' ') { i--; x -= W1(' '); }
    *widthP = x;
    *lengthP = (s16)i;
    if (fitP) *fitP = fit;
}

u16 HgWordWrap(const char *chars, u16 maxWidth) {
    const u8 *t = (const u8 *)chars;
    u16 l;
    s16 x = 0, w;
    u32 i = 0, brk = 0;
    if (Idle() || !chars) return ((WordWrapFn)ORIG(O_WordWrap))(chars, maxWidth);
    l = ((WordWrapFn)ORIG(O_WordWrap))(chars, maxWidth);
    if (!HasHi(t, (u32)l + 1)) return l;
    while (t[i] != 0) {
        u16 sz;
        if (t[i] == '\n') return (u16)(i + 1);
        sz = Step(t, i, 0xFFFFFFFFu, x, &w);
        if (x + w > (s16)maxWidth) {
            if (t[i] == ' ') {
                while (t[i] == ' ') i++;
                if (t[i] == '\n') i++;
                return (u16)i;
            }
            if (brk > 0) return (u16)brk;
            return (u16)((i > 0) ? i : sz);
        }
        x += w;
        i += sz;
        if (t[i - 1] == ' ' && sz == 1) brk = i;
    }
    return (u16)i;
}

s16 HgWidthToOffset(const char *chars, u16 len, s16 px, Boolean *leadingP, s16 *truncP) {
    const u8 *t = (const u8 *)chars;
    s16 x = 0, w;
    u32 i = 0;
    if (Idle() || !chars || len == 0 || !HasHi(t, len))
        return ((WidthToOffsetFn)ORIG(O_WidthToOffset))(chars, len, px, leadingP, truncP);
    while (i < len) {
        u16 sz = Step(t, i, len, x, &w);
        if (px < x + w) {
            if (leadingP) *leadingP = (px - x) < (w + 1) / 2;
            if (truncP) *truncP = x;
            return (s16)i;
        }
        x += w;
        i += sz;
    }
    if (leadingP) *leadingP = 0;
    if (truncP) *truncP = x;
    return (s16)len;
}

/* ---- 그리기 ---- */
static void DrawMixed(u32 origIdx, const char *chars, s16 len, s16 x, s16 y) {
    const u8 *s = (const u8 *)chars;
    s16 i = 0, start, cx = x;
    while (i < len) {
        if (Hi(s[i]) && i + 1 < len && Hi(s[i + 1])) {
            DrawGlyph(s[i], s[i + 1], cx, y);
            cx += HW;
            i += 2;
        } else {
            start = i;
            while (i < len && !(Hi(s[i]) && i + 1 < len && Hi(s[i + 1]))) i++;
            ((DrawCharsFn)ORIG(origIdx))(chars + start, i - start, cx, y);
            cx += ((CharsWidthFn)ORIG(O_CharsWidth))(chars + start, i - start);
        }
    }
}

void HgDrawChars(const char *chars, s16 len, s16 x, s16 y) {
    if (Idle() || !chars || len <= 0 || !HasHi((const u8 *)chars, len))
        ((DrawCharsFn)ORIG(O_DrawChars))(chars, len, x, y);
    else
        DrawMixed(O_DrawChars, chars, len, x, y);
}

void HgPaintChars(const char *chars, s16 len, s16 x, s16 y) {
    if (Idle() || !chars || len <= 0 || !HasHi((const u8 *)chars, len))
        ((DrawCharsFn)ORIG(O_PaintChars))(chars, len, x, y);
    else
        DrawMixed(O_PaintChars, chars, len, x, y);
}

void HgDrawChar(WChar ch, s16 x, s16 y) {
    if (!Idle() && ch > 0xFF && Hi(ch >> 8) && Hi(ch & 0xFF))
        DrawGlyph((u8)(ch >> 8), (u8)ch, x, y);
    else
        ((DrawCharFn)ORIG(O_DrawChar))(ch, x, y);
}

/* WinDrawTruncChars: 글자 경계에서 자르고 "…" */
void HgDrawTruncChars(const char *chars, s16 len, s16 x, s16 y, s16 maxWidth) {
    const u8 *t = (const u8 *)chars;
    s16 w, cx = 0, ell;
    u32 i = 0;
    char e = (char)chrEllipsis;
    if (Idle() || !chars || len <= 0 || !HasHi(t, len)) {
        ((DrawTruncFn)ORIG(O_DrawTrunc))(chars, len, x, y, maxWidth);
        return;
    }
    if (Measure(t, len, 0) <= maxWidth) {
        DrawMixed(O_DrawChars, chars, len, x, y);
        return;
    }
    ell = W1((u8)chrEllipsis);
    while (i < (u32)len) {
        u16 sz = Step(t, i, len, cx, &w);
        if (cx + w + ell > maxWidth) break;
        cx += w;
        i += sz;
    }
    DrawMixed(O_DrawChars, chars, (s16)i, x, y);
    ((DrawCharsFn)ORIG(O_DrawChars))(&e, 1, x + cx, y);
}
