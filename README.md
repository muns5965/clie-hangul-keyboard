# clie-hangul-keyboard

Sony CLIÉ PEG-NR70V(Palm OS 4.1S)용 한글 소프트 키보드 입력기 프로젝트.
목표 자판: 영문 QWERTY · 한글 단모음 · 나랏글. 한글 표시는 CJKOS에 의존한다.

## 시험 환경

- 에뮬레이터: [CloudpilotEmu](https://github.com/cloudpilot-emu/cloudpilot-emu) (웹)
- 기기 ROM: PEG-NR70V, Palm OS 4.1S
- 한글: CJKOS 설치, 한글 표시 확인됨

## 현재 상태

| 항목 | 상태 |
| --- | --- |
| 가상 실크스크린(Graffiti 영역) 자리에 자체 키보드 표시 | 실측 성공 (런처·메모장 등 모든 앱) |
| 키 입력 → 현재 앱 글자 입력 (영문·숫자·기호·한글) | 실측 성공 |
| 한글 조합 (구글 단모음 비슷한 자판) | 실측 성공 |
| 고해상도(320×130) 키보드 그리기 | 실측 성공 |
| 리셋 없는 본체 업데이트 / 삭제 보호 자체 해제(로더 11) | 실측 성공 |
| 리셋 1번 로더 업데이트 (ClieDiag `Soft Reset`) | 실측 성공 |
| 나랏글·키 반복 등 | 보류 (키보드 동결) |

배포 파일: `bin/MnSkPlugin.prc`(로더), `bin/MnSkCore.prc`(키보드 본체), `bin/ClieDiag.prc`(상태 확인·소프트 리셋·진단).

## NR70V 가상 실크스크린에 키보드를 구현하는 방법

NR70V(Palm OS 4.1S)에는 OS 5의 Pen Input Manager가 없고, 가상 실크스크린(Graffiti 영역·상태바)은
ROM의 Sony 확장 `SilkExtn`(타입 `extn`, 크리에이터 `SeSi`)이 그린다. 이 영역에 다른 화면을 띄우는 공식 경로는
NX 이후의 Silklet(`Slkw`)·Vsk API인데 **NR70V에는 없다**(호출하면 Fatal Exception). 대신 ROM을 분석해 찾은
**`SkWr` 플러그인**("SilkWare") 구조를 쓴다. 아래는 모두 ROM 역분석과 에뮬레이터 실측으로 확인한 내용이다.

### 1. SkWr 플러그인이 불리는 방식
- 부팅 때 `SilkExtn`이 타입 **`SkWr`**인 DB를 모두 찾아(내장 1개 + 외부 최대 3개) 각 DB의 `code 0` 리소스 **맨 앞 주소로 바로 jsr** 한다.
  일반 앱이 아니므로 시작 코드·A5(전역변수)가 없다 → **전역변수·문자열 상수·초기화된 배열 금지**, 위치 독립 코드로 짠다.
- 호출 형식: `Err entry(UInt32 *slot, UInt8 *event, void *out)`, 반환 0 = 등록 유지(0이 아니면 등록 취소).
- Open 때만 첫 인자가 **슬롯 주소**이고, 이후 이벤트에는 거기 넣어 둔 **값**이 그대로 온다 → 필요한 값(창 핸들) 저장소로 쓴다.
  상태는 Feature Manager(`FtrSet/FtrGet`)에 둔다(리셋 때 지워짐).
- 이벤트 번호(`event[0]`): 1 Open, 3 Start, 4 Stop, 5 Draw, 6 DispChange, 7 PenDown, 8 PenMove, 9 PenUp.
  - Open +2(32비트): **실크 영역 창 핸들**. 창은 160×65(저해상도 좌표).
  - Start / DispChange +2 너비, +6 높이, +10 색 깊이, +14 **고해상도 여부**(1바이트).
  - PenDown / PenMove / PenUp +2 x, +4 y(16비트): 실크 영역 기준 저해상도 좌표(0~159, 0~64).
- 켜고 끄기: 외부 플러그인이 있으면 상태바 5번째에 **□→(전환) 아이콘**이 생긴다. 누를 때마다 활성 플러그인이 바뀌며
  이전 것에 Stop, 새 것에 Start가 간다. 이벤트는 활성 플러그인에만 온다.

### 2. 그리기
- `WinSetDrawWindow(Open 때 받은 창)`으로 그린 뒤 원래 창으로 되돌린다. Start·Draw·DispChange 때 전체를 다시 그린다.
- 고해상도: `SilkExtn`이 부팅 때 `Sony HR Library`를 열어 두므로 `SysLibFind`로 참조 번호만 얻어 HR 함수를 쓴다.
  창은 저해상도 크기 그대로 두고 **좌표만 2배**로 넘긴다(`SilkExtn` 자신도 이렇게 그림). 고해상도 여부는 Start/DispChange +14로 판단.
- 한글 키 이름: CJKOS 글꼴은 저해상도라 고해상도에서 굵게 보이므로, 자모를 12×12 격자 선분으로 그린다(`tools/gen-jamo-glyphs.py`).

### 3. 글자 보내기
- `EvtEnqueueKey(문자, 0, 0)` → 현재 앱에 `keyDownEvent`로 들어간다(메모장 등 모든 입력 필드).
- 한글(CJKOS)은 **KSC5601 2바이트 코드를 키 하나로** 보낸다(`EvtEnqueueKey(0xB0A1)` → 가). 바이트를 나눠 보내면 깨진다.
  BS(`chrBackspace`) 한 번에 한글 한 글자가 지워진다.
- 조합 중인 글자는 화면에 이미 나간 글자를 BS로 지우고 새 글자를 보내는 방식으로 고친다. KSC5601에 없는 음절은 조합을 끊는다.

### 4. 키보드 (구현 개요)
구글 단모음 키보드와 비슷한 4줄 자판을 적용했다(자세한 조합 규칙은 소스 `src/skwr/skwr.c` 참고).
- 한글: `ㅂㅈㄷㄱㅅㅗㅐㅔ / ㅁㄴㅇㄹㅎㅓㅏㅣ / ⇧ㅋㅌㅊㅍㅜㅡ⌫`, Shift = 쌍자음, 같은 모음 연타 = ㅑㅕㅛㅠㅒㅖ.
- 영문 QWERTY, 123(숫자·기호), 아랫줄 `한/A 123 , space . Ent`.

### 5. 설치·업데이트 구조 (로더 + 본체)
- `SilkExtn`은 부팅 때 플러그인 DB에 **삭제 보호**(`DmDatabaseProtect`)를 건다 → 그대로는 덮어쓸 수 없다.
- 그래서 SkWr 플러그인은 작은 **로더**(`MnSkPlugin.prc`, 약 1KB)로 두고, 키보드는 일반 DB인 **본체**(`MnSkCore.prc`, 타입 `SkCo`,
  리소스 `MnCo 0`)에 둔다. 로더는 자판이 켜질 때 본체 코드를 잠가 이벤트를 넘기고, 꺼질 때 놓는다.
- 로더는 Open 때 자기 DB의 삭제 보호를 스스로 푼다(SilkExtn이 Open 직전에 보호를 걸고, 코드 잠금은 Open 뒤에 푼다).

| 작업 | 절차 |
| --- | --- |
| 처음 설치 | `MnSkPlugin.prc`, `MnSkCore.prc`, `ClieDiag.prc` 설치 → ClieDiag `Soft Reset` 3초 → 상태바 □→로 켜기 |
| 본체(키보드) 업데이트 | □→로 끄기 → `MnSkCore.prc` 설치 → □→로 켜기 (리셋 없음) |
| 로더 업데이트 | □→로 끄기 → `MnSkPlugin.prc` 설치 → 바로 리셋(ClieDiag `Soft Reset` 3초). 리셋 전 □→ 금지 |
| 플러그인 없이 부팅(비상) | 타입 `DisS`·크리에이터 `SeSi` DB가 있으면 `SilkExtn`이 실크 초기화 전체를 건너뛴다(실크 영역도 사라짐) |

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

## bin/ClieDiag.prc (상태·진단, v18)

소스: `src/diag/diag.c`, 빌드: `./tools/build-diag.sh`. 크리에이터 ID `MnDg`. 160×160 호환 모드로 실행된다. 소니 라이브러리 함수는 호출하지 않는다.

기본 화면은 **Main 한 페이지**: 로더 설치·빌드·삭제 보호 해제 여부, 본체 설치·빌드·불러오기 결과, 자판 켜짐, 업데이트 방법.
Main의 `Debug` 체크박스를 탭하면 아래 진단 페이지가 나타난다(체크 상태는 앱 설정에 저장).
v14부터 복구용 DiagRescue(크리에이터 `todo`, To Do 버튼 대체)는 폐기 — 로더 11은 DisS 없이 교체된다.
기기에 남은 DiagRescue는 SkWr 진단 페이지의 `Delete DiagRescue` 버튼으로 지운다(런처 삭제는 같은 크리에이터 `todo`의 To Do 데이터까지 지울 수 있음).
v17: 누를 수 있는 줄은 둥근 테두리 버튼(누르면 반전), 디버그는 네모 체크박스로 표시.
v18: 리셋 버튼 폭 고정·가운데 정렬, 누르는 동안 `Hold... N`(취소 안내는 맨 아래 줄).
v16: Main의 `Soft Reset (hold 3s)` 줄을 3초 누르고 있으면 소프트 리셋(`SysReset`), 중간에 떼면 취소.
v15: Events의 INT(실크 펜 가로채기)는 맨 아래 `INT` 버튼으로만 전환(아래 절반 탭으로 실수로 켜져 홈 아이콘이 막힌 일이 있음).
- **로더 11 실측 (2026-10-16)**: 마지막 DisS 교체 후 Main `loader: build 11`, `protect: released` 확인.
- **리셋 1번 로더 업데이트 실측 성공 (2026-10-16)**: □→ 끄기 → MnSkPlugin.prc 재설치 → ClieDiag `Soft Reset` → 정상 부팅·한글 입력 확인.

| 진단 페이지 | 내용 |
| --- | --- |
| Info | ROM 버전, OEM/기기 ID, 화면 모드, `SoNy` 기능 번호, HR/Silk 라이브러리 탐색 결과 |
| Libr | 설치된 공유 라이브러리(`libr`) DB 목록 |
| SkWr | 설치된 `SkWr` 플러그인 목록, 플러그인 기록(feature `MnSk` #0) 해석, `[clear record]` 기록 삭제, `[DisS ...]` 실크 비활성화 스위치 전환(리셋 후 적용) |
| SkEv | SkWr 플러그인 이벤트 카운터: 이벤트 번호 0~15별 수신 횟수, 16 이상, out≠NULL 횟수, 마지막 이벤트, 보낸 키 횟수·키 번호·코드·자판 상태(#47~#49), 조합 상태(#50) |
| SkRaw | SkWr 플러그인 3단계 기록: Open/Start/PenDown/PenUp/DispChange 이벤트 앞 16바이트(16진수), Start 때 받은 슬롯 값, 그리기 창 크기, 직전 그리기 창 |
| Events | 펜·키 이벤트 기록, Graffiti 영역 가로채기(INT) 토글. `*`는 시스템에 넘기지 않은 이벤트 |

조작: 위/아래 버튼 → 이전/다음 페이지. 그 외 페이지별 탭 동작은 소스 주석 참고.
v2의 `SysLibTblEntry` 표 읽기, v3의 Silk(Vsk) 페이지는 Fatal Exception을 일으켜 제거했다.

## bin/MnSkPlugin.prc (SkWr 로더) + bin/MnSkCore.prc (자판 본체) — 10단계

| 파일 | 소스 / 빌드 | DB | 역할 |
| --- | --- | --- | --- |
| `MnSkPlugin.prc` | `src/skwr/loader.c` / `tools/build-loader.sh` | 타입 `SkWr`, 크리에이터 `MnSk`, `code 0` | SilkExtn이 부팅 때 불러오는 작은 로더(약 1KB). 한 번 설치하면 바꾸지 않음 |
| `MnSkCore.prc` | `src/skwr/skwr.c` / `tools/build-core.sh` | 타입 `SkCo`, 크리에이터 `MnSk`, 리소스 `MnCo 0`, DB 버전 1(로더 ABI) | 자판·조합 본체 |

- 로더: Open 때 실크 창 핸들을 슬롯에 저장. 자판이 켜지면(Start) 본체 DB를 열어 버전 확인 → `MnCo 0` 잠금 → DB 닫기 →
  이후 모든 이벤트를 본체 진입점으로 넘김. Stop 때 잠금 해제. 본체가 없거나 버전이 다르면 아무것도 안 함(#56에 사유).
- 본체는 위치 독립 코드(전역·A5·재배치 없음)라 다른 DB에서 불러와 그대로 실행된다. 빌드 스크립트가 검사.
- 리소스 타입을 `code`가 아닌 `MnCo`로 한 것은 본체 DB가 열린 동안 앱의 `DmGetResource('code', …)`에 걸리지 않게 하려는 것.
- **본체 업데이트 (10단계부터)**: 상태바 □→로 자판 끄기 → `MnSkCore.prc` 설치 → □→로 켜기. 리셋·DisS 불필요.
  자판이 켜진 채로 설치하면 안 된다(본체 코드가 잠겨 실행 중).
- **로더 11**: Open 때 자기 DB의 삭제 보호를 스스로 푼다(#60). ROM SilkExtn(0x2F00~0x3030)은 code 0 잠금 → 보호 →
  Open 호출 → 잠금 해제 → DB 닫기 순서라, 교체를 막는 것은 보호뿐이다. 등록은 유지.
  **로더 업데이트**: □→로 자판 끄기 → 새 MnSkPlugin.prc 설치 → 리셋 1번. 설치 후 리셋 전에 □→로 켜면 옛 코드 주소를
  불러 멈출 수 있다(강제 리셋). 로더 10 이하에서 11로 바꾸는 마지막 1회만 아래 DisS 절차가 필요하다.

이하 1~9단계 기록(당시 `tools/build-skwr.sh`로 SkWr 플러그인 하나에 전부 들어 있었음).
시작 코드·데이터 세그먼트 없이 진입 함수 `SkwrEntry`만 `code 0` 맨 앞에 둔다. 빌드 스크립트가 전역변수 없음, 진입점 오프셋 0, `%a5` 참조 없음을 검사한다.

- 1단계(빌드 1): Open(이벤트 1)을 받으면 `FtrSet('MnSk', 0, 0x534B0101)` 한 번만 호출. 모든 이벤트에 0 반환.
- 2단계(빌드 2): 그리기 없음. 모든 이벤트를 기록한다. #0 Open 표시(`0x534B0102`), #1~#16 이벤트 번호 0~15별 횟수(포화 증가),
  #17 마지막 이벤트(31~16 out≠NULL, 7~0 번호), #18 번호 16 이상 횟수, #19 out≠NULL 횟수.
- 2단계 실측(2026-10-16): 상태바 5번째 아이콘(□→, 외부 플러그인이 있을 때만 표시)이 **플러그인 전환 버튼**. 전환 후 카운터
  Open 1, Start 14, Stop 13, DispChange 5, PenDown 10, PenMove 4006, PenUp 10, Draw 0. 활성 플러그인은 Graffiti 영역의 펜 이동까지 받는다.
  상태바 4번째(흐린 ↓)는 Graffiti 영역 접기 버튼으로, 허용 플래그가 꺼져 있어 비활성.
- 3단계(빌드 3): 가설 — Open 이벤트 +10의 32비트 값이 실크 화면 창(SilkExtn 전역 172, SilkWareLib GetScrWindow와 같은 값).
  Open 때 이 값을 슬롯에 넣고, Start 때 돌려받아 그 창에 지우기·테두리·채운 사각형을 그린다. 이벤트 앞 16바이트를 #20~#42에 기록.
- 3단계 실측: 전환 버튼을 누르자 리부팅(그리기에서 예외). 부팅 후 Open 원본 `01 0F 00 00 48 AA 00 00 49 0A 00 00 00 17 00 06`.
  +10 값은 `0x17`로 창이 아니었다. +2 값 `0x000048AA`는 SilkExtn 전역 84(전환 직후 WinSetDrawWindow로 실크 영역을 지우는 창)와 일치.
- 4단계(빌드 4): Open +2 값을 창으로 사용. 그리기 전 핸들 범위(0 아님, 짝수, 0x400000 미만)와 창 구조체 windowBounds(오프셋 10)
  크기(너비 160/320, 높이 1~240)를 검사해 통과할 때만 그린다. #43 창 위치, #44 창 크기, #45 판정(1 그림, 2 핸들 이상, 3 크기 이상).
- 4단계 실측: **그리기 성공**(Graffiti 자리에 흰 바탕·테두리·사각형). 창 160×65(저해상도 좌표, 판정 r=1).
  Start `03 0F 00000140 00000082 00000008 01` = 너비 320, 높이 130, 8bpp, 고해상도 1. DispChange는 저해상도 앱에서 160×65·고해상도 0을 알림.
  PenDown/PenUp +2 x, +4 y(16비트): 오른쪽 아래 끝 탭 → (155,59)/(154,62). 펜 좌표 = 실크 영역 기준 저해상도 좌표(0~159, 0~64).
- 5단계(빌드 5): Start·Draw·DispChange에 A·B·C 버튼 3개를 그림. PenDown이 버튼 안이면 반전, PenUp이 같은 버튼이면
  `EvtEnqueueKey('A'~'C')`. 눌린 버튼 상태는 feature #46, 보낸 키 횟수 #47, 마지막 키 #48.
- **5단계 실측 성공 (2026-10-16)**: 런처에서 전환하자 Graffiti 자리에 A·B·C 버튼 표시. 메모장으로 가도 버튼이 유지되고,
  탭한 글자가 메모 본문에 그대로 입력됨("ABCBABCBAC"). **Graffiti 영역 자체 키보드 → 기존 앱 입력 경로 완성.**
  플러그인이 활성인 동안은 메모장 등 다른 앱에서도 상태바 아이콘 줄이 보인다.
- 6단계(빌드 6): [ga][gak][BS] 버튼, 각 버튼 위/아래 절반으로 전송 방식 비교. 위 = 방식 ① 2바이트 코드를 키 하나로
  `EvtEnqueueKey(0xB0A1)`, 아래 = 방식 ② 바이트 두 개를 키 두 개로(`0xB0`, `0xA1`). BS = `chrBackspace`.
  #46 눌린 버튼(1~3 위, 11~13 아래), #48 = (방식 << 16) | 코드.
- **6단계 실측 (2026-10-16)**: 방식 ① → 메모장에 가·각 정상 표시, **BS 한 번에 한글 한 글자씩 삭제**.
  방식 ② → `°¡`, `°¢`처럼 1바이트 문자 두 개로 들어가 깨짐(다시 그려지면 우연히 붙어 보이기도 하나 커서·삭제가 어긋남).
  **결론: 한글은 KSC5601 2바이트 코드를 키 이벤트 하나로 보낸다. 백스페이스는 시스템 처리로 충분.**
- 7단계(빌드 7): 안 B 4줄 자판. 한글 `ㅂㅈㄷㄱㅅㅗㅐㅔ / ㅁㄴㅇㄹㅎㅓㅏㅣ / ^ㅋㅌㅊㅍㅜㅡBS`, 영문 QWERTY, 123 숫자·기호,
  아랫줄 `한/A 123 , space . Ent`. 키 높이 16(저해상도). 자모는 KSC5601 호환 자모(0xA4A1~)로 단독 출력.
  전역 데이터 금지라 자판 표는 `switch`(PC 상대 점프표)로, 키 이름은 한 글자씩 스택에 채워 그린다.
- **7단계 실측 성공**: 자모 키 이름(CJKOS 글꼴)·반전 표시 정상, 자모·쌍자음·영문·숫자·기호·Ent 입력 정상.
- 8단계(빌드 8): 한글 조합. Shift는 자음만 쌍자음, 모음 변형은 연타(ㅏㅏ→ㅑ ㅓㅓ→ㅕ ㅗㅗ→ㅛ ㅜㅜ→ㅠ ㅐㅐ→ㅒ ㅔㅔ→ㅖ).
  복합모음·겹받침·받침 넘김, 조합 중 BS는 자모 단위. 화면의 조합 중 글자를 BS로 지우고 새 글자를 보내는 방식.
  KSC5601 2,350자 판정표(1398바이트 비트표 + 누계)를 `.text` 끝에 어셈블리로 넣고 `lea 표(%pc)`로 읽는다
  (`tools/gen-ksc-table.py` → `src/skwr/ksc_table.h`). KSC에 없는 음절(예: 똠)이 되면 앞 글자를 확정하고 새로 시작.
  조합 상태는 #50, 조합 중인 앱은 #51 — 앱이 바뀌면 조합을 버린다. 조합 중 본문 탭으로 커서를 옮기는 것은 감지 불가(알려진 한계).
- **8단계 실측 성공 (2026-10-16)**: 메모장에 "간단히 이렇게 쓸수있어요" 입력. 겹받침(ㅀ)·쌍자음 받침(ㅆ)·받침 넘김·연타(ㅛ) 정상.
- 9단계(빌드 9): HR 라이브러리로 고해상도 그리기 시도. SilkExtn이 부팅 때 `SysLibFind("Sony HR Library")` → `HROpen` 해 두고
  상태바를 HR 함수로 그린다(ROM 0x693C, 0x0178). 플러그인은 참조 번호만 찾아 쓴다. HR 트랩은 `sysLibTrapCustom`+순번(소니 헤더 미포함).
- 9단계 실측: `hr ref=6` 찾음. 그러나 `HRWinGetWindowExtent`가 실크 창에서 160×65를 돌려줘 안전장치가 저해상도로 그림(선명해지지 않음).
- 10단계: SilkExtn처럼 실크 창은 저해상도 크기로 배치를 계산하고 **좌표만 2배**로 HR 함수에 넘김.
  고해상도 여부는 Start/DispChange +14(#54). 저해상도 앱에서는 기존대로 그림. 로더·본체 분리(위 표).
- 10단계 실측: 로더 → 본체 실행 정상, 런처에서 고해상도 그리기 성공(얇은 테두리, 선명한 영문·숫자, 한글 키 이름 정상).
  아랫줄 오른쪽(space·.·Ent)이 안 그려짐: `BottomEdge * w / 160`이 w=320에서 16비트 곱셈 넘침 → 본체 11에서 배율 방식으로 수정.
- **본체 11 실측 (2026-10-16)**: □→ 끄기 → MnSkCore 설치 → □→ 켜기(리셋 없음)로 교체 성공, 아랫줄 정상.
  참고: 영문·숫자는 저해상도로 그려도 원래 선명했다(시스템이 고해상도 글꼴로 그림). HR 그리기로 달라진 것은 얇은 테두리뿐이고,
  한글 키 이름은 CJKOS 저해상도 글꼴을 늘려 그려 그대로 굵다.
- 본체 12: 고해상도일 때 한글 자모 키 이름을 선(벡터)으로 그림. 12×12 격자 선분, 2픽셀 굵기, `HRWinDrawLine`.
  27자(쌍자음 포함) 232바이트. `tools/gen-jamo-glyphs.py`(→ `src/skwr/jamo_glyphs.h`, `--preview`로 미리보기 PNG).
- 조합 로직 PC 시험: `tools/test-compose.sh` (Palm API를 `test/host_stub.h`로 흉내, 41개 사례). CI에서도 실행.
- 빌드 8부터 CI와 로컬의 플러그인 해시가 다르다. CI 진단(코드 1KB 블록 해시·트랩 목록) 결과 차이는 스택 슬롯 오프셋뿐이고
  트랩 호출과 나머지 코드는 같다(gcc 2.95 빌드 간 지역변수 배치 차이). 기능상 동일.
- 플러그인 교체 시: 기존 플러그인은 부팅 때 삭제 보호가 걸리므로, ClieDiag `[DisS]` 켜기 → 리셋 → 새 PRC 설치(덮어쓰기) → `[DisS]` 끄기 → 리셋.
  DisS가 켜지면 실크 영역(홈 아이콘 포함)이 없어 런처를 못 연다. 이때는 `bin/DiagRescue.prc`(빌드: `tools/build-rescue.sh`)를 설치하고
  하드웨어 To Do 버튼으로 진단 앱을 연다. ClieDiag와 같은 코드를 크리에이터 `todo`, 버전 2로 빌드한 것으로, ROM To Do(버전 1)보다 우선 실행된다.
  시험이 끝나면 DiagRescue를 지우면 원래 To Do가 돌아온다.
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
