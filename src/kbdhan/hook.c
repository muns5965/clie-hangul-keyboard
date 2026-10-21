/*
 * MnKbdHan 키보드 화면 자모 표시 (리소스 KhHk 0) — v1 진단: 표시만, 입력은 영문 그대로
 *
 * 시스템 키보드 화면(SysKeyboardDialog)이 열려 있는 동안 WinDrawChars로 그리는 영문 한 글자(키 글자)를,
 * 쉬프트 고정(cap, Graffiti caps) 상태이면 두벌식 자모(KS X 1001 0xA4 xx)로 바꿔 원래 WinDrawChars로 그린다.
 * 2바이트 자모 글자는 MnHangul 그리기 패치가 그리므로 MnHangul이 켜져 있을 때만('MnHg' #21) 바꾼다.
 * KbdScan 실측(T600C 영문 4.1): 키보드 화면은 SysKeyboardDialog로 열리고, 키 글자·입력칸 글자를 WinDrawChars로 그린다.
 *   0 SysKeyboardDialog  1 SysKeyboardDialogV10  — 열려 있는 동안 = 버퍼 [0] 깊이
 *   2 WinDrawChars
 * 버퍼(앱이 동적 힙에 잡아 오프셋 24에 주소, UInt32 칸):
 *   [0] 깊이, [1] 키보드 화면 연 수, [2] 자모로 바꿔 그린 수, [3] 화면 안 영문 한 글자 그리기 수,
 *   [4] 마지막 영문 한 글자 = 글자<<24 | caps<<16 | temp<<8 | autoShifted, [5] 그 위치 x<<16 | y,
 *   [6] 마지막 판정(1 바꿈, 2 cap 아님, 3 MnHangul 꺼짐, 4 자모 아님), [7] 바꾼 자모 폭<<16 | 영문 폭
 * 어느 앱에서든 불리므로 전역변수·A5·재배치·문자열 상수 금지(빌드 스크립트가 검사).
 */

#include <PalmOS.h>
#include "../common/dubeol_map.h"

void KhKbd(KeyboardType kbd);
void KhKbdV10(void);
void KhDraw(const Char *s, Int16 len, Coord x, Coord y);

asm(".text\n.even\nKhBase:\n"
    " .long KhKbd-KhBase, KhKbdV10-KhBase, KhDraw-KhBase\n"
    "KhOrig:\n .long 0,0,0\nKhBuf:\n .long 0\n");

typedef void (*KbdFn)(KeyboardType);
typedef void (*KbdV10Fn)(void);
typedef void (*CharsFn)(const Char *, Int16, Coord, Coord);

static inline UInt32 *Buf(void)
{
    UInt32 * const *bp;

    asm("lea KhBuf(%%pc),%0" : "=a" (bp));
    return *bp;
}

static inline UInt32 Orig(UInt16 i)
{
    const UInt32 *p;

    asm("lea KhOrig(%%pc),%0" : "=a" (p));
    return p[i];
}

static void Enter(void)
{
    UInt32 *b = Buf();

    if (b != NULL) {
        b[0]++;
        b[1]++;
    }
}

static void Leave(void)
{
    UInt32 *b = Buf();

    if (b != NULL && b[0] != 0)
        b[0]--;
}

void KhKbd(KeyboardType kbd)
{
    Enter();
    ((KbdFn)Orig(0))(kbd);
    Leave();
}

void KhKbdV10(void)
{
    Enter();
    ((KbdV10Fn)Orig(1))();
    Leave();
}

void KhDraw(const Char *s, Int16 len, Coord x, Coord y)
{
    UInt32 *b = Buf();

    if (b != NULL && b[0] != 0 && len == 1 && s != NULL &&
        ((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z'))) {
        UInt8   c = (UInt8)s[0], j = Dubeol(c);
        Boolean caps = false, num = false, autoS = false;
        UInt16  temp = 0;
        UInt32  hg = 0;

        GrfGetState(&caps, &num, &temp, &autoS);
        FtrGet('MnHg', 21, &hg);
        b[3]++;
        b[4] = ((UInt32)c << 24) | ((UInt32)(caps ? 1 : 0) << 16) | ((UInt32)(temp & 0xFF) << 8) | (autoS ? 1 : 0);
        b[5] = ((UInt32)(UInt16)x << 16) | (UInt16)y;
        if (!caps)
            b[6] = 2;
        else if (hg == 0)
            b[6] = 3;
        else if (j == 0)
            b[6] = 4;
        else {
            Char  jm[2];
            Int16 w1 = FntCharWidth((Char)c), w2;

            jm[0] = (Char)0xA4;
            jm[1] = (Char)j;
            w2 = FntCharsWidth(jm, 2);
            b[6] = 1;
            b[7] = ((UInt32)(UInt16)w2 << 16) | (UInt16)w1;
            b[2]++;
            ((CharsFn)Orig(2))(jm, 2, x + (w1 - w2) / 2, y);   /* 키 글자 가운데에 맞춤 */
            return;
        }
    }
    ((CharsFn)Orig(2))(s, len, x, y);
}
