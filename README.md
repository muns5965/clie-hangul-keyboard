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
| 1 | 진단 PRC(`bin/ClieDiag.prc`): OS·기기 정보, 화면 모드, Sony HR/Silk 라이브러리 존재 여부, libr 목록, 펜·키 이벤트 | v4 (SkWr 시험·DisS 스위치) 빌드 완료 |
| 2 | 입력 시험 PRC (자체 폼 필드): 영문 → KSC 완성형 삽입·삭제 → 조합 중 음절 교체 | 대기 |
| 3 | Graffiti 영역 입력기: NR70V 전용 `SkWr` 플러그인 방식 검토 중. ROM의 `SilkExtn`에서 플러그인 로딩 및 이벤트 호출 구조가 확인되었으나, 자체 제작 플러그인의 실제 실행은 미검증 (순서: 무동작 → 사각형 → 펜 수신 → ABC → 한글) | 1단계 실측 성공(로드·Open·FtrSet), 2단계(이벤트 카운터) 대기 |

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
Ubuntu 24.04 컨테이너에 prc-tools-remix v2.3.6 noble 패키지(주소 고정)와 Palm OS SDK 4를 설치하고,
`tools/build-*.sh`를 파일명 순서로 모두 실행한다(`build-hello.sh` 제외). 빌드 전에 저장소에 커밋된 `bin/*.prc`는 지운다.
새 PRC를 추가할 때는 `tools/build-<이름>.sh`만 만들면 CI에 자동으로 포함된다. 공통 코드 디렉터리를 만들면 워크플로의 `paths` 트리거에 추가할 것.
빌드된 PRC마다 `tools/prc-info.sh`로 DB 이름·타입·크리에이터·리소스 목록을 실행 요약에 표시한다.
결과 PRC와 스크립트별 로그(`build/logs/`)는 실행 페이지의 Artifacts(`prc`)에서 받는다. 실행 요약에 날짜를 뺀 PRC 해시가 표시되며,
로컬 빌드와 동일함을 확인했다(ClieDiag v3: size 4808, hash 1dbacad7e3b57cb0).

주의: 컴파일러를 소스에서 빌드할 때는 `make -j1`로 해야 한다. 병렬 빌드 시 `cc1plus` 생성 전에 libgcc2 빌드가 시작돼 실패한다.

## bin/ClieDiag.prc (진단, v4)

소스: `src/diag/diag.c`, 빌드: `./tools/build-diag.sh`. 크리에이터 ID `MnDg`. 160×160 호환 모드로 실행된다. 소니 라이브러리 함수는 호출하지 않는다.

| 페이지 | 내용 |
| --- | --- |
| Info | ROM 버전, OEM/기기 ID, 화면 모드, `SoNy` 기능 번호, HR/Silk 라이브러리 탐색 결과 |
| Libr | 설치된 공유 라이브러리(`libr`) DB 목록 |
| SkWr | 설치된 `SkWr` 플러그인 목록, 플러그인 기록(feature `MnSk` #0) 해석, `[clear record]` 기록 삭제, `[DisS ...]` 실크 비활성화 스위치 전환(리셋 후 적용) |
| Events | 펜·키 이벤트 기록, Graffiti 영역 가로채기(INT) 토글. `*`는 시스템에 넘기지 않은 이벤트 |

조작: 위/아래 버튼 → 이전/다음 페이지. 그 외 페이지별 탭 동작은 소스 주석 참고.
v2의 `SysLibTblEntry` 표 읽기, v3의 Silk(Vsk) 페이지는 Fatal Exception을 일으켜 제거했다.

## bin/MnSkPlugin.prc (SilkWare 플러그인 1단계: 무동작 + Open 기록)

소스: `src/skwr/skwr.c`, 빌드: `./tools/build-skwr.sh`. DB 타입 `SkWr`, 크리에이터 `MnSk`, 리소스는 `code 0` 하나.
시작 코드·데이터 세그먼트 없이 진입 함수 `SkwrEntry`만 `code 0` 맨 앞에 둔다. 빌드 스크립트가 전역변수 없음, 진입점 오프셋 0, `%a5` 참조 없음을 검사한다.

- 동작: Open(이벤트 1)을 받으면 `FtrSet('MnSk', 0, 0x534B0101)` 한 번만 호출하고, 모든 이벤트에 0을 반환한다.
- 기록 형식: 31~16 식별값 `0x534B`('SK'), 15~8 이벤트 번호, 7~0 빌드 번호.
- 판정: ClieDiag SkWr 페이지에 `OK evt=1 build=1`이 나오면 로드·Open 수신·기록 성공. `record: none`이면 로드 안 됨/Open 미수신/기록 실패 중 하나(구분 불가).
- **시험은 CloudpilotEmu 복사본 세션에서만.** 설치 후 리셋해야 로드된다(부팅 시 로딩).
- **실측 결과 (2026-10-16, CloudpilotEmu 복사본 세션)**: 설치 전 `record: none (e=0C02)`·`SkWr installed: 0` →
  설치·리셋 후 부팅 정상, `record: 534B0101`·`OK evt=1 build=1`·`MnSk MnSkPlugin` 표시.
  자체 제작 `SkWr` 플러그인이 SilkExtn에 로드되고 Open(1)을 받으며, 부팅 중 `FtrSet`이 동작함을 확인.
- 복구 수단(신뢰도 순): ① 세션 백업 복원 ② ClieDiag `[DisS]` 스위치 켜고 리셋 → 플러그인 미로딩·미보호 → 삭제 (코드상 확인, 실측 전) ③ 확장 없이 리셋 후 삭제 (미검증).

## 실리클릿(Slkw) 분석 메모

- 근거: Sony CLIÉ SDK 3.0(`SonySilkLib.h`는 Resize API만 포함), 소니 원본 헤더 `SonySlkw.h`·Vsk판 `SonySilkLib.h`(PumpkinOS 보존본),
  실리클릿 SlkCalc/SlkClock/SlkCalendar와 관리 앱 SilkletMgr(H.Imazeki, 2002) 역분석. 소니 헤더는 저작권 문제로 저장소에 넣지 않는다.
- 실리클릿 DB 타입은 `Slkw`. `code 0`에 `SlkwEntryPoint`가 있고, SilkWareLib가 이를 호출하면 자기 DB를 실행 코드 `0x800A`로 `SysAppLaunch`하며
  `{silkWareP, SilkEventP}`를 넘긴다. 이벤트: Open/Close/Start/Stop/Draw/DispChange/PenDown/PenMove/PenUp/TimerFired/DoCommand.
- 그리기는 일반 `Win*` 함수를 `VskSetDrawWindow`/`VskRestoreDrawWindow`(0xA811/0xA812)로 감싸서 한다.
- SlkCalc는 `FrmGetFocus` → `FldInsert`로 현재 앱 입력 칸에 글자를 넣는다.
- Vsk 함수는 "Sony Silk Library"에 있음(트랩 0xA808~0xA812). 활성화는 `VskSetCurrentSlkw(ref, 0, creator)`, 조회는 `VskGetCurrentSlkw(ref, 0, &creator)`.
- CLIE NetMon은 `aSkw`(OS 5 ARM)로 NR70V에서는 사용 불가.
- **결론: NR70V에서는 `Slkw` 실리클릿을 쓸 수 없다.** SDK 5.0 문서상 NR70 시리즈는 Silk Manager v1(Resize만 지원)이고,
  Silk Plug-in·Vsk 함수는 v2 이상(NX 등 OS 5)부터다. NR70V ROM의 SilkLib v100 디스패치 테이블도 8개 함수뿐이며,
  ClieDiag v3에서 `VskGetCurrentSlkw` 호출 시 Fatal Exception이 났다(Silk 페이지는 v4에서 정리 예정).

## NR70V `SkWr` 플러그인 분석 메모 (ROM 역분석, 미검증)

- ROM `SilkExtn`(type `extn`, creator `SeSi`)이 부팅 시 타입 `SkWr` DB를 모두 찾아 `code 0` 리소스를 잠그고 진입점으로 등록한다.
  슬롯은 내장 처리기 1개 + 외부 최대 3개. 진입점은 `(플러그인 저장 슬롯, 이벤트, 0)`으로 호출되며 첫 이벤트는 Open(1).
- `SilkWareLib`(`SlSW`) API: Open/Close/Sleep/Wake/WinSaveBits/WinRestoreBits/GetRefNum/TimerWrite/GetScrWindow/GetAPIVersion.
- Graffiti 영역 그림은 `SknG`(Graffiti 스킨)·`SknS`(상태바 스킨) DB에서 읽는다. 스킨 교체는 그림만 바뀌고 입력은 바뀌지 않는다.
- 호출 규약(확정, SilkExtn 0x2FE0~0x3002, 내장 처리기 0x2530): `Err entry(UInt32 *slot, UInt8 *event, void *out)`, 스택 전달, 반환 `d0` 16비트(0 = 등록 유지),
  일반 C 레지스터 규약, 호출 전 A5 미설정(전역변수 금지), `code 0` 맨 앞으로 `jsr`. Open 때 `out`은 NULL.
- 로딩 시점(확정): 부팅 중 `extn` 초기화. `SilkStartup`(0x278C) → `PrvInitStage2` → `PrvInitSilkWare`(0x2E2A). 직전에 `FtrGet`을 쓰므로 Feature Manager 사용 가능(높음).
- 실크 비활성화 스위치(확정, 0x27EC): 타입 `DisS`·크리에이터 `SeSi` DB가 있으면 실크 초기화 전체를 건너뜀.
- 플러그인 전환 방법, Draw·Pen 이벤트 구조체 배치는 미확정. 첫 시험은 CloudpilotEmu 세션 백업 후 진행한다.

### 1차 결과 (v1, 2026-10-15)

- ROM `04103000`(Palm OS 4.1), OEM `sony`, 기기 `rdwd`, 화면 160×160 8bpp(호환 모드)
- `Sony HR Library`(ref 6), `Sony Silk Library`(ref 14) 모두 이미 적재돼 있음
- libr DB: `SlHr` HRLib v200, `SlSi` SilkLib v100, `SlSW` SilkWareLib v100 등 20개
- 가상 Graffiti 영역 획이 `penDown`/`penUp`(y 160~240)으로 앱에 전달되고, 그 뒤 인식 결과가 `keyDown`(예: 0x08, 0x20)으로 옴. `penMove`는 오지 않음
  → 앱이 SysHandleEvent 전에 Graffiti 영역 터치를 가로챌 수 있음. 남은 관문은 그 영역에 그리기(HR/Silk API 필요)

## test/hello.prc

도구 체인 동작 확인용 예제 앱(prc-tools-remix 예제 소스). 크리에이터 ID `WRLD`.
CloudpilotEmu에 설치해 실행되면 빌드 환경이 정상이다.
