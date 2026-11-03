# Palm OS 5(TG50) 한글 구현 조사

> 조사 기록이다. 이 결과로 만든 OS5 모듈은 `MnHangul5`(`src/os5/`, 7절)다. NX70V는 7절 끝.
> 시험 환경: [TG50 포팅 포크](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-tg50)의 네이티브 `cp-uarm --tg50`,
> uARM 기준 해시와 같은 TG50 NOR ROM(OS 5.0 영문). 실기 시험은 하지 않았다. 날짜: 2026-10-22~23.

## 요약

| 항목 | OS4(NR70V) | OS5(TG50) 조사 결과 |
| --- | --- | --- |
| 입력 후킹 | 68k 트랩(`EvtGetEvent` 등) | 메모장은 68k 앱이라 `EvtGetEvent`·`SysHandleEvent`를 68k 트랩으로 부르지만, **68k 후킹은 안 된다**: `SysSetTrapAddress`는 `sysErrNotAllowed`(0x0505), `SysGetTrapAddress`가 주는 68k 스텁을 고쳐도 PACE가 거치지 않음(7절). 표 8 `EvtGetEvent` 칸(`0x824`)을 바꾼다 |
| 물리 키 이벤트 | (해당 없음) | 키마다 `keyDown` 3개: `chr 0x1720`(누름, `keyCode` = 글자) → 글자(`keyCode 0`, `modifiers 0`) → `chr 0x1721`(뗌). 원시 이벤트는 `modifiers 0x0008` |
| 그래피티 | 키 이벤트로 들어옴 | Graffiti 키 → 대화상자. 글자는 키 이벤트 없이 Done 때 입력칸에 한꺼번에 들어감 |
| 글자 그리기·폭·글자 경계 | 68k 트랩(`BltDrawChars`, 폭 5개, Intl 6개) 교체(MnHangul) | 입력칸 처리가 ARM 안에서 끝나 68k 트랩을 거치지 않음 → MnHangul 방식은 효과 없음 |
| 시스템 전체 패치 지점 | 68k 트랩 테이블 | **RAM에 있는 ARM 함수 주소표**(표 8·12). 입력칸도 이 표를 거쳐 그리기·폭 함수를 부름 |
| 기존 3개 키보드 | 동작 | MnSkCore(실크 없음)·MnSonyKbd(Sony 가상 키보드 없음)는 해당 없음. MnKbdHan의 `SysKeyboardDialog`도 ARM에서 직접 불려 68k 패치로 안 걸림 |

## 1. 68k 트랩 기록 (PACE)

방법: 포크 작업 사본의 `CPU.cpp`(68k `trap #15` 처리 지점)에서 트랩 번호를 세어, 동작별 구간으로 나눠 출력(진단 전용, 커밋 안 함).

| 구간 | 68k 앱이 부른 트랩 |
| --- | --- |
| 대기 | `EvtGetEvent`, `SysHandleEvent`, `MenuHandleEvent`, `FrmDispatchEvent`, `FtrGet` |
| 메모장에 `abcde` 입력(화면 정상) | 위 5개뿐(`SysHandleEvent`가 16회 더 많음 = 원시 키 이벤트) |
| Edit → Keyboard 대화상자 동안 | 68k 호출 거의 없음 |

`BltDrawChars`·`WinDrawChars`·`Fnt*`·`Fld*`·`IntlDispatch`·`SysKeyboardDialog`는 한 번도 없었다.

## 2. ARM 쪽 호출 구조

- PACE 분배 함수(ROM `0x200f4c6c`)는 RAM 표(시험 세션 `0x00500cdc`)에서 트랩 번호로 연결 함수(glue) 주소를 읽는다.
  `BltDrawChars`(0xA378)의 연결 함수는 "지원 안 함" 처리로 간다(OS5에는 없음).
- 연결 함수와 OS 내부 ARM 코드는 같은 형식의 가져오기 스텁으로 다른 모듈 함수를 부른다.

  ```
  ldr ip, [r9, #-8]      ; 표 8
  ldr pc, [ip, #0xb18]   ; 표 8의 0xb18번 칸 = WinDrawChars
  ```

  `[r9-8]` = 표 8(`0x1ff0501c`), `[r9-12]` = 표 12(`0x1ff07f28`). 두 주소는 이번 ROM·세션 기준.
- 롬을 정적 분석해 68k 트랩 연결 함수 1,148개와 Intl 선택자 32개가 표 몇 번 몇 번째 칸으로 가는지 대응표를 만들었다(capstone).
  일부 칸은 휴리스틱이라 틀릴 수 있다. GDB로 확인한 칸은 `0xb18`, `0xb14`, `0x3ec`.

| 분류 | 표 칸 → 함수 |
| --- | --- |
| 그리기 | 표8 `0xb18` WinDrawChars, `0xb14` WinDrawChar, `0xbd0` WinPaintChars, `0xbcc` WinPaintChar, `0xb24` WinDrawInvertedChars, `0xbac` WinInvertChars, `0xb44` WinEraseChars, `0xb38` WinDrawTruncChars |
| 폭·줄바꿈 | 표8 `0x3ec` FntCharsWidth, `0x3e8` FntCharsInWidth, `0x40c` FntLineWidth, `0x418` FntWidthToOffset, `0x41c` FntWordWrap |
| 글자 경계(Text Manager) | 표8 `0x97c` TxtByteAttr, `0x984` TxtCharAttr, `0x98c` TxtCharIsValid, `0x994` TxtCharXAttr, `0x998` TxtCharBounds, `0x99c` TxtCharSize, `0x9b0` TxtGetChar, `0x9b4` TxtGetNextChar, `0x9b8` TxtGetPreviousChar, `0x9bc` TxtGetTruncationOffset, `0x9c0` TxtGetWordWrapOffset, `0x9cc` TxtParamString, `0x9d8` TxtSetNextChar, `0x414` TxtCharWidth, `0x980` TxtCaselessCompare, `0x9a0` TxtCompare, `0x9ac` TxtFindString, `0x9d0` TxtPrepFindString, `0x9d4` TxtReplaceStr, `0x9e4` TxtWordBounds (뒤 6개는 v8 때 같은 방법 — Intl 선택자 → 68k 연결 함수 → 표 8 칸 — 으로 TG50·NX70V ROM에서 확인) (**수정**: 처음 대응표는 이름이 한 칸씩 밀려 있었다. ROM `IntlDispatch` 분기표 `0x200ea1fc`를 직접 읽어 바로잡음. `WChar` 출력 인자는 16비트, `TxtCharBounds`의 시작·끝은 32비트) |
| 입력 | 표8 `0x824` EvtGetEvent, `0x298` EvtEnqueueKey (PACE 연결 함수가 부르는 가져오기 스텁에서 확인) |
| 폼 | 표12 `0x224` FrmGetActiveForm, `0x250` FrmGetNumberOfObjects, `0x254` FrmGetObjectBounds, `0x26c` FrmGetObjectType, `0x278` FrmGetWindowHandle |
| 그 밖 그리기 | 표8 `0xb2c` WinDrawPixel, `0xb50` WinEraseRectangle, `0xb64` WinGetActiveWindow, `0xc38` WinSetDrawWindow, `0xc3c` WinSetForeColor, `0xc4c` WinSetTextColor, `0xc98` WinSetCoordinateSystem, `0x408` FntLineHeight |
| 입력칸 | 표12 `0x12c` FldDrawField |

## 3. GDB 확인

포크 작업 사본을 `-DGDB_STUB_ENABLED`로 빌드(`gdbstub.cpp`의 `<unicstd>` 오타는 로컬에서 `<unistd.h>`로 고침)하고 원격 디버깅 클라이언트로 시험.

- 메모장에서 `r` 입력: `FntCharsWidth` 실제 함수에서 멈춤(호출자 = UI 모듈 입력칸 코드, `r12` = 표 8),
  `WinDrawChar` 실제 함수에서 멈춤(`r0` = `0x72`, 좌표 16,16, 역시 표 8 경유).
  한 글자 입력 때는 `WinDrawChars`·`WinPaintChars`·`WinDrawTruncChars`는 불리지 않았다.
- **표 칸 교체 시험**: 표 8 `0xb14`(WinDrawChar)를 ROM의 `bx lr` 주소로 바꾸고 `def` 입력 → 화면에 그려지지 않음(저장은 됨).
  원래 주소로 되돌리고 `g` 입력 → 그려짐. 메모 목록에는 `Abcdefg`. → **표 칸을 바꾸면 ARM 입력칸 코드의 호출이 바뀐다.**
  단, 이 쓰기는 디버거 메모리 쓰기라 MMU 쓰기 보호를 우회했을 수 있다 → 아래 4절에서 앱 코드로 다시 확인.

## 4. 앱 코드에서 확인 (진단 PRC, 저장소 밖)

| 시험 | 방법 | 결과 |
| --- | --- | --- |
| 68k 앱에서 표 쓰기 | 일반 68k 앱이 표 8 `0xb14` 칸을 읽고 같은 값을 다시 씀 | 오류 없이 써짐(데이터 중단 없음). 68k에서 읽으면 바이트 순서가 반대(리틀엔디언 값)라 바꿔 읽어야 함 |
| ARM 네이티브 코드 실행 | `gcc-arm-none-eabi`(13.2, `-march=armv5te -marm -fPIC -ffreestanding -nostdlib`)로 빌드한 60바이트 함수를 `armc 1000` 리소스로 넣고 68k에서 `PceNativeCall`(트랩 0xA45A, 헤더 없이 직접 선언) | 실행·반환값 정상 |
| 표 위치 찾기 | ARM 함수 안에서 `r9`과 `[r9-8]` 읽기 | `r9` = `0x1ff04a68`, `[r9-8]` = `0x1ff0501c`(표 8), 표 8 `0xb14` = `0x20a52250`(WinDrawChar). 고정 주소 없이 `r9`으로 표를 찾을 수 있음 |

에뮬레이터(uARM 계열)가 MMU 쓰기 권한까지 실기와 똑같이 흉내 내는지는 확인하지 않았다. 실기에서는 KOSPI가 같은 구조로 동작하는 점을 근거로 쓸 수 있다고 추정한다.

## 5. 참고: Korean OS Plug-In(KOSPI) 2.3.0

OS5에서 시스템 전체 한글이 되는 기존 제품. 사용자가 시험용으로 제공한 파일을 에뮬레이터에서만 설치했다(재배포 금지 라이선스라 저장소에 넣지 않음, 코드 분석 안 함).

- TG50에서 물리 키보드 두벌식 입력·표시 확인(Shift+Space 한/영).
  포크의 Memory Stick 레지스터 영역이 1KiB뿐이라 `0x140010c0~0x140010f8` 접근에서 리셋이 반복됐고, 진단 빌드에서 영역을 넓혀 동작시켰다(포크 반영 대기).
- `KoreanOSLib`의 `info` 리소스 31개는 (표 번호, 칸) 목록으로 보이며, 위 대응표로 이름을 붙이면
  그리기 7개, 폭 5개, Text Manager 15개, 표12 3개(`FldSetAttributes`, `MenuEraseStatus`, `InsPtSetHeight`), `SysGetOSVersionString`이다
  (이름은 처음 대응표 기준이라 Text Manager 쪽은 한 칸씩 밀려 있을 수 있다 — 2절 수정 참고. 개수는 그대로).
  → 같은 구조(표 칸 교체)로 구현돼 있다고 추정한다.

## 6. OS5 모듈 구상 (처음 계획)

| 부분 | 방법 | 실제(7절) |
| --- | --- | --- |
| 설치 | 리셋 알림 때 68k 앱이 `PceNativeCall`로 ARM 코드를 불러 `r9`에서 표 8·12를 찾고 칸을 교체, 원래 주소 보관 | 표 주소만 ARM으로 얻고 칸 교체는 68k 앱이 함. 리셋 뒤에는 리셋 완료 알림에서 |
| 그리기 | `WinDrawChar(s)`·`WinPaintChar(s)` 등: 한글 2바이트면 조합형 글꼴로 그리고, 아니면 원래 함수 | 그대로 |
| 폭·경계 | NR70V `width.c`·`intl.c` 규칙을 ARM으로 빌드해 Fnt 5개·Text Manager 칸에 연결 | 그대로 |
| 입력 | 68k `EvtGetEvent` 후킹으로 `0x1720` 원시 키를 받아 `hangul_compose.h`로 조합 | **68k 후킹 불가** → 표 8 `EvtGetEvent` 칸 |
| 재사용 | 조합형 글꼴 데이터, 조합 엔진, 폭·경계 규칙(C) | 그대로 |

## 7. 구현: MnHangul5 (`src/os5/`, `tools/build-os5.sh`)

시험: 포크 네이티브 `cp-uarm --tg50`, 메모장(목록·본문). 실기는 시험하지 않았다. 날짜: 2026-10-24.

**68k 트랩 후킹이 안 되는 이유**

- `SysSetTrapAddress`는 어떤 트랩이든(같은 주소로 다시 써도) `sysErrNotAllowed`(0x0505)를 돌려준다.
- `SysGetTrapAddress`가 주는 주소(동적 힙)는 12바이트 PACE 스텁이다: `TRAP #15`, `0x07FE`, ARM 연결 함수 주소(리틀엔디언).
  이 스텁 앞을 `JMP`로 바꿔도 앱의 `EvtGetEvent` 호출은 오지 않았다 → PACE 분배는 이 스텁을 거치지 않는다.
- PACE 연결 함수(예: `EvtGetEvent` `0x200e0398`)는 68k 인자를 읽어 표 8 칸을 부르는 가져오기 스텁(`ldr ip,[r9,#-8]; ldr pc,[ip,#0x824]`)을 부른다.
  그래서 표 칸을 바꾸면 68k 앱과 ARM 앱의 호출이 모두 바뀐다(그리기·폭과 같은 방식).

**TG50 키 입력**

| 누른 키 | `keyDown` 순서(`chr` / `keyCode` / `modifiers`) |
| --- | --- |
| `a` | `0x1720 / 0x0061 / 0x0008` → `0x0061 / 0 / 0` → `0x1721 / 0x0061 / 0x0008` (앞에 `0x012D`가 오기도 함) |
| Shift+`a` | Shift `0x1720 / 0x8400` → `0x1720 / 0x8461` → `0x0041 / 0 / 0` → … |
| Shift+Space | `0x1720 / 0x8420` → `0x0020 / 0 / 0` → … (글자 이벤트에는 Shift 표시 없음) |

글자 이벤트는 앱이 원시 이벤트(`0x1720`)를 `SysHandleEvent`에 넘길 때 만들어져 다음 `EvtGetEvent`로 온다.
한/영 Shift+Space는 바로 앞 원시 이벤트의 `keyCode 0x8420`으로 판단한다.

**구성**

| 부분 | 내용 |
| --- | --- |
| ARM 코드 | 위치 독립 한 덩어리. 앱이 동적 힙(소유자 0)에 복사. 상태는 코드 앞 `ctx`(PC 상대). `-ffixed-r9`(r9 = 시스템 표) |
| 바꾸는 칸 | 표 8의 20칸(`src/os5/slots.txt`): 그리기 4, 폭 5, Text Manager 10, `EvtGetEvent` |
| '한' 표시 | 활성 폼의 그래피티 상태 표시(GSI, 폼 객체 종류 11) 자리. 표 12 Frm 함수로 찾음. 앱이 다 그리고 다음 이벤트를 받으러 올 때 그림 |
| 기기 확인 | `sysFtrNumOEMCompanyID` = `'sony'`, `sysFtrNumOEMDeviceID` = `'vrna'`(TG50), 바꿀 칸의 지금 값이 알려진 원래 값 |
| 리셋 | 리셋 실행에서 리셋 완료 알림(`sysNotifyResetFinishedEvent`)을 등록하고 그 알림에서 켬 |
| 끄기 | 칸이 모두 아직 우리 코드면 되돌리고 메모리를 품. 위에 다른 것이 있으면 쉬게만 함(모든 함수가 원래 함수로 넘김, 다음 리셋에 빠짐) |

**구현하며 확인한 것**

- **Sony 모듈이 부팅 뒤 그리기 3칸 위에 올라온다**: 리셋 완료 알림 때 `WinDrawChars`·`WinPaintChars`·`WinDrawTruncChars` 칸은
  ROM 함수(`0x20a521f8`·`0x20a506d0`·`0x2005efc0`)이고, 나중에 Sony 함수(`0x20c12800`·`0x20c1287c`·`0x20c1277c`)로 바뀐다.
  Sony 함수는 저장해 둔 원래 값을 부르는 연결이라, 그 아래에 있어도 한글 표시는 된다. 다만 이때 칸을 되돌리면 Sony가 우리 코드를 부르게 되므로
  끄기는 "쉬기"로 한다. 기기 확인은 칸마다 두 값을 모두 허용한다.
- **gcc의 switch 상수 표**: `-O2`는 큰 `switch`(두벌식 배치·초성 번호)를 `CSWTCH` 상수 표 + 절대 주소로 바꾼다.
  복사한 코드에서는 이 주소가 틀려 글자 키가 0(영문 그대로)으로 처리됐다(`Q`처럼 작은 switch로 가는 키만 됨).
  → `-fno-tree-switch-conversion`. 빌드 스크립트가 `CSWTCH` 기호와 `R_ARM_ABS32` 재배치를 검사한다.
- 68k가 ARM에 넘기는 인자 묶음은 4바이트 정렬이어야 한다(68k 스택은 2바이트 정렬) → `MemPtrNew`로 만든다.
- 글자 색: 점 그리기(`WinDrawPixel`)는 전경색을 쓰므로 글자색으로 잠시 바꿔 그린다(그대로 그리면 회색).
- **그리기 속도(v2)**: v1은 점마다 `WinDrawPixel`(한 글자 약 100번). 시험 앱(한글 9자 줄 60번 = 540자)에서 0.33초,
  같은 길이 영문은 0.05초. 조합(글꼴 합성)만 하면 0.02초라 대부분이 점 그리기였다.
  가로로 이어진 점을 사각형 하나로 그리면 0.15초, 글자마다 1비트 그림 하나(`WinPaintBitmap`, 표 8 `0xbc8`)로 그리면 0.04초.
  `WinPaintPixels`는 ARM 쪽에서도 점마다 한 번씩 부르는 반복이라 이득이 없다.
  그림은 ARM 코드가 직접 만든다. ARM `BmpCreate`(표 8 `0x50`)가 만든 그림을 읽어 보니 필드가 리틀엔디언이고
  version 바이트에 `0x80`이 붙어 있었다(V2 = `0x82`). 이에 맞춰 `BitmapTypeV3`(version `0x83`, density 144)를 만들었다.
  `flags`는 ARM 비트필드 순서라 hasTransparency가 비트 2(`0x0004`)이고, 투명 번호 0으로 두면 점 없는 자리를 그리지 않는다
  (빅엔디언 순서 `0x2000`이나 `winOverlay`로는 배경색 네모가 생김). 1비트 점은 전경색으로 그려져 흰 글자·빨간 글자도 맞게 나온다.

- **글꼴 크기(v3)**: TG50 글꼴 줄 높이(표준 좌표)는 stdFont 11, boldFont 11, largeFont 14, largeBoldFont 15(메모장 Font의 세 번째).
  줄 높이 14 이상이면 24×24 묶음(`HgJ24`, 한 칸 12), 아니면 16×16(한 칸 9) — OS 4 MnHangul v22와 같은 기준. 그리기·폭·글자 경계가 모두
  같은 칸(`HangulCell`)을 써야 줄바꿈·커서가 맞는다. 굵은 글꼴(`FntGetFont` 표 8 `0x3fc` = 1·7)은 글자 그림을 오른쪽으로 1픽셀 겹쳐 굵게 한다.
  `'한'` 표시는 GSI 칸에 맞도록 잠시 stdFont(`FntSetFont` 표 8 `0x410`)로 그린다.
- **빠른 입력 시험의 한계**: 에뮬레이터에 xdotool로 0.12초 간격 키를 길게 넣으면 모듈을 끈 영문 입력도 키가 빠지거나 순서가 바뀐다
  (Shift가 붙거나 글자가 사라짐). 한글 모드에서 키가 빠지면 조합이 낱자모로 남으므로, 긴 글의 빠른 입력은 이 시험으로 판단할 수 없다.
  0.25초 간격에서는 큰 글꼴 줄바꿈까지 정상.

- **NX70V(v4)**: 장치 ID `crdb`(TG50은 `vrna`). NX70V ROM의 PACE 연결 함수를 같은 방법으로 읽어 보니 바꾸는 20칸과 부르기만 하는 칸
  (표 8 `EvtEnqueueKey`·그리기·폭·색·`FntGetFont` 등, 표 12 Frm 5개)의 번호가 TG50과 모두 같았다. Text Manager 칸의 값도 같은 간격으로 놓여 있다.
  다른 것은 칸에 든 함수 주소뿐이라 `slots.txt`에 NX70V 값을 더했다. NX70V도 리셋 완료 알림 때는 WinDrawChars·WinPaintChars가
  ROM 함수(`0x2097bf4c`·`0x2097a424`)이고 나중에 Sony 함수(`0x20b3e710`·`0x20b3e7d4`)로 바뀐다. 키 이벤트(`0x1720`, Shift+Space `0x8420`)도 같다.
  NX70V `EvtGetEvent` 연결 함수는 표 8 `0x824` 뒤에 표 12 `0x1f4`도 부른다(용도 미확인, 동작에는 영향 없음).

**남은 것**: 실기 시험(복사한 ARM 코드 실행 전 캐시 처리가 필요할 수 있음),
조합 상태의 앱 구분, 다른 OS5 기종(NX70V 등)의 칸 번호.

### 찾기(v8)

- 대조(TG50 메모장, 검색어 `가`): `가`·`한 가` 찾음, `한가`·`ABC가` 못 찾음, **`갚가`(B0 B1 B0 A1) 찾음** — Palm 찾기는 낱말 첫머리만 맞추는데
  원래 `TxtFindString`이 앞 바이트 하나로 첫머리를 판단해, 한글 앞 글자의 끝 바이트가 Latin-1 기호(0xB1 `±`)면 낱말 가운데도 맞췄다.
  두 글자에 걸친 바이트(`갚가`의 B1 B0 = `굅`)는 맞추지 않았다.
- 진단(임시 빌드, 기록은 세션 저장 → RAM 덤프로 읽음): 찾기 실행 중 표 8 호출은 `TxtPrepFindString` 1회, `TxtCompare`·`TxtFindString` 여러 번이고
  `TxtCharAttr`·`TxtByteAttr`·`TxtGetPreviousChar`는 0회 — 첫머리 판단은 `TxtFindString` 안에서 끝난다(우리 글자 속성 함수를 거치지 않음).
- 처리: 0x9ac `TxtFindString`을 바꿔 원래 결과를 거른다. 맞은 자리 바로 앞 바이트가 0xA1–0xFE면 버리고 다음 바이트부터 다시 찾는다.
  그 밖이면 원래 판단 그대로(영문 검색 결과 같음). 한글 기호 행(0xA1–0xAC) 뒤도 첫머리로 보지 않는다.
- 결과: TG50 Memo·Address·Date Book, NX70V Memo에서 `가`는 `갚가`를 찾지 않고, `갚`·`한글`·`mix`·`abc`는 전과 같다.

### 찾기 부분 검색(v10)

- `TxtFindString`이 받는 검색어는 원래 바이트가 아니다: `TxtPrepFindString`이 바이트마다 1:1로 비교용 값으로 바꾼다
  (실측 `한글AB` C7 D1 B1 DB 41 42 → 98 C3 5E E8 85 96). 그래서 원문과 직접 비교하지 않는다.
- 원문에 한글 2바이트 쌍이 있고 검색어가 64바이트 미만이면, 글자 경계마다 원문을 검색어 길이만큼 잘라 원래 함수에 넘기고
  맨 앞(0)에서 맞을 때만 받아들인다(원래 함수의 비교 규칙을 그대로 씀). 한글 없는 레코드는 v8 경로(Palm 규칙: 낱말 첫머리).
- 정책: 한글이 있는 레코드에서는 낱말 가운데도 찾는다(`가나다`에서 `나`, `한글입력`에서 `글입`, `갚가 가`는 앞쪽 `가`).
  검색어가 가공되어 한글·영문을 구별할 수 없으므로 한글 레코드에서는 영문 검색어도 부분 검색된다(`ABC 가나다`에서 `bc`).
- 속도(TG50 에뮬레이터, 한글 메모 30개 × 약 3.9KB): 못 찾음 0.42 → 0.72초, 찾음 0.42 → 0.78초, 영문 못 찾음 0.40 → 0.41초.
- 확인: TG50 Memo·Address·Date Book, NX70V Memo(결과로 이동해 선택 위치·길이까지).
- 검색어(가공된 값)가 64바이트 이상이면 버퍼에 담지 않고 v8 규칙(Palm 낱말 첫머리 + 필터)으로 찾는다.

### 두 번 눌러 낱말 선택(v11)

- 원래 `TxtWordBounds`(표 8 0x9e4)는 바이트를 Latin-1 글자로 분류해, `가나다`의 `나`를 두 번 누르면 1바이트만 고른다(지우면 한글이 깨짐).
  Memo Pad의 두 번 누르기는 표를 거쳐 0x9e4를 부른다(임시 빌드로 호출 수 확인). 원래 값 TG50 `0x2005ee6c`, NX70V `0x2005e41c`(ROM 분기 명령·연결 함수로 확인).
- v11: 문자열에 한글 쌍이 있고 누른 글자가 낱말 글자(2바이트 글자, 원래 `TxtCharAttr`로 영문자·숫자)면 좌우로 넓힌다. 그 밖은 원래 함수.
  영문과 한글이 붙어 있으면 한 낱말(`ABC가`). 진단 앱 결과(TG50·NX70V 같음, OS 4 MnHangul v72도 같음):

| 문자열 @ 위치 | 원래 함수 | 고친 뒤 |
| --- | --- | --- |
| `가나다` @2 | 2..3 | 0..6 |
| `가나다` @3 | 3..4 | 0..6 |
| `가 나` @3 | 3..4 | 3..5 |
| `ABC 가나` @6 | 6..7 | 4..8 |
| `갚가` @2 | 2..3 | 0..4 |
| `한글.A` @0 | 0..2 | 0..4 |
| `ab cd` @4 | 3..5 | 3..5 |

추가 사례(v12, OS 4 MnHangul v73도 같음). v12부터 누른 글자가 1바이트 영문이면 원래 결과를 먼저 보고, 그 범위와 바로 양옆에
한글 바이트(0xA1–0xFE)가 없으면 그대로 쓴다(원래 함수가 낱말 안 글자로 보는 `'` 등을 유지):

| 문자열 @ 위치 | 원래 함수 | 고친 뒤 |
| --- | --- | --- |
| `한글 입력` @2 | 2..3 | 0..4 |
| `가123나` @3 | 2..5 | 0..7 |
| `가,나` @3 | 3..4 | 3..5 |
| `a_b 가` @1(`_`) | 1..2(거짓) | 1..2(거짓) |
| `don't 가` @1(`o`) | 0..5 | 0..5(v11은 0..3) |
| `ABC가` @1 | 0..3 | 0..5(한글에 닿아 넓힘) |
| `「가나다」` @4 | 4..5 | 0..10(2바이트 기호도 낱말 글자) |
| `don't가` @1 | 0..5 | 0..7(v13: 한글에 닿으면 원래 낱말 범위를 출발점으로. v12는 0..3) |
| `가ABC나` @3 | (미측정) | 0..7 |
| `don't 가나다` @1 | 0..5 | 0..5 |

- 남는 점: 한글 쪽에서 넓혀 나갈 때(`가don't`에서 `가`)는 영문자·숫자만 이어 `가don`까지(원래 함수를 다시 부를 근거가 없음).

### 비교·정렬(v14)

- 원래 `TxtCompare`(표 8 0x9a0)·`TxtCaselessCompare`(0x980)는 바이트마다 Latin-1 가중치로 비교한다(OS 4도 같음).
  원래 값 TG50 `0x2005ee28`·`0x2005ee08`, NX70V `0x2005e3d8`·`0x2005e3b8`(RAM 표 8 덤프, ROM 분기 명령으로 확인).
- 진단 앱으로 원래 함수를 직접 부른 결과(TG50, 일치 길이는 `(s1, s2)` 바이트):

| 비교 | Compare | Caseless | 문제 |
| --- | --- | --- | --- |
| `나` : `다` | 양수 | 양수 | 순서가 틀림 |
| `걋` : `겯` | 1 (1,1) | 0 (2,2) | 다른 글자를 같다고 함(0xC0·0xE0을 같은 글자로 봄) |
| `각` : `간` | 음수 (1,1) | 음수 (1,1) | 일치 길이가 글자 가운데 |
| `AB가` : `ABC` | −44 | −44 | 한글 첫 바이트가 영문자보다 앞 |
| `aB가` : `Ab가` | −1 (0,0) | 0 (4,4) | (정상) 대소문자는 2단계 — 1단계가 같을 때만, 소문자가 앞 |
| `eé` : `ee` | 2 (1,1) | 0 (2,2) | (정상) Caseless는 악센트도 무시 |
| `ß` : `ss`, `æx` : `aex` | 4 / 15 (0,0) | 같음 | (정상) 합자를 풀지 않음 — 측정한 24쌍 모두 s1·s2 일치 길이가 같음 |

- Address: 목록 순서가 `다, 가, 값, 나, 한`. Look Up은 비교의 일치 길이로 입력 칸을 잘라
  `각`(없는 이름)을 넣으면 칸에 `°`(0xB0 한 바이트)가 남는다. Look Up은 길이 인자에 큰 값(0xFFFF)을 넘기고 문자열은 NUL에서 끝난다.
- v14: 두 문자열 중 하나라도 한글 쌍이 있으면 글자 단위로 비교한다(그 밖은 원래 함수에 인자 그대로).
  1바이트 구간끼리는 원래 `TxtCaselessCompare`를 그 길이만큼 부르고(대소문자·악센트 규칙 유지), 2바이트 글자끼리는 16비트 코드값,
  1바이트 글자가 2바이트 글자보다 앞, 문자열 끝이 가장 앞. 일치 길이는 다른 글자 바로 앞(글자 경계).
  여기까지 모두 같으면 Caseless는 0, Compare는 원래 `TxtCompare`를 전체에 불러 대소문자 차이로 정한다.
  길이 안에서 NUL을 만나면 거기서 끝(첫 빌드는 NUL 뒤까지 비교해 Look Up이 한글을 못 찾았음 — 호출 기록 임시 빌드로 확인).
- 결과(TG50·NX70V 같음): 진단 24쌍 중 1바이트끼리는 원래 값과 모두 같음, `걋`:`겯` −1 (0,0), `AB가`:`ABC` 1.
  Address 목록 `가, 가나, 값, 나, 다, 한`(새 레코드 넣기, TG50은 List By를 바꿔 다시 정렬한 기존 레코드도).
  Look Up: `가`·`값`·`다`·`가나`·`한` 선택, `각`은 칸을 비움(반 글자 없음). 입력 중간 글자(`간`, `하`)는 이름 앞부분과 같은 글자가 아니라
  잠시 칸이 비지만, 조합이 이어지면 끝 글자(`가나`, `한`)로 찾는다.
- 호스트 시험 `test/compare_host_test.py`(CI): 문자열 38개의 모든 쌍에서 반사·반대칭·추이성, 일치 길이가 글자 경계, 길이 0xFFFF와 정확한 길이가 같은 결과.

### 그리기 4함수 진단(B, TG50·v14 기준, 수정 보류)

- 대상: 표 8 `0xb24` WinDrawInvertedChars, `0xb44` WinEraseChars, `0xbac` WinInvertChars, `0xbcc` WinPaintChar(MnHangul5가 바꾸지 않는 칸).
  TG50 원래 값 `0x20a51ff4`·`0x20a51d10`·`0x20a513e0`·`0x20a5071c`, NX70V `0x2097bd48`·`0x2097ba64`·`0x2097b134`·`0x2097a470`(RAM 표 8 덤프).
- 방법: B-1 = 네 칸을 원래 함수로 넘기면서 호출 수(전체·상위 바이트 있는 호출)만 세는 임시 빌드(v14B, 저장소 밖).
  B-2 = 진단 앱 DrDiag(저장소 밖)가 `가나Ab`·`ABab`를 각 함수로 직접 그림.
- 조사 범위(TG50만): 메모장(목록·열기·두 번 누르기 시도 — 선택 표시는 확인 못 함), 찾기(`가` 검색·결과 목록),
  Address(한글 이름 추가·목록·레코드 보기·메뉴), Date Book(한글 일정 추가·하루·주·월 보기), SMS 화면.
  미시험: NX70V, Date Book 일정 목록, To Do, 한글 분류 이름 팝업, 경고창 제목, 다른 회사 앱.

B-1 호출 수(DrDiag 직접 호출 제외):

| 함수 | 전체 | 한글이 든 호출 |
| --- | ---: | ---: |
| WinDrawInvertedChars | 23 | 0 |
| WinEraseChars | 0 | 0 |
| WinInvertChars | 0 | 0 |
| WinPaintChar | 0 | 0 |

WinDrawInvertedChars 23회에는 호출 수를 보려고 연 MnHangul5 설정 앱 제목(영문)이 포함된다(문자열은 마지막 호출만 기록).
한글이 든 호출은 DrDiag의 1회뿐. 나머지 세 함수의 누계(4·4·3)는 처음부터 끝까지 DrDiag 호출 수와 같았다.

B-2 직접 호출(v14):

| 함수 | 한글 | 영문 |
| --- | --- | --- |
| WinDrawChars·WinPaintChars(기준) | 정상 | 정상 |
| WinDrawInvertedChars | `°¡³ª`(Latin-1 글자) | 정상 |
| WinPaintChar(`0xB0A1`) | `°¡`(Latin-1 글자) | 정상 |
| WinInvertChars | 빈 바탕에 한 번: 정상처럼 보임. 그린 뒤 같은 자리에 반전: **한글이 남음(원래 화면으로 돌아오지 않음)** | 그린 뒤 반전하면 지워짐 |
| WinEraseChars | 그린 뒤 같은 자리를 지우면 **한글이 남고 같은 문자열의 뒤 `Ab`는 일부만 지워짐** | 지워짐 |

- 확인된 사실: 네 함수 모두 직접 부르면 한글을 바르게 처리하지 못한다. 조사한 화면에서는 한글이 든 호출이 없었다.
- 확인하지 않은 가설: WinInvertChars가 원래 화면으로 돌아오지 않는 것은 원래 함수가 표 8의 그리기 칸(우리 함수)을 거치고 v6의
  칸 지우기가 반전 모드에서도 일어나기 때문일 수 있다(경로 미확인). WinEraseChars가 덜 지우는 것은 한글 두 바이트를
  Latin-1 글자 폭으로 계산한 범위와 맞다(내부 폭 계산 경로 미확인).
- 칸 확장 사전 확인(v14B): 원래 주소 칸 24 → 28, 설정 앱의 ctx 오프셋 +16, ctx 400바이트, 크기 검사 28.
  TG50에서 켜기, 28칸 상태로 위 앱 실행, 디버그 화면이 정상. 리셋 뒤 자동 시작, OFF 때 되돌리기, 반복 ON/OFF,
  다른 확장과의 연결은 미시험. 구조 확장이 확인된 것이지 추가 그리기 함수의 동작이 검증된 것은 아니다.
- 결론: 결함은 확인했으나 조사한 화면에서 한글 호출이 없어 제품 코드 수정은 보류한다. 한글이 든 호출을 하는 앱·화면이
  발견되거나 표시 오류가 보고되면 함수별 원래 동작(반전·지우기 범위·그리기 모드)을 확인한 뒤 다시 검토한다.

### 시스템 키보드 대화상자(v15)

- Edit → Keyboard 대화상자에서 누른 키도 표 8 `0x824`(EvtGetEvent) 연결을 거친다(v14 키 기록에 `Q`·`g`가 "그대로"로 남음).
  한글 모드에서는 자모로 바뀌었지만 `ㅂㅗㄱ`처럼 낱낱이 들어갔다: 펜 누름마다 조합을 끝내는 규칙(ime.c) 때문 — 화면 키는 모두 펜 누름이다.
- 진단 빌드(저장소 밖)로 펜 누름 때의 폼을 기록(TG50·NX70V 같음): 대화상자는 일반 폼 — 폼 번호 32000, 제목 `Keyboard`,
  객체 10개, 창 테두리 (0,12) 160×148, 초점 = 객체 3(입력칸, (1,12) 154×47), 활성 창 = 폼 창. 키를 누른 y는 입력칸 아래(예 69),
  Done 줄은 y 131 근처. 펜 좌표와 객체 좌표는 같은 창 기준 표준 좌표. 메뉴가 떠 있을 때는 활성 창이 폼 창과 다르다.
  (처음 기록한 "활성 폼 = 메모 폼 1000, 초점 없음"은 메모 목록의 New 단추를 누른 순간이었다 — 정정.)
- 표 12 `0x238` FrmGetFocus, `0x240` FrmGetFormId(그리고 진단에 쓴 `0x228` FrmGetActiveFormID, `0x270` FrmGetTitle)는
  Frm 함수 알파벳 순서로 정했다. 이미 쓰던 5칸(`0x224`·`0x250`·`0x254`·`0x26c`·`0x278`)이 같은 순서에 정확히 맞고,
  읽은 제목·폼 번호·초점 객체가 화면과 맞아 확인했다. 표 8 `0xba0`(WinGetWindowFrameRect, 진단에만 씀)도 같은 방법.
- v15: 펜 누름 때 활성 폼이 32000이고 그 창이 활성 창이며, 누른 곳이 초점 입력칸 아래 끝 이상 ~ 그 아래 단추 중 가장 위 단추의
  윗변 미만이면 조합을 끝내지 않는다(경계는 폼 객체에서 계산). 입력칸·단추 줄(Done 등)·그 밖의 펜 누름은 그대로 조합 끝.
- 결과(TG50·NX70V): 대화상자에서 `보건 가나 한글` 입력 → Done → 메모에 그대로. 대화상자 글 영역을 누른 뒤 키는 새 글자
  (`가` 뒤 글 영역 누르고 `ㄴ` → `ㄴ`이 누른 곳에, `가`는 그대로). Done 뒤 하드웨어 키는 앞 글자를 지우지 않음.
  메모장에서 하드웨어 키로 조합 중 펜 누름 → 조합 끝(v14와 같음).
- 시험 도구 제한: NX70V에서 x < 15를 누르는 시험 탭은 에뮬레이터에 들어가지 않는다.
- 키 글자(v16): 진단 빌드로 대화상자(폼 32000)가 활성일 때의 글자 그리기를 기록 — 키 글자는 그림이 아니라 WinDrawChars(표 8 `0xb18`,
  MnHangul5가 바꾼 칸)로 한 글자씩 그린다(`Q`…`P` y 67, `A`…`L` y 81, 아래 줄 `abc`·`123` y 127, 창 기준 표준 좌표).
  대화상자를 열 때, 하드웨어 Shift+Space 뒤, shift 키를 누른 뒤마다 키 글자 26개를 다시 그렸다.
- v16: 한글 모드이고 활성 폼이 32000이며 영문자 한 글자를 초점 입력칸 아래에 그릴 때 그 자리에 두벌식 자모를 그린다
  (입력과 같은 규칙 — 대화상자가 그리는 글자를 누르면 들어가는 자모, 대문자 = shift = 쌍자음·ㅒ·ㅖ). 한글 칸이 영문 글자보다 넓어 2 왼쪽으로.
  결과(TG50·NX70V): 한 모드 `ㅂㅈㄷㄱㅅㅛㅕㅑㅐㅔ / ㅁㄴㅇㄹㅎㅗㅓㅏㅣ / ㅋㅌㅊㅍㅠㅜㅡ`, shift `ㅃㅉㄸㄲㅆ … ㅒㅖ`, 영문 모드로 돌리면 영문.
  남은 점: `ㅑ` 등 몇 자모가 키 경계선에 닿는다. 화면 키보드만으로는 한/영을 바꿀 수 없다(하드웨어 Shift+Space).

## 8. VZ90 영문 Mini(v17)

> 시험 환경: [cloudpilot-emu 포크](https://github.com/muns5965/cloudpilot-emu) master `441f9ca2`(VZ90 지원, 내장 DAL `pv-vz90-dal` ffaed62)의
> 네이티브 `cp-uarm`, RomFlash.VZ `fullrom.mini.enUS`(MD5 `c8ddf3af…`, Palm OS 5.2.1). 실기 없음. 날짜: 2026-11-03.

- 칸 번호: 부팅 뒤 ROM·RAM을 덤프(임시 진단 빌드, 저장소 밖)해 PACE 트랩 표(RAM `0x20800bcc`, 0x488칸)의 연결 함수 1,160개를
  capstone으로 읽고, TG50 대응표와 맞춰 보니 표 8·12 칸이 있는 연결 함수 740개가 **모두 같은 칸**이었다(`0xb18` WinDrawChars,
  `0x3ec` FntCharsWidth, `0x824` EvtGetEvent, 표 12 `0x44c` SysKeyboardDialog 등). 표 8 `0x20085df4`, 표 12 `0x20082e10`.
- 칸의 원래 값: 그리기·폭·Text 함수는 OS 본체(`0x108xxxxx`), `0x824`는 Sony 모듈(`0x101ade6c`, JogAssist 근처)이 먼저 바꿔 둔 함수.
  에뮬레이터가 VZ90 ROM을 DAL과 함께 `0x10000000`에 다시 조립하므로, 값은 DAL 판에 따라 달라질 수 있다 →
  v17은 VZ90에서 알려진 값 대신 "조립된 ROM 안(`0x10000000`~`0x10FFFFFF`)"인지만 본다(다른 패치는 RAM 주소라 걸러짐).
- 기기 확인(진단 앱, 저장소 밖): 회사 `sony`, 기기 ID `PvPv`(PV 플랫폼 = DAL의 HAL ID — 다른 PV ROM도 같다),
  문자 인코딩 3 = Palm Latin(영문 Mini도 일본어 ROM에서 만들었지만 시스템 인코딩은 Latin), ROM 판 `0x0521xxxx`.
  v17은 `PvPv` + Palm Latin + ROM 5.2일 때만 VZ90으로 본다(일본어 순정 ROM은 Shift-JIS라 켜지지 않음).
- 키보드 대화상자(입력 영역의 키보드 단추로 뜸): 폼 32000, 객체 10개(진단 빌드로 기록, 창 기준 표준 좌표) —
  제목, GSI (131,130) 11×10, Done (5,126) 38×13, 입력칸 (1,12) 154×47 = 초점, abc (49,126) 19×13, 123 (69,126), Int'l (89,126) 18×13,
  작은 단추 둘 (142,124)·(142,132) 13×8, 키 가젯. 창은 화면 위에서 20 아래. 펜 좌표는 창 기준이라 v15·v16 계산이 그대로 맞다.
- 한/영(v17): VZ90은 하드웨어 키보드가 없어 Shift+Space를 쓸 수 없다. 대화상자 아래 단추 줄(가장 왼쪽 단추 Done의 y)의
  가장 오른쪽 단추(Int'l) 오른쪽 4에 14 폭 칸을 그리고(`A`/`한`, OS 4 MnKbdHan과 같은 자리), 누르면 한/영을 바꾼 뒤
  FrmUpdateForm(표 12 `0x2f0`, frmRedrawUpdateCode)으로 대화상자가 키 글자를 다시 그리게 한다. 칸이 창 밖이거나 다른 단추와 겹치면 그리지 않는다.
  처음 판(입력칸 아래 단추 중 가장 오른쪽)은 오른쪽 작은 단추 둘 때문에 칸이 창 밖으로 계산돼 그려지지 않았다(진단 빌드로 객체 기록 후 수정).
- 자동 대문자(v17): 새 메모 첫머리에서 대화상자가 shift를 걸고 열려, 한 모드 키 글자가 쌍자음으로 보이고 그대로 들어갔다.
  대화상자의 대문자(원시 키 없음)는 shift 키(단추 줄 바로 위 줄 왼쪽 끝 x < 32)를 손으로 눌렀을 때만 쌍자음, 아니면 기본 자모(키 글자도 같은 규칙).
- 결과(VZ90 영문 Mini): 설정 앱 `VZ90 OK` → `ON`, 메모장 키보드 대화상자에서 `A` → `한`, 키 글자 두벌식 자모, `한글` 입력 → Done → 메모에 그대로,
  자동 대문자 상태에서 `ㅂㅏ` → `바`, shift 누르고 `ㅂㅏ` → `빠`. 리셋 뒤 자동 시작, 메모 목록에 한글 표시.
  TG50(대화상자 `A`/`한` 칸, 한 모드 뒤 하드웨어 키 조합), NX70V(`NX70V OK`, ON) 회귀 확인. 일본어 순정 ROM 거부는 코드 확인만(실행 시험 안 함).
- 남은 점: 칸 테두리(simpleFrame)가 어두운 테마에서 보이지 않는다. VZ90 Graffiti 영역 입력은 영문 그대로. Address·To Do는 미시험.
- v18: 키보드 대화상자에 한/영 칸이 그려지면 GSI 자리의 '한'(기존 한글 모드 표시)은 그리지 않는다 — v17에서 대화상자에 '한'이 두 개
  보였다(칸 + GSI). 칸을 그릴 수 없는 대화상자는 GSI에 그대로. 새 대화상자(frmOpenEvent)는 손으로 누른 shift 없음으로 시작.
- 결과(v18, CloudpilotEmu): VZ90 영문 Mini — 대화상자 '한' 하나, 메모 화면 오른쪽 아래 '한' 유지, 닫았다 다시 열면 한 모드·자모 키 글자 그대로.
  Address 새 항목 `홍길동`(대화상자) → 목록·다시 열기에서 표시. To Do `학교 값` → 완료 체크·다시 열기. 세션 저장 → 복원 뒤 켜진 그대로(목록 한글 표시).
  TG50·NX70V 메모장 대화상자 `A` → `한` → `한글` → Done → 메모에 그대로(대화상자 '한' 하나), TG50은 뒤이어 하드웨어 `a` → `ㅁ`.
  일본어 순정 ROM: 설치 → 리셋 → 리셋 완료 알림의 켜기 결과 Feature 9 = `0x10003`(not supported, RAM 덤프로 확인), 칸 바꾸지 않음.
- v19: shift 초기화는 키보드 대화상자가 열릴 때(frmOpenEvent의 폼 번호 32000)만. 대화상자에 한/영 칸이 있을 때 GSI 자리에
  우리가 그 창에 그린 '한'이 남아 있으면 그 자리만 지운다(그린 창을 ctx에 기록 — 다른 창에 그린 표시나 대화상자 자신의 shift 표시는 건드리지 않음).
  개발 중 상태 블록(struct Ctx)이 ctx.S의 384바이트를 넘어(388) 바로 뒤 코드를 덮어, 키보드 대화상자를 다시 열면 시스템이 멈췄다
  (VZ90, v18 같은 순서로는 정상). ctx를 416바이트로 늘리고 os5.h에 크기 확인(컴파일 오류)을 넣었다. v17·v18은 384바이트 안이라 영향 없음.
- 결과(v19, CloudpilotEmu): VZ90 영문 Mini — shift만 누르고 닫은 뒤 다시 연 대화상자에서 `바` → shift → `빠`, 메모에 그대로, '한' 하나.
  TG50·NX70V — 대화상자 `한까`(shift ㄲ) → Done → 하드웨어 `a` → `ㅁ`, 대화상자 '한' 하나. 남은 '한'을 지우는 경로(칸을 그릴 수 없다가
  그릴 수 있게 되는 경우)는 아래 시험 빌드로 확인했다.
- v19 보완(같은 판): 새 폼이 열리면(frmOpenEvent) '그린 창' 기록을 0으로 — 닫힌 창의 주소를 다음 창이 다시 쓸 수 있어 주소 비교만으로는
  같은 창이라고 보장할 수 없다. `NEvent.chr`은 Palm OS `EventType`의 data 공용체 첫 16비트(오프셋 8)라 keyDown에서는 글자,
  frmOpenEvent에서는 `frmOpen.formID`다. 빌드 스크립트가 ctx.S의 예약 크기와 os5.h `kCtxSpace`가 같은지 검사한다.
- 잔상 지우기 시험(`OS5_CFLAGS=-DMN_TEST_STALE bash tools/build-os5.sh`, 시험 빌드에만 들어가는 코드): 키보드 대화상자가 열린 뒤 처음
  3번의 표시는 칸이 없다고 보고 GSI에 '한'을 그려 잔상을 만들고, 그리기·지우기를 키 기록에 남긴다. VZ90 영문 Mini 결과 —
  대화상자 GSI (131,130)에 '한' 그림(3번) → 다음 표시에서 지우기 경로(같은 창) → (131,130)만 지움, 한/영 칸 `한`은 그대로.
  Done 뒤 메모 화면 GSI (135,150)에는 다시 그림(다른 창의 표시는 대화상자 쪽 처리와 따로). 최종 PRC는 이 코드 없이 빌드.
- v20 한/영 칸 테두리: v17~v19는 `WinDrawRectangleFrame`(표 8 `0xb34`, PACE 연결 함수로 칸 확인)을 ARM에서 직접 불렀는데 아무것도 그려지지
  않았다(밝은 TG50 화면에서도). 시험 빌드로 같은 자리에 `WinDrawRectangle`(`0xb30`)·`WinDrawLine`(`0xb28`)을 그리면 보였다 —
  ARM 쪽 함수의 인자 형식이 68k와 다른 것으로 보이나 확인하지 않았다. v20은 `WinPushDrawState` → 그리기 색 =
  `UIColorGetTableEntryIndex(UIObjectFrame)`(표 12 `0x540`) → 칸 바깥 1에 `WinDrawLine` 넷(모서리 비움) → `WinPopDrawState`.
  결과: VZ90 영문 Mini(어두운 화면)·TG50·NX70V(밝은 화면) 모두 다른 단추와 같은 색 테두리, `A` ↔ `한` 전환·입력 정상.
  칸 위치 검사(창 폭·다른 단추와 겹침)에 바깥 테두리 1을 포함했다(창 오른쪽 끝에 붙으면 테두리가 창 밖으로 나갈 수 있어서 — 리뷰 지적).
- v21 VZ90 판별 조사(진단 앱, 저장소 밖): `PvPv`는 DAL이 주는 PV 플랫폼 공통 ID라, 다른 기종을 PV로 돌리면(예: Palm OS 5.2.1 영문 TH55)
  회사·인코딩·ROM 판·칸 주소 범위 조건을 모두 만족할 수 있다. Sony 문자열 정보(Feature `SoNy` 2, `SonySysFtrStringInfoType`)는
  VZ90 영문 Mini에서 maker `Sony Corp.`, model `PEG-VZ90`, ship `enUS`, os `Palm OS(R) 5.2.1`, cpu `PV ARM926EJ`
  (cpu만 실행 때 바뀜). model 칸 오프셋 16은 Sony SDK `SonySystemFtr.h`의 구조체 정의(maker[16] 다음, 널 종료 ASCII)와 같다(app.c `kSonyStrModel`). ROM 파일 안에 같은 틀이 있다 — 일본어 순정·영문 Full·영문 Mini 모두 model `PEG-VZ90`(ship `jpJP`/`enUS`),
  DAL과 상관없는 Sony 자료. v21은 `PvPv`일 때 이 model이 `PEG-VZ90`인지도 본다(인코딩·ROM 판·주소 범위 조건은 그대로).
  결과: VZ90 영문 Mini `VZ90 OK`·ON, 리셋 뒤 자동 시작·대화상자 입력 정상. 비교 문자열을 바꾼 시험 빌드는 `not TG50/NX70V/VZ90`(거부).
  TG50은 `TG50 OK`·ON(이 조건은 `PvPv`에만 적용). 다른 기종의 PV ROM은 갖고 있지 않아 실제 거부는 시험하지 못했다.
