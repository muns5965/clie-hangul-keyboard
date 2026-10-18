/*
 * MnHangul 키보드 대화상자 진단 코드 (리소스 HgKd 0) — v28
 *
 * CJKOS는 SysKeyboardDialog를 바꿔 자체 화면 키보드를 띄운다(ClieDiag v22 Trap 실측).
 * 이 코드는 화면 동작을 바꾸지 않고 호출만 기록한 뒤 원래 함수를 부른다.
 *   0 SysKeyboardDialog(KeyboardType kbd)   (0xA2E2)
 *   1 SysKeyboardDialogV10(void)            (0xA0B3)
 * 기록(feature 'MnHg'):
 *   #102 SysKeyboardDialog 호출 수, #103 SysKeyboardDialogV10 호출 수
 *   #104 마지막 호출 = 함수<<24 | kbd 종류<<16 | 폼 있음<<8 | 포커스 있음<<4 | 포커스가 필드
 *   #105 마지막 호출 때 실행 중인 앱 크리에이터
 * v29: 2 EvtEnqueueKey(ascii, keycode, modifiers) (0xA12D) — 가상 키보드 글자가 이 함수로 들어오는지
 *   하드웨어 버튼 등은 인터럽트 안에서 이 함수를 부를 수 있어 Feature(메모리 관리자)를 쓰지 않는다.
 *   앱이 동적 힙에 잡아 둔 버퍼(오프셋 24에 주소)에 바로 쓴다:
 *   [0] 호출 수, [1] 다음 칸 번호, [2 + 2k] 글자<<16 | modifiers, [3 + 2k] 부른 곳(복귀 주소) - 최근 8개(v31)
 * v32: 3 EvtGetEvent(event, timeout) (0xA11D) — 앱에 건네는 keyDown을 원래 함수 다음에 기록
 *   [18] 수, [19] 다음 칸, [20 + 2k] 글자<<16 | modifiers, [21 + 2k] keyCode<<16 | Graffiti 상태
 *   Graffiti 상태(GrfGetState) = capsLock<<12 | numLock<<8 | tempShift<<4 | autoShifted
 * v32: 4 EvtAddEventToQueue(event) (0xA11B) — keyDown을 큐에 넣는 곳(v32 실측: Sony 가상 키보드 글자는 SilkExtn이 여기로)
 *   v33: [36] keyDown 수, [37] 다음 칸, [38 + 2k] 글자<<16 | Graffiti 상태(넣는 순간), [39 + 2k] 부른 곳 — 최근 8개
 *
 * 리소스 배치: 오프셋 0 = 진입점 5개의 오프셋 표, 20 = 원래 함수 주소 5개, 40 = 기록 버퍼 주소(앱이 DmWrite).
 * 어느 앱에서든 불리므로 전역변수·A5·재배치·문자열 상수 금지(빌드 스크립트가 검사).
 */

#include <PalmOS.h>

#define kHgCreator 'MnHg'

void HgKbdDialog(KeyboardType kbd);
void HgKbdDialogV10(void);
Err  HgEnqueueKey(WChar ascii, UInt16 keycode, UInt16 modifiers);
void HgGetEvent(EventType *event, Int32 timeout);
void HgAddEvent(const EventType *event);

asm(".text\n.even\nKbdBase:\n"
    " .long HgKbdDialog-KbdBase, HgKbdDialogV10-KbdBase, HgEnqueueKey-KbdBase\n"
    " .long HgGetEvent-KbdBase, HgAddEvent-KbdBase\n"
    "KbdOrig:\n .long 0,0,0,0,0\nKbdBuf:\n .long 0\n");

typedef void (*KbdFn)(KeyboardType);
typedef void (*KbdV10Fn)(void);
typedef Err  (*EnqFn)(WChar, UInt16, UInt16);
typedef void (*GetFn)(EventType *, Int32);
typedef void (*AddFn)(const EventType *);

static inline UInt32 *Buf(void)
{
    UInt32 * const *bp;

    asm("lea KbdBuf(%%pc),%0" : "=a" (bp));
    return *bp;
}

static inline UInt32 Orig(UInt16 i)
{
    const UInt32 *p;

    asm("lea KbdOrig(%%pc),%0" : "=a" (p));
    return p[i];
}

static inline UInt32 Ftr(UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(kHgCreator, n, &v) != errNone)
        v = 0;
    return v;
}

static void Log(UInt16 fn, UInt16 kbd)
{
    FormType *frm = FrmGetActiveForm();
    UInt32    flags = 0, crid = 0;
    UInt16    card;
    LocalID   id;

    FtrSet(kHgCreator, 102 + fn, Ftr(102 + fn) + 1);
    if (frm != NULL) {
        UInt16 f = FrmGetFocus(frm);

        flags |= 1 << 8;
        if (f != noFocus) {
            flags |= 1 << 4;
            if (FrmGetObjectType(frm, f) == frmFieldObj)
                flags |= 1;
        }
    }
    FtrSet(kHgCreator, 104, ((UInt32)fn << 24) | ((UInt32)(kbd & 0xFF) << 16) | flags);
    if (SysCurAppDatabase(&card, &id) == errNone)
        DmDatabaseInfo(card, id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &crid);
    FtrSet(kHgCreator, 105, crid);
}

void HgKbdDialog(KeyboardType kbd)
{
    Log(0, (UInt16)kbd);
    ((KbdFn)Orig(0))(kbd);
}

void HgKbdDialogV10(void)
{
    Log(1, 0xFF);
    ((KbdV10Fn)Orig(1))();
}

Err HgEnqueueKey(WChar ascii, UInt16 keycode, UInt16 modifiers)
{
    UInt32 * const *bp;
    UInt32         *b;

    asm("lea KbdBuf(%%pc),%0" : "=a" (bp));
    b = *bp;
    if (b != NULL) {
        UInt16 k = (UInt16)(b[1] & 7);

        b[0]++;
        b[2 + 2 * k] = ((UInt32)ascii << 16) | modifiers;
        b[3 + 2 * k] = (UInt32)__builtin_return_address(0);
        b[1] = k + 1;
    }
    return ((EnqFn)Orig(2))(ascii, keycode, modifiers);
}

void HgGetEvent(EventType *event, Int32 timeout)
{
    UInt32 *b;

    ((GetFn)Orig(3))(event, timeout);
    b = Buf();
    if (b != NULL && event != NULL && event->eType == keyDownEvent) {
        UInt16  k = (UInt16)(b[19] & 7), temp = 0;
        Boolean caps = false, num = false, autoS = false;

        GrfGetState(&caps, &num, &temp, &autoS);
        b[18]++;
        b[20 + 2 * k] = ((UInt32)event->data.keyDown.chr << 16) | event->data.keyDown.modifiers;
        b[21 + 2 * k] = ((UInt32)event->data.keyDown.keyCode << 16) | ((caps ? 1 : 0) << 12) |
                        ((num ? 1 : 0) << 8) | ((temp & 0xF) << 4) | (autoS ? 1 : 0);
        b[19] = k + 1;
    }
}

void HgAddEvent(const EventType *event)
{
    UInt32 *b = Buf();

    if (b != NULL && event != NULL && event->eType == keyDownEvent) {
        UInt16  k = (UInt16)(b[37] & 7), temp = 0;
        Boolean caps = false, num = false, autoS = false;

        GrfGetState(&caps, &num, &temp, &autoS);
        b[36]++;
        b[38 + 2 * k] = ((UInt32)event->data.keyDown.chr << 16) | ((caps ? 1 : 0) << 12) | ((num ? 1 : 0) << 8) |
                        ((temp & 0xF) << 4) | (autoS ? 1 : 0);
        b[39 + 2 * k] = (UInt32)__builtin_return_address(0);
        b[37] = k + 1;
    }
    ((AddFn)Orig(4))(event);
}
