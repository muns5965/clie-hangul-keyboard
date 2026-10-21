/*
 * MnKbdHan 키보드 화면 자모 표시 (리소스 KhHk 0) — v2 진단: 표시만, 입력은 영문 그대로
 *
 * 시스템 키보드 화면(SysKeyboardDialog)이 열려 있는 동안 WinDrawChars로 그리는 영문 한 글자(키 글자)를,
 * 한글 모드이면 두벌식 자모(KS X 1001 0xA4 xx)로 바꿔 원래 WinDrawChars로 그린다(대문자 = 기본 자모, 소문자 = cap+shift 쌍자음).
 * v2: 한글 모드 = 우리 표시([8]). v1 실측(T600C): 키보드는 cap을 누르면 키 글자를 먼저 다시 그리고 Graffiti caps를 나중에 바꿔,
 *   그리는 순간의 caps는 직전 상태였다(첫 cap → 영문 대문자, 둘째 cap → 소문자 자리에 쌍자음). 그래서 키보드 화면이 열려 있는
 *   동안 EvtGetEvent에서 펜 누름이 cap 키 위치이면 한글 모드를 바꾼다(펜 이벤트가 다시 그리기보다 먼저, MnSonyKbd v39와 같은 방식).
 *   열 때는 그때의 Graffiti caps로 맞춘다.
 * 2바이트 자모 글자는 MnHangul 그리기 패치가 그리므로 MnHangul이 켜져 있을 때만('MnHg' #21) 바꾼다.
 * KbdScan 실측(T600C 영문 4.1): 키보드 화면은 SysKeyboardDialog로 열리고, 키 글자·입력칸 글자를 WinDrawChars로 그린다.
 *   0 SysKeyboardDialog  1 SysKeyboardDialogV10  — 열려 있는 동안 = 버퍼 [0] 깊이
 *   2 WinDrawChars  3 EvtGetEvent(v2, 펜 누름 위치)
 * cap 키 위치(T600C 영문 키보드, 저해상도 화면 좌표, 화면 사진으로 추정 — 진단 [10]~[12]로 확인): x 0~22, y 107~121
 * 버퍼(앱이 동적 힙에 잡아 오프셋 32에 주소, UInt32 칸):
 *   [0] 깊이, [1] 키보드 화면 연 수, [2] 자모로 바꿔 그린 수, [3] 화면 안 영문 한 글자 그리기 수,
 *   [4] 마지막 영문 한 글자 = 글자<<24 | caps<<16 | temp<<8 | autoShifted, [5] 그 위치 x<<16 | y,
 *   [6] 마지막 판정(1 바꿈, 2 한글 모드 아님, 3 MnHangul 꺼짐, 4 자모 아님), [7] 바꾼 자모 폭<<16 | 영문 폭
 *   [8] 한글 모드, [9] 다음 펜 칸, [10]~[12] 화면 안 펜 누름 3개 x<<16 | y, [13] cap 키 누름 수
 * 어느 앱에서든 불리므로 전역변수·A5·재배치·문자열 상수 금지(빌드 스크립트가 검사).
 */

#include <PalmOS.h>
#include "../common/dubeol_map.h"

void KhKbd(KeyboardType kbd);
void KhKbdV10(void);
void KhDraw(const Char *s, Int16 len, Coord x, Coord y);
void KhGetEvent(EventType *event, Int32 timeout);

asm(".text\n.even\nKhBase:\n"
    " .long KhKbd-KhBase, KhKbdV10-KhBase, KhDraw-KhBase, KhGetEvent-KhBase\n"
    "KhOrig:\n .long 0,0,0,0\nKhBuf:\n .long 0\n");

typedef void (*KbdFn)(KeyboardType);
typedef void (*KbdV10Fn)(void);
typedef void (*CharsFn)(const Char *, Int16, Coord, Coord);
typedef void (*GetFn)(EventType *, Int32);

#define kCapKey(x, y)  ((x) >= 0 && (x) <= 22 && (y) >= 107 && (y) <= 121)

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
        Boolean caps = false, num = false, autoS = false;
        UInt16  temp = 0;

        if (b[0] == 0) {   /* 열 때 한글 모드 = 지금 Graffiti caps */
            GrfGetState(&caps, &num, &temp, &autoS);
            b[8] = caps ? 1 : 0;
        }
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

        GrfGetState(&caps, &num, &temp, &autoS);   /* 기록만(v2: 판정은 [8]) */
        FtrGet('MnHg', 21, &hg);
        b[3]++;
        b[4] = ((UInt32)c << 24) | ((UInt32)(caps ? 1 : 0) << 16) | ((UInt32)(temp & 0xFF) << 8) | (autoS ? 1 : 0);
        b[5] = ((UInt32)(UInt16)x << 16) | (UInt16)y;
        if (b[8] == 0)
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

/* v2: 키보드 화면이 열려 있는 동안 펜 누름 — cap 키면 한글 모드를 바꾼다(키 글자 다시 그리기보다 먼저 지나감) */
void KhGetEvent(EventType *event, Int32 timeout)
{
    UInt32 *b;

    ((GetFn)Orig(3))(event, timeout);
    b = Buf();
    if (b == NULL || b[0] == 0 || event == NULL || event->eType != penDownEvent)
        return;
    {
        UInt16 k = (UInt16)b[9];

        if (k > 2)
            k = 0;
        b[10 + k] = ((UInt32)(UInt16)event->screenX << 16) | (UInt16)event->screenY;
        b[9] = (k + 1 > 2) ? 0 : k + 1;
    }
    if (kCapKey(event->screenX, event->screenY)) {
        b[8] = b[8] ? 0 : 1;
        b[13]++;
    }
}
