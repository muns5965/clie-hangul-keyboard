# Palm OS 5(TG50) 한글 구현 조사

> 조사 기록이다. 이 결과로 만든 OS5 모듈은 `MnHangul5`(`src/os5/`, 7절)다.
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
| 글자 경계(Text Manager) | 표8 `0x97c` TxtByteAttr, `0x984` TxtCharAttr, `0x98c` TxtCharIsValid, `0x994` TxtCharXAttr, `0x998` TxtCharBounds, `0x99c` TxtCharSize, `0x9b0` TxtGetChar, `0x9b4` TxtGetNextChar, `0x9b8` TxtGetPreviousChar, `0x9bc` TxtGetTruncationOffset, `0x9c0` TxtGetWordWrapOffset, `0x9cc` TxtParamString, `0x9d8` TxtSetNextChar, `0x414` TxtCharWidth (**수정**: 처음 대응표는 이름이 한 칸씩 밀려 있었다. ROM `IntlDispatch` 분기표 `0x200ea1fc`를 직접 읽어 바로잡음. `WChar` 출력 인자는 16비트, `TxtCharBounds`의 시작·끝은 32비트) |
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

**남은 것**: 실기 시험(복사한 ARM 코드 실행 전 캐시 처리가 필요할 수 있음), 16×16 글꼴만(다른 글꼴 크기의 폭),
조합 상태의 앱 구분, 다른 OS5 기종(NX70V 등)의 칸 번호.
