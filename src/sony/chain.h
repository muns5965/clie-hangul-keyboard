/*
 * MnSonyKbd v3: 시스템 함수 연결 확인·다시 잇기 (app.c·ime.c 공용, 전역변수 없음)
 *
 * CJKOS는 꺼질 때 자기가 처음 저장한 ROM 주소로 BltDrawChars를 되돌려, 그 위에 걸린 MnSonyKbd 키 글자 패치가
 * 연결에서 빠진다(v2 실측: ⇪ 그림은 계속 그려지는데 키 글자 그리기 수가 멈춤, blt below = CJKOS RAM 주소).
 * 연결 확인: 트랩 주소에서 시작해 아는 패치(MnHangul 그리기 패치 'MnHg' #21, 아래 주소는 진입점 - 4)를 따라 내려간다.
 *   1 = MnSonyKbd를 만남(연결됨), 0 = 못 만나고 ROM에 닿음(빠짐), 2 = 모르는 패치(CJKOS 등)라 알 수 없음.
 * 다시 잇기는 0일 때만 한다: 연결 안 어디에도 MnSonyKbd가 없음이 확인되어야 자기 아래 주소를 새로 적어도
 * 순환(무한 호출)이 생기지 않는다.
 */
#ifndef SY_CHAIN_H
#define SY_CHAIN_H

#define kSyTrapBlt   0xA378
#define kSyFtrEntry  1        /* 'MnSy' #1 = 키 글자 패치 진입점(코드 + 4) */
#define kSyFtrImC    5        /* 'MnSy' #5 = 입력 코드(오프셋 0·4 진입점 오프셋, 8·12 아래 주소) */
#define kSyFtrHeal   153      /* 'MnSy' #153 = 다시 이은 횟수(하위 16 = 키 글자, 상위 16 = 입력·그림) */

static inline Boolean SyRom(UInt32 a)
{
    return a >= 0x10000000UL && a < 0x10800000UL;
}

static inline UInt16 SyBltLinked(UInt32 ours)
{
    UInt32 t = (UInt32)SysGetTrapAddress(kSyTrapBlt), e;
    UInt16 n;

    for (n = 0; n < 8; n++) {
        if (t == ours)
            return 1;
        if (SyRom(t))
            return 0;
        e = 0;
        FtrGet('MnHg', 21, &e);   /* MnHangul 그리기 패치 진입점 */
        if (e == 0 || t != e)
            return 2;
        t = *(const UInt32 *)(e - 4);
    }
    return 2;
}

static inline void SyHealCount(UInt32 add)
{
    UInt32 v = 0;

    FtrGet('MnSy', kSyFtrHeal, &v);
    FtrSet('MnSy', kSyFtrHeal, v + add);
}

/* 키 글자 패치가 빠졌으면 맨 위에 다시 끼운다. 다시 이었으면 true */
static inline Boolean SyBltHeal(void)
{
    UInt32 ours = 0, cur;

    FtrGet('MnSy', kSyFtrEntry, &ours);
    if (ours == 0 || SyBltLinked(ours) != 0)
        return false;
    cur = (UInt32)SysGetTrapAddress(kSyTrapBlt);
    DmWrite((void *)(ours - 4), 0, &cur, 4);   /* 아래 주소 = 지금 맨 위(ROM 또는 MnHangul) */
    SysSetTrapAddress(kSyTrapBlt, (void *)ours);
    FtrSet('MnSy', 150, 0);   /* v7: 빠져 있던 동안 Sony 실제 ⇪ 상태(#150)는 갱신되지 않았으므로 모름으로 */
    SyHealCount(1);
    return true;
}

/* 입력(EvtGetEvent)·그림(WinDrawBitmap) 연결: 1 맨 위, 0 ROM이 맨 위(빠짐), 2 다른 패치가 위(알 수 없음) */
static inline UInt16 SyImLinked(UInt16 trap, UInt32 entry)
{
    UInt32 t = (UInt32)SysGetTrapAddress(trap);

    return t == entry ? 1 : SyRom(t) ? 0 : 2;
}

/* v9: 입력(k = 0, EvtGetEvent)·그림(k = 1, WinDrawBitmap) 중 하나가 빠졌으면(ROM이 맨 위) 다시 잇는다. 다시 이었으면 1.
 * 다른 패치가 위에 있으면(2) 손대지 않는다. */
static inline UInt16 SyImHealOne(UInt16 k)
{
    UInt32  c = 0;
    UInt8  *code;
    UInt16  trap = k == 0 ? sysTrapEvtGetEvent : sysTrapWinDrawBitmap;
    UInt32  entry, cur;

    FtrGet('MnSy', kSyFtrImC, &c);
    if (c == 0)
        return 0;
    code = (UInt8 *)c;
    entry = (UInt32)(code + ((const UInt32 *)code)[k]);
    if (SyImLinked(trap, entry) != 0)
        return 0;
    cur = (UInt32)SysGetTrapAddress(trap);
    DmWrite(code, 8 + 4 * k, &cur, 4);
    SysSetTrapAddress(trap, (void *)entry);
    SyHealCount(1UL << 16);
    return 1;
}

/* 입력·그림 둘 다 확인(앱 ON). v9: ⇪ 그림 연결(ime.c HgImBmp)은 입력(k = 0)만 SyImHealOne으로 부른다
 * (CJKOS를 꺼도 WinDrawBitmap 연결은 남음 실측, 실행 중인 그림 연결 자신은 건드리지 않음) */
static inline UInt16 SyImHeal(void)
{
    return SyImHealOne(0) + SyImHealOne(1);
}

#endif
