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
| 1 | 진단 PRC(`bin/ClieDiag.prc`): OS·기기 정보, 화면 모드, Sony HR/Silk 라이브러리 존재 여부, libr 목록, 펜·키 이벤트 | 빌드 완료, 실기 확인 대기 |
| 2 | 입력 시험 PRC (자체 폼 필드): 영문 → KSC 완성형 삽입·삭제 → 조합 중 음절 교체 | 대기 |
| 3 | 다른 앱 위 입력 방식 결정: 알림(Notification) 기반 팝업 키보드 우선, HackMaster/X-Master 확장은 최후 수단 | 대기 |

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

주의: 컴파일러 빌드는 `make -j1`로 해야 한다. 병렬 빌드 시 `cc1plus` 생성 전에 libgcc2 빌드가 시작돼 실패한다.

## bin/ClieDiag.prc (1단계 진단)

소스: `src/diag/diag.c`, 빌드: `./tools/build-diag.sh`. 크리에이터 ID `MnDg`.
Sony SDK 헤더 없이 표준 API만 사용하므로 160×160 호환 모드로 실행된다.

| 페이지 | 내용 |
| --- | --- |
| 1/3 Info | ROM 버전, OEM/기기 ID, 화면 모드, `SoNy` 기능 번호, HR/Silk 라이브러리 탐색·적재 결과 |
| 2/3 Libr | 설치된 공유 라이브러리(`libr`) 목록: 크리에이터, 버전, 이름 |
| 3/3 Events | 펜 좌표(penDown/penUp), 키 이벤트(chr/keyCode/modifiers), 기타 이벤트 번호 |

조작: 제목줄 탭 → 다음 페이지, 위/아래 버튼 → 스크롤.
이름에 HR/Silk가 들어간 라이브러리는 적재를 시도하고, 직접 적재한 것은 종료 시 해제한다.

## test/hello.prc

도구 체인 동작 확인용 예제 앱(prc-tools-remix 예제 소스). 크리에이터 ID `WRLD`.
CloudpilotEmu에 설치해 실행되면 빌드 환경이 정상이다.
