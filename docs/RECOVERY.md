# 복구

> 문제가 생겼을 때 되살리는 방법. 평소 설치·업데이트 절차는 [README](../README.md#설치업데이트)에 있다
> (Mn 가상 키보드: 설치 A·C, MnHangul·MnSonyKbd: [공통 절](../README.md#mnhangulmnsonykbd-공통-켜기업데이트cjkos-전환)).
> 구조 설명은 [ARCHITECTURE.md](ARCHITECTURE.md), 시험 기록은 [KOREAN_SUPPORT.md](KOREAN_SUPPORT.md).
> 시험은 CloudpilotEmu **백업 복사본 세션**에서 한다. 아래 방법으로도 안 되면 백업을 복원한다.

## 개념

- **DB 식별자**: Palm OS DB는 4바이트 타입·크리에이터 ID로 구분한다.
  Mn 가상 키보드 로더 `SkWr`·본체 `SkCo`(크리에이터 `MnSk`), MnHangul `appl`/`MnHg`, MnSonyKbd `appl`/`MnSy`
  (전체 목록: [ARCHITECTURE.md](ARCHITECTURE.md#9-4바이트-식별자)).
- **DB 삭제 보호·잠금**: 코드가 쓰이는 동안은 DB를 지우거나 덮어쓸 수 없다("database exists on device and could not be overwritten").
  - Mn 가상 키보드: `SilkExtn`이 부팅 때 플러그인 DB를 보호하고 로더(11 이상)가 스스로 푼다. 본체는 켜져 있는 동안 잠긴다.
  - MnHangul·MnSonyKbd: 켜져 있는 동안(시스템 함수가 앱 코드를 가리킴) 잠근다. 리셋하면 풀린다.
- **연결(패치)**: 시스템 함수(트랩) 주소를 자기 코드로 바꿔 끼운 것. RAM의 트랩 표에 있으므로 **어떤 리셋이든 풀린다.**
  리셋 뒤 켜져 있는 것은 `Start at reset`이 다시 켰기 때문이다.
- **소프트 리셋**: 데이터는 그대로 두고 다시 부팅. ClieDiag·MnHangul `Soft Reset (hold 3s)`, 에뮬레이터 Reset 메뉴, 실기기 리셋 구멍.
- **`Update` 리셋**(MnHangul·MnSonyKbd): 다음 리셋 한 번만 자동 시작을 건너뛰고 리셋한다. 설치되어 있는 두 앱 모두 건너뛴다.
- **No extensions 리셋**(위 버튼을 누른 채 리셋, CloudpilotEmu: Reset → No extensions): 리셋 알림과 시스템 확장을 모두 건너뛴다.
  NR70V에서는 **`SilkExtn`도 불러오지 않아** 가상 실크 영역의 상태 표시줄 아이콘·시계가 사라지고 Sony 가상 키보드·Mn 가상 키보드를 쓸 수 없다
  (2026-10-18 실측). 설치를 끝낸 뒤 **일반 리셋을 한 번** 해야 돌아온다.
- **DisS**: 타입 `DisS`·크리에이터 `SeSi`인 DB. 있으면 `SilkExtn`이 실크 초기화 전체를 건너뛴다(플러그인 로딩·보호 포함, ROM 0x27EC 실측).
  실크 영역과 홈 아이콘도 사라진다. ClieDiag 디버그 SkWr 페이지 `DisS` 버튼.

## 상황별 복구

### 공통

| 상황 | 방법 |
| --- | --- |
| 화면이 멈춤(먹통) | 소프트 리셋. 리셋 뒤 자동 시작으로 다시 멈추면 No extensions 리셋 → 원인 앱에서 `Start at reset: off` → 일반 리셋 |
| 상태 표시줄 아이콘·시계가 사라짐, 키보드를 열 수 없음 | No extensions 리셋 뒤라면 일반 리셋. DisS를 켰다면 ClieDiag에서 `DisS` 끄기 → 리셋 |
| 이상 동작 전반 | CloudpilotEmu 세션 백업 복원(가장 확실) |

### Mn 가상 키보드 (`MnSkPlugin`·`MnSkCore`)

| 상황 | 방법 |
| --- | --- |
| 본체를 켜 둔 채 설치해 `MemoryMgr.c … Chunk under-locked` 오류 | 리셋. 다음부터 □→로 끈 뒤 설치 |
| 로더 설치 후 리셋 전에 □→로 켜서 멈춤 | 리셋(`SilkExtn`이 옛 로더 코드 주소를 써서 생긴 장애, 자동 복구는 보장하지 않음) |
| 플러그인 때문에 부팅이 멈춤 | DisS를 만들고 리셋 → 플러그인 없이 부팅 → 문제 PRC 지우거나 교체 → DisS 끄기 → 리셋 |
| 상태 확인 | ClieDiag Main: `loader: build N`, `protect: released`, `core: build N`, `load: ok` |

### MnHangul·MnSonyKbd

| 상황 | 원인 | 방법 |
| --- | --- | --- |
| 설치 실패("could not be overwritten") | 앱이 켜져 있어 코드가 잠김. 한쪽 앱의 `Update`로 다른 앱도 건너뛰는 기능은 MnHangul v57·MnSonyKbd v5부터 | 그 앱에서 `Update (hold 3s)` → 설치. 옛 판이라 안 되면 그 앱 자신의 `Update` |
| `Hangul OFF`·`OFF`에서 `kept: use Update` | 다른 패치(CJKOS 등)가 위에 걸려 있어 안전을 위해 풀지 않음 | `Update` → 설치 |
| MnHangul `Hangul ON`에서 `CJKOS? running` | CJKOS가 켜져 있음(같은 함수를 바꾸므로 충돌) | CJKOS를 끈 뒤 `Hangul ON`. CJKOS와 쓸 때는 MnSonyKbd만(설치 B) |
| MnHangul을 켠 채 CJKOS를 켬 | 같은 함수를 이중으로 바꿈(MnHangul은 CJKOS가 먼저 켜져 있을 때만 막음) | CJKOS 끔 → 리셋(MnHangul `Start at reset`이 ON이면 MnHangul만 켜짐). 다음부터 CJKOS를 켜기 전에 `Hangul OFF`(`kept`이면 `Update`), CJKOS를 계속 쓸 때는 `Start at reset: off` |
| CJKOS를 끈 뒤 `Hangul ON` 했는데 한글은 보이고 BS·커서가 반쪽 글자 단위 | Intl·Width 연결이 빠졌을 가능성(코드 검토로 추정, **미확인**). v58 `Hangul ON`은 그리기 연결만 다시 잇는다 | 리셋 → `Hangul ON`. 재현되면 CJKOS를 끈 직후 ClieDiag Trap 페이지 캡처 |
| MnHangul `Hangul ON`을 다시 누르면 먹통(v57 이하) | MnSonyKbd가 위에 있을 때 자기를 한 번 더 끼워 순환 | 소프트 리셋 → v58 이상으로 업데이트 |
| CJKOS를 켜거나 끈 뒤 Sony 키 글자가 영문 그대로, `한A`만 동작 | CJKOS가 시스템 함수를 되돌려 MnSonyKbd 키 글자 연결이 빠짐 | 가상 실크를 누르면 스스로 다시 이음(v6~). 안 되면 MnSonyKbd `Now: broken` → `ON`. 그래도 안 되면 리셋 |
| 한/영 표시가 한 번 틀림 | Sony 그리기 순서에 기댄 판정의 한계 | `한A`를 한 번 더 누르거나 키보드를 다시 연다. 반복되면 MnSonyKbd 제목을 눌러 판정 기록 캡처 |
| 입력한 한글이 라틴 문자 두 개로 깨짐 | 한글 표시 모듈 없음 | CJKOS 또는 MnHangul `Hangul ON`. MnSonyKbd 첫 화면 `Hangul display:` 줄로 확인 |

## 옛 절차 (로더 10 이하 → 11 전환 때만 사용, 현재는 쓰지 않음)
로더 10 이하는 삭제 보호를 풀지 않으므로 다음 절차로만 교체할 수 있었다.
1. ClieDiag `DisS` 켜기 → 리셋 (실크 영역이 사라져 런처를 열 수 없음)
2. 새 PRC 설치
3. 하드웨어 To Do 버튼으로 `DiagRescue` 실행 → `DisS` 끄기 → 리셋

`DiagRescue.prc`는 이 절차를 위해 쓰던 **임시 앱**(ClieDiag와 같은 코드, 크리에이터 `todo`·버전 2로 ROM To Do를 대신 실행)이다.
v14에서 폐기했고 배포 구성에서 뺐다. 기기에 남아 있으면 ClieDiag 디버그 SkWr 페이지의 `Delete DiagRescue`로 지운다
(런처 삭제는 같은 크리에이터 `todo`의 To Do 데이터까지 지울 수 있음). To Do 하드웨어 버튼과 원래 앱은 바꾸지 않는다.
