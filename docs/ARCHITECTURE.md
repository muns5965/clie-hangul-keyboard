# 구조: NR70V 가상 실크스크린 키보드 (현행: 로더 11 · 본체 12) · 의존 수준(10절)

> 현행 구조만 적는다. 단계별 시험·실패한 가설은 [DEVELOPMENT_HISTORY.md](DEVELOPMENT_HISTORY.md),
> 업데이트·복구는 [RECOVERY.md](RECOVERY.md).

NR70V(Palm OS 4.1S)에는 OS 5의 Pen Input Manager가 없고, 가상 실크스크린(Graffiti 영역·상태바)은
ROM의 Sony 확장 `SilkExtn`(타입 `extn`, 크리에이터 `SeSi`)이 그린다. 이 영역에 다른 화면을 띄우는 공식 경로는
NX 이후의 Silklet(`Slkw`)·Vsk API인데 **NR70V에는 없다**(호출하면 Fatal Exception). 대신 ROM을 분석해 찾은
**`SkWr` 플러그인**("SilkWare") 구조를 쓴다. 아래는 모두 ROM 역분석과 에뮬레이터 실측으로 확인한 내용이다.

표기: 처리 순서는 `A → B → C`, 분기는 "조건 충족 시 A / 실패 시 B"로 쓴다. 함수는 처음 나올 때 짧게 설명하고,
자세한 역할은 [주요 Palm OS API](#8-주요-palm-os-api) 표에 모은다. 4바이트 식별자는 [식별자 표](#9-4바이트-식별자)를 본다.

### 1. SkWr 플러그인 호출 방식 (SilkExtn → 플러그인)
- 부팅 때 `SilkExtn`이 타입 **`SkWr`**인 DB를 모두 찾아(내장 1개 + 외부 최대 3개) 각 DB의 `code 0` 리소스 **맨 앞 바이트를 함수 입구로 보고 곧바로 호출**한다(68000 `jsr` 명령, 앱 실행 절차 없음).
  일반 앱이 아니므로 시작 코드·A5(전역변수)가 없다 → **전역변수·문자열 상수·초기화된 배열 금지**, 위치 독립 코드로 짠다.
- 호출 형식: `Err entry(UInt32 *slot, UInt8 *event, void *out)`, 반환 0 = 등록 유지(0이 아니면 등록 취소).
- Open 때만 첫 인자가 **슬롯 주소**이고, 이후 이벤트에는 거기 넣어 둔 **값**이 그대로 온다 → 필요한 값(창 핸들) 저장소로 쓴다.
  조합 상태와 진단값은 Palm OS의 Feature Manager로 관리한다. `FtrSet()`으로 기록하고 `FtrGet()`으로 조회하며(식별값은 크리에이터 `MnSk`),
  이 프로젝트의 기능값은 리셋 후 초기화된다.
- 이벤트 번호(`event[0]`): 1 Open, 3 Start, 4 Stop, 5 Draw, 6 DispChange, 7 PenDown, 8 PenMove, 9 PenUp.
  - Open +2(32비트): **실크 영역 창 핸들**. 창은 160×65(저해상도 좌표).
  - Start / DispChange +2 너비, +6 높이, +10 색 깊이, +14 **고해상도 여부**(1바이트).
  - PenDown / PenMove / PenUp +2 x, +4 y(16비트): 실크 영역 기준 저해상도 좌표(0~159, 0~64).
- 켜고 끄기: 외부 플러그인이 있으면 상태바 5번째에 **□→(전환) 아이콘**이 생긴다. 누를 때마다 활성 플러그인이 바뀌며
  이전 것에 Stop, 새 것에 Start가 간다. 이벤트는 활성 플러그인에만 온다.

### 2. 그리기
- 그릴 대상 창 지정(`WinSetDrawWindow`, Open 때 받은 실크 창) → 그리기 → 원래 창으로 복구. Start·Draw·DispChange 때 전체를 다시 그린다.
- 고해상도: `SilkExtn`이 부팅 때 `Sony HR Library`를 열어 두므로, 이름으로 라이브러리를 찾는 `SysLibFind`로 참조 번호만 얻어 HR 함수를 쓴다.
  창은 저해상도 크기 그대로 두고 **좌표만 2배**로 넘긴다(`SilkExtn` 자신도 이렇게 그림). 고해상도 여부는 Start/DispChange +14로 판단.
- 한글 키 이름: CJKOS 글꼴은 저해상도라 고해상도에서 굵게 보이므로, 자모를 12×12 격자 선분으로 그린다(`HRWinDrawLine`, `tools/gen-jamo-glyphs.py`).

### 3. 글자 보내기
- `EvtEnqueueKey()`는 지정한 문자 코드를 Palm OS 키 이벤트 큐에 넣는다. 현재 앱은 이를 `keyDownEvent`(키 입력 이벤트)로 처리한다.
  시험한 앱: 메모장·런처·ClieDiag. 다른 앱은 검증 대상.
- 한글은 **KS X 1001 완성형 EUC-KR 2바이트 코드를 키 하나로** 보낸다(`EvtEnqueueKey(0xB0A1)` → 가). 바이트를 나눠 보내면 깨진다.
  표시와 글자 경계 처리는 CJKOS가 한다([KOREAN_SUPPORT.md](KOREAN_SUPPORT.md)).
- BS 구분: 이미 확정된 완성형 글자는 시스템(CJKOS)이 BS 한 번에 한 글자씩 지운다.
  **조합 중**에는 입력기가 BS를 받아 자모 하나를 되돌린 글자로 교체한다(각 → 가 → ㄱ → 없음).
- 조합 중인 글자는 화면에 이미 나간 글자를 BS로 지우고 새 글자를 보내는 방식으로 고친다. KS X 1001 완성형 2,350자에 없는 음절이 되면 앞 글자를 확정하고 마지막 자모부터 새로 조합한다(예: 또 + ㅁ → `또ㅁ`, 똠은 2,350자에 없음).

### 4. 키보드 (구현 개요)
구글 단모음 키보드와 비슷한 4줄 자판을 적용했다(자세한 조합 규칙은 소스 `src/common/hangul_compose.h` 참고. MnSkCore·MnSonyKbd가 같은 파일을 각자 소스로 포함해 빌드한다).
- 조합: 초성·중성·종성, 복합모음, 겹받침, 받침 넘김, 조합 중 자모 단위 BS. KS X 1001에 없는 음절이 되면 앞 글자를 확정하고 새로 시작.
- 조합 시험: `tools/test-compose.sh`(Palm API를 `test/host_stub.h`로 흉내, 41개 사례, CI에서도 실행). 공용 조합 파일을 시험하므로 두 키보드에 함께 적용된다.
- 한글: `ㅂㅈㄷㄱㅅㅗㅐㅔ / ㅁㄴㅇㄹㅎㅓㅏㅣ / ⇧ㅋㅌㅊㅍㅜㅡ⌫`, Shift = 쌍자음, 같은 모음 연타 = ㅑㅕㅛㅠㅒㅖ.
- 영문 QWERTY, 123(숫자·기호), 아랫줄 `한/A 123 , space . Ent`.


### 5. 로더 + 본체

| 파일 | 소스 / 빌드 | DB | 역할 |
| --- | --- | --- | --- |
| `MnSkPlugin.prc` | `src/skwr/loader.c` / `tools/build-loader.sh` | 타입 `SkWr`, 크리에이터 `MnSk`, `code 0` | SilkExtn이 부팅 때 부르는 작은 로더(약 1KB). 변경 빈도 낮음, 필요하면 리셋 1번으로 교체 |
| `MnSkCore.prc` | `src/skwr/skwr.c` / `tools/build-core.sh` | 타입 `SkCo`, 크리에이터 `MnSk`, 리소스 `MnCo 0`, DB 버전 1(로더 ABI) | 자판·조합 본체 |

- `SilkExtn`은 부팅 때 각 `SkWr` DB를 다음 순서로 처리한다(ROM 0x2F00~0x3030).

  `code 0` 리소스 잠금 → DB 삭제 보호(`DmDatabaseProtect`) → 플러그인 `Open` 호출 → 리소스 잠금 해제(`MemHandleUnlock`) → DB 닫기

  교체를 막는 것은 DB 삭제 보호뿐이므로, **로더는 `Open` 처리 중 자기 DB의 삭제 보호를 해제**한다(feature #60). 등록은 유지한다.
- 로더 `Open`: 실크 창 핸들을 슬롯에 저장 → 자기 DB 찾기(`DmGetNextDatabaseByTypeCreator`, 타입 `SkWr`·크리에이터 `MnSk`) → 삭제 보호 해제(`DmDatabaseProtect(…, false)`).
- 로더의 본체 불러오기(`src/skwr/loader.c` `LoadCore()` 순서 그대로). 자판이 켜진 동안 Start·Draw·DispChange에서 본체가 없으면 실행한다.

  `DmOpenDatabaseByTypeCreator('SkCo','MnSk', 읽기 전용)` → `DmOpenDatabaseInfo` + `DmDatabaseInfo`로 DB 버전 읽기 → 버전 = 1(ABI) 확인
  → `DmFindResource(db, 'MnCo', 0)`로 리소스 번호 → `DmGetResourceIndex`로 리소스 핸들 → `MemHandleLock`으로 코드 주소 → `DmCloseDatabase`
  → 핸들·주소를 feature #57·#58에 저장 → 이후 모든 이벤트를 본체 진입점(코드 주소)으로 넘김

  실패 시: DB 없음 2 / 버전 다름 4 / 리소스 없음 3 / 잠금 실패 5를 #56에 기록하고 아무것도 하지 않는다(멈추지 않음).
  Stop: 본체에 넘긴 뒤 `MemHandleUnlock`으로 잠금 해제 → 본체 DB를 바꿀 수 있는 상태.
- 본체는 위치 독립 코드(전역·A5·재배치 없음)라 다른 DB에서 불러와 그대로 실행된다. 빌드 스크립트가 검사.
- 리소스 타입을 `code`가 아닌 `MnCo`로 한 것은 본체 DB가 열린 동안 앱의 `DmGetResource('code', …)`에 걸리지 않게 하려는 것.
- 데이터 표(KS X 1001 판정표 `tools/gen-ksc-table.py`, 자모 선 글꼴 `tools/gen-jamo-glyphs.py`)는 `.text` 끝에 어셈블리로 넣고 PC 상대로 읽는다.

### 6. 조합 중 커서 이동
본문 탭은 앱으로 가는 펜 입력이라 SkWr 플러그인에는 오지 않는다. 그래서 조합 중 커서가 옮겨져도 입력기는 알 수 없고,
다음 자모 입력 때 "조합 중 글자 교체용 BS"가 옮겨진 커서 위치에 적용될 수 있었다. 앱이 바뀌는 것은 감지해 조합을 버린다(#51).
MnSkCore build 17: 키를 누를 때 폼 초점 입력칸(`FrmGetActiveForm`·`FrmGetFocus`·`FrmGetObjectPtr`)의 `FldGetInsPtPosition`을
직전 키 때 위치 + 보낸 키로 생길 변화(2바이트 글자 +2, 1바이트 +1, 조합 BS -2)와 비교해, 조합 중인데 다르거나 입력칸이 바뀌었으면
조합을 끝낸다(BS를 보내지 않음). 폼 필드가 아닌 입력(표 안 필드 등)과 조합 아닌 BS 직후는 비교하지 않는다.
바이트 계산이므로 2바이트 글자 경계 처리(MnHangul 또는 CJKOS)가 있어야 정확하다(없으면 조합이 일찍 끝나는 쪽으로 안전).
MnSonyKbd는 EvtGetEvent에서 앱 화면 펜 누름을 직접 보고 조합을 끝내고, v10부터 같은 위치 비교도 한다(펜 없이 앱이 커서를 옮긴 경우).
비교 코드는 `src/common/cursor_track.h` 하나를 두 모듈이 포함한다(기록: MnSkCore `MnSk` #81~#87, MnSonyKbd `MnSy` #191~#197).

### 7. 상태 기록 (feature `MnSk`)
주요 번호: #0 로더 Open 기록, #46 눌린 키, #47 보낸 키 수, #48 마지막 키, #49 자판 상태, #50 조합 상태, #51 조합 중인 앱,
#52~#54 HR 라이브러리·고해상도, #55 본체 빌드, #56 본체 불러오기 결과, #57~#59 본체 핸들·주소·켜짐, #60 보호 해제 결과.
ClieDiag의 디버그 페이지에서 볼 수 있다.

### 8. 주요 Palm OS API
| 함수 | 기능 | 이 프로젝트에서 쓰는 곳 |
| --- | --- | --- |
| `FtrSet` / `FtrGet` | Feature Manager에 값 기록 / 조회(리셋 때 초기화) | 조합 상태·자판 상태·진단값 저장(크리에이터 `MnSk`) |
| `WinSetDrawWindow` | 그릴 대상 창 지정, 이전 창을 돌려줌 | 실크 창에 그린 뒤 원래 창으로 복구 |
| `EvtEnqueueKey` | 키 입력 이벤트를 이벤트 큐에 추가 | 글자·BS를 현재 앱으로 보냄 |
| `SysLibFind` | 이름으로 이미 열린 시스템 라이브러리 찾기 | `Sony HR Library` 참조 번호 |
| `HRWinDrawLine` 등 HR 함수 | Sony 고해상도 라이브러리로 선·글자·사각형 그리기 | 고해상도 자판, 자모 선 글꼴 |
| `DmDatabaseProtect` | **DB 삭제 보호** 켜기/끄기(보호 상태는 리셋 때 사라짐) | SilkExtn이 켬, 로더가 자기 DB에서 끔 |
| `DmGetNextDatabaseByTypeCreator` | 타입·크리에이터로 DB 찾기 | 로더가 자기 DB 찾기 |
| `DmOpenDatabaseByTypeCreator` | 타입·크리에이터로 DB 열기 | 본체 DB 열기 |
| `DmOpenDatabaseInfo` / `DmDatabaseInfo` | 열린 DB의 ID / DB 정보(버전 등) 읽기 | 본체 버전(ABI) 확인 |
| `DmFindResource` | 리소스 타입·ID로 DB 안 리소스 번호 찾기 | 본체 `MnCo 0` 찾기 |
| `DmGetResourceIndex` | 리소스 번호로 리소스 핸들 얻기 | 본체 코드 핸들 |
| `MemHandleLock` / `MemHandleUnlock` | **메모리 핸들 잠금**(주소 고정) / 해제 | 본체 코드 주소 얻기 / Stop 때 해제 |
| `DmCloseDatabase` | DB 닫기 | 본체 잠근 뒤 바로 닫음(앱 종료 때 자동 닫힘 문제 회피) |
| `SysReset` | 소프트 리셋 | ClieDiag `Soft Reset` |

`DmDatabaseProtect`(DB 삭제 보호)와 `MemHandleUnlock`(메모리 잠금 해제)은 서로 다른 보호·잠금이다.
`DmGetResource`(열린 모든 리소스 DB에서 타입·ID로 검색)는 이 프로젝트에서 쓰지 않는다. 본체 리소스 타입을 `code`가 아닌
`MnCo`로 한 것은, 본체 DB가 열린 순간 앱의 `DmGetResource('code', …)` 검색에 잘못 걸리지 않게 하기 위해서다.

### 9. 4바이트 식별자
| 식별자 | 종류 | 의미 |
| --- | --- | --- |
| `SkWr` | DB 타입 | `SilkExtn`이 부팅 때 찾는 플러그인(로더) |
| `SkCo` | DB 타입 | 키보드 본체 DB |
| `MnCo` | 리소스 타입 | 본체 코드 |
| `code` | 리소스 타입 | 실행 코드(로더의 `code 0`) |
| `MnSk` | 크리에이터 ID | 로더·본체 공통. Feature Manager 식별값으로도 사용 |
| `MnDg` | 크리에이터 ID | ClieDiag |
| `SeSi` | 크리에이터 ID | Sony `SilkExtn`(ROM), DisS DB |
| `DisS` | DB 타입 | 실크 초기화를 건너뛰는 비상 스위치 |
| `extn` | DB 타입 | 시스템 확장(`SilkExtn`) |
| `MnHg` | 크리에이터 ID | MnHangul(한글 표시). 리소스 `HgPt`(그리기 패치)·`HgIt`(Intl)·`HgWd`(폭)·`HgJb`(조합형 글꼴) |
| `MnSy` | 크리에이터 ID | MnSonyKbd(Sony 가상 키보드 한글). 리소스 `SyPt`(키 글자 패치)·`SyIm`(입력·`한A`)·`SyIc`(`한A` 그림)·`SyJm`(자모) |

### 10. 의존 수준 (이식할 때 다시 확인할 곳)

이 저장소의 모듈이 무엇에 기대어 동작하는지 세 단계로 나눈다. 아래로 갈수록 ROM·OS 버전이나 기종이 바뀌면 깨지기 쉽다.
MnSonyKbd는 거의 전부가 3단계(Sony 키보드 동작 관찰)에 기대므로 NR70V 전용이다. MnHangul은 2단계(`BltDrawChars`)와
3단계 중 글꼴 구조에 기대지만 Sony 키보드와는 무관해, 같은 Palm OS 4.x 기종으로 옮길 가능성이 상대적으로 높다.

**1단계: 공개 API (SDK에 원형·문서 있음)**
- 트랩 교체 `SysGetTrapAddress`/`SysSetTrapAddress`, Intl 루틴 교체 `IntlGetRoutineAddress`/`IntlSetRoutineAddress`
- 기록·설정 `FtrGet`/`FtrSet`, `PrefGetAppPreferences`/`PrefSetAppPreferences`
- 리소스 잠금·보호 `DmGetResource`, `DmWrite`, `DmDatabaseProtect`
- 입력 `EvtEnqueueKey`, `GrfGetState`, 이벤트 구조의 펜 좌표 / 리셋 알림 `sysAppLaunchCmdSystemReset`, `SysReset`
- 조합 중 커서 이동 감지 `FrmGetActiveForm`·`FrmGetFocus`·`FrmGetObjectType`·`FrmGetObjectPtr`·`FldGetInsPtPosition`(`src/common/cursor_track.h`)
- CJKOS 감지 `FtrGet('dCJK', 0)`: CJKOS가 켜질 때 등록하고 끌 때 지우는 자기 Feature(MnHangul v61·MnSonyKbd v11, ClieDiag Ftr 페이지 실측)

**2단계: 비공개 함수 (존재하지만 문서·원형 없음, 역분석)**

| 대상 | 쓰는 곳 | 근거 |
| --- | --- | --- |
| `BltDrawChars`(트랩 0xA378) | MnHangul 그리기, MnSonyKbd 키 글자 | ROM 호출부 역분석으로 인자 7개(26바이트) 확인([KOREAN_SUPPORT.md](KOREAN_SUPPORT.md)) |
| Sony `SkWr` 플러그인 호출 방식 | Mn 가상 키보드 로더 | `SilkExtn` ROM 역분석(1절) |
| Sony "Silk Ware Library" `GetScrWindow`(custom + 4) | MnSonyKbd v8 진단만 | ROM `SlSW` v100 정적 분석: `Err (UInt16 ref, WinHandle *out)`, SilkExtn 전역 +172를 읽기만 함 |

**1.5단계: 제조사 공개 확장 API (Sony CLIÉ SDK에 원형 있음)**
- Sony HR 라이브러리 `HRWinScreenMode`(sysLibTrapCustom + 48): 앱 화면 고해상도. Sony SDK 헤더는 저작권 문제로 커밋하지 않아 필요한 트랩만 직접 선언

**3단계: 함수가 아니라 내부 구조·관찰에 기댄 부분 (가장 취약)**

| 분류 | 내용 | 쓰는 곳 |
| --- | --- | --- |
| OS 내부 구조체 직접 읽기 | 창 구조 `+8`(windowFlags, 0x4000 = 화면 밖 창)·`+26`(비트맵) | MnSonyKbd 가상 실크 창 판별 |
| | 글꼴 구조(`FontType`) 필드·폭 표를 읽고 1글자 임시 글꼴을 만들어 `BltDrawChars`에 넘김 | MnHangul 한글, MnSonyKbd 자모 |
| ROM 고정 주소 | `BltDrawChars` = `0x1008359A`, 시작 바이트 `4E56FFF0 48E71E30` 확인 뒤 사용 | MnSonyKbd 자모 그리기 |
| | ROM = `0x10000000`~`0x107FFFFF` 범위 판정 | 연결 확인(`src/sony/chain.h`, MnHangul `BltFind`), CJKOS 감지 보조(아래) |
| ROM 리소스 내용 | `SilkExtn`(`extn`/`SeSi`)의 `Tbmp` 2400·2500 = ⇪ 그림(보통·켜짐) | `한A` 그림, 한/영 판정 |
| Sony 키보드 동작 관찰 | 자판 그리는 순서(숫자 줄 → qwerty 줄 → `\` → ⇪ 그림 → …), 키 글자 글꼴 높이 22 | 한/영 표시 판정 |
| | 상태(`GrfSetState`)를 그린 **뒤에** 바꿈, 키를 누르는 순간 잠깐 ⇪ 꺼짐 + 일시 쉬프트 | 판정 규칙 |
| | 펜이 Sony 다시 그리기보다 앱 `EvtGetEvent`에 먼저 옴 | 펜 누름 때 연결 복구, ⇪ 판단 |
| 화면 좌표 | ⇪ 키 x 0~12·y 182~191, 키 영역 y < 213(저해상도 좌표) | MnSonyKbd |
| 다른 프로그램 동작 | CJKOS 감지 보조 = `FntSetFont`·`FrmDrawForm`·`FldHandleEvent`·`StrCompare`가 ROM 밖인가. MnHangul v61·MnSonyKbd v11부터 주 판정은 1단계 `FtrGet('dCJK', 0)`(CJKOS가 켜질 때 등록·끌 때 삭제, 실측)이고, 이 방식은 시험 기간 비교용(OR)으로만 남김 | MnHangul `CJKOS`, MnSonyKbd 첫 화면 |
| | CJKOS는 꺼질 때 자기가 켜질 때 저장한 주소로 되돌린다(실측) | 연결 복구 설계 |
| 자체 약속 | 다른 모듈의 아래 함수 주소 = 진입점 − 4, 자기가 넣은 키 표시 keyCode `0x4D48` | 연결 따라가기, 입력 되먹임 방지 |
| 부팅·복구 | `DisS` DB가 있으면 `SilkExtn`이 실크 초기화를 건너뜀(ROM 0x27EC) | 복구 수단([RECOVERY.md](RECOVERY.md)) |

자세한 실측 근거는 [KOREAN_SUPPORT.md](KOREAN_SUPPORT.md)(MnHangul·MnSonyKbd 판별 기록)와
[DEVELOPMENT_HISTORY.md](DEVELOPMENT_HISTORY.md)(ROM 역분석)에 있다.
