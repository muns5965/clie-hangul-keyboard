/*
 * MnSk - NR70V SilkWare(SkWr) 플러그인 1단계: 무동작 + Open 기록
 *
 * ROM SilkExtn(PrvInitSilkWare, 0x2E2A) 역분석 기준:
 *   - 부팅 중 시스템 확장 초기화 단계에서 타입 'SkWr' DB를 모두 찾아
 *     'code' 0 리소스를 잠그고, 그 맨 앞 주소를 진입점으로 jsr 한다.
 *   - 호출: Err entry(UInt32 *slot, void *event, void *out)
 *       slot  : 플러그인 전용 4바이트 저장 슬롯 (1단계는 사용하지 않음)
 *       event : 첫 바이트가 이벤트 번호. 1 = Open
 *       out   : 출력 버퍼(Open 때는 NULL)
 *   - 반환값 16비트. 0 = 등록 유지, 0이 아니면 등록 취소.
 *   - 호출 전에 A5를 설정하지 않는다 → 전역변수·문자열 상수 사용 금지.
 *
 * 1단계 동작: Open(1)을 받으면 FtrSet 한 번만 호출하고 0을 반환한다.
 * 다른 이벤트는 아무것도 하지 않고 0을 반환한다.
 *
 * 기록 형식 (feature 'MnSk' #0, 32비트):
 *   31~16 식별값 0x534B('SK')  15~8 이벤트 번호  7~0 빌드 번호
 *
 * 이 파일은 code 0 리소스 하나로만 빌드된다(tools/build-skwr.sh).
 * 진입 함수가 반드시 코드 맨 앞에 와야 하므로 이 파일에는 함수를 하나만 둔다.
 */

#include <PalmOS.h>

#define kSkCreator   'MnSk'
#define kSkFtrNum    0
#define kSkMagic     0x534BUL
#define kSkBuild     1
#define kEvtOpen     1

Err SkwrEntry(UInt32 *slot, UInt8 *event, void *out)
{
    if (event != NULL && event[0] == kEvtOpen) {
        FtrSet(kSkCreator, kSkFtrNum,
               (kSkMagic << 16) | ((UInt32)kEvtOpen << 8) | kSkBuild);
    }
    return 0;
}
