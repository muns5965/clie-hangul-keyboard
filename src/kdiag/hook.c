/*
 * KbdScan 기록 코드 (리소스 KdHk 0) — 다른 기종(N700C 등)의 화면 키보드가 어떻게 그리고 입력하는지 보는 진단
 *
 * 화면 동작은 바꾸지 않고 기록만 한 뒤 원래 함수를 부른다. 앱의 [Hook ON]을 눌렀을 때만 걸린다.
 *   0 SysKeyboardDialog   1 SysKeyboardDialogV10      — 키보드 화면이 이 함수로 열리는지, 열려 있는 동안 = "대화상자 안"
 *   2 WinDrawBitmap       3 WinPaintBitmap            — 키보드 그림(크기·위치)
 *   4 WinDrawChars        5 WinPaintChars   6 BltDrawChars(0xA378, 인자 모름 → 세기만) — 키 글자를 글자로 그리는지
 *   7 WinCopyRectangle                                — 키 누름 표시 등
 *   8 EvtGetEvent         9 FldInsert      10 EvtEnqueueKey — 키가 키 이벤트로 지나가는지, 입력칸에 어떻게 들어가는지
 * 대화상자 안에서만 기록(0·1은 항상 셈). 버퍼(앱이 동적 힙에 잡아 오프셋 88에 주소를 씀, UInt32 칸):
 *   [0] 대화상자 깊이, [1] SysKeyboardDialog 수, [2] V10 수, [3] 마지막 키보드 종류
 *   [4 + i] 함수 i가 대화상자 안에서 불린 수(i = 2~10)
 *   [20] 다음 칸, [21 + 2k]·[22 + 2k] 그림 4개: (종류<<31 | 폭<<16 | 높이), (x<<16 | y)
 *   [30] 다음 칸, [31 + 2k]·[32 + 2k] 글자 6개: (종류<<28 | 길이<<16 | x), 앞 4바이트
 *   [44] 다음 칸, [45 + k] 받은 keyDown 8개: chr<<16 | modifiers
 *   [54] 다음 칸, [55 + 2k]·[56 + 2k] FldInsert 4개: 길이, 앞 4바이트
 *   [64] 다음 칸, [65 + k] EvtEnqueueKey 4개: ascii<<16 | modifiers
 *   [70] 대화상자 안 펜 누름 수
 * 어느 앱에서든 불리므로 전역변수·A5·재배치·문자열 상수 금지(빌드 스크립트가 검사).
 */

#include <PalmOS.h>

#define kHookN 11
#define kBufN  72

void KdKbd(KeyboardType kbd);
void KdKbdV10(void);
void KdDrawBmp(BitmapPtr bmp, Coord x, Coord y);
void KdPaintBmp(BitmapPtr bmp, Coord x, Coord y);
void KdDrawChars(const Char *s, Int16 len, Coord x, Coord y);
void KdPaintChars(const Char *s, Int16 len, Coord x, Coord y);
void KdBlt(void);
void KdCopy(WinHandle src, WinHandle dst, const RectangleType *r, Coord x, Coord y, WinDrawOperation mode);
void KdGetEvent(EventType *event, Int32 timeout);
Boolean KdFldInsert(FieldType *fld, const Char *s, UInt16 len);
Err  KdEnqueue(WChar ascii, UInt16 keycode, UInt16 modifiers);

asm(".text\n.even\nKdBase:\n"
    " .long KdKbd-KdBase, KdKbdV10-KdBase, KdDrawBmp-KdBase, KdPaintBmp-KdBase\n"
    " .long KdDrawChars-KdBase, KdPaintChars-KdBase, KdBlt-KdBase, KdCopy-KdBase\n"
    " .long KdGetEvent-KdBase, KdFldInsert-KdBase, KdEnqueue-KdBase\n"
    "KdOrig:\n .long 0,0,0,0,0,0,0,0,0,0,0\nKdBuf:\n .long 0\n"
    /* 6 BltDrawChars: 인자를 모르므로 스택을 건드리지 않고 세기만 한 뒤 원래 함수로 점프(d0·a0은 호출 사이 보존 안 됨) */
    "KdBlt:\n"
    " lea KdBuf(%pc),%a0\n move.l (%a0),%d0\n beq.s 1f\n move.l %d0,%a0\n"
    " tst.l (%a0)\n beq.s 1f\n addq.l #1,40(%a0)\n"
    "1:\n move.l KdOrig+24(%pc),%a0\n jmp (%a0)\n");

typedef void (*KbdFn)(KeyboardType);
typedef void (*KbdV10Fn)(void);
typedef void (*BmpFn)(BitmapPtr, Coord, Coord);
typedef void (*CharsFn)(const Char *, Int16, Coord, Coord);
typedef void (*CopyFn)(WinHandle, WinHandle, const RectangleType *, Coord, Coord, WinDrawOperation);
typedef void (*GetFn)(EventType *, Int32);
typedef Boolean (*InsFn)(FieldType *, const Char *, UInt16);
typedef Err  (*EnqFn)(WChar, UInt16, UInt16);

static inline UInt32 *Buf(void)
{
    UInt32 * const *bp;

    asm("lea KdBuf(%%pc),%0" : "=a" (bp));
    return *bp;
}

static inline UInt32 Orig(UInt16 i)
{
    const UInt32 *p;

    asm("lea KdOrig(%%pc),%0" : "=a" (p));
    return p[i];
}

/* 대화상자 안이면 버퍼, 아니면 NULL */
static inline UInt32 *In(void)
{
    UInt32 *b = Buf();

    return (b != NULL && b[0] != 0) ? b : NULL;
}

static inline UInt32 First4(const Char *s, UInt16 len)
{
    UInt32 v = 0;
    UInt16 i;

    for (i = 0; i < 4; i++)
        v = (v << 8) | ((s != NULL && i < len) ? (UInt8)s[i] : 0);
    return v;
}

static void Kbd(UInt16 fn, UInt16 kbd)
{
    UInt32 *b = Buf();

    if (b != NULL) {
        b[1 + fn]++;
        b[3] = kbd;
        b[0]++;
    }
}

static void KbdEnd(void)
{
    UInt32 *b = Buf();

    if (b != NULL && b[0] != 0)
        b[0]--;
}

void KdKbd(KeyboardType kbd)
{
    Kbd(0, (UInt16)kbd);
    ((KbdFn)Orig(0))(kbd);
    KbdEnd();
}

void KdKbdV10(void)
{
    Kbd(1, 0xFF);
    ((KbdV10Fn)Orig(1))();
    KbdEnd();
}

static void Bmp(UInt16 fn, BitmapPtr bmp, Coord x, Coord y)
{
    UInt32 *b = In();

    if (b != NULL) {
        UInt16 k = (UInt16)(b[20] & 3);

        b[4 + fn]++;
        b[21 + 2 * k] = ((UInt32)(fn == 3 ? 1 : 0) << 31) |
                        (bmp != NULL ? (((UInt32)(UInt16)((const Int16 *)bmp)[0] & 0x7FFF) << 16) |
                                       (UInt16)((const Int16 *)bmp)[1] : 0);
        b[22 + 2 * k] = ((UInt32)(UInt16)x << 16) | (UInt16)y;
        b[20] = k + 1;
    }
}

void KdDrawBmp(BitmapPtr bmp, Coord x, Coord y)
{
    Bmp(2, bmp, x, y);
    ((BmpFn)Orig(2))(bmp, x, y);
}

void KdPaintBmp(BitmapPtr bmp, Coord x, Coord y)
{
    Bmp(3, bmp, x, y);
    ((BmpFn)Orig(3))(bmp, x, y);
}

static void Chars(UInt16 fn, const Char *s, Int16 len, Coord x)
{
    UInt32 *b = In();

    if (b != NULL) {
        UInt16 k = (UInt16)b[30];   /* 나눗셈은 라이브러리 함수를 불러 쓰지 않는다 */

        if (k >= 6)
            k = 0;

        b[4 + fn]++;
        b[31 + 2 * k] = ((UInt32)fn << 28) | ((UInt32)((UInt16)len & 0xFFF) << 16) | (UInt16)x;
        b[32 + 2 * k] = First4(s, len > 0 ? (UInt16)len : 0);
        b[30] = (k + 1 >= 6) ? 0 : k + 1;
    }
}

void KdDrawChars(const Char *s, Int16 len, Coord x, Coord y)
{
    Chars(4, s, len, x);
    ((CharsFn)Orig(4))(s, len, x, y);
}

void KdPaintChars(const Char *s, Int16 len, Coord x, Coord y)
{
    Chars(5, s, len, x);
    ((CharsFn)Orig(5))(s, len, x, y);
}

void KdCopy(WinHandle src, WinHandle dst, const RectangleType *r, Coord x, Coord y, WinDrawOperation mode)
{
    UInt32 *b = In();

    if (b != NULL)
        b[4 + 7]++;
    ((CopyFn)Orig(7))(src, dst, r, x, y, mode);
}

void KdGetEvent(EventType *event, Int32 timeout)
{
    UInt32 *b;

    ((GetFn)Orig(8))(event, timeout);
    b = In();
    if (b == NULL || event == NULL)
        return;
    b[4 + 8]++;
    if (event->eType == penDownEvent)
        b[70]++;
    if (event->eType == keyDownEvent) {
        UInt16 k = (UInt16)(b[44] & 7);

        b[45 + k] = ((UInt32)event->data.keyDown.chr << 16) | event->data.keyDown.modifiers;
        b[44] = k + 1;
    }
}

Boolean KdFldInsert(FieldType *fld, const Char *s, UInt16 len)
{
    UInt32 *b = In();

    if (b != NULL) {
        UInt16 k = (UInt16)(b[54] & 3);

        b[4 + 9]++;
        b[55 + 2 * k] = len;
        b[56 + 2 * k] = First4(s, len);
        b[54] = k + 1;
    }
    return ((InsFn)Orig(9))(fld, s, len);
}

Err KdEnqueue(WChar ascii, UInt16 keycode, UInt16 modifiers)
{
    UInt32 *b = In();   /* 인터럽트에서 불릴 수 있으므로 버퍼에만 쓴다(Feature·메모리 관리자 안 씀) */

    if (b != NULL) {
        UInt16 k = (UInt16)(b[64] & 3);

        b[4 + 10]++;
        b[65 + k] = ((UInt32)ascii << 16) | modifiers;
        b[64] = k + 1;
    }
    return ((EnqFn)Orig(10))(ascii, keycode, modifiers);
}
