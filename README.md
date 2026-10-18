# clie-hangul-keyboard

Sony CLIÉ PEG-NR70V(Palm OS 4.1S)의 가상 실크스크린(Graffiti 영역) 자리에 한글 소프트 키보드를 띄우는 입력기.

- 목표 자판은 영문 QWERTY, 한글 단모음, 나랏글이다. 현재 영문 QWERTY와 **구글 단모음과 비슷한 한글 자판**의 입력·조합을 구현했고, **나랏글은 향후 구현 대상으로 개발을 보류**한다.
- 한글 **표시와 글자 처리는 현재 CJKOS에 의존**한다. CJKOS를 대체할 한글 처리 모듈은 시험 단계다([docs/KOREAN_SUPPORT.md](docs/KOREAN_SUPPORT.md)).
- 키보드 기능은 현재 동결(안정화) 상태다.

## 소개

<table>
<tr>
<td><img src="docs/images/01-overview.png" width="380" alt="메모장에서 한글 키보드를 쓰는 화면"></td>
<td><img src="docs/images/02-hangul-layout.png" width="380" alt="단모음 한글 자판과 Shift 쌍자음"></td>
</tr>
<tr>
<td><img src="docs/images/03-english-number.png" width="380" alt="영문 QWERTY와 숫자·기호 자판"></td>
<td><img src="docs/images/04-compose.png" width="380" alt="받침 넘김과 자모 단위 지우기"></td>
</tr>
<tr>
<td><img src="docs/images/05-mnhangul.png" width="380" alt="CJKOS 없이 조합형 글꼴로 한글 표시(시험 중)"></td>
<td></td>
</tr>
</table>

그림은 실제 캡처가 아니라 같은 자판 배치·자모 선 그림·조합형 글꼴 데이터로 다시 그린 것이라 실제 화면과 조금 다를 수 있다
(영문 글자는 Sony 글꼴 대신 픽셀 글꼴). 다시 만들기: `python3 tools/gen-readme-images.py <neodgm.ttf>`

## 용어

| 용어 | 뜻 |
| --- | --- |
| 가상 실크스크린 | NR70V 화면 아래쪽에서 Graffiti 입력 영역과 상태바를 소프트웨어로 그리는 영역 |
| Graffiti | Palm OS의 필기 인식 입력 방식(그 입력 영역) |
| `SilkExtn` | NR70V ROM에 들어 있는 Sony의 실크스크린 제어 확장 모듈 |
| `SkWr` | `SilkExtn`이 부팅 때 찾아 호출하는 플러그인 DB의 타입 식별자 |
| 로더(Loader) | 부팅 때 `SilkExtn`에 등록되어, 키보드 본체에 이벤트를 넘겨주는 작은 프로그램(`MnSkPlugin.prc`) |
| 본체(Core) | 자판 그리기, 터치 판정, 한글 조합을 실제로 처리하는 프로그램(`MnSkCore.prc`) |
| PRC | Palm OS의 앱·코드·리소스를 담아 배포하는 데이터베이스 파일 형식 |
| CJKOS | 영문 Palm OS에서 한중일 문자 표시·입력을 지원하는 외부 소프트웨어 |

## 현재 상태

| 기능 | 상태 |
| --- | --- |
| 가상 실크스크린 자리에 키보드 표시(고해상도 320×130) | 구현 완료 (런처·메모장·ClieDiag에서 실측) |
| 영문 QWERTY | 구현 완료 |
| 한글 단모음 (Shift 쌍자음, 모음 연타 ㅑㅕㅛㅠㅒㅖ) | 구현 완료, 안정화 단계 |
| 한글 조합: 초성·중성·종성, 복합모음, 겹받침, 받침 넘김, 조합 중 자모 단위 BS | 구현 완료 (메모장 실측, PC 조합 시험 41개 통과) |
| 숫자·특수문자 | 구현 완료 |
| 리셋 없는 본체 업데이트 / 리셋 1번 로더 업데이트 | 구현 완료 (CloudpilotEmu 실측) |
| 나랏글 | 개발 보류 |
| 키 반복 | 미구현 |
| CJKOS 대체 한글 지원 모듈 | 시험 단계: 시험 앱(MnHangul)으로 CJKOS 없이 한글 그리기 실측. 폭·글자 경계 처리는 미구현 |

## 배포 파일

| 파일 | 역할 |
| --- | --- |
| `bin/MnSkPlugin.prc` | 로더. `SilkExtn`에 등록되는 `SkWr` 플러그인(약 1KB)으로, 부팅 때 호출되어 키보드 본체와 연결한다 |
| `bin/MnSkCore.prc` | 키보드 본체(자판 그리기·터치 판정·한글 조합). 로더가 불러 실행한다 |
| `bin/ClieDiag.prc` | 상태 확인·소프트 리셋(3초 누르기)·진단(Debug 체크 시) |

## 설치·업데이트

| 작업 | 절차 | 리셋 |
| --- | --- | --- |
| 처음 설치 | 세 PRC 설치 → ClieDiag `Soft Reset` 3초 → 상태바 □→로 키보드 켜기 | 1번 |
| 본체(키보드) 업데이트 | □→로 끄기 → `MnSkCore.prc` 설치 → □→로 켜기 | 없음 |
| 로더 업데이트 | □→로 끄기 → `MnSkPlugin.prc` 설치 → 바로 ClieDiag `Soft Reset` 3초 | 1번 |

- □→는 상태바 5번째 아이콘(외부 플러그인이 있을 때만 보임). 누를 때마다 기본 실크와 한글 키보드가 바뀐다.
- 확인: ClieDiag Main에 `loader: build N`, `protect: released`, `core: build N`, `load: ok`.
- 로더 리셋 1번 업데이트는 CloudpilotEmu에서 실측 성공했다. 단, **설치 후 리셋 전에 □→로 키보드를 켜지 않는다**(실크 확장이 옛 로더 코드 주소를 쓰므로 멈출 수 있음).
  이상이 생겨도 자동 복구는 보장하지 않으므로 시험 전에 에뮬레이터 세션을 백업한다. 자세한 내용: [docs/RECOVERY.md](docs/RECOVERY.md).

## 제한사항

- **조합 중 커서 이동**: 한글 조합 중 메모장 등의 본문을 탭하여 커서를 옮기면 키보드는 그 이동을 감지하지 못한다.
  이 상태에서 자모를 더 입력하면 이전 조합 글자를 고치기 위한 BS가 옮겨진 커서 위치에 적용되어 다른 글자가 지워질 수 있다.
  조합 중 커서 이동의 안전성은 보장하지 않는다. 스페이스 등으로 조합을 끝낸 뒤 옮긴다. (원인: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md))
- **CJKOS 의존**: 한글 글꼴 표시와 2바이트 글자 경계 처리(완성된 글자 단위 BS, 커서 위치, 줄바꿈)는 CJKOS에 의존한다.
  한글 조합과 조합 중 자모 단위 BS는 키보드 본체(`MnSkCore`)가 처리한다. CJKOS를 끄면 이 시험 환경(NR70V 영문 ROM)에서는
  한글이 라틴 문자 두 개로 깨져 보이고, BS가 2바이트 글자의 반쪽만 지울 수 있다.
- **한글 범위**: KS X 1001 완성형(EUC-KR) 2,350자. 그 밖의 음절이 되면 앞 글자를 확정하고 마지막 자모부터 새로 조합한다(예: 똠 → `또ㅁ`).
- **시험 범위**: CloudpilotEmu(NR70V ROM)의 런처·메모장·ClieDiag에서 시험했다. 다른 앱(주소록, 일정 등)과 실기는 검증 대상이다.

## 문서

| 문서 | 내용 |
| --- | --- |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 가상 실크스크린 키보드 구현 방법: SkWr 플러그인 호출 방식, 그리기, 글자 보내기, 로더·본체 구조 |
| [docs/RECOVERY.md](docs/RECOVERY.md) | 설치·업데이트·복구 절차, 위험 조건, 옛 DisS 절차 |
| [docs/KOREAN_SUPPORT.md](docs/KOREAN_SUPPORT.md) | CJKOS 의존성, 대체 모듈 검토안과 시험 기록 |
| [docs/DEVELOPMENT_HISTORY.md](docs/DEVELOPMENT_HISTORY.md) | 1~12단계 개발 기록, ROM 역분석, ClieDiag 변경 기록, 실패한 가설 |

## 시험 환경

- 에뮬레이터: [CloudpilotEmu](https://github.com/cloudpilot-emu/cloudpilot-emu) (웹)
- 기기 ROM: PEG-NR70V, Palm OS 4.1S
- 한글: CJKOS

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
결과 PRC와 스크립트별 로그(`build/logs/`)는 실행 페이지의 Artifacts(`prc`)에서 받는다. 실행 요약에 날짜를 뺀 PRC 해시와
한글 조합 시험 결과가 표시된다. 로컬·CI 일치 여부는 PRC별 해시로 확인하고, 본체(`MnSkCore`)는 gcc 지역변수 배치 차이로
해시가 달라 코드 블록 해시·트랩 목록으로 비교한다(기록: [docs/DEVELOPMENT_HISTORY.md](docs/DEVELOPMENT_HISTORY.md#빌드-해시-기록)).

주의: 컴파일러를 소스에서 빌드할 때는 `make -j1`로 해야 한다. 병렬 빌드 시 `cc1plus` 생성 전에 libgcc2 빌드가 시작돼 실패한다.

## test/hello.prc

도구 체인 동작 확인용 예제 앱(prc-tools-remix 예제 소스). 크리에이터 ID `WRLD`.
CloudpilotEmu에 설치해 실행되면 빌드 환경이 정상이다.
