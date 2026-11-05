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
 *     v17: VZ90 영문 Mini(Palm OS 5.2.1, CloudpilotEmu PV)도 칸 번호가 같다(PACE 연결 함수 740개 대조).
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
       O_Bounds, O_GetChar, O_CharAttr, O_CharWidth, O_IsValid, O_FindString, O_WordBounds, O_Compare, O_Caseless, O_GetEvent, O_COUNT,
       O_PaintChar = 98 };   /* v25: orig[24]가 꽉 차 원래 주소는 ctx +392(pcorig) — 칸 표의 저장 번호(×4 = ctx 오프셋) */
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
    u32 kbd;             /* +376 v17: 키보드 대화상자 한/영 칸을 다음 이벤트 전에 그림(ime.c) */
    u32 kshift;          /* +380 v17: 키보드 대화상자에서 손으로 누른 shift(다음 영문자까지) — 아니면 대문자도 기본 자모 */
    u32 markwin;         /* +384 v19: GSI '한'을 그린 창(그 창에서만 지운다, 새 폼이 열리면 0) */
    u32 pen;             /* +388 v23: 앱 화면(실크 영역 밖) 펜 누름 수 — Feature 'MnH5' #10이 이 칸 주소(MnSk5가 읽음) */
    u32 pcorig;          /* +392 v25: WinPaintChar 원래 주소(O_PaintChar = 98 = 392/4, 설치 코드가 칸 표의 저장 번호로 씀) */
    u32 sklo, skhi;      /* +396 v25: Sony 표준 입력(StdInput, 'aSkw'/'SkAi') 코드 리소스 'amdc' 0의 범위 — 앱이 켤 때 채움, 없으면 0 */
    u32 raw;             /* +404 v26: StdInput이 부른 글자 그리기 원래 함수를 도는 동안 1 — 글자 경계 함수가 한글로 읽지 않음(text.c Off) */
    u32 skr;             /* +408 v27: Sony 자판 다시 그리기 — 1 다음 이벤트 전에, 2 하드웨어 Shift를 뗀 뒤(ime.c SkRedraw) */
    u32 skhold;          /* +412 v27: 넣은 vchrSilkResize 중 아직 지나가지 않은 수 — 그동안 '한'을 그리지 않음 */
    u32 skbpen;          /* +416 v29: 비트 8 = StdInput이 'm'을 그림(그 뒤 한/A 단추를 그림). v31: 단추 펜을 막던 비트 0은 쓰지 않음 */
    u32 skchk;           /* +420 v31: 한/A 단추 누름 때 화면 버퍼 확인 결과(ime.c SkBtnTap) — 설정 앱 Debug 화면 */
#ifdef MN_TEST_STALE
    u32 test;            /* 시험 빌드만 */
#endif
};
struct Ctx *CtxPtr(void);
#define kCtxSpace 432   /* v29: 416 → 432. ctx.S의 .space와 같게. v19: 384를 넘겨 뒤 코드를 덮었던 일이 있어 컴파일 때 확인 */
typedef char CtxFits[(sizeof(struct Ctx) <= kCtxSpace) ? 1 : -1];
typedef char PcOrigAt[(__builtin_offsetof(struct Ctx, pcorig) == 4 * O_PaintChar) ? 1 : -1];   /* v25 */
typedef char SkLoAt[(__builtin_offsetof(struct Ctx, sklo) == 396) ? 1 : -1];                  /* app.c kCtxSkLo */
typedef char SkChkAt[(__builtin_offsetof(struct Ctx, skchk) == 420) ? 1 : -1];                /* app.c kCtxSkChk */

static inline u32 R9(void) {
    u32 r;
    __asm__ volatile("mov %0, r9" : "=r"(r));
    return r;
}
#define T8(slot)  (*(u32 *)(*(u32 *)(R9() - 8) + (slot)))
#define T12(slot) (*(u32 *)(*(u32 *)(R9() - 12) + (slot)))
#define ORIG(i)   (CtxPtr()->orig[i])
#define Idle()    (CtxPtr()->idle != 0)
/* v26: 부른 곳(돌아갈 주소)이 Sony 표준 입력(StdInput) 코드 안인가 — 그 글자는 한글로 읽지 않는다(영문 ROM かな·記号 탭의 Latin-1) */
static inline int InStdInput(u32 lr) {
    struct Ctx *c = CtxPtr();
    return c->sklo && lr >= c->sklo && lr < c->skhi;
}
#define FromStdInput() InStdInput((u32)__builtin_return_address(0))

/* 표 8 (TG50·NX70V 같음) — 부르기만 하는 함수 */
#define T8_EvtEnqueueKey          0x298
#define T8_FntBaseLine            0x3e0
#define T8_FntGetFont             0x3fc
#define T8_FntLineHeight          0x408
#define T8_FntSetFont             0x410
#define T8_WinDrawLine            0xb28   /* v20 */
#define T8_WinDrawGrayLine        0xb1c   /* v28: 68k 트랩 0xA214 */
#define T8_WinSetUnderlineMode    0xc54   /* v28: 68k 트랩 0xA225 */
#define T8_WinGetDisplayWindow    0xb74   /* v29: 68k 트랩 0xA200 */
#define T8_WinGetBitmap           0xb68   /* v29: 0xA3A2 */
#define T8_BmpGetBits             0x05c   /* v29: 0xA376 */
#define T8_BmpGetBitDepth         0x058   /* v29: 0xA44F */
#define T8_BmpGetDimensions       0x064   /* v29: 0xA44E */
#define T8_WinIndexToRGB          0xba4   /* v29: 0xA39F */
#define T8_WinGetPixel            0xb90   /* v31: 0xA381 */
#define T8_WinGetPixelRGB         0xb94   /* v31: 0xA3EB */
/* v29: Sony 자판 탭 줄 빈자리의 한/A 단추(docs/OS5_RESEARCH.md 15절). VZ90 `コード` 오른쪽·NX70V `sym` 오른쪽 — 두 기종 같은 자리.
 *   그리기는 StdInput 화면 밖 창 좌표(기기 픽셀, 화면 y − 약 325), 누름은 표준 좌표 */
#define kSkBtnCX    265   /* v30: 단추 칸 가운데(빈자리 256~274의 가운데) */
#define kSkBtnCY    118   /* v30: 탭 줄(화면 426~449) 가운데 437.5 − 화면 밖 창과의 차 320 */
#define kSkBtnL     127
#define kSkBtnR     139
#define kSkBtnT     213
#define kSkBtnB     226
void SkButtonDraw(void);
#define T8_WinEraseRectangle      0xb50
#define T8_WinPopDrawState        0xbf0   /* v20 */
#define T8_WinPushDrawState       0xbf4   /* v20 */
#define T8_WinGetActiveWindow     0xb64
#define T8_WinGetDisplayExtent    0xb70   /* v23: 앱 화면 크기(실크 최소화면 커짐), 68k 트랩 0xA20B */
#define T8_WinPaintBitmap         0xbc8
#define T8_WinSetDrawMode         0xc34
#define T8_WinSetDrawWindow       0xc38
#define T8_WinSetBackColor        0xc24   /* v32: 68k 트랩 0xA399 */
#define T8_WinSetForeColor        0xc3c
#define T8_WinSetTextColor        0xc4c
#define T8_WinSetCoordinateSystem 0xc98
/* 표 12 (TG50·NX70V 같음) */
#define T12_FrmGetActiveForm      0x224
#define T12_FrmGetFocus           0x238   /* v15: 0x238·0x240은 Frm 함수 알파벳 순서로 정하고 진단 빌드로 확인(제목·폼 번호·초점 객체가 화면과 맞음) */
#define T12_FrmGetFormId          0x240
#define T12_FrmGetNumberOfObjects 0x250
#define T12_FrmGetObjectBounds    0x254
#define T12_FrmGetObjectType      0x26c
#define T12_FrmGetWindowHandle    0x278
#define T12_FrmUpdateForm         0x2f0   /* v17: 키 글자 다시 그리기(frmUpdateEvent) */
#define T12_UIColorGetTableEntryIndex 0x540   /* v20: 단추 테두리 색(UIObjectFrame) */

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
