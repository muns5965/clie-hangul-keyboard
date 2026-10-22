/*
 * MnHangul v64 · MnSonyKbd v14 공용: 설치 전 모듈을 순서대로 끄기 (앱 화면에서만 씀). v69·v50·MnKbdHan v13·MnSkKbd v2: 세 모듈(ModOffAll).
 *
 * 두 모듈이 같은 시스템 함수(BltDrawChars)에 차례로 끼면 나중에 켠 쪽이 위에 있고, 아래쪽은 위가 먼저 빠지기 전에는
 * 안전하게 풀 수 없다("kept"). 그래서 "자기 끄기 → 다른 모듈 끄기"를 두 번 돌린다: 첫 바퀴에서 위에 있는 쪽이 풀리고,
 * 둘째 바퀴에서 나머지가 풀린다. 다른 모듈은 그 앱을 사용자 정의 실행 코드 kCmdModOff로 불러 그 앱이 직접 끄게 한다
 * (자기 패치·잠금 해제 코드를 그대로 씀). 새 전역 공간(sysAppLaunchFlagNewGlobals)을 주어 부른다.
 * 결과: 0 꺼짐(또는 원래 꺼져 있음), 1 앱 없음, 2 위에 다른 패치가 있어 못 끔, 3 실행 실패.
 */
#ifndef MOD_OFF_H
#define MOD_OFF_H

#define kCmdModOff   (sysAppLaunchCmdCustomBase + 0x10)

static UInt16 ModOff(UInt32 creator)
{
    DmSearchStateType st;
    UInt16            card = 0;
    LocalID           id = 0;
    UInt32            r = 3;

    if (DmGetNextDatabaseByTypeCreator(true, &st, sysFileTApplication, creator, true, &card, &id) != errNone)
        return 1;
    if (SysAppLaunch(card, id, sysAppLaunchFlagNewGlobals, kCmdModOff, NULL, &r) != errNone)
        return 3;
    return (UInt16)r;
}

/* 세 패치 모듈(MnHangul·MnSonyKbd·MnKbdHan) 모두 끄기(공용 화면 All OFF). self = 자기 끄기(없으면 NULL, MnSkKbd),
 *   selfCr = 자기 크리에이터(다른 모듈 목록에서 뺌). 셋이 같은 함수에 차례로 낄 수 있어 최대 세 바퀴.
 *   결과 = 자기 | 다른 모듈 중 가장 나쁜 결과(3 실행 실패 > 2 남김 > 0) << 4 */
static UInt16 ModOffAll(UInt16 (*self)(void), UInt32 selfCr)
{
    UInt16 a = 0, b = 0, k, i;

    for (k = 0; k < 3; k++) {
        a = self != NULL ? self() : 0;
        b = 0;
        for (i = 0; i < 3; i++) {
            UInt32 cr = i == 0 ? 'MnHg' : i == 1 ? 'MnSy' : 'MnKh';
            UInt16 r;

            if (cr == selfCr)
                continue;
            r = ModOff(cr);
            if (r == 3)
                b = 3;
            else if (r == 2 && b != 3)
                b = 2;
        }
        if (a != 2 && b != 2)
            break;
    }
    return (UInt16)(a | (b << 4));
}

/* 결과를 화면 글귀로 */
static void ModOffMsg(Char *m, UInt16 r, Boolean cjkos)
{
    UInt16 a = r & 0xF, b = r >> 4;

    if (a == 0 && (b == 0 || b == 1))   /* 성공을 명시적으로: 자기 꺼짐 + 다른 모듈 꺼짐 또는 없음 */
        StrCopy(m, "all off: ready to install");
    else if (b == 3)
        StrCopy(m, "other app: launch failed");
    else if (cjkos)
        StrCopy(m, "kept: turn CJKOS off, retry");
    else
        StrCopy(m, "kept: use Update, then install");
}

#endif
