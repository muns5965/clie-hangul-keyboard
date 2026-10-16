# 설치·업데이트·복구

> 현행 절차(로더 11 · 본체 12 · ClieDiag v19 기준). 구조 설명은 [ARCHITECTURE.md](ARCHITECTURE.md). (기준 커밋 `117932a`)

## 현행 절차

| 작업 | 절차 | 리셋 |
| --- | --- | --- |
| 처음 설치 | `MnSkPlugin.prc`, `MnSkCore.prc`, `ClieDiag.prc` 설치 → ClieDiag `Soft Reset` 3초 → 상태바 □→로 켜기 | 1번 |
| 본체(키보드) 업데이트 | □→로 끄기 → `MnSkCore.prc` 설치 → □→로 켜기 | 없음 |
| 로더 업데이트 | □→로 끄기 → `MnSkPlugin.prc` 설치 → 바로 ClieDiag `Soft Reset` 3초 | 1번 |

- 본체: 자판이 켜진 채로 설치하지 않는다(본체 코드가 잠겨 실행 중).
- 로더: 설치 후 리셋 전에 □→로 자판을 켜지 않는다. SilkExtn은 부팅 때 받은 **옛 로더 코드 주소**를 계속 쓰므로 멈출 수 있다.
  이 경우는 장애이며 자동 복구를 보장하지 않는다. 시험은 CloudpilotEmu **백업 복사본 세션**에서 한다.
- 실측: 본체 무리셋 교체(본체 11), 리셋 1번 로더 업데이트(로더 11) 모두 CloudpilotEmu에서 성공(2026-10-16).
- 확인: ClieDiag Main에 `loader: build N`, `protect: released`, `core: build N`, `load: ok`.

## 비상 수단
| 상황 | 방법 | 비고 |
| --- | --- | --- |
| 이상 동작 전반 | CloudpilotEmu 세션 백업 복원 | 가장 확실 |
| 플러그인 없이 부팅 | 타입 `DisS`·크리에이터 `SeSi` DB를 만들고 리셋 → SilkExtn이 실크 초기화 전체를 건너뜀(ROM 0x27EC, 실측) | **실크 영역·홈 아이콘도 사라진다.** ClieDiag 디버그 SkWr 페이지 `DisS` 버튼 |
| Warm reset(위 버튼 + 리셋) | 시스템 패치·확장 생략 | SilkExtn까지 건너뛰는지 **미검증** |

## 옛 절차 (로더 10 이하 → 11 전환 때만 사용, 현재는 쓰지 않음)
로더 10 이하는 삭제 보호를 풀지 않으므로 다음 절차로만 교체할 수 있었다.
1. ClieDiag `DisS` 켜기 → 리셋 (실크 영역이 사라져 런처를 열 수 없음)
2. 새 PRC 설치
3. 하드웨어 To Do 버튼으로 `DiagRescue` 실행 → `DisS` 끄기 → 리셋

`DiagRescue.prc`는 이 절차를 위해 쓰던 **임시 앱**(ClieDiag와 같은 코드, 크리에이터 `todo`·버전 2로 ROM To Do를 대신 실행)이다.
v14에서 폐기했고 배포 구성에서 뺐다. 기기에 남아 있으면 ClieDiag 디버그 SkWr 페이지의 `Delete DiagRescue`로 지운다
(런처 삭제는 같은 크리에이터 `todo`의 To Do 데이터까지 지울 수 있음). To Do 하드웨어 버튼과 원래 앱은 바꾸지 않는다.
