/*
 * MnSonyKbd Sony 가상 키보드 한글 입력 (리소스 SyIm 0) — MnHangul v35에서 시작, v54에 MnSonyKbd로 분리
 *
 * EvtGetEvent를 바꿔, 앱이 받는 영문 글자 키를 두벌식 자모로 바꾸고 조합한다.
 *   - 한글 모드 = 받는 순간 쉬프트 고정(⇪) 켜짐 (v34 실측: ⇪ = g1000, ⇪+쉬프트 소문자도 받는 순간 g1000,
 *     영문 소문자는 g0000). 배치는 dubeol.h(오버레이와 같은 표).
 *   - 원래 키 이벤트는 nilEvent로 바꾸고, 조합 결과는 BS + 완성형 2바이트 글자로 EvtEnqueueKey
 *     (MnSkCore와 같은 방식). 우리가 넣은 키는 keyCode kImMark로 표시해 다시 바꾸지 않는다.
 *   - 조합 끝: 글자 아닌 키, ⇪ 꺼짐, 명령 키(홈 등), 앱 영역 펜 누름(y < 160), 다른 앱.
 *   - BS: 조합 중이면 자모 하나씩 되돌림(MnSkCore와 같음), 아니면 그대로.
 * 조합 함수(KscJamo ~ SylKsc, Compose, ComposeBack)는 src/common/hangul_compose.h(MnSkCore와 공용).
 *
 * v43: WinDrawBitmap도 바꾼다. SilkExtn이 ⇪ 키 그림(ROM Tbmp 2400 보통 / 2500 반전 = 쉬프트 고정 켜짐)을 그리면
 *   (1) 그 그림으로 한글 모드(#143)를 정하고(Sony 키보드의 실제 ⇪ 상태 — 새 메모 등 시스템이 바꿔도 맞음),
 *   (2) 대신 '한A' 그림(SyIc 0·1, 22×18, 2픽셀 왼쪽)을 그린다.
 *   기록: #146 = ⇪ 그림 그린 순간의 다른 창 글자 그리기 수(#108, 키 글자보다 먼저인지 확인용), #147 = 횟수
 * MnSonyKbd v10: 조합 중 커서 이동 감지(src/common/cursor_track.h, MnSkCore와 공용) — 키를 받을 때 입력칸·커서가
 *   기대와 다르면 조합 끝. 기록 #191~#197.
 * MnSonyKbd v9: ⇪ 그림 때 입력 연결(EvtGetEvent)이 빠졌으면 다시 잇는다(CJKOS 끈 뒤 ON 없이 복구).
 *   진단: #188 입력 함수 불린 수, #189·#190 ⇪ 그림 순간의 EvtGetEvent·BltDrawChars 맨 위 주소
 *
 * 리소스 배치: 오프셋 0·4 = 진입점 오프셋(EvtGetEvent, WinDrawBitmap), 8·12 = 원래 주소, 16 = 상태 버퍼 주소,
 *   20·24 = ROM ⇪ 그림 주소(보통·반전), 28·32 = 대신 그릴 그림 주소(보통·켜짐) — 모두 앱이 DmWrite.
 * 상태 버퍼(동적 힙, 소유자 0): [0] 조합(초성<<24 | 중성<<16 | 종성<<8 | 화면에 나간 글자 수),
 *   [1] 조합 중인 앱 LocalID, [2] 바꾼 키 수, [3] 마지막 바꾼 키(원래 글자<<16 | 자모 바이트)
 * 어느 앱에서든 불리므로 전역변수·A5·재배치·문자열 상수 금지(빌드 스크립트가 검사).
 */

#include <PalmOS.h>

#define kImMark 0x4D48   /* 우리가 넣은 키의 keyCode ('MH') */

void HgImGet(EventType *event, Int32 timeout);
void HgImBmp(BitmapPtr bmp, Coord x, Coord y);

asm(".text\n.even\nImBase:\n"
    " .long HgImGet-ImBase, HgImBmp-ImBase\n"
    "ImOrig:\n .long 0,0\nImBuf:\n .long 0\nImIcon:\n .long 0,0,0,0,0,0\n");

typedef void (*GetFn)(EventType *, Int32);

/* v30: Sony HR 라이브러리(소니 헤더 없이 순번만 선언, sysLibTrapCustom + 순번) — 앱 쪽 Win 함수는 저해상도 좌표(2배)라서 */
void HRWinDrawBitmap(UInt16 ref, BitmapPtr bmp, Coord x, Coord y) SYS_TRAP(sysLibTrapCustom + 7);
void HRWinEraseRectangle(UInt16 ref, const RectangleType *r, UInt16 corner) SYS_TRAP(sysLibTrapCustom + 21);
IndexedColorType HRWinGetPixel(UInt16 ref, Coord x, Coord y) SYS_TRAP(sysLibTrapCustom + 28);
void HRWinPaintBitmap(UInt16 ref, BitmapPtr bmp, Coord x, Coord y) SYS_TRAP(sysLibTrapCustom + 37);
Err HRWinGetPixelRGB(UInt16 ref, Coord x, Coord y, RGBColorType *rgb) SYS_TRAP(sysLibTrapCustom + 60);   /* v38 */
typedef void (*BmpFn)(BitmapPtr, Coord, Coord);

static inline const UInt32 *Icons(void)
{
    const UInt32 *p;

    asm("lea ImIcon(%%pc),%0" : "=a" (p));
    return p;
}

static inline BmpFn OrigBmp(void)
{
    const BmpFn *p;

    asm("lea ImOrig+4(%%pc),%0" : "=a" (p));
    return *p;
}

static inline UInt32 *St(void)
{
    UInt32 * const *bp;

    asm("lea ImBuf(%%pc),%0" : "=a" (bp));
    return *bp;
}

static inline GetFn OrigGet(void)
{
    const GetFn *p;

    asm("lea ImOrig(%%pc),%0" : "=a" (p));
    return *p;
}

#include "dubeol.h"
#include "chain.h"

/* ---------------- 한글 조합 (src/common/hangul_compose.h, MnSkCore와 공용) ---------------- */
#define kSyFtrFld 191   /* #191~#197 조합 중 커서 이동 감지(src/common/cursor_track.h, MnSonyKbd v10) */
#include "../common/cursor_track.h"

static inline void HcSend(UInt16 c)
{
    EvtEnqueueKey(c, kImMark, 0);
    CtDelta(kSyCr, kSyFtrFld, c);
}

#define HC_FN static
#include "../common/hangul_compose.h"

/* v33: 한/영 누를 때 자판 영문 글자 키를 우리가 다시 그림(Sony는 이 단추를 몰라 다시 그리지 않음).
 *   키 글자 패치가 기록한 글자·자리(#300 + 2i: x<<16 | y, #301 + 2i: 세대<<24 | 글자<<16 | 폭<<8 | 높이, i = 글자 a~z),
 *   Sony 글꼴(#298), 글자 색(#297), 자판 다시 그리기 세대(#299)를 쓴다. 이번 세대에 그린 키만(sym 자판 등에서는 안 그림).
 *   글자 칸을 키 바탕색으로 지우고, 1비트 그림(32 × 글꼴 높이)에 자모(SyJm, 임시 글꼴과 같은 자리) 또는 Sony 글꼴의 영문을 옮겨
 *   겹쳐 그리기(winOverlay)로 그린다. 기록 #296 = 다시 그린 횟수<<16 | 마지막에 그린 키 수 */
#define kRdCell 18
static inline UInt32 GetShift(void)
{
    UInt32 v = 0;

    FtrGet(kSyCr, kFtrShift, &v);
    return v;
}

/* v34: 건너뛴 까닭 #295(1 기록 없음, 2 글꼴 v2, 3 글꼴 크기, 4 그림 못 만듦, 5 그림 줄 폭) */
static inline UInt32 GetLog293(void)
{
    UInt32 v = 0;

    FtrGet(kSyCr, 293, &v);
    return v;
}

static inline void RdWhy(UInt32 w)
{
    FtrSet(kSyCr, 295, w);
}
/* 글자 하나를 1비트 그림 줄(32 × fh, 줄마다 2워드)에. han = 자모(SyJm, spatch.c PutJamo와 같은 자리), 아니면 Sony 글꼴 영문.
 *   돌려주는 값: 그림 왼쪽 x(키 글자 기록 x 기준), 못 만들면 -1 */
static Int16 Glyph(UInt16 *bits, const Int16 *f, const UInt8 *p, UInt8 c, UInt8 cw, Boolean han, UInt16 fh)
{
    UInt16 row;

    MemSet(bits, fh * 4, 0);
    if (han) {
        UInt8        t = Dubeol(HanLabelKey(c));
        UInt16       H = p[3], w = p[2] >= 2 ? 4 : 2;
        Int16        top = f[9] - 15;
        const UInt8 *j;

        if (t == 0)
            return -1;
        j = p + (((UInt16)p[124] << 8) | p[125]) + (UInt16)(t - 0xA1) * H * w;
        if (top > (Int16)fh - (Int16)H)
            top = (Int16)fh - (Int16)H;
        if (top < 0)
            top = 0;
        for (row = 0; row < H && row + top < fh; row++) {
            UInt32 v = (((UInt32)j[row * w] << 8) | j[row * w + 1]) << 16;

            if (w == 4)
                v |= ((UInt32)j[row * w + 2] << 8) | j[row * w + 3];
            v >>= 1;
            bits[(row + top) * 2] = (UInt16)(v >> 16);
            bits[(row + top) * 2 + 1] = (UInt16)v;
        }
        return ((Int16)cw - kRdCell) / 2;
    } else {
        UInt16        rw = (UInt16)f[12], x0, x1, k;
        const UInt16 *img = (const UInt16 *)(f + 13), *loc = img + rw * fh;

        if (c < (UInt16)f[1] || c > (UInt16)f[2])
            return -1;
        x0 = loc[c - f[1]];
        x1 = loc[c - f[1] + 1];
        if (x1 <= x0 || x1 - x0 > 32)
            return -1;
        for (row = 0; row < fh; row++)
            for (k = 0; k < x1 - x0; k++) {
                UInt16 b = x0 + k;

                if (img[row * rw + (b >> 4)] & (0x8000 >> (b & 15)))
                    bits[row * 2 + (k >> 4)] |= 0x8000 >> (k & 15);
            }
        return 0;
    }
}

static inline Boolean Bit(const UInt16 *bits, UInt16 row, UInt16 k)
{
    return (bits[row * 2 + (k >> 4)] & (0x8000 >> (k & 15))) != 0;
}

/* v36: v35 실측 — 사각형으로 지우면 키 테두리·옆 키 글자까지 지워지고, 바탕색을 글자 왼쪽 위 한 점에서 읽어 테두리(검정)가 되기도 하며,
 *   글자 색(#297, WinSetTextColor)은 회색으로 나옴.
 *   → 지금 화면의 옛 글자(반대 모드)를 바탕색으로 덧그려 지운다(글자 점만). 색은 화면에서 읽는다:
 *   글자 색 = 옛 글자 점 자리, 바탕색 = 옛 글자 범위 안 빈 자리(키 안쪽). 그다음 새 글자를 글자 색으로. */
static BitmapPtr GlyphBmp(UInt16 fh, UInt16 **bits)
{
    Err       e = 0;
    UInt16    rb = 0;
    BitmapPtr bmp = BmpCreate(32, fh, 1, NULL, &e);

    if (bmp == NULL) {
        RdWhy(4);
        return NULL;
    }
    BmpGetDimensions(bmp, NULL, NULL, &rb);
    *bits = (UInt16 *)BmpGetBits(bmp);
    if (rb != 4 || *bits == NULL) {
        RdWhy(0x500 | rb);
        BmpDelete(bmp);
        return NULL;
    }
    return bmp;
}

/* v39: v38 실측 — 색은 맞게 읽히는데(f FFFFFF, b 222222) 새 글자가 여전히 안 보임. 한 그림으로 키마다 바탕색 → 글자색을 번갈아
 *   칠하면 두 번째(글자색)가 안 나옴(v35처럼 색을 한 번만 정하면 보였음). → 지우기·새 글자 그림을 따로 두고 두 번에 나눠 그림:
 *   1) 바탕색으로 모든 키 옛 글자 지우기, 2) 그리는 창을 다시 정하고 글자색으로 지운 키에만 새 글자 */
static void RedrawKeys(UInt16 hr, Boolean han)
{
    UInt32        gen = 0, fp = 0, pk = 0, n = 0, done = 0;
    const Int16  *f;
    BitmapPtr     bmpE, bmpN;
    UInt16       *bitsE = NULL, *bitsN = NULL, fh, i, drawn = 0;
    RGBColorType  of, ob, lastF, lastB;   /* v38: 16비트 화면이라 색 번호 대신 RGB(v37 실측: 흰 글자가 번호 0C로 읽힘) */
    Boolean       haveF = false, haveB = false;
    WinDrawOperation om;

    FtrGet(kSyCr, 299, &gen);
    FtrGet(kSyCr, 298, &fp);
    FtrGet(kSyCr, 3, &pk);
    f = (const Int16 *)fp;
    /* 글꼴 모양이 예상과 다르면 그리지 않음. v34: 그림 줄 폭 상한 1024(v33 실측: 16워드는 너무 작음) */
    if (f == NULL || pk == 0) {
        RdWhy(1);
        return;
    }
    if ((f[0] & 0x0200) != 0) {
        RdWhy(2);
        return;
    }
    if (f[7] <= 0 || f[7] > 32 || f[12] <= 0 || f[12] > 1024) {
        RdWhy(3);
        return;
    }
    fh = (UInt16)f[7];
    bmpE = GlyphBmp(fh, &bitsE);
    if (bmpE == NULL)
        return;
    bmpN = GlyphBmp(fh, &bitsN);
    if (bmpN == NULL) {
        BmpDelete(bmpE);
        return;
    }
    RdWhy(0);
    WinSetForeColorRGB(NULL, &of);
    WinSetBackColorRGB(NULL, &ob);
    lastF = of;
    lastB = ob;
    om = WinSetDrawMode(winOverlay);
    /* 1) 옛 글자(반대 모드) 색 읽기 + 바탕색으로 지우기 */
    for (i = 0; i < 26; i++) {
        UInt32           xy = 0, inf = 0;
        Coord            x, y;
        Int16            ox;
        UInt8            c, cw;
        UInt16           row, k;
        Boolean          gotF = false, gotB = false;
        RGBColorType     fg, bg;

        FtrGet(kSyCr, 300 + 2 * i, &xy);
        FtrGet(kSyCr, 301 + 2 * i, &inf);
        if (inf == 0 || (UInt8)(inf >> 24) != (UInt8)gen || (UInt8)inf != fh)
            continue;
        c = (UInt8)(inf >> 16);
        cw = (UInt8)(inf >> 8);
        x = (Coord)(xy >> 16);
        y = (Coord)xy;
        ox = Glyph(bitsE, f, (const UInt8 *)pk, c, cw, !han, fh);
        if (ox == -1)
            continue;
        for (row = 0; row < fh && !(gotF && gotB); row++) {
            Int16 lo = -1, hi = -1;

            for (k = 0; k < 32; k++)
                if (Bit(bitsE, row, k)) {
                    if (lo < 0)
                        lo = (Int16)k;
                    hi = (Int16)k;
                }
            if (lo < 0)
                continue;
            if (!gotF)
                gotF = HRWinGetPixelRGB(hr, x + ox + lo, y + (Coord)row, &fg) == errNone;
            for (k = (UInt16)lo + 1; k < (UInt16)hi && !gotB; k++)   /* 같은 줄 글자 점 사이 빈 자리 = 키 안쪽 */
                if (!Bit(bitsE, row, k))
                    gotB = HRWinGetPixelRGB(hr, x + ox + (Coord)k, y + (Coord)row, &bg) == errNone;
        }
        /* 'ㅣ'·'l'처럼 한 줄 안에 빈 자리가 없는 글자는 앞 키에서 읽은 색(모든 키 같은 색, a부터 차례) */
        if (gotF) {
            lastF = fg;
            haveF = true;
        } else if (haveF)
            fg = lastF;
        else
            continue;
        if (gotB) {
            lastB = bg;
            haveB = true;
        } else if (haveB)
            bg = lastB;
        else
            continue;
        if (fg.r == bg.r && fg.g == bg.g && fg.b == bg.b)
            continue;
        WinSetForeColorRGB(&bg, NULL);   /* 옛 글자 점만 바탕색으로 */
        HRWinPaintBitmap(hr, bmpE, x + ox, y);
        done |= 1UL << i;
        if (drawn == 0) {   /* v37 진단: 첫 키 — #294 글자색 RGB, #292 바탕 RGB(앞 바이트 = 읽음) */
            FtrSet(kSyCr, 294, ((UInt32)(gotF ? 2 : 0) << 24) | ((UInt32)fg.r << 16) | ((UInt32)fg.g << 8) | fg.b);
            FtrSet(kSyCr, 292, ((UInt32)(gotB ? 1 : 0) << 24) | ((UInt32)bg.r << 16) | ((UInt32)bg.g << 8) | bg.b);
            FtrSet(kSyCr, 293, ((UInt32)c << 24) | ((UInt32)(UInt8)ox << 16));
        }
        drawn++;
    }
    /* 2) 글자색으로 새 글자(지운 키만) */
    if (done != 0 && haveF) {
        UInt16 newN = 0;

        WinSetDrawWindow(WinGetDrawWindow());   /* 그리기 상태 다시 읽게 */
        WinSetForeColorRGB(&lastF, NULL);
        for (i = 0; i < 26; i++) {
            UInt32 xy = 0, inf = 0;
            Int16  nx;

            if ((done & (1UL << i)) == 0)
                continue;
            FtrGet(kSyCr, 300 + 2 * i, &xy);
            FtrGet(kSyCr, 301 + 2 * i, &inf);
            nx = Glyph(bitsN, f, (const UInt8 *)pk, (UInt8)(inf >> 16), (UInt8)(inf >> 8), han, fh);
            if (nx == -1)
                continue;
            if (newN == 0) {   /* 진단: 첫 새 글자 x·점 수(#293 아래 2바이트) */
                UInt32 dots = 0;
                UInt16 row, k;

                for (row = 0; row < fh; row++)
                    for (k = 0; k < 32; k++)
                        if (Bit(bitsN, row, k))
                            dots++;
                FtrSet(kSyCr, 293, (GetLog293() & 0xFFFF0000UL) | ((UInt32)(UInt8)nx << 8) | (dots > 255 ? 255 : dots));
            }
            HRWinPaintBitmap(hr, bmpN, (Coord)(xy >> 16) + nx, (Coord)xy);
            newN++;
        }
    }
    WinSetDrawMode(om);
    WinSetBackColorRGB(&ob, NULL);
    WinSetForeColorRGB(&of, NULL);
    BmpDelete(bmpN);
    BmpDelete(bmpE);
    FtrGet(kSyCr, 296, &n);
    FtrSet(kSyCr, 296, (((n >> 16) + 1) << 16) | drawn);
}

void HgImGet(EventType *event, Int32 timeout)
{
    UInt32 *st;
    WChar   ch;
    UInt8   j;
    Coord   px, py;

    OrigGet()(event, timeout);
#if MN_DIAG
    {   /* v9 진단: 입력 함수가 불린 횟수(CJKOS가 위에 있을 때 아래로 부르는지 확인용) */
        UInt32 n = 0;

        FtrGet(kSyCr, 188, &n);
        FtrSet(kSyCr, 188, n + 1);
    }
#endif
    st = St();
    if (st == NULL || event == NULL)
        return;
    px = event->screenX;
    py = event->screenY;
    if (event->eType == penDownEvent || event->eType == penUpEvent || event->eType == penMoveEvent) {
        /* v32: 펜 좌표는 활성 창 기준 — 시스템 키보드 화면(대화상자)이 떠 있으면 어긋나므로 화면 좌표로 바꿔 판정 */
        WinHandle aw = WinGetActiveWindow();

        if (aw != NULL) {
            WinHandle ow = WinSetDrawWindow(aw);

            WinWindowToDisplayPt(&px, &py);
            WinSetDrawWindow(ow);
        }
    }
    if (event->eType == penDownEvent) {          /* 앱 영역을 누르면 커서가 옮겨질 수 있어 조합 끝 */
        UInt32 v = 0;

        FtrGet(kSyCr, 250, &v);   /* v27 진단: 한/영 누름 뒤 첫 펜 누름 2개(위치 상관없이) */
        if (v != 0) {
            FtrSet(kSyCr, v == 2 ? 248 : 249, ((UInt32)(UInt16)px << 16) | (UInt16)py);
            FtrSet(kSyCr, 250, v - 1);
        }
        v = 0;

        if (py >= 160) {
            FtrSet(kSyCr, kFtrShiftIn, GetShift());   /* v34: 누르는 순간(Sony 다시 그리기 전)의 shift = 이 키 입력의 쌍자음 여부 */
            SyBltHeal();   /* MnSonyKbd v6: 가상 실크를 누르면 Sony가 다시 그리기 전에 키 글자 연결부터 확인·복구 */
            {   /* v20 진단: 가상 실크 펜 누름 3개 */
                UInt32 k = 0;

                FtrGet(kSyCr, 210, &k);
                if (k > 2)
                    k = 0;
                FtrSet(kSyCr, 211 + (UInt16)k, ((UInt32)(UInt16)px << 16) | (UInt16)py);
                FtrSet(kSyCr, 210, k + 1 > 2 ? 0 : k + 1);
            }
            if (kHanTab(px, py)) {   /* v20 진단: 한/영 단추 후보 — 시험 표시만 바꾸고 Sony에는 넘기지 않음 */
                UInt32 now = TimGetTicks(), last = 0;

                /* v29: 한 번 누름이 두 번 처리됨(v28 실측: 누름 하나에 t +2, 값이 제자리) → 마지막 전환 뒤 0.5초 안의 누름은 무시 */
                FtrGet(kSyCr, 252, &last);
                if (last != 0 && now - last < (UInt32)(SysTicksPerSecond() / 2)) {
                    FtrGet(kSyCr, 253, &v);
                    FtrSet(kSyCr, 253, v + 1);
                    event->eType = nilEvent;
                    return;
                }
                FtrSet(kSyCr, 252, now);
                st[0] = 0;                        /* v32: 전환하면 조합 끝 */
                CtUnknown(kSyCr, kSyFtrFld);
                v = 0;
                FtrGet(kSyCr, kFtrHanTab, &v);
                FtrSet(kSyCr, kFtrHanTab, v ? 0 : 1);
                v = 0;
                FtrGet(kSyCr, 219, &v);
                FtrSet(kSyCr, 219, v + 1);
                v = 0;
                FtrGet(kSyCr, 108, &v);
                FtrSet(kSyCr, 215, v);
                /* v28: v27 실측 — 고해상도 좌표로 넣은 펜 점은 엉뚱한 자리(159,0)로 옴(펜 점은 디지타이저 값으로 읽힘) → 넣기 포기.
                 *   이번 누름은 버리고, 가상 실크 창(#251, 키 글자 그릴 때 기록)에 표시를 바로 다시 그린다(v37 실측: 가상 실크 창에 그리면
                 *   바로 보임). 바탕은 표시 왼쪽 점의 색으로 지운다. 키 글자는 다음에 Sony가 자판을 다시 그릴 때 바뀐다. */
                event->eType = nilEvent;
                {
                    WinHandle sw = (WinHandle)0;
                    UInt32    a = 0, h = 0;

                    UInt32    hr = 0;

                    FtrGet(kSyCr, 251, (UInt32 *)&sw);
                    FtrGet(kSyCr, kFtrHanTab, &h);
                    FtrGet(kSyCr, h ? 246 : 247, &a);
                    FtrGet(kSyCr, 254, &hr);
                    if (sw != NULL && a != 0 && hr != 0) {   /* v30: 고해상도 좌표로 그림(v29 실측: 일반 함수는 실크 창 밖) */
                        WinHandle     old = WinSetDrawWindow(sw);
                        RectangleType rr;
                        RGBColorType  bg, ob;   /* v38: RGB로(16비트 화면) */

                        WinSetBackColorRGB(NULL, &ob);
                        if (HRWinGetPixelRGB((UInt16)hr, 139, 113, &bg) == errNone) {
                            WinSetBackColorRGB(&bg, NULL);
                            RctSetRectangle(&rr, kMarkX, kMarkY, 14, 18);
                            HRWinEraseRectangle((UInt16)hr, &rr, 0);
                            WinSetBackColorRGB(&ob, NULL);
                        }
                        HRWinDrawBitmap((UInt16)hr, (BitmapPtr)a, kMarkX, kMarkY);
                        RedrawKeys((UInt16)hr, h != 0);   /* v33: 자판 영문 글자 키도 바로 자모↔영문 */
                        WinSetDrawWindow(old);
                        FtrSet(kSyCr, 243, ((UInt32)bg.r << 16) | ((UInt32)bg.g << 8) | bg.b);
                    }
                }
                return;
            }
        }
        if (py < 160) {
            st[0] = 0;
            CtUnknown(kSyCr, kSyFtrFld);     /* v10: 앱 화면 누름은 커서를 옮길 수 있음(조합은 이미 끝) */
            FtrSet(kSyCr, kFtrPenCaps, 0);   /* v44: 앱 화면 누름도 '마지막 펜이 ⇪ 아님' */
        } else if (kCapsKey(px, py)) {   /* v39: ⇪ → 한글 모드 전환 */
            UInt32 sc = 0;

            /* v45: Sony 실제 상태(마지막 자판 전체 그리기의 ⇪ 그림)의 반대로. 모르면 지금 값을 뒤집음 */
            FtrGet(kSyCr, kFtrSonyCaps, &sc);
            FtrGet(kSyCr, kFtrHan, &v);
            if (sc & 0x100)
                FtrSet(kSyCr, kFtrHan, !(sc & 1));
            else {   /* v7: 모르면(시작 직후·연결 복구 직후) 지금 Graffiti ⇪ 상태의 반대(그리기 전이라 지금 상태 = Sony 상태) */
                Boolean caps = false, num = false, autoS = false;
                UInt16  temp = 0;

                GrfGetState(&caps, &num, &temp, &autoS);
                FtrSet(kSyCr, kFtrHan, caps ? 0 : 1);
            }
            FtrSet(kSyCr, kFtrPenCaps, 1);
            v = 0;
            FtrGet(kSyCr, 108, &v);
            FtrSet(kSyCr, kFtrPenDraw, v);
        } else {  /* v48: 가상 키보드 일반 키 = 3(키 누름 중 순간 상태로 영문 내림 안 함)
                   * v50: 키 영역(y < 213)만 3. 아래 탭·상태 표시줄(키보드 띄우는 버튼 등)은 0 */
            FtrSet(kSyCr, kFtrPenCaps, py < kKeyAreaEnd ? 3 : 0);
        }
        return;
    }
    if ((event->eType == penUpEvent || event->eType == penMoveEvent) && kHanTab(px, py)) {
        event->eType = nilEvent;                 /* v25: 한/영 자리 누름은 Sony에 넘기지 않음 */
        return;
    }
    if (event->eType == penUpEvent) {            /* v39: ⇪ 자리를 눌렀는데 키 글자 다시 그리기가 없었으면 되돌림 */
        UInt32 pc = 0, d0 = 0, d1 = 0, v = 0;

        FtrGet(kSyCr, kFtrPenCaps, &pc);
        if (pc == 1) {
            FtrGet(kSyCr, kFtrPenDraw, &d0);
            FtrGet(kSyCr, 108, &d1);
            if (d1 - d0 < 20) {   /* v45: Sony 실제 상태로 되돌림(모르면 다시 뒤집음) */
                UInt32 sc = 0;

                FtrGet(kSyCr, kFtrSonyCaps, &sc);
                FtrGet(kSyCr, kFtrHan, &v);
                FtrSet(kSyCr, kFtrHan, (sc & 0x100) ? (sc & 1) : (v ? 0 : 1));
            }
            FtrSet(kSyCr, kFtrPenCaps, 2);   /* 처리함(다시 맞추기 막음은 다음 펜 누름까지 유지) */
        }
        return;
    }
    if (event->eType != keyDownEvent || event->data.keyDown.keyCode == kImMark)
        return;
    ch = event->data.keyDown.chr;
    if (ch < 0x80)   /* v34: PC 두벌식 — 누를 때의 shift(#256)대로 쌍자음 또는 기본 자모, 쓰고 나면 0 */
        ch = HanInKey((UInt8)ch);
    FtrSet(kSyCr, kFtrShiftIn, 0);
    /* v10: 조합 중인데 커서가 옮겨졌거나 입력칸이 바뀌었으면(펜 없이 앱이 옮긴 경우 등) 조합 끝 */
    if (CtCheck(kSyCr, kSyFtrFld, (GetComp(st) & 0xFF) != 0))
        st[0] = 0;
    if ((event->data.keyDown.modifiers & commandKeyMask) != 0 || !HanMode()) {
        st[0] = 0;
        CtUnknown(kSyCr, kSyFtrFld);   /* 앱에 그대로 넘기는 키: 커서 변화 모름 */
        return;
    }
    if (ch == chrBackspace) {
        if (ComposeBack(st))
            event->eType = nilEvent;
        else
            CtUnknown(kSyCr, kSyFtrFld);
        return;
    }
    j = (ch < 0x80) ? Dubeol((UInt8)ch) : 0;
    if (j == 0) {
        st[0] = 0;
        CtUnknown(kSyCr, kSyFtrFld);
        return;
    }
    st[2]++;
    st[3] = ((UInt32)ch << 16) | j;
    Compose(st, (UInt8)(j - 0xA0));
    event->eType = nilEvent;
}

/* v43: ⇪ 키 그림 → 한글 모드 판정 + '한A' 그림 */
/* v21 진단: 가상 실크(화면 밖 창) 그림 기록 — 탭 줄(abc·int'l·sym)이 그림인지, 어디에 그리는지 */
static void BmpLog(BitmapPtr bmp, Coord x, Coord y)
{
    if (bmp != NULL) {
        WinHandle w = WinGetDrawWindow();

        if (w != NULL && WinGetBitmap(w) != WinGetBitmap(WinGetDisplayWindow())) {
            UInt32 k = 0, n = 0;
            Coord  bw = 0, bh = 0;

            BmpGetDimensions(bmp, &bw, &bh, NULL);
            FtrGet(kSyCr, 220, &k);
            k &= 3;
            FtrSet(kSyCr, 221 + 3 * (UInt16)k, (UInt32)bmp);
            FtrSet(kSyCr, 222 + 3 * (UInt16)k, ((UInt32)(UInt16)bw << 16) | (UInt16)bh);
            FtrSet(kSyCr, 223 + 3 * (UInt16)k, ((UInt32)(UInt16)x << 16) | (UInt16)y);
            FtrSet(kSyCr, 220, (k + 1) & 3);
            FtrGet(kSyCr, 233, &n);
            FtrSet(kSyCr, 233, n + 1);
        }
    }
}

void HgImBmp(BitmapPtr bmp, Coord x, Coord y)
{
    const UInt32 *ic = Icons();

    BmpLog(bmp, x, y);   /* v21 진단 */
    if (0 && bmp != NULL && ic[4] != 0 && ic[5] != 0) {   /* v22: 320×130은 자판이 아니라 Graffiti 판 그림이었음(v22 실측) → 끔 */
        WinHandle w = WinGetDrawWindow();
        Coord     bw = 0, bh = 0;

        BmpGetDimensions(bmp, &bw, &bh, NULL);
        if (bw == 320 && bh == 130 && w != NULL && WinGetBitmap(w) != WinGetBitmap(WinGetDisplayWindow())) {
            UInt32 v = 0;

            OrigBmp()(bmp, x, y);
            FtrGet(kSyCr, kFtrHanTab, &v);
            OrigBmp()((BitmapPtr)ic[v ? 4 : 5], x + 136, y + 108);
            return;
        }
    }

    if (bmp != NULL && ic[0] != 0 && ((UInt32)bmp == ic[0] || (UInt32)bmp == ic[1])) {
        UInt16 on = (UInt32)bmp == ic[1] ? 1 : 0;
        UInt32 v = 0, pc = 0, d0 = 0, last = 0;
        Boolean use, healed;

        /* MnSonyKbd v3: 키 글자 패치가 연결에서 빠졌으면(CJKOS 끄기 등) 다시 잇는다.
         * v6: 방금 이었으면 앞 키 글자 기록(#151)이 없으니 이 그림을 실제 상태로 쓴다(v5 실측: 기준값 S가 옛 값이라
         *     다음 ⇪에서 영문인데 쌍자음 표시) */
        healed = SyBltHeal();
        /* v9: CJKOS를 끄면 입력(EvtGetEvent) 연결도 빠진다(v8 실측: 키 글자만 복구되고 Now: broken, ON = re-attached 1).
         *     이 그림 연결은 CJKOS를 꺼도 남으므로 여기서 입력만 다시 잇는다(ROM이 맨 위일 때만, 그림 연결 자신은 건드리지 않음) */
        SyImHealOne(0);
        /* v9 진단: 이 순간의 맨 위 주소(입력·키 글자) */
#if MN_DIAG
        FtrSet(kSyCr, 189, (UInt32)SysGetTrapAddress(sysTrapEvtGetEvent));
        FtrSet(kSyCr, 190, (UInt32)SysGetTrapAddress(kSyTrapBlt));
#endif

        /* v47: 자판 전체 그리기에서 ⇪ 그림은 2줄 끝 키(\ 또는 |) 바로 다음(v46 실측). 그때만 Sony 실제 상태로 쓴다.
         *      ⇪ 누를 때의 눌림 표시(반전 그림, 앱이 펜을 받기 전에 그려짐)는 판정에 쓰지 않는다 */
        FtrGet(kSyCr, kFtrPenCaps, &pc);
        FtrGet(kSyCr, kFtrPenDraw, &d0);
        FtrGet(kSyCr, 108, &v);
        FtrGet(kSyCr, 151, &last);
        use = healed || (last == '\\' || last == '|');
        if (use) {
            FtrSet(kSyCr, kFtrHan, on);
            FtrSet(kSyCr, kFtrSonyCaps, 0x100 | on);   /* v45 */
        }
#if MN_DIAG
        DecLog((2UL << 30) | ((v & 0xFFF) << 18) | ((pc & 3) << 7) |   /* v46 진단 */
               (use ? 2 : 0) | on);
        FtrSet(kSyCr, 177, 0);   /* 그림 다음 키 글자는 다시 기록 */
        FtrSet(kSyCr, 146, v);
        v = 0;
        FtrGet(kSyCr, 147, &v);
        FtrSet(kSyCr, 147, v + 1);
#endif
        /* v32: ⇪ 자리 '한A' 그림 바꾸기는 뺌(한/영은 sym 오른쪽 단추, ⇪는 원래 그림·영문 대문자 고정) */
    }
    OrigBmp()(bmp, x, y);
}

#include "../skwr/ksc_table.h"
