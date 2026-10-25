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
