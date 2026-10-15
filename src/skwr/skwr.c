/*
 * MnSk - NR70V SilkWare(SkWr) 플러그인 2단계: 무동작 + 이벤트 카운터
 *
 * ROM SilkExtn(PrvInitSilkWare, 0x2E2A) 역분석 + 1단계 실측(2026-10-16) 기준:
 *   - 부팅 중 시스템 확장 초기화 단계에서 타입 'SkWr' DB를 모두 찾아
 *     'code' 0 리소스를 잠그고, 그 맨 앞 주소를 진입점으로 jsr 한다.
 *   - 호출: Err entry(UInt32 *slot, UInt8 *event, void *out)
 *       slot  : 플러그인 전용 4바이트 저장 슬롯 (사용하지 않음)
 *       event : 첫 바이트가 이벤트 번호. 1 = Open (실측 확인)
 *       out   : 출력 버퍼 (Open 때는 NULL)
 *   - 반환값 16비트. 0 = 등록 유지.
 *   - 호출 전에 A5를 설정하지 않는다 → 전역변수·문자열 상수 사용 금지.
 *
 * 2단계 동작: 그리기·타이머 없음. 모든 이벤트에 0을 반환하며 다음만 기록한다.
 *   feature 'MnSk' #0      : Open 표시 (31~16 0x534B, 15~8 이벤트 번호 1, 7~0 빌드 번호)
 *   feature 'MnSk' #1~#16  : 이벤트 번호 0~15별 수신 횟수 (포화 증가)
 *   feature 'MnSk' #17     : 마지막 이벤트 (31~16 out!=NULL 이면 1, 7~0 이벤트 번호)
 *   feature 'MnSk' #18     : 번호 16 이상 이벤트 수신 횟수 (포화 증가)
 *   feature 'MnSk' #19     : out != NULL 로 호출된 횟수 (포화 증가)
 *
 * 이 파일은 code 0 리소스 하나로만 빌드된다(tools/build-skwr.sh).
 * 진입 함수가 반드시 코드 맨 앞에 와야 하므로 SkwrEntry를 맨 처음에 정의하고,
 * 보조 함수는 static inline 으로 펼친다.
 */

#include <PalmOS.h>

#define kSkCreator    'MnSk'
#define kSkMagic      0x534BUL
#define kSkBuild      2
#define kEvtOpen      1

#define kFtrOpen      0
#define kFtrCount0    1       /* +이벤트 번호(0~15) */
#define kFtrLast      17
#define kFtrOver      18
#define kFtrOut       19

static inline void BumpFtr(UInt16 num)
{
    UInt32 v = 0;

    if (FtrGet(kSkCreator, num, &v) != errNone)
        v = 0;
    if (v != 0xFFFFFFFFUL)
        v++;
    FtrSet(kSkCreator, num, v);
}

Err SkwrEntry(UInt32 *slot, UInt8 *event, void *out)
{
    UInt8 e;

    if (event == NULL)
        return 0;
    e = event[0];

    if (e == kEvtOpen)
        FtrSet(kSkCreator, kFtrOpen,
               (kSkMagic << 16) | ((UInt32)kEvtOpen << 8) | kSkBuild);

    if (e < 16)
        BumpFtr(kFtrCount0 + e);
    else
        BumpFtr(kFtrOver);

    if (out != NULL)
        BumpFtr(kFtrOut);

    FtrSet(kSkCreator, kFtrLast, ((UInt32)(out != NULL) << 16) | e);
    return 0;
}
