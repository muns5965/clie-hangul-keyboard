/*
 * MnKbdHan 키보드 화면 자모 표시 (리소스 KhHk 0) — v4 진단: 표시만, 입력은 영문 그대로
 *
 * 시스템 키보드 화면(SysKeyboardDialog)이 열려 있는 동안 WinDrawChars로 그리는 영문 한 글자(키 글자)를,
 * 한글 모드이면 두벌식 자모(KS X 1001 0xA4 xx, MnHangul이 그림)로 바꿔 원래 WinDrawChars로 그린다.
 * v4: PC 두벌식과 같은 규칙 — 소문자 = 기본 자모, 대문자(shift 한 글자·cap) = 쌍자음 ㅃㅉㄸㄲㅆ·ㅒㅖ(나머지는 기본 자모).
 *   한글 모드는 cap과 따로, 아래 줄 Int'l 오른쪽 빈칸(kHanKey)을 누르면 바뀐다(키보드 화면을 닫아도 유지).
 *   v3 실측(T600C): Palm 키보드는 cap이 켜진 채 shift를 누르면 cap이 꺼져 "cap+shift = 쌍자음"(MnSonyKbd 규칙)을 쓸 수 없음.
 *   전환 직후 키보드가 키 글자를 다시 그리게 할 공개 방법이 없어, 이번 화면에서 그린 키 글자 위치(표 [16]~)를 기억해 두었다가
 *   그 자리를 지우고 우리가 다시 그린다. 빈칸 누름은 nilEvent로 바꿔 키보드에 넘기지 않는다.
 * v2·v3: 펜이 cap 키를 누르면 한글 모드(v1 실측: 그리는 순간의 Graffiti caps가 한 박자 늦음). v3에서 좌표를 창 기준으로 고침.
 * 좌표는 모두 키보드 화면 창 기준(v3 실측: z~m 줄 글자 y 95). 빈칸은 GSI(⇧ 표시, 약 x 135~)를 피해 x 110~131, y 121~141.
 *   0 SysKeyboardDialog  1 SysKeyboardDialogV10  — 열려 있는 동안 = 버퍼 [0] 깊이
 *   2 WinDrawChars  3 EvtGetEvent(펜 누름 위치, 한/영 빈칸)
 * 버퍼(앱이 동적 힙에 잡아 오프셋 32에 주소, UInt32 칸):
 *   [0] 깊이, [1] 키보드 화면 연 수, [2] 자모로 바꿔 그린 수, [3] 화면 안 영문 한 글자 그리기 수,
 *   [4] 마지막 영문 한 글자 = 글자<<24 | caps<<16 | temp<<8 | autoShifted, [5] 그 위치 x<<16 | y,
 *   [6] 마지막 판정(1 바꿈, 2 한글 모드 아님, 3 MnHangul 꺼짐, 4 자모 아님), [7] 바꾼 자모 폭<<16 | 영문 폭
 *   [8] 한글 모드, [9] 다음 펜 칸, [10]~[12] 화면 안 펜 누름 3개 x<<16 | y, [13] 한/영 빈칸 누름 수,
 *   [14] 키 글자 표 수, [15] 한/영 표시를 그려야 함, [16 + 2k]·[17 + 2k] 키 글자 표(최대 40): x<<16 | y, 글자<<24 | 글꼴
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

#define kHanKey(x, y)  ((x) >= 110 && (x) <= 131 && (y) >= 121 && (y) <= 141)
#define kMarkX   113
#define kMarkY   126
#define kTabN    40

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
        if (b[0] == 0) {   /* 새로 열림: 키 글자 표 비우고 한/영 표시 그리기 예약 */
            b[14] = 0;
            b[15] = 1;
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

static inline Boolean Letter(UInt8 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* v4: 소문자 = 기본 자모, 대문자 = 쌍자음(Dubeol 표는 MnSonyKbd 규칙이라 대·소문자를 바꿔 묻는다) */
static inline UInt8 Jamo(UInt8 c)
{
    return Dubeol((UInt8)(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c - 'A' + 'a'));
}

/* 키 글자 표에 위치·글자 기억(같은 자리는 덮어씀) */
static void Remember(UInt32 *b, UInt8 c, Coord x, Coord y)
{
    UInt32 xy = ((UInt32)(UInt16)x << 16) | (UInt16)y, v = ((UInt32)c << 24) | (UInt8)FntGetFont();
    UInt16 k, n = (UInt16)b[14];

    for (k = 0; k < n; k++)
        if (b[16 + 2 * k] == xy) {
            b[17 + 2 * k] = v;
            return;
        }
    if (n < kTabN) {
        b[16 + 2 * n] = xy;
        b[17 + 2 * n] = v;
        b[14] = n + 1;
    }
}

/* 키 글자 하나 그리기(한글 모드면 자모). 키 가운데에 맞춘다 */
static void DrawKey(UInt32 *b, UInt8 c, Coord x, Coord y)
{
    UInt8   j = Jamo(c);
    UInt32  hg = 0;
    Boolean caps = false, num = false, autoS = false;
    UInt16  temp = 0;
    Char    one = (Char)c;

    GrfGetState(&caps, &num, &temp, &autoS);
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
        ((CharsFn)Orig(2))(jm, 2, x + (w1 - w2) / 2, y);
        return;
    }
    ((CharsFn)Orig(2))(&one, 1, x, y);
}

void KhDraw(const Char *s, Int16 len, Coord x, Coord y)
{
    UInt32 *b = Buf();

    if (b != NULL && b[0] != 0 && len == 1 && s != NULL && Letter((UInt8)s[0])) {
        Remember(b, (UInt8)s[0], x, y);
        DrawKey(b, (UInt8)s[0], x, y);
        return;
    }
    ((CharsFn)Orig(2))(s, len, x, y);
}

/* 한/영 표시(빈칸 왼쪽): 한글 모드 '한'(C7D1, MnHangul이 그림), 영문 'A' */
static void Mark(UInt32 *b)
{
    RectangleType r;
    Char          m[2];

    RctSetRectangle(&r, 110, 121, 22, 20);
    WinEraseRectangle(&r, 0);
    if (b[8] != 0) {
        m[0] = (Char)0xC7;
        m[1] = (Char)0xD1;
        ((CharsFn)Orig(2))(m, 2, kMarkX, kMarkY);
    } else {
        m[0] = 'A';
        ((CharsFn)Orig(2))(m, 1, kMarkX + 2, kMarkY);
    }
}

/* 전환 직후: 기억해 둔 키 글자 자리를 지우고 새 모드로 다시 그린다 */
static void Redraw(UInt32 *b)
{
    UInt16 k, n = (UInt16)b[14];
    FontID f0 = FntGetFont();

    for (k = 0; k < n; k++) {
        UInt32        xy = b[16 + 2 * k], v = b[17 + 2 * k];
        Coord         x = (Coord)(Int16)(xy >> 16), y = (Coord)(Int16)xy;
        UInt8         c = (UInt8)(v >> 24);
        RectangleType r;
        Char          jm[2];
        Int16         w1, w2, l;

        FntSetFont((FontID)(v & 0xFF));
        w1 = FntCharWidth((Char)c);
        jm[0] = (Char)0xA4;
        jm[1] = (Char)(Jamo(c) ? Jamo(c) : 0xA1);
        w2 = FntCharsWidth(jm, 2);
        l = (w1 - w2) / 2 < 0 ? (w1 - w2) / 2 : 0;
        RctSetRectangle(&r, x + l, y, (w1 > w2 ? w1 : w2), FntLineHeight());
        WinEraseRectangle(&r, 0);
        DrawKey(b, c, x, y);
    }
    FntSetFont(f0);
}

void KhGetEvent(EventType *event, Int32 timeout)
{
    UInt32 *b;

    ((GetFn)Orig(3))(event, timeout);
    b = Buf();
    if (b == NULL || b[0] == 0 || event == NULL)
        return;
    if (b[15] != 0 && event->eType != penDownEvent) {   /* 열린 뒤 첫 이벤트 때 한/영 표시 */
        b[15] = 0;
        Mark(b);
    }
    if (event->eType != penDownEvent && event->eType != penMoveEvent && event->eType != penUpEvent)
        return;
    if (event->eType == penDownEvent) {
        UInt16 k = (UInt16)b[9];

        if (k > 2)
            k = 0;
        b[10 + k] = ((UInt32)(UInt16)event->screenX << 16) | (UInt16)event->screenY;
        b[9] = (k + 1 > 2) ? 0 : k + 1;
    }
    if (kHanKey(event->screenX, event->screenY)) {
        if (event->eType == penDownEvent) {
            b[8] = b[8] ? 0 : 1;
            b[13]++;
            Redraw(b);
            Mark(b);
        }
        event->eType = nilEvent;   /* 키보드에는 넘기지 않음 */
    }
}
