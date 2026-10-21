/*
 * MnHangul v67 · MnSonyKbd v19 공용: 시스템 함수 맨 위에 있는 패치가 누구인지 (지우기 거부 안내용)
 *
 * 맨 위 주소 t가 상대 모듈의 잠긴 코드 리소스 범위(Feature에 적어 둔 코드 주소 ~ + MemHandleSize) 안이면 그 모듈.
 *   MnHangul('MnHg'): 그리기 #21(진입점, 코드 = -4)/#20 핸들, Intl #46/#45, 폭 #48/#47, 키보드 진단 #101/#100, 호출 조사 #42/#40
 *   MnSonyKbd('MnSy'): 키 글자 #1(진입점, 코드 = -4)/#2 핸들, 입력 #5/#6
 * 둘 다 아니고 CJKOS Feature('dCJK' #0)가 있으면 CJKOS, 아니면 알 수 없음. 공개 함수(FtrGet, MemHandleSize)만 쓰고
 * 코드 주소로 힙·DB를 찾지 않는다(ClieDiag v26 MemPtrHeapID 사고). 전역변수·문자열 상수 없음(알림 처리에서 부름).
 */
#ifndef TOP_WHO_H
#define TOP_WHO_H

#define kWhoNone 0
#define kWhoHg   1
#define kWhoSy   2
#define kWhoCjk  3
#define kWhoUnk  4

static Boolean WhoIn(UInt32 t, UInt32 cr, UInt16 codeF, UInt16 handleF, UInt32 adj)
{
    UInt32 c = 0, h = 0;

    FtrGet(cr, codeF, &c);
    FtrGet(cr, handleF, &h);
    if (c == 0 || h == 0)
        return false;
    c -= adj;
    return t >= c && t < c + MemHandleSize((MemHandle)h);
}

/* self = 자기 크리에이터(자기 코드는 판정에서 뺀다) */
static UInt16 TopWho(UInt32 t, UInt32 self)
{
    UInt32 v = 0;

    if (self != 'MnHg' && (WhoIn(t, 'MnHg', 21, 20, 4) || WhoIn(t, 'MnHg', 46, 45, 0) || WhoIn(t, 'MnHg', 48, 47, 0) ||
                           WhoIn(t, 'MnHg', 101, 100, 0) || WhoIn(t, 'MnHg', 42, 40, 0)))
        return kWhoHg;
    if (self != 'MnSy' && (WhoIn(t, 'MnSy', 1, 2, 4) || WhoIn(t, 'MnSy', 5, 6, 0)))
        return kWhoSy;
    if (FtrGet('dCJK', 0, &v) == errNone)
        return kWhoCjk;
    return kWhoUnk;
}

/* 앱 화면에서만 부름: 지우기 거부 안내 글귀 */
static void WhoMsg(Char *m, UInt16 who)
{
    StrCopy(m, "can't delete: ");
    StrCat(m, who == kWhoHg ? "MnHangul" : who == kWhoSy ? "MnSonyKbd" : who == kWhoCjk ? "CJKOS" : "other patch");
    StrCat(m, " on top");
}

#endif
