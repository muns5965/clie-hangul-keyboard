/*
 * MnHangul 패치 코드 (리소스 HgPt 0) — 2a 단계: BltDrawChars(트랩 0xA378) 통과 시험
 *
 * BltDrawChars는 SDK에 원형이 없는 내부 함수다. ROM 호출부(0x50D8C, 0x2BC4B8) 역분석으로 본 인자(34바이트 중 26):
 *   p1 포인터(그리기 상태로 추정), p2 사각형 포인터, p3 점 포인터, p4 문자열, p5 길이(16비트),
 *   p6 글꼴 구조체 포인터(FntGetFontPtr 결과), p7 포인터(글꼴 보조로 추정)
 * 2a: 인자를 feature('MnHg')에 기록하고 원래 함수를 그대로 부른다. 화면이 바뀌지 않아야 정상.
 *
 * 리소스 배치: 오프셋 0 = 원래 함수 주소 칸(4바이트, 앱이 DmWrite로 기록), 오프셋 4 = 진입점 PatchBlt.
 * 어느 앱에서든 불리므로 전역변수·A5·재배치 금지(빌드 스크립트가 검사).
 *
 * 기록 (feature 'MnHg'):
 *   #0 호출 수, #1 마지막 길이, #2 마지막 첫 4바이트, #3 p6, #4 p2 사각형 (x<<16|y), #5 (w<<16|h),
 *   #6 p3 점 (x<<16|y), #7 p1, #8 p7,
 *   #9 0x80 이상 바이트가 든 호출 수, #10~#13 그 마지막 호출의 길이·첫 4바이트·사각형 xy·wh
 */

#include <PalmOS.h>

#define kHgCreator 'MnHg'

typedef UInt32 (*BltFn)(void *p1, void *p2, void *p3, const UInt8 *chars, UInt16 len, void *p6, void *p7);

/* 오프셋 0: 원래 BltDrawChars 주소 (앱이 설치 때 기록) */
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

    if (FtrGet(kHgCreator, n, &v) != errNone)
        v = 0;
    return v;
}

static inline UInt32 First4(const UInt8 *c, UInt16 len)
{
    UInt32 v = 0;
    UInt16 i;

    for (i = 0; i < 4; i++)
        v = (v << 8) | ((c != NULL && i < len) ? c[i] : 0);
    return v;
}

static inline UInt32 Pair(const Int16 *p)
{
    return (p == NULL) ? 0xFFFFFFFFUL : (((UInt32)(UInt16)p[0] << 16) | (UInt16)p[1]);
}

UInt32 PatchBlt(void *p1, void *p2, void *p3, const UInt8 *chars, UInt16 len, void *p6, void *p7)
{
    UInt32 orig = OrigAddr();
    UInt16 i;
    Boolean hi = false;

    FtrSet(kHgCreator, 0, GetF(0) + 1);
    FtrSet(kHgCreator, 1, len);
    FtrSet(kHgCreator, 2, First4(chars, len));
    FtrSet(kHgCreator, 3, (UInt32)p6);
    FtrSet(kHgCreator, 4, Pair((const Int16 *)p2));
    FtrSet(kHgCreator, 5, (p2 == NULL) ? 0xFFFFFFFFUL : Pair((const Int16 *)p2 + 2));
    FtrSet(kHgCreator, 6, Pair((const Int16 *)p3));
    FtrSet(kHgCreator, 7, (UInt32)p1);
    FtrSet(kHgCreator, 8, (UInt32)p7);
    for (i = 0; chars != NULL && i < len; i++)
        if (chars[i] >= 0x80) {
            hi = true;
            break;
        }
    if (hi) {
        FtrSet(kHgCreator, 9, GetF(9) + 1);
        FtrSet(kHgCreator, 10, len);
        FtrSet(kHgCreator, 11, First4(chars, len));
        FtrSet(kHgCreator, 12, Pair((const Int16 *)p2));
        FtrSet(kHgCreator, 13, (p2 == NULL) ? 0xFFFFFFFFUL : Pair((const Int16 *)p2 + 2));
    }
    if (orig == 0)
        return 0;
    return ((BltFn)orig)(p1, p2, p3, chars, len, p6, p7);
}
