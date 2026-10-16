# 구조: NR70V 가상 실크스크린 키보드 (현행: 로더 11 · 본체 12)

> 현행 구조만 적는다. 단계별 시험·실패한 가설은 [DEVELOPMENT_HISTORY.md](DEVELOPMENT_HISTORY.md),
> 업데이트·복구는 [RECOVERY.md](RECOVERY.md). (문서 분리 기준 커밋 `117932a`)

NR70V(Palm OS 4.1S)에는 OS 5의 Pen Input Manager가 없고, 가상 실크스크린(Graffiti 영역·상태바)은
ROM의 Sony 확장 `SilkExtn`(타입 `extn`, 크리에이터 `SeSi`)이 그린다. 이 영역에 다른 화면을 띄우는 공식 경로는
NX 이후의 Silklet(`Slkw`)·Vsk API인데 **NR70V에는 없다**(호출하면 Fatal Exception). 대신 ROM을 분석해 찾은
**`SkWr` 플러그인**("SilkWare") 구조를 쓴다. 아래는 모두 ROM 역분석과 에뮬레이터 실측으로 확인한 내용이다.

### 1. SkWr 플러그인 호출 방식 (SilkExtn → 플러그인)
- 부팅 때 `SilkExtn`이 타입 **`SkWr`**인 DB를 모두 찾아(내장 1개 + 외부 최대 3개) 각 DB의 `code 0` 리소스 **맨 앞 바이트를 함수 입구로 보고 곧바로 호출**한다(68000 `jsr` 명령, 앱 실행 절차 없음).
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
- `EvtEnqueueKey(문자, 0, 0)` → 현재 앱에 `keyDownEvent`로 들어간다(시험한 앱: 메모장·런처. 다른 앱은 검증 대상).
- 한글은 **KS X 1001 완성형 EUC-KR 2바이트 코드를 키 하나로** 보낸다(`EvtEnqueueKey(0xB0A1)` → 가). 바이트를 나눠 보내면 깨진다.
  표시와 글자 경계 처리는 CJKOS가 한다([KOREAN_SUPPORT.md](KOREAN_SUPPORT.md)).
- BS 구분: 이미 확정된 완성형 글자는 시스템(CJKOS)이 BS 한 번에 한 글자씩 지운다.
  **조합 중**에는 입력기가 BS를 받아 자모 하나를 되돌린 글자로 교체한다(각 → 가 → ㄱ → 없음).
- 조합 중인 글자는 화면에 이미 나간 글자를 BS로 지우고 새 글자를 보내는 방식으로 고친다. KSC5601에 없는 음절은 조합을 끊는다.

### 4. 키보드 (구현 개요)
구글 단모음 키보드와 비슷한 4줄 자판을 적용했다(자세한 조합 규칙은 소스 `src/skwr/skwr.c` 참고).
- 조합: 초성·중성·종성, 복합모음, 겹받침, 받침 넘김, 조합 중 자모 단위 BS. KS X 1001에 없는 음절이 되면 앞 글자를 확정하고 새로 시작.
- 조합 시험: `tools/test-compose.sh`(Palm API를 `test/host_stub.h`로 흉내, 41개 사례, CI에서도 실행).
- 한글: `ㅂㅈㄷㄱㅅㅗㅐㅔ / ㅁㄴㅇㄹㅎㅓㅏㅣ / ⇧ㅋㅌㅊㅍㅜㅡ⌫`, Shift = 쌍자음, 같은 모음 연타 = ㅑㅕㅛㅠㅒㅖ.
- 영문 QWERTY, 123(숫자·기호), 아랫줄 `한/A 123 , space . Ent`.


### 5. 로더 + 본체

| 파일 | 소스 / 빌드 | DB | 역할 |
| --- | --- | --- | --- |
| `MnSkPlugin.prc` | `src/skwr/loader.c` / `tools/build-loader.sh` | 타입 `SkWr`, 크리에이터 `MnSk`, `code 0` | SilkExtn이 부팅 때 부르는 작은 로더(약 1KB). 변경 빈도 낮음, 필요하면 리셋 1번으로 교체 |
| `MnSkCore.prc` | `src/skwr/skwr.c` / `tools/build-core.sh` | 타입 `SkCo`, 크리에이터 `MnSk`, 리소스 `MnCo 0`, DB 버전 1(로더 ABI) | 자판·조합 본체 |

- `SilkExtn`은 부팅 때 SkWr DB마다 `code 0` 잠금 → **삭제 보호**(`DmDatabaseProtect`) → Open 호출 → 잠금 해제 → DB 닫기 순서로 처리한다(ROM 0x2F00~0x3030).
  교체를 막는 것은 보호뿐이므로, **로더는 Open 안에서 자기 DB의 보호를 푼다**(feature #60). 등록은 유지한다.
- 로더: Open 때 실크 창 핸들을 슬롯에 저장. 자판이 켜지면(Start) 본체 DB를 열어 버전 확인 → `MnCo 0` 잠금 → DB 닫기 →
  이후 모든 이벤트를 본체 진입점으로 넘김. Stop 때 잠금 해제. 본체가 없거나 버전이 다르면 아무것도 안 함(#56에 사유).
- 본체는 위치 독립 코드(전역·A5·재배치 없음)라 다른 DB에서 불러와 그대로 실행된다. 빌드 스크립트가 검사.
- 리소스 타입을 `code`가 아닌 `MnCo`로 한 것은 본체 DB가 열린 동안 앱의 `DmGetResource('code', …)`에 걸리지 않게 하려는 것.
- 데이터 표(KS X 1001 판정표 `tools/gen-ksc-table.py`, 자모 선 글꼴 `tools/gen-jamo-glyphs.py`)는 `.text` 끝에 어셈블리로 넣고 PC 상대로 읽는다.

### 6. 조합 중 커서 이동 (알려진 한계의 원인)
본문 탭은 앱으로 가는 펜 입력이라 SkWr 플러그인에는 오지 않는다. 그래서 조합 중 커서가 옮겨져도 입력기는 조합 상태를 유지하고,
다음 자모 입력 때 "조합 중 글자 교체용 BS"가 옮겨진 커서 위치에 적용될 수 있다. 앱이 바뀌는 것은 감지해 조합을 버린다(#51).

### 7. 상태 기록 (feature `MnSk`)
주요 번호: #0 로더 Open 기록, #46 눌린 키, #47 보낸 키 수, #48 마지막 키, #49 자판 상태, #50 조합 상태, #51 조합 중인 앱,
#52~#54 HR 라이브러리·고해상도, #55 본체 빌드, #56 본체 불러오기 결과, #57~#59 본체 핸들·주소·켜짐, #60 보호 해제 결과.
ClieDiag의 디버그 페이지에서 볼 수 있다.
