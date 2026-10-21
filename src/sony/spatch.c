/*
 * MnSonyKbd 키 글자 자모 오버레이 (리소스 SyPt 0) — BltDrawChars(트랩 0xA378) 패치
 *
 * MnHangul patch.c의 오버레이(v30~v53)를 떼어 낸 것(v54). 한글 표시(2바이트 글자 그리기)는 하지 않는다:
 * 가상 실크 창(화면 밖 창, flags 0x4000)에 높이 22 글꼴로 찍는 영문 한 글자만, 한글 모드이면 두벌식 자모로 바꿔 그리고
 * 나머지는 모두 아래 함수(ROM, 또는 먼저 설치된 CJKOS·MnHangul 패치)로 넘긴다.
 *
 * 자모 그리기: 원래 글꼴 머리를 본뜬 1글자 임시 글꼴(칸 폭 18, 글자 'H')에 자모 그림(SyJm 0, 16×16)을 넣어
 *   ROM BltDrawChars(#152, 앱이 시작 바이트를 확인한 주소. 0이면 아래 함수)로 그린다.
 * 한/영 판정(HanLabel)은 MnHangul v36~v50과 같다(dubeol.h, ime.c와 같은 기록 'MnSy').
 *
 * 기록(feature 'MnSy'): #106 MnSonyKbd 실행 중(그동안 기록 안 함), #108 다른 창 글자 그리기 수,
 *   #127 자모로 바꿔 그린 수, #128 걸러진 이유<<24 | 글꼴 높이<<8 | 사각형 높이
 *   (3 글꼴 높이 22 아님, 4 화면 밖 창 아님, 5 임시 메모리 없음, 6 영문 모드, 0 바꿔 그림),
 *   #151 마지막 한 글자 키 글자(⇪ 그림 판정용, ime.c), #3 자모 그림 주소(앱), #152 자모 그리기 함수(앱)
 *
 * 리소스 배치: 오프셋 0 = 아래 함수 주소 칸(앱이 DmWrite), 오프셋 4 = 진입점 PatchBlt.
 * 어느 앱에서든 불리므로 전역변수·A5·재배치 금지(빌드 스크립트가 검사).
 */

#include <PalmOS.h>
#include "dubeol.h"

typedef UInt32 (*BltFn)(void *p1, void *p2, void *p3, const UInt8 *chars, UInt16 len, void *p6, void *p7);

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

    if (FtrGet(kSyCr, n, &v) != errNone)
        v = 0;
    return v;
}

#define kFtrJamo  3
#define kFtrBlt   152
#define kTmpChar  0x48    /* 임시 글꼴의 유일한 글자 */
#define kCell     18      /* 16 글자 칸 폭(고해상도 픽셀) */
#define TmpWords(h)  (13 + (h) * 2 + 6)

static inline UInt16 RdW(const UInt8 *p)
{
    return ((UInt16)p[0] << 8) | p[1];
}

/* 글꼴 f(SDK FontType)에서 글자 c의 폭. 없는 글자는 '없는 글자' 칸의 폭.
 * v15: 동작에는 쓰지 않음(FntCharWidth로 바꿈) — v8 진단 비교(Diag)에서만 쓴다 */
static inline UInt16 CharW(const Int16 *f, UInt8 c)
{
    const UInt16 *owt = (const UInt16 *)((const UInt8 *)f + 16 + (UInt16)f[8] * 2);
    Int16         first = f[1], last = f[2];
    UInt16        e;

    if (c < first || c > last)
        e = owt[last - first + 1];
    else {
        e = owt[c - first];
        if (e == 0xFFFF)
            e = owt[last - first + 1];
    }
    return e & 0xFF;
}

/* 1글자 임시 글꼴 (MnHangul patch.c MakeTmpFont와 같음) */
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

/* 자모 그림(16줄, 판 1 = 2바이트 줄)을 임시 글꼴 그림 칸에. 세로 시작 = ascent - 15(MnHangul GlyphTop 16과 같음) */
static inline void PutJamo(UInt16 *t, const UInt8 *pk, UInt8 trail, const Int16 *f)
{
    UInt16       H = pk[3], w = pk[2] >= 2 ? 4 : 2, fh = (UInt16)f[7], y;
    const UInt8 *j = pk + RdW(pk + 124) + (UInt16)(trail - 0xA1) * H * w;
    Int16        top = FntBaseLine() - 15;   /* v15: 글꼴 구조 ascent(f[9]) 대신 공개 함수(v8 실측 일치) */

    if (top > (Int16)fh - (Int16)H)
        top = (Int16)fh - (Int16)H;
    if (top < 0)
        top = 0;
    for (y = 0; y < H && y + top < fh; y++) {
        UInt32 v = (UInt32)RdW(j + y * w) << 16;

        if (w == 4)
            v |= RdW(j + y * w + 2);
        v >>= 1;                                   /* 칸(18) 안 왼쪽 1칸 띄움 */
        t[13 + (y + top) * 2] = (UInt16)(v >> 16);
        t[13 + (y + top) * 2 + 1] = (UInt16)v;
    }
}

/* 지금 그리는 곳이 화면 밖 창(가상 실크)인가.
 * v15: 창 구조 +8 windowFlags 0x4000 직접 읽기 대신 공개 함수 — 화면 밖 창은 자기 비트맵을 따로 가진다
 *      (v8 실측: 두 방식 80·224·422·522개 모두 일치, CJKOS 켬·끔) */
static inline Boolean SilkWin(void)
{
    WinHandle w = WinGetDrawWindow();

    return w != NULL && WinGetBitmap(w) != WinGetBitmap(WinGetDisplayWindow());
}

/* v8 진단에서만: 예전 판정(창 구조 +8 windowFlags 0x4000) */
static inline Boolean SilkWinFlags(void)
{
    const UInt8 *w = (const UInt8 *)WinGetDrawWindow();

    return w != NULL && (*(const UInt16 *)(w + 8) & 0x4000) != 0;
}

/* 키 글자를 그리는 순간의 한/영 판정 (MnHangul v36·v39·v44·v46·v49·v50과 같음, 자세한 이유는 docs/KOREAN_SUPPORT.md) */
static inline Boolean HanLabel(UInt8 c)
{
    Boolean caps = false, num = false, autoS = false;
    UInt16  temp = 0;

    GrfGetState(&caps, &num, &temp, &autoS);
    /* v39: ⇪ 켜짐이고 마지막 펜이 ⇪가 아니면 한글로 다시 맞춤(v48: 키 누름 3 포함) */
    if (caps && (GetF(kFtrPenCaps) == 0 || GetF(kFtrPenCaps) == 3) && GetF(kFtrHan) == 0)
        FtrSet(kSyCr, kFtrHan, 1);
    /* v44: ⇪ 꺼짐 + 일시 쉬프트이고 마지막 펜이 앱 화면이면 영문(새 메모에서 시스템이 ⇪를 푼 경우) */
    if (!caps && temp != 0 && GetF(kFtrPenCaps) == 0 && GetF(kFtrHan) != 0)
        FtrSet(kSyCr, kFtrHan, 0);
    /* v49·v50: 소문자 키 글자 + ⇪ 꺼짐 = 영문(키보드 처음 띄울 때). 키 누름(3) 동안은 적용 안 함 */
    if (!caps && c >= 'a' && c <= 'z' && GetF(kFtrPenCaps) != 3 && GetF(kFtrHan) != 0)
        FtrSet(kSyCr, kFtrHan, 0);
    {   /* v46 진단: (H, p, caps, temp)가 바뀔 때만 기록 */
        UInt32 st = ((GetF(kFtrHan) ? 1UL : 0) << 9) | ((GetF(kFtrPenCaps) & 3) << 7) | ((caps ? 1UL : 0) << 6) |
                    ((UInt32)(temp & 0xF) << 2);

        if ((GetF(177) & 0xFFFF) != (st | 0x8000)) {
            FtrSet(kSyCr, 177, st | 0x8000);
            DecLog((1UL << 30) | ((GetF(108) & 0xFFF) << 18) | ((UInt32)c << 10) | st);
        }
    }
    return GetF(kFtrHan) != 0;
}

/* v8 진단(동작 변경 없음): 내부 구조 대신 공개 함수로 같은 판정이 나오는지 비교한다.
 *   창: a = 창 구조 windowFlags 0x4000(지금 쓰는 판정), b = 그리는 창 비트맵 != 화면 창 비트맵(WinGetBitmap),
 *       c = 그리는 창 == SilkWareLib GetScrWindow(앱이 #154에 기록, 0이면 모름=2)
 *       #180 표본 수, #181 모두 일치 수, #182 마지막 불일치(a | b<<1 | c<<2 | 글자<<8), #183 그때 그리는 창
 *   글꼴: #184 표본 수, #185 FntGetFontPtr() == p6 수, #186 마지막 불일치(FntGetFont<<24 | 높이<<16 | FntCharWidth<<8 | 폭 표),
 *       #187 불일치 수(포인터·폭·기준선 FntBaseLine 중 하나라도 다름) */
static inline void Diag(UInt8 c, const Int16 *f)
{
    UInt32 dw = (UInt32)WinGetDrawWindow(), scr = GetF(154);
    UInt16 a = SilkWinFlags() ? 1 : 0;   /* v15: a = 예전 판정, b = 지금 판정(SilkWin) */
    UInt16 b = SilkWin() ? 1 : 0;
    UInt16 k = scr == 0 ? 2 : (dw == scr ? 1 : 0);
    UInt16 fw = (UInt16)FntCharWidth((Char)c), tw = CharW(f, c);
    Boolean pm = (const Int16 *)FntGetFontPtr() == f;

    FtrSet(kSyCr, 180, GetF(180) + 1);
    if (a == b && (k == 2 || k == a))
        FtrSet(kSyCr, 181, GetF(181) + 1);
    else {
        FtrSet(kSyCr, 182, (UInt32)a | ((UInt32)b << 1) | ((UInt32)k << 2) | ((UInt32)c << 8));
        FtrSet(kSyCr, 183, dw);
    }
    FtrSet(kSyCr, 184, GetF(184) + 1);
    if (pm)
        FtrSet(kSyCr, 185, GetF(185) + 1);
    if ((UInt16)FntCharHeight() != (UInt16)f[7]) {   /* v15: 글꼴 높이 f[7] → FntCharHeight 전환 가능성(#198 다름 수, #199 마지막) */
        FtrSet(kSyCr, 198, GetF(198) + 1);
        FtrSet(kSyCr, 199, ((UInt32)FntCharHeight() << 8) | (UInt8)f[7]);
    }
    if (!pm || fw != tw || FntBaseLine() != f[9]) {
        FtrSet(kSyCr, 186, ((UInt32)FntGetFont() << 24) | ((UInt32)(f[7] & 0xFF) << 16) | ((UInt32)(fw & 0xFF) << 8) |
                           (tw & 0xFF));
        FtrSet(kSyCr, 187, GetF(187) + 1);
    }
}

static inline void Reject(UInt16 why, const Int16 *f, const Int16 *rc)
{
    FtrSet(kSyCr, 128, ((UInt32)why << 24) | ((UInt32)(f[7] & 0xFF) << 8) | (UInt8)rc[3]);
}

UInt32 PatchBlt(void *p1, void *p2, void *p3, const UInt8 *chars, UInt16 len, void *p6, void *p7)
{
    UInt32 orig = OrigAddr();

    if (orig == 0)
        return 0;
    if (p2 != NULL && (void *)WinGetDrawWindow() != (void *)WinGetActiveWindow() && GetF(106) == 0) {
        UInt8 c = (len == 1 && chars != NULL) ? chars[0] : 0;

        FtrSet(kSyCr, 108, GetF(108) + 1);
        FtrSet(kSyCr, 151, c);
        if (c != 0 && p6 != NULL && Dubeol(c) != 0 && GetF(kFtrJamo) != 0) {
            const Int16 *f = (const Int16 *)p6, *rc = (const Int16 *)p2;
            UInt16      *tmp = NULL;

            Diag(c, f);   /* v8 진단 */
            if (f[7] != 22)
                Reject(3, f, rc);
            else if (!SilkWin())
                Reject(4, f, rc);
            else if (!HanLabel(c))
                Reject(6, f, rc);
            else if ((tmp = MemPtrNew(TmpWords(f[7]) * 2)) == NULL)
                Reject(5, f, rc);
            else {
                RectangleType r = *(const RectangleType *)p2;
                Int16         rh = rc[3];
                UInt16        ratio = (rh > 0 && f[7] == rh * 2) ? 2 : 1;
                UInt8         one = kTmpChar;
                UInt32        res, b = GetF(kFtrBlt);

                MakeTmpFont(tmp, f, kCell);
                PutJamo(tmp, (const UInt8 *)GetF(kFtrJamo), Dubeol(c), f);
                r.topLeft.x += ((Int16)FntCharWidth((Char)c) - kCell) / 2 / (Int16)ratio;   /* 키 글자 가운데에 맞춤(v15: 공개 함수) */
                r.extent.x = kCell / ratio;
                res = ((BltFn)(b != 0 ? b : orig))(p1, &r, p3, &one, 1, tmp, p7);
                MemPtrFree(tmp);
                FtrSet(kSyCr, 127, GetF(127) + 1);
                FtrSet(kSyCr, 128, ((UInt32)(f[7] & 0xFF) << 8) | (UInt8)rh);
                return res;
            }
        }
    }
    return ((BltFn)orig)(p1, p2, p3, chars, len, p6, p7);
}
