/*
 * MnSk 로더 - NR70V SilkWare(SkWr) 플러그인 (bin/MnSkPlugin.prc, 타입 SkWr, 크리에이터 MnSk)
 *
 * 왜 로더인가:
 *   SilkExtn은 부팅 때 SkWr DB의 code 0을 잠그고 DB에 삭제 보호를 건다. 그래서 플러그인을 바꾸려면
 *   DisS 켜기 → 리셋 → 설치 → DisS 끄기 → 리셋을 거쳐야 했다.
 *   이 로더는 한 번 설치하면 바꾸지 않고, 자판 본체는 보통 DB(MnSkCore.prc)에 둔다.
 *   본체 업데이트: 상태바 □→로 자판 끄기 → MnSkCore.prc 설치 → □→로 켜기 (리셋 불필요).
 *
 * 로더 11: 삭제 보호를 스스로 푼다 (로더 업데이트도 DisS 없이).
 *   ROM SilkExtn(0x2F00~0x3030)은 SkWr DB마다 code 0 잠금 → DmDatabaseProtect(켜기) → Open 호출 →
 *   MemHandleUnlock → DmCloseDatabase 순서다. 즉 코드 잠금은 Open 뒤에 풀리고, 교체를 막는 것은 보호뿐이다.
 *   그래서 Open 안에서 자기 DB의 보호를 끄면 된다. 등록은 유지(0 반환).
 *   로더 업데이트: □→로 자판 끄기 → 새 MnSkPlugin.prc 설치 → 리셋 1번.
 *   주의: 설치 후 리셋 전에 □→로 켜면 SilkExtn이 옛 코드 주소로 호출해 멈출 수 있다(강제 리셋).
 *
 * 동작:
 *   - Open(1): 실크 창 핸들(event+2)을 슬롯에 넣는다(SilkExtn이 이후 첫 인자로 돌려준다). 본체는 부르지 않는다.
 *   - Start(3) 때 '켜짐'(#59)을 기록한다. 켜짐 상태의 Start/Draw(5)/DispChange(6)에서 본체가 안 불려 있으면 불러온다.
 *     (꺼진 동안 Draw 등이 와도 본체를 불러 기본 실크 위에 그리지 않도록)
 *       DmOpenDatabaseByTypeCreator('SkCo','MnSk') → DB 버전 = kCoreAbi 확인 → 리소스 'MnCo' 0 → MemHandleLock
 *       → DmCloseDatabase (DB는 닫고 잠금만 유지. 앱이 끝날 때 DB가 자동으로 닫히는 문제를 피한다)
 *   - 본체가 있으면 모든 이벤트를 본체 진입점(리소스 맨 앞)으로 넘긴다: Err core(slotValue, event, out).
 *   - Stop(4): 본체에 넘긴 뒤 잠금을 풀어 본체 DB를 바꿀 수 있게 한다.
 *   - 본체가 없거나 버전이 다르면 아무것도 하지 않는다(멈추지 않음). 결과는 feature #56.
 *
 * 전역변수·문자열 상수 금지(A5 없음). 빌드 스크립트가 검사한다.
 *
 * 기록 (feature 'MnSk'):
 *   #0 Open 표시(31~16 'SK', 15~8 이벤트 1, 7~0 로더 빌드), #2 Open 횟수, #20~#23 Open 앞 16바이트
 *   #56 불러오기 결과(1 성공, 2 본체 DB 없음, 3 리소스 없음, 4 버전 다름, 5 잠금 실패)
 *   #57 본체 리소스 핸들, #58 본체 코드 주소, #59 켜짐(Start~Stop) (ClieDiag [clear record]가 지우지 않는다)
 *   #60 보호 해제 결과(31~16 'PR', 15~0 DmDatabaseProtect 오류. 0 = 해제됨, 0xFFFF = 자기 DB 못 찾음)
 */

#include <PalmOS.h>

#define kSkCreator    'MnSk'
#define kSkMagic      0x534BUL
#define kLoaderBuild  11
#define kCoreType     'SkCo'
#define kCoreRes      'MnCo'
#define kCoreAbi      1

#define kEvtOpen      1
#define kEvtStart     3
#define kEvtStop      4
#define kEvtDraw      5
#define kEvtDisp      6

#define kFtrOpen      0
#define kFtrOpenCount 2
#define kFtrOpenRaw   20
#define kFtrLoad      56
#define kFtrCoreH     57
#define kFtrCoreP     58
#define kFtrActive    59
#define kFtrUnprotect 60

typedef Err (*CoreFn)(UInt32 *slot, UInt8 *event, void *out);

static inline UInt32 GetFtr(UInt16 num)
{
    UInt32 v = 0;

    if (FtrGet(kSkCreator, num, &v) != errNone)
        v = 0;
    return v;
}

/* 자기 DB(SkWr/MnSk)의 삭제 보호 해제. SilkExtn이 Open 직전에 건 보호를 되돌린다 */
static inline void Unprotect(void)
{
    DmSearchStateType st;
    UInt16            card = 0;
    LocalID           id = 0;
    Err               err;

    if (DmGetNextDatabaseByTypeCreator(true, &st, 'SkWr', kSkCreator, true, &card, &id) != errNone ||
        id == 0)
        err = 0xFFFF;
    else
        err = DmDatabaseProtect(card, id, false);
    FtrSet(kSkCreator, kFtrUnprotect, (0x5052UL << 16) | (UInt16)err);
}

static inline UInt16 LoadCore(void)
{
    DmOpenRef db;
    LocalID   id = 0;
    UInt16    card = 0, ver = 0, idx;
    MemHandle h;
    void     *p;

    db = DmOpenDatabaseByTypeCreator(kCoreType, kSkCreator, dmModeReadOnly);
    if (db == NULL)
        return 2;
    if (DmOpenDatabaseInfo(db, &id, NULL, NULL, &card, NULL) != errNone ||
        DmDatabaseInfo(card, id, NULL, NULL, &ver, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL) != errNone ||
        ver != kCoreAbi) {
        DmCloseDatabase(db);
        return 4;
    }
    idx = DmFindResource(db, kCoreRes, 0, NULL);
    if (idx == 0xFFFF) {
        DmCloseDatabase(db);
        return 3;
    }
    h = DmGetResourceIndex(db, idx);
    p = (h != NULL) ? MemHandleLock(h) : NULL;
    DmCloseDatabase(db);
    if (p == NULL)
        return 5;
    FtrSet(kSkCreator, kFtrCoreH, (UInt32)h);
    FtrSet(kSkCreator, kFtrCoreP, (UInt32)p);
    return 1;
}

static inline void UnloadCore(void)
{
    UInt32 h = GetFtr(kFtrCoreH);

    FtrSet(kSkCreator, kFtrCoreP, 0);
    FtrSet(kSkCreator, kFtrCoreH, 0);
    if (h != 0)
        MemHandleUnlock((MemHandle)h);
}

Err LoaderEntry(UInt32 *slot, UInt8 *event, void *out)
{
    UInt8  e;
    UInt32 p;

    if (event == NULL)
        return 0;
    e = event[0];

    if (e == kEvtOpen) {
        UInt16 i;

        FtrSet(kSkCreator, kFtrOpen,
               (kSkMagic << 16) | ((UInt32)kEvtOpen << 8) | kLoaderBuild);
        FtrSet(kSkCreator, kFtrOpenCount, GetFtr(kFtrOpenCount) + 1);
        for (i = 0; i < 4; i++) {
            const UInt8 *q = event + i * 4;
            FtrSet(kSkCreator, kFtrOpenRaw + i,
                   ((UInt32)q[0] << 24) | ((UInt32)q[1] << 16) | ((UInt32)q[2] << 8) | q[3]);
        }
        /* 부팅 직후: 이전 기록의 핸들은 무효(리셋 때 feature는 지워지지만 안전하게) */
        FtrSet(kSkCreator, kFtrCoreP, 0);
        FtrSet(kSkCreator, kFtrCoreH, 0);
        FtrSet(kSkCreator, kFtrActive, 0);
        Unprotect();
        /* event+2 = 실크 영역 창. 슬롯에 넣어 두면 이후 첫 인자로 돌아온다. */
        if (slot != NULL)
            *slot = ((UInt32)event[2] << 24) | ((UInt32)event[3] << 16) |
                    ((UInt32)event[4] << 8) | event[5];
        return 0;
    }

    if (e == kEvtStart)
        FtrSet(kSkCreator, kFtrActive, 1);
    p = GetFtr(kFtrCoreP);
    if (p == 0 && GetFtr(kFtrActive) != 0 &&
        (e == kEvtStart || e == kEvtDraw || e == kEvtDisp)) {
        FtrSet(kSkCreator, kFtrLoad, LoadCore());
        p = GetFtr(kFtrCoreP);
    }
    if (p != 0)
        ((CoreFn)p)(slot, event, out);
    if (e == kEvtStop) {
        UnloadCore();
        FtrSet(kSkCreator, kFtrActive, 0);
    }
    return 0;
}
