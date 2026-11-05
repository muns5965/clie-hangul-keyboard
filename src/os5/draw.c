/*
 * MnHangul5 그리기·폭 — 표 8의 WinDrawChars·WinDrawChar·WinPaintChars·WinDrawTruncChars와
 * 폭 함수 5개(FntCharsWidth·FntLineWidth·FntCharsInWidth·FntWordWrap·FntWidthToOffset)를 바꾼다.
 * 규칙은 OS 4 MnHangul(src/hangul/width.c·patch.c)과 같다. 한글 한 칸 = 표준 좌표 9(HW), 큰 글꼴 12(HW24).
 * 글자 모양: 조합형 글꼴 묶음(HgJ16, Neo둥근모 16×16 기반 OFL)을 GlyphRows로 합성해 1비트 고밀도 그림 하나로 그린다(v2).
 *   v1은 점마다 WinDrawPixel을 불러 한 글자에 약 100번 → 한글 줄이 영문의 약 7배 느렸다(시험 앱: 540자 0.33초).
 *   v2는 글자마다 WinPaintBitmap 한 번(540자 0.04초, 영문과 비슷).
 * v3: 큰 글꼴(줄 높이 14 이상)은 24×24 묶음(HgJ24)으로 칸 12, 굵은 글꼴(boldFont·largeBoldFont)은 1픽셀 겹쳐 굵게.
 *   그리기·폭·글자 경계가 같은 칸(HangulCell)을 써야 줄바꿈·커서가 맞는다.
 * v16: 한글 모드에서 시스템 키보드 대화상자(폼 32000)의 키 글자(영문자 한 글자, 입력칸 아래)는 두벌식 자모로 그린다(KbdLabel).
 * v17: 대문자 키 글자는 대화상자에서 shift를 손으로 눌렀을 때만 쌍자음(ime.c kshift), 그 밖(새 글 첫머리 자동 대문자·cap)은 기본 자모.
 *   대화상자는 키 글자를 WinDrawChars로 한 글자씩 그리고, 한/영(Shift+Space)·shift 때 다시 그린다(TG50 실측).
 * v32: 한/A 단추 칸을 배경색으로 지운 뒤 그림(키 글자만 다시 그려질 때 앞 글자와 겹쳤음).
 * v30: 한/A 단추를 탭 줄 가운데에(v29는 왼쪽 위 고정 자리라 '한'이 약 6픽셀 위).
 * v29: Sony 자판 탭 줄 빈자리에 한/A 단추(SkButtonDraw).
 * v28: 밑줄 모드면 한글 칸 아래에 밑줄도 그린다(입력칸을 다시 그리면 한글 아래 밑줄이 빠졌음).
 * v27: RawPaint가 raw 앞 값을 되돌림.
 * v26: StdInput이 부른 WinPaintChar(s)는 원래 함수로, 그 안의 글자 경계 함수도 원래대로(RawPaint, ctx raw).
 * v25: WinPaintChar도 바꾼다 — Sony 소프트 키보드(StdInput) 키 글자를 한글 모드에서 두벌식 자모로(HgPaintChar).
 * v24: 24×24 글자를 기기 1픽셀 위로. 한글은 기준선이 아니라 줄 높이 가운데에 놓는데, 큰 굵은 글꼴(Address 이름 칸)에서 같은 칸 영문보다
 *   윗끝 1·아랫끝 2픽셀 아래였다(VZ90 측정). 올린 뒤 윗끝 = 영문 대문자 윗끝, 아랫끝 = 기준선 + 1. 16×16은 윗끝·아랫끝이 1픽셀 위라 그대로.
 *   또 글자 칸을 지울 때 줄 맨 아래 한 줄은 남긴다(입력칸 점선 밑줄이 한글 아래에서만 지워졌음).
 */
#include "os5.h"

#define kCoordinatesNative 0
#define kWinPaint 0
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
typedef u16 (*DrawModeFn)(u16);
typedef void (*PaintBitmapFn)(void *, s16, s16);
typedef u8 (*ColorFn)(u8);
struct ERect { s16 x, y, w, h; };
typedef u8 UInt8;
#include "../common/dubeol_map.h"
#define kKeyboardFormId 32000   /* ime.c와 같음 */
#define frmFieldObj 0
typedef void (*EraseFn)(const struct ERect *, u16);

static int HasHi(const u8 *t, u32 n) {
    u32 i;
    for (i = 0; i < n; i++)
        if (Hi(t[i])) return 1;
    return 0;
}
static inline u16 RdW(const u8 *p) { return (u16)((p[0] << 8) | p[1]); }
/* 묶음 그림 한 줄을 32비트(왼쪽 = 비트 31)로. 판 1 = 2바이트 줄(16×16), 판 2 = 4바이트 줄(24×24) — MnHangul patch.c와 같음 */
static inline u16 RowBytes(const u8 *pk) { return pk[2] >= 2 ? 4 : 2; }
static inline u32 RdRow(const u8 *pk, const u8 *p) {
    return pk[2] >= 2 ? ((u32)RdW(p) << 16) | RdW(p + 2) : (u32)RdW(p) << 16;
}

#define boldFont 1
#define largeBoldFont 7
typedef u8 (*GetFontFn)(void);

static inline int Big(void) {
    return CtxPtr()->pack24 && ((LineHeightFn)T8(T8_FntLineHeight))() >= kBigLine;
}

u16 HangulCell(void) { return Big() ? HW24 : HW; }

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

static void GlyphRows(const u8 *pk, u16 sy, u32 *rows) {
    u16 H = pk[3], Nc = pk[6], Nj = pk[7], w = RowBytes(pk), rb = H * w, i, k;
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
            for (i = 0; i < H; i++) rows[i] = RdRow(pk, pk + oS + mid * (2 + rb) + 2 + i * w);
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
            for (i = 0; i < H; i++) rows[i] |= RdRow(pk, pk + oP + (u32)id[k] * rb + i * w);
}

/* 1비트 글자 줄(rows, 32비트, 왼쪽 = 비트 31)을 기기 좌표 (nx, ny)에 지금 글자색으로 그린다(점 없는 자리는 투명). v25: DrawGlyph에서 떼어 냄 */
static void PaintRows(const u32 *rows, u16 H, u16 gw, s16 nx, s16 ny) {
    struct Ctx *c = CtxPtr();
    u16 r, old, mode;
    ColorFn setText = (ColorFn)T8(T8_WinSetTextColor), setFore = (ColorFn)T8(T8_WinSetForeColor);
    u8 text, fore;

    text = setText(0);  /* 글자 색으로 그림 */
    setText(text);
    fore = setFore(text);
    old = ((CoordSysFn)T8(T8_WinSetCoordinateSystem))(kCoordinatesNative);
    {
        /* BitmapTypeV3를 ARM 바이트 순서로 직접 만든다(ctx->bmp). 실측으로 확인한 형식:
         *   ARM BmpCreate가 만든 그림은 필드가 리틀엔디언이고 version 바이트에 0x80이 붙는다(V2 = 0x82).
         *   flags는 ARM 비트필드 순서(hasTransparency = 비트 2) → 투명 번호 0(점 없는 자리)은 그리지 않음.
         *   1비트 점은 전경색(위에서 글자색으로 바꿈)으로 그려진다. 밀도 144 = 기기 픽셀 1:1.
         *   줄은 4바이트(rowBytes 4)로 16·24 폭 모두 같은 형식. */
        u8 *b = (u8 *)c->bmp;
        u16 i;

        for (i = 0; i < 24; i++) b[i] = 0;
        b[0] = (u8)gw;      /* width */
        b[2] = (u8)H;       /* height */
        b[4] = 4;           /* rowBytes */
        b[6] = 0x04;        /* flags: hasTransparency */
        b[8] = 1;           /* pixelSize */
        b[9] = 0x83;        /* version 3 | 리틀엔디언 */
        b[10] = 24;         /* 머리 크기 */
        b[13] = 0xFF;       /* 압축 없음 */
        b[14] = 144;        /* density 144(고밀도) */
        for (r = 0; r < H; r++) {   /* 1비트 점: 왼쪽이 높은 비트 */
            b[24 + r * 4] = (u8)(rows[r] >> 24);
            b[25 + r * 4] = (u8)(rows[r] >> 16);
            b[26 + r * 4] = (u8)(rows[r] >> 8);
            b[27 + r * 4] = (u8)rows[r];
        }
        mode = ((DrawModeFn)T8(T8_WinSetDrawMode))(kWinPaint);
        ((PaintBitmapFn)T8(T8_WinPaintBitmap))(b, nx, ny);
        ((DrawModeFn)T8(T8_WinSetDrawMode))(mode);
    }
    ((CoordSysFn)T8(T8_WinSetCoordinateSystem))(old);
    setFore(fore);
}

/* 2바이트 글자 하나를 표준 좌표(x, y = 글줄 위)에 그린다.
 * erase: WinDrawChars·WinDrawChar·WinDrawTruncChars처럼 배경까지 칠하는 함수 대신일 때 1 —
 *   그림이 투명(점 없는 자리는 안 그림)이라, 먼저 칸을 지금 배경색으로 지운다(v6).
 *   앱(Memo Pad 등)은 줄 중간을 고칠 때 지우지 않고 다시 그리므로, 안 지우면 예전 글자와 겹친다.
 *   WinPaintChars(그리기 모드를 따르는 함수) 대신일 때는 0. */
static void DrawGlyph(u8 lead, u8 trail, s16 x, s16 y, int erase) {
    struct Ctx *c = CtxPtr();
    int big = Big();
    const u8 *pk = big ? c->pack24 : c->pack;
    u32 rows[24];
    u16 H = pk[3], w = RowBytes(pk), r, gw = big ? 24 : 16;
    s16 lineH = ((LineHeightFn)T8(T8_FntLineHeight))();
    s16 nx = x * 2 + (big ? 0 : 1), ny = y * 2 + (lineH * 2 - (s16)H) / 2 - (big ? 1 : 0);   /* 16×16은 칸 18에서 왼쪽 1 띄움.
                                                                                       v24: 24×24는 기기 1픽셀 위로(아래 설명) */
    u8 font;

    if (lead == 0xA4) {
        const u8 *j = pk + RdW(pk + 124) + (trail - 0xA1) * H * w;
        for (r = 0; r < H; r++) rows[r] = RdRow(pk, j + r * w);
    } else if (lead >= 0xB0 && lead <= 0xC8 && trail >= 0xA1) {
        GlyphRows(pk, RankToSyl(c->ksc, (lead - 0xB0) * 94 + (trail - 0xA1)), rows);
    } else {   /* 그 밖의 KS X 1001 행: 빈 네모 */
        u32 edge = 0xFFFFFFFFu << (32 - (gw - 4)) >> 2, side = (0x80000000u >> 2) | (0x80000000u >> (gw - 3));
        for (r = 0; r < H; r++) rows[r] = (r == 2 || r == H - 3) ? edge : (r > 2 && r < H - 3 ? side : 0);
    }
    if (erase) {
        struct ERect er;
        er.x = x;
        er.y = y;
        er.w = (s16)HangulCell();
        er.h = lineH - 1;   /* v24: 줄 맨 아래 한 줄(입력칸 점선 밑줄 자리)은 지우지 않는다 — 글자 그림은 그 줄에 닿지 않는다
                             * (그림 맨 아래와 남기는 줄 사이 기기 1픽셀 이상 — 줄 높이 11(16×16)·14·15(24×24)로 계산.
                             * 다른 글꼴·세로 자리를 더하면 다시 확인). 전에는 한글 아래 밑줄이 지워졌다 */
        ((EraseFn)T8(T8_WinEraseRectangle))(&er, 0);
    }
    font = ((GetFontFn)T8(T8_FntGetFont))();
    if (font == boldFont || font == largeBoldFont)   /* 굵게: 오른쪽으로 1픽셀 겹침(칸 안에서) */
        for (r = 0; r < H; r++) rows[r] |= (rows[r] >> 1) & (0xFFFFFFFFu << (32 - gw));
#if MN_DIAG
    c->hits++;
#endif

    PaintRows(rows, H, gw, nx, ny);
    {   /* v28: 밑줄 모드(입력칸)면 한글 칸 아래에도 밑줄 — 원래 WinDrawChars는 글자마다 칸 맨 아래 줄(y + 줄 높이 − 1)에 밑줄을 그린다.
         *   입력칸을 통째로 다시 그릴 때(실크 크기 바뀜·폼 다시 그리기 등) 줄을 지우고 글자와 함께 밑줄을 그리므로, 한글 칸만 밑줄이 빠졌다.
         *   (v24의 '맨 아래 줄 남기기'는 글자를 하나씩 넣을 때만 막았다.) 1 = 점선(gray), 2·3 = 실선 — 지금 전경색으로 */
        u16 um = ((DrawModeFn)T8(T8_WinSetUnderlineMode))(0);
        ((DrawModeFn)T8(T8_WinSetUnderlineMode))(um);
        if (um) {
            s16 ly = y + lineH - 1, x2 = x + (s16)HangulCell() - 1;
            ((void (*)(s16, s16, s16, s16))T8(um == 1 ? T8_WinDrawGrayLine : T8_WinDrawLine))(x, ly, x2, ly);
        }
    }
}

/* ---- 폭 (MnHangul width.c, 한글 칸 = cw = HangulCell()) ---- */
static inline s16 W1(u8 ch) {
    char b = (char)ch;
    return ((CharsWidthFn)ORIG(O_CharsWidth))(&b, 1);
}

static u16 Step(const u8 *t, u32 i, u32 n, s16 x, s16 *w, s16 cw) {
    u8 ch = t[i];
    if (Hi(ch) && i + 1 < n && Hi(t[i + 1])) {
        *w = cw;
        return 2;
    }
    *w = (ch == '\t') ? (TAB - (x % TAB)) : W1(ch);
    return 1;
}

static s16 Measure(const u8 *t, u32 n, int tabs) {
    s16 x = 0, w, cw = (s16)HangulCell();
    u32 i = 0, run;
    while (i < n) {
        if (!Hi(t[i]) && !(tabs && t[i] == '\t')) {
            run = i;
            while (i < n && !Hi(t[i]) && !(tabs && t[i] == '\t')) i++;
            x += ((CharsWidthFn)ORIG(O_CharsWidth))((const char *)t + run, (s16)(i - run));
            continue;
        }
        i += Step(t, i, n, x, &w, cw);
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
    s16 maxW, x = 0, w, cw;
    u32 n, i = 0;
    Boolean fit = 1;
    if (Idle() || !str || !widthP || !lengthP || *lengthP <= 0 || !HasHi(t, (u32)*lengthP)) {
        ((CharsInWidthFn)ORIG(O_CharsInWidth))(str, widthP, lengthP, fitP);
        return;
    }
    maxW = *widthP;
    n = (u32)*lengthP;
    cw = (s16)HangulCell();
    while (i < n) {
        u16 sz;
        if (t[i] == '\n' || t[i] == 0) { fit = 0; break; }
        sz = Step(t, i, n, x, &w, cw);
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
    s16 x = 0, w, cw;
    u32 i = 0, brk = 0;
    if (Idle() || !chars) return ((WordWrapFn)ORIG(O_WordWrap))(chars, maxWidth);
    l = ((WordWrapFn)ORIG(O_WordWrap))(chars, maxWidth);
    if (!HasHi(t, (u32)l + 1)) return l;
    cw = (s16)HangulCell();
    while (t[i] != 0) {
        u16 sz;
        if (t[i] == '\n') return (u16)(i + 1);
        sz = Step(t, i, 0xFFFFFFFFu, x, &w, cw);
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
    s16 x = 0, w, cw;
    u32 i = 0;
    if (Idle() || !chars || len == 0 || !HasHi(t, len))
        return ((WidthToOffsetFn)ORIG(O_WidthToOffset))(chars, len, px, leadingP, truncP);
    cw = (s16)HangulCell();
    while (i < len) {
        u16 sz = Step(t, i, len, x, &w, cw);
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
    s16 i = 0, start, cx = x, cw = (s16)HangulCell();
    while (i < len) {
        if (Hi(s[i]) && i + 1 < len && Hi(s[i + 1])) {
            DrawGlyph(s[i], s[i + 1], cx, y, origIdx != O_PaintChars);
            cx += cw;
            i += 2;
        } else {
            start = i;
            while (i < len && !(Hi(s[i]) && i + 1 < len && Hi(s[i + 1]))) i++;
            ((DrawCharsFn)ORIG(origIdx))(chars + start, i - start, cx, y);
            cx += ((CharsWidthFn)ORIG(O_CharsWidth))(chars + start, i - start);
        }
    }
}

/* v16: 키보드 대화상자의 키 글자면 그릴 자모(KS X 1001 0xA4 다음 바이트), 아니면 0.
 *   한글 모드 + 영문자 한 글자 + 활성 폼 32000 + 그리는 y가 초점 입력칸 아래 끝 이상(창 기준 좌표 = 객체 좌표).
 *   자모는 입력과 같은 규칙: 대화상자가 그리는 글자(대문자 = shift)를 그대로 눌렀을 때 들어가는 자모(ime.c: Dubeol(ch ^ 0x20)). */
static u8 KbdLabel(const char *chars, s16 len, s16 y) {
    u8 c;
    void *f;
    u16 fo, n;
    struct ERect r;

    if (len != 1 || !CtxPtr()->st[2]) return 0;
    c = (u8)chars[0];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return 0;
    f = ((void *(*)(void))T12(T12_FrmGetActiveForm))();
    if (!f || ((u16 (*)(void *))T12(T12_FrmGetFormId))(f) != kKeyboardFormId) return 0;
    fo = ((u16 (*)(void *))T12(T12_FrmGetFocus))(f);
    n = ((u16 (*)(void *))T12(T12_FrmGetNumberOfObjects))(f);
    if (fo >= n || ((u16 (*)(void *, u16))T12(T12_FrmGetObjectType))(f, fo) != frmFieldObj) return 0;
    ((void (*)(void *, u16, struct ERect *))T12(T12_FrmGetObjectBounds))(f, fo, &r);
    if (y < r.y + r.h) return 0;
    if (c >= 'A' && c <= 'Z' && !CtxPtr()->kshift) c |= 0x20;   /* v17: 자동 대문자 등 손으로 누르지 않은 shift는 기본 자모 */
    return Dubeol((u8)(c ^ 0x20));
}

void HgDrawChars(const char *chars, s16 len, s16 x, s16 y) {
    u8 j;
    if (!Idle() && chars && (j = KbdLabel(chars, len, y)) != 0) {   /* v16: 키 글자 자리 가운데(한글 칸이 영문 글자보다 2 넓음 쪽으로) */
        DrawGlyph(0xA4, j, x - 2, y, 1);
        return;
    }
    if (Idle() || !chars || len <= 0 || !HasHi((const u8 *)chars, len))
        ((DrawCharsFn)ORIG(O_DrawChars))(chars, len, x, y);
    else
        DrawMixed(O_DrawChars, chars, len, x, y);
}

/* v26: StdInput이 부른 글자 그리기는 원래 함수로, 그 안에서 부르는 글자 경계 함수(표 8)도 원래대로(raw).
 *   영문 ROM의 かな·記号 탭 글자(Latin-1 0xA1~)를 한글 2바이트로 읽어 깨졌다(v17~v25). */
static void RawPaint(u32 fn, const char *chars, s16 len, WChar ch, s16 x, s16 y) {
    struct Ctx *c = CtxPtr();
    u32 prev = c->raw;   /* v27: 앞 값을 되돌림(중첩 호출 대비) */
    c->raw = 1;
    if (chars) ((DrawCharsFn)fn)(chars, len, x, y);
    else ((DrawCharFn)fn)(ch, x, y);
    c->raw = prev;
}

void HgPaintChars(const char *chars, s16 len, s16 x, s16 y) {
    if (FromStdInput()) {
        RawPaint(ORIG(O_PaintChars), chars, len, 0, x, y);
        return;
    }
    if (Idle() || !chars || len <= 0 || !HasHi((const u8 *)chars, len))
        ((DrawCharsFn)ORIG(O_PaintChars))(chars, len, x, y);
    else
        DrawMixed(O_PaintChars, chars, len, x, y);
}

void HgDrawChar(WChar ch, s16 x, s16 y) {
    if (!Idle() && ch > 0xFF && Hi(ch >> 8) && Hi(ch & 0xFF))
        DrawGlyph((u8)(ch >> 8), (u8)ch, x, y, 1);
    else
        ((DrawCharFn)ORIG(O_DrawChar))(ch, x, y);
}

/* v25: WinPaintChar — Sony 소프트 키보드(표준 입력 StdInput)가 키 글자를 화면 밖 창에 한 글자씩 그린다(docs/OS5_RESEARCH.md 12절).
 *   한글 모드이고 부른 곳(돌아갈 주소)이 StdInput 코드(ctx sklo~skhi, 앱이 켤 때 찾음) 안이면 영문 키 글자를 두벌식 자모로 그린다.
 *   자모는 입력과 같은 규칙(ime.c: Dubeol(ch ^ 0x20)) — 자판이 대문자(Shift)로 그린 키는 그 키를 눌렀을 때 들어가는 쌍자음·ㅒ·ㅖ.
 *   StdInput이 넘기는 좌표·글자 폭은 이미 기기 픽셀 단위다(진단: 키 간격 23). 좌표계 값을 보고 2배 하면 두 칸 간격으로 어긋났다(VZ90 실측) → 그대로 쓴다.
 *   글자 점 범위의 가운데를 영문 글자 칸 가운데에 맞춘다. */
void HgPaintChar(WChar ch, s16 x, s16 y) {
    struct Ctx *c = CtxPtr();
    u32 lr = (u32)__builtin_return_address(0);

    if (!Idle() && InStdInput(lr) && (ch == 'm' || ch == 'M'))
        c->skbpen |= 0x100;   /* v29: 영문자 탭의 마지막 줄 — 이 글자를 그린 뒤 한/A 단추도(아래 끝) */
    if (!Idle() && c->st[2] && InStdInput(lr) && ch < 0x80 &&
        ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))) {
        u8 j = Dubeol((u8)(ch ^ 0x20));
        const u8 *pk = c->pack;
        if (j && pk) {
            u32 rows[16];
            u16 H = pk[3], w = RowBytes(pk), r, k;
            u16 rmin = 16, rmax = 0, cmin = 16, cmax = 0;
            const u8 *g = pk + RdW(pk + 124) + (j - 0xA1) * H * w;
            char b = (char)ch;
            s16 cw = ((CharsWidthFn)ORIG(O_CharsWidth))(&b, 1), lh = ((LineHeightFn)T8(T8_FntLineHeight))();

            for (r = 0; r < H && r < 16; r++) {
                rows[r] = RdRow(pk, g + r * w);
                if (rows[r]) {
                    if (r < rmin) rmin = r;
                    rmax = r;
                    for (k = 0; k < 16; k++)
                        if (rows[r] & (0x80000000u >> k)) {
                            if (k < cmin) cmin = k;
                            if (k > cmax) cmax = k;
                        }
                }
            }
            if (rmin <= rmax) {
                s16 cx = (s16)(x + cw / 2), cy = (s16)(y + lh / 2);
                PaintRows(rows, H, 16, (s16)(cx - (s16)((cmin + cmax + 1) / 2)), (s16)(cy - (s16)((rmin + rmax + 1) / 2)));
                SkButtonDraw();
                return;
            }
        }
    }
    if (InStdInput(lr)) RawPaint(c->pcorig, 0, 0, ch, x, y);
    else ((DrawCharFn)c->pcorig)(ch, x, y);
    SkButtonDraw();
}

/* v29: Sony 자판(영문자 탭)의 탭 줄 빈자리에 지금 모드 — 한글 '한', 영문 'A'. StdInput이 'm'/'M'을 그린 직후 같은 화면 밖 창에 그린다
 *   (그 창이 실크로 옮겨짐, Sony가 탭 줄 배경으로 덮지 않음 — 15.3). 누름은 ime.c SkBtnTap. */
void SkButtonDraw(void)
{
    struct Ctx *c = CtxPtr();

    if (!(c->skbpen & 0x100)) return;
    c->skbpen &= ~0x100u;
    {   /* v32: 단추 칸을 그 자리 배경색으로 먼저 지움 — 탭 줄은 그대로 두고 키 글자만 다시 그리면(Shift가 풀릴 때 등) 앞 글자 위에 겹쳤다 */
        struct ERect er;
        u16 oc = ((CoordSysFn)T8(T8_WinSetCoordinateSystem))(kCoordinatesNative);
        u8 bg = ((u8 (*)(s16, s16))T8(T8_WinGetPixel))(kSkBtnCX - 9, kSkBtnCY - 10);   /* 칸 왼쪽 위 모서리(글자 점이 닿지 않는 곳) */
        u8 ob = ((ColorFn)T8(T8_WinSetBackColor))(bg);
        er.x = kSkBtnCX - 9; er.y = kSkBtnCY - 10; er.w = 19; er.h = 20;
        ((EraseFn)T8(T8_WinEraseRectangle))(&er, 0);
        ((ColorFn)T8(T8_WinSetBackColor))(ob);
        ((CoordSysFn)T8(T8_WinSetCoordinateSystem))(oc);
    }
    if (c->st[2] && c->pack) {   /* 한: 점 범위의 가운데를 단추 칸 가운데에(HgPaintChar와 같은 방식) */
        u32 rows[16];
        u16 r, k, H = c->pack[3], rmin = 16, rmax = 0, cmin = 16, cmax = 0;
        GlyphRows(c->pack, RankToSyl(c->ksc, (0xC7 - 0xB0) * 94 + (0xD1 - 0xA1)), rows);   /* 한 (KSC C7D1) */
        for (r = 0; r < H && r < 16; r++)
            if (rows[r]) {
                if (r < rmin) rmin = r;
                rmax = r;
                for (k = 0; k < 16; k++)
                    if (rows[r] & (0x80000000u >> k)) {
                        if (k < cmin) cmin = k;
                        if (k > cmax) cmax = k;
                    }
            }
        if (rmin <= rmax)
            PaintRows(rows, H, 16, (s16)(kSkBtnCX - (s16)((cmin + cmax + 1) / 2)), (s16)(kSkBtnCY - (s16)((rmin + rmax + 1) / 2)));
    } else {   /* A: StdInput 글꼴의 글자 칸(폭 × 줄 높이) 가운데를 단추 칸 가운데에 */
        char a = 'A';
        s16 w = ((CharsWidthFn)ORIG(O_CharsWidth))(&a, 1), lh = ((LineHeightFn)T8(T8_FntLineHeight))();
        RawPaint(c->pcorig, 0, 0, 'A', (s16)(kSkBtnCX - w / 2), (s16)(kSkBtnCY - lh / 2));
    }
}

/* WinDrawTruncChars: 글자 경계에서 자르고 "…" */
void HgDrawTruncChars(const char *chars, s16 len, s16 x, s16 y, s16 maxWidth) {
    const u8 *t = (const u8 *)chars;
    s16 w, cx = 0, ell, cw;
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
    cw = (s16)HangulCell();
    while (i < (u32)len) {
        u16 sz = Step(t, i, len, cx, &w, cw);
        if (cx + w + ell > maxWidth) break;
        cx += w;
        i += sz;
    }
    DrawMixed(O_DrawChars, chars, (s16)i, x, y);
    ((DrawCharsFn)ORIG(O_DrawChars))(&e, 1, x + cx, y);
}
