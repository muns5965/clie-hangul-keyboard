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
 *   [0] 호출 수, [1] 마지막 = 글자<<16 | modifiers, [2] 부른 곳(복귀 주소), [3] keycode
 *
 * 리소스 배치: 오프셋 0 = 진입점 3개의 오프셋 표, 12 = 원래 함수 주소 3개, 24 = 기록 버퍼 주소(앱이 DmWrite).
 * 어느 앱에서든 불리므로 전역변수·A5·재배치·문자열 상수 금지(빌드 스크립트가 검사).
 */

#include <PalmOS.h>

#define kHgCreator 'MnHg'

void HgKbdDialog(KeyboardType kbd);
void HgKbdDialogV10(void);
Err  HgEnqueueKey(WChar ascii, UInt16 keycode, UInt16 modifiers);

asm(".text\n.even\nKbdBase:\n"
    " .long HgKbdDialog-KbdBase, HgKbdDialogV10-KbdBase, HgEnqueueKey-KbdBase\n"
    "KbdOrig:\n .long 0,0,0\nKbdBuf:\n .long 0\n");

typedef void (*KbdFn)(KeyboardType);
typedef void (*KbdV10Fn)(void);
typedef Err  (*EnqFn)(WChar, UInt16, UInt16);

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
        b[0]++;
        b[1] = ((UInt32)ascii << 16) | modifiers;
        b[2] = (UInt32)__builtin_return_address(0);
        b[3] = keycode;
    }
    return ((EnqFn)Orig(2))(ascii, keycode, modifiers);
}
