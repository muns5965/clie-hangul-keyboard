/*
 * MnHangul5 (Palm OS 5, TG50·NX70V) ARM 부분 공용 정의.
 *
 * OS 5에서는 시스템 함수가 RAM에 있는 ARM 함수 주소표(r9 - 8 = 표 8, r9 - 12 = 표 12)를 거쳐 불린다.
 * 68k 앱의 트랩도 PACE 연결 함수(glue)가 이 표로 부르므로, 표의 칸을 바꾸면 68k·ARM 호출이 모두 바뀐다.
 * (68k SysSetTrapAddress는 OS 5에서 sysErrNotAllowed — docs/OS5_RESEARCH.md)
 *
 * 코드 규칙(tools/build-os5.sh가 검사):
 *   - 앱이 동적 힙(소유자 0)에 복사해 실행한다 → 전역 데이터·상수 표 금지. 상태는 ctx(PC 상대, CtxPtr)에만.
 *   - -ffixed-r9 (r9 = 시스템 표), -fno-tree-switch-conversion (큰 switch가 절대 주소 상수 표가 되지 않게).
 *   - 칸 번호는 TG50·NX70V ROM(Palm OS 5.0 영문)에서 같다. 앱이 설치 전에 원래 값이 알려진 값인지 확인한다(slots.txt).
 */
#ifndef OS5_H
#define OS5_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef short s16;
typedef int s32;
typedef u16 WChar;
typedef u8 Boolean;

#ifndef MN_DIAG
#define MN_DIAG 1
#endif

/* 바꾸는 함수(원래 주소 칸 번호). 순서 = slots.txt 순서(빌드 스크립트가 확인) */
enum { O_DrawChars, O_DrawChar, O_PaintChars, O_DrawTrunc, O_CharsWidth, O_LineWidth, O_CharsInWidth,
       O_WordWrap, O_WidthToOffset, O_ByteAttr, O_CharSize, O_GetNext, O_GetPrev, O_SetNext,
       O_Bounds, O_GetChar, O_CharAttr, O_CharWidth, O_IsValid, O_FindString, O_WordBounds, O_Compare, O_Caseless, O_GetEvent, O_COUNT };
typedef char OrigFits[(O_COUNT <= 24) ? 1 : -1];   /* Ctx.orig 칸 수 */

/* 앱(app.c)이 같은 오프셋으로 쓴다 — 바꾸면 app.c의 kCtx*도 같이 */
struct Ctx {
    u32 orig[24];        /* +0   원래 함수 주소 (O_*) */
    const u8 *pack;      /* +96  조합형 글꼴 묶음(HgJ16) */
    const u8 *ksc;       /* +100 KS X 1001 판정표: 비트 1398바이트 + 누계 44개(16비트 빅엔디언) */
    u32 hits;            /* +104 그린 한글 수(진단) */
    u32 mark;            /* +108 '한' 표시: 1 다시 그림, 4 그려져 있음 */
    u32 st[8];           /* +112 입력 상태(ime.c) */
    u32 ring[24];        /* +144 최근 키 기록(진단 빌드) */
    u32 rpos;            /* +240 */
    u32 idle;            /* +244 1 = 쉬는 중(모든 함수가 원래 함수로 넘김) — 표를 되돌릴 수 없을 때의 OFF */
    const u8 *pack24;    /* +248 24×24 조합형 묶음(HgJ24, 큰 글꼴용, v3) */
    u32 bmp[30];         /* +252 글자 그림(BitmapTypeV3 머리 24바이트 + 24줄 × 4바이트, draw.c) */
    u32 qfail;           /* +372 조합 결과 키를 큐에 못 넣은 수(v5, ime.c) */
};
struct Ctx *CtxPtr(void);

static inline u32 R9(void) {
    u32 r;
    __asm__ volatile("mov %0, r9" : "=r"(r));
    return r;
}
#define T8(slot)  (*(u32 *)(*(u32 *)(R9() - 8) + (slot)))
#define T12(slot) (*(u32 *)(*(u32 *)(R9() - 12) + (slot)))
#define ORIG(i)   (CtxPtr()->orig[i])
#define Idle()    (CtxPtr()->idle != 0)

/* 표 8 (TG50·NX70V 같음) — 부르기만 하는 함수 */
#define T8_EvtEnqueueKey          0x298
#define T8_FntBaseLine            0x3e0
#define T8_FntGetFont             0x3fc
#define T8_FntLineHeight          0x408
#define T8_FntSetFont             0x410
#define T8_WinEraseRectangle      0xb50
#define T8_WinGetActiveWindow     0xb64
#define T8_WinPaintBitmap         0xbc8
#define T8_WinSetDrawMode         0xc34
#define T8_WinSetDrawWindow       0xc38
#define T8_WinSetForeColor        0xc3c
#define T8_WinSetTextColor        0xc4c
#define T8_WinSetCoordinateSystem 0xc98
/* 표 12 (TG50·NX70V 같음) */
#define T12_FrmGetActiveForm      0x224
#define T12_FrmGetNumberOfObjects 0x250
#define T12_FrmGetObjectBounds    0x254
#define T12_FrmGetObjectType      0x26c
#define T12_FrmGetWindowHandle    0x278

/* 한글 한 칸(표준 좌표, 2배 밀도): 16×16 글자 = 9(기기 18, 왼쪽 1 띄움), 24×24 글자 = 12(기기 24) */
#define HW 9
#define HW24 12
#define kBigLine 14      /* 줄 높이가 이 이상(큰 글꼴 14·큰 굵은 15)이면 24×24 — OS 4 MnHangul v22와 같은 기준 */

static inline int Hi(u8 b) { return b >= 0xA1 && b <= 0xFE; }

typedef void (*DrawCharsFn)(const char *, s16, s16, s16);
typedef s16 (*CharsWidthFn)(const char *, s16);

void HgDrawChars(const char *chars, s16 len, s16 x, s16 y);   /* draw.c — ime.c의 '한' 표시도 씀 */
u16 HangulCell(void);                                          /* draw.c: 지금 글꼴의 한글 칸 폭(표준 좌표) */

#endif
