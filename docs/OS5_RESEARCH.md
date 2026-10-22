# Palm OS 5(TG50) 한글 구현 조사

> 조사 기록이다. 아직 OS5용 모듈은 만들지 않았다.
> 시험 환경: [TG50 포팅 포크](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-tg50)의 네이티브 `cp-uarm --tg50`,
> uARM 기준 해시와 같은 TG50 NOR ROM(OS 5.0 영문). 실기 시험은 하지 않았다. 날짜: 2026-10-22~23.

## 요약

| 항목 | OS4(NR70V) | OS5(TG50) 조사 결과 |
| --- | --- | --- |
| 입력 후킹 | 68k 트랩(`EvtGetEvent` 등) | 메모장은 68k 앱이라 `EvtGetEvent`·`SysHandleEvent`를 68k 트랩으로 부른다 → 68k 후킹 가능 |
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
| 글자 경계(Text Manager) | 표8 `0x9b0` TxtGetNextChar, `0x9b4` TxtGetPreviousChar, `0x998` TxtSetNextChar, `0x9d0` TxtCharBounds, `0x984` TxtByteAttr, `0x994` TxtCharAttr, `0x99c` TxtCharXAttr, `0x9b8` TxtCharSize, `0x9d8` TxtGetChar, `0x9bc` TxtCharWidth, `0x9a0` TxtCharIsValid 등 |
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
  그리기 7개, 폭 5개, Text Manager 15개, 표12 3개(`FldSetAttributes`, `MenuEraseStatus`, `InsPtSetHeight`), `SysGetOSVersionString`이다.
  → 같은 구조(표 칸 교체)로 구현돼 있다고 추정한다.

## 6. OS5 모듈 구상(미구현)

| 부분 | 방법 |
| --- | --- |
| 설치 | 리셋 알림 때 68k 앱이 `PceNativeCall`로 ARM 코드를 불러 `r9`에서 표 8·12를 찾고 칸을 교체, 원래 주소 보관 |
| 그리기 | `WinDrawChar(s)`·`WinPaintChar(s)` 등: 한글 2바이트면 조합형 글꼴로 그리고, 아니면 원래 함수 |
| 폭·경계 | NR70V `width.c`·`intl.c` 규칙을 ARM으로 빌드해 Fnt 5개·Text Manager 칸에 연결 |
| 입력 | 68k `EvtGetEvent` 후킹으로 `0x1720` 원시 키를 받아 `hangul_compose.h`로 조합 |
| 재사용 | 조합형 글꼴 데이터, 조합 엔진, 폭·경계 규칙(C) |

남은 확인: (1) 교체한 ARM 함수가 원래 함수를 다시 부르는 연결(원래 주소 보관·`r9` 유지), (2) 실기의 쓰기 보호, (3) 다른 OS5 기종·버전에서 칸 번호 차이(같은 방법으로 대응표를 다시 만든다).
