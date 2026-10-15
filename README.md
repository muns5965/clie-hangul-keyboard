# clie-hangul-keyboard

Sony CLIÉ PEG-NR70V(Palm OS 4.1S)용 한글 소프트 키보드 입력기 프로젝트.
목표 자판: 영문 QWERTY · 한글 단모음 · 나랏글. 한글 표시는 CJKOS에 의존한다.

## 시험 환경

- 에뮬레이터: [CloudpilotEmu](https://github.com/cloudpilot-emu/cloudpilot-emu) (웹)
- 기기 ROM: PEG-NR70V, Palm OS 4.1S
- 한글: CJKOS 설치, 한글 표시 확인됨

## 현재 상태

| 단계 | 내용 | 상태 |
| --- | --- | --- |
| 0 | PRC 빌드 도구 체인 구성 | 완료 |
| 1 | 진단 PRC(`bin/ClieDiag.prc`): OS·기기 정보, 화면 모드, Sony HR/Silk 라이브러리 존재 여부, libr 목록, 펜·키 이벤트 | v1 확인 완료, v3(실리클릿 전환 시험) 확인 대기 |
| 2 | 입력 시험 PRC (자체 폼 필드): 영문 → KSC 완성형 삽입·삭제 → 조합 중 음절 교체 | 대기 |
| 3 | Graffiti 영역 입력기: 소니 실리클릿(`Slkw`) 플러그인 방식 우선 (최소 목표: ABC 버튼 3개) | 조사 중 |

## 설계 메모

- **가상 Graffiti 영역**: NR70V는 Palm OS 4.1S라서 Pen Input Manager(PINMgr, OS 5 계열)가 없다.
  가상 Graffiti 영역은 Sony CLIÉ SDK의 Silk Manager(Sony Silk Library)가 담당한다.
  영역을 접고 펴는 것과, 그 영역에 임의의 키보드를 그리는 것은 별개 문제다.
- **Sony SDK 헤더**: 사용 중인 palm-os-sdk 묶음에 Sony CLIÉ SDK(SilkLib) 헤더는 없다.
  1차 진단은 함수 호출 없이 존재 여부만 확인한다(소니 기능 플래그, `SysLibFind`).
- **인코딩**: CJKOS는 KSC5601(KS X 1001) 완성형 기반이라 한글 2,350자만 표현된다.
  조합 엔진은 현대 한글 전체를 조합하고, 출력 단계에서 인코딩 가능 여부를 판정한다.
  미지원 음절 처리는 UX로 별도 결정한다.
- **문자 삽입**: `EvtEnqueueKey`(백스페이스 + 재입력) 방식과 포커스 필드에 `FldDelete`/`FldInsert` 방식을 비교 검증한다.
  2바이트 문자의 키 이벤트 전달과 문자 단위 삭제는 CJKOS 위에서 실측해야 한다.

## 빌드 환경

Linux(Ubuntu 24.04 x86_64 검증)에서 아래 스크립트로 도구 체인을 구성한다.
컴파일러·SDK 원본은 이 저장소에 포함하지 않고 원 저장소에서 받는다.

```bash
sudo ./tools/setup-toolchain.sh
./tools/build-hello.sh ~/palm-toolchain-src/prc-tools-remix   # → test/hello.prc
```

| 도구 | 버전 / 출처 |
| --- | --- |
| m68k-palmos-gcc | 2.95.3-kgpd, [prc-tools-remix](https://github.com/jichu4n/prc-tools-remix) |
| Palm OS SDK | 4.0, [palm-os-sdk](https://github.com/jichu4n/palm-os-sdk) |
| PilRC | [jichu4n/pilrc](https://github.com/jichu4n/pilrc) |

### GitHub Actions 자동 빌드

`.github/workflows/build-prc.yml`: `src/` 또는 `tools/build-*.sh`가 바뀌어 푸시되면 GitHub 서버에서 PRC를 빌드한다.
Ubuntu 24.04 컨테이너에 prc-tools-remix v2.3.6 noble 패키지(주소 고정)와 Palm OS SDK 4를 설치하고 `tools/build-diag.sh`를 실행한다.
결과 PRC와 빌드 로그는 실행 페이지의 Artifacts(`prc`)에서 받는다. 실행 요약에 날짜를 뺀 PRC 해시가 표시되며,
로컬 빌드와 동일함을 확인했다(ClieDiag v3: size 4808, hash 1dbacad7e3b57cb0).

주의: 컴파일러를 소스에서 빌드할 때는 `make -j1`로 해야 한다. 병렬 빌드 시 `cc1plus` 생성 전에 libgcc2 빌드가 시작돼 실패한다.

## bin/ClieDiag.prc (진단, v3)

소스: `src/diag/diag.c`, 빌드: `./tools/build-diag.sh`. 크리에이터 ID `MnDg`. 160×160 호환 모드로 실행된다.

| 페이지 | 내용 |
| --- | --- |
| Info | ROM 버전, OEM/기기 ID, 화면 모드, `SoNy` 기능 번호, HR/Silk 라이브러리 탐색 결과 |
| Libr | 설치된 공유 라이브러리(`libr`) DB 목록 |
| Silk | `VskGetCurrentSlkw`로 현재 활성 실리클릿 조회, 설치된 `Slkw` 목록. 줄을 탭하면 `VskSetCurrentSlkw`로 전환, `[restore orig]`로 원상복구 |
| Events | 펜·키 이벤트 기록, Graffiti 영역 가로채기(INT) 토글. `*`는 시스템에 넘기지 않은 이벤트 |

조작: 위/아래 버튼 → 이전/다음 페이지. 그 외 페이지별 탭 동작은 소스 주석 참고.
소니 함수 호출 직전 화면 맨 위에 진행 단계(`> 함수명`)를 표시한다. 크래시 시 위치 확인용.

v2의 적재 라이브러리 표 읽기(`SysLibTblEntry`)는 Fatal Exception을 일으켜 v3에서 제거했다.

## 실리클릿(Slkw) 분석 메모

- 근거: Sony CLIÉ SDK 3.0(`SonySilkLib.h`는 Resize API만 포함), 소니 원본 헤더 `SonySlkw.h`·Vsk판 `SonySilkLib.h`(PumpkinOS 보존본),
  실리클릿 SlkCalc/SlkClock/SlkCalendar와 관리 앱 SilkletMgr(H.Imazeki, 2002) 역분석. 소니 헤더는 저작권 문제로 저장소에 넣지 않는다.
- 실리클릿 DB 타입은 `Slkw`. `code 0`에 `SlkwEntryPoint`가 있고, SilkWareLib가 이를 호출하면 자기 DB를 실행 코드 `0x800A`로 `SysAppLaunch`하며
  `{silkWareP, SilkEventP}`를 넘긴다. 이벤트: Open/Close/Start/Stop/Draw/DispChange/PenDown/PenMove/PenUp/TimerFired/DoCommand.
- 그리기는 일반 `Win*` 함수를 `VskSetDrawWindow`/`VskRestoreDrawWindow`(0xA811/0xA812)로 감싸서 한다.
- SlkCalc는 `FrmGetFocus` → `FldInsert`로 현재 앱 입력 칸에 글자를 넣는다.
- Vsk 함수는 "Sony Silk Library"에 있음(트랩 0xA808~0xA812). 활성화는 `VskSetCurrentSlkw(ref, 0, creator)`, 조회는 `VskGetCurrentSlkw(ref, 0, &creator)`.
- CLIE NetMon은 `aSkw`(OS 5 ARM)로 NR70V에서는 사용 불가.

### 1차 결과 (v1, 2026-10-15)

- ROM `04103000`(Palm OS 4.1), OEM `sony`, 기기 `rdwd`, 화면 160×160 8bpp(호환 모드)
- `Sony HR Library`(ref 6), `Sony Silk Library`(ref 14) 모두 이미 적재돼 있음
- libr DB: `SlHr` HRLib v200, `SlSi` SilkLib v100, `SlSW` SilkWareLib v100 등 20개
- 가상 Graffiti 영역 획이 `penDown`/`penUp`(y 160~240)으로 앱에 전달되고, 그 뒤 인식 결과가 `keyDown`(예: 0x08, 0x20)으로 옴. `penMove`는 오지 않음
  → 앱이 SysHandleEvent 전에 Graffiti 영역 터치를 가로챌 수 있음. 남은 관문은 그 영역에 그리기(HR/Silk API 필요)

## test/hello.prc

도구 체인 동작 확인용 예제 앱(prc-tools-remix 예제 소스). 크리에이터 ID `WRLD`.
CloudpilotEmu에 설치해 실행되면 빌드 환경이 정상이다.
