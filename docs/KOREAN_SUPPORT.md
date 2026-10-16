# 한글 지원과 CJKOS

> 검토·시험 문서. B안 중 그리기 함수(`BltDrawChars`) 교체는 시험 앱 MnHangul로 실측했다(아래 2a~2c).
> 그 밖의 함수 교체와 전체 구조는 아직 **검토안**이며 구현·검증되지 않았다.

## 현재 의존 관계
| 기능 | 담당 | CJKOS가 없으면 |
| --- | --- | --- |
| 한글 키 입력·조합 | 이 프로젝트(MnSkCore) | 영향 없음 |
| 2바이트(KS X 1001, EUC-KR) 글자를 한글로 그리기 | CJKOS | `°¡`처럼 라틴 문자 두 개로 보임 |
| 글자 경계(커서 이동, BS, 줄바꿈, 선택) | CJKOS | BS가 반쪽만 지우고 줄바꿈이 글자 중간에서 끊김 |
| 한글 글꼴 | CJKOS | 없음 |

CJKOS는 NJStar 제품으로 2010년 판매가 끝났다([NJStar](https://www.njstar.com/cms/cjk-os-for-palm)). 체험판은 30일 제한이 있다.

## 대체 검토안
| 안 | 내용 | 판단 |
| --- | --- | --- |
| A | CJKOS 정식 사용 | 판매 종료. 정식 사용은 개발사 문의 필요 |
| B | 시스템 트랩 교체로 시스템 전체에 2바이트 한글 표시·처리 추가(CJKOS와 같은 방식) | **우선 검토 대상.** 단, Palm OS 4.1S에서 트랩 교체만으로 CJKOS와 같은 호환성을 얻을 수 있다고 단정하지 않는다 |
| C | 직접 만든 앱 안에서만 한글 표시·편집 | 안전하고 쉬움. 기존 메모장 등에서는 한글이 안 보임 |
| (제외) | ROM 수정 | 실기 ROM 재기록 위험, Sony ROM 저작권 문제로 검토하지 않음 |

### B안 세부 (검토)

#### 실측 1: CJKOS가 바꾼 함수 (ClieDiag v22 Trap 페이지, 2026-10-16)
- **관찰**: CJKOS를 켰을 때 ROM 밖을 가리키는 시스템 트랩 33개·Intl 선택자 18개, **CJKOS를 끄면 0개**.
  가리키는 코드는 `0x0034xxxx`(트랩 31·Intl 18)와 `0x000F3xxx`(`FtrGet`, `IntlDispatch`) 두 곳.
- **판단**: 다른 RAM 패치가 없는 시험 환경에서 켜고/끄고 차이로 나타났으므로 CJKOS가 바꾼 것으로 본다.
  다만 주소가 바뀌었다는 사실만으로 각 함수가 한글 처리에 꼭 필요한지, CJKOS가 어떤 목적으로 바꿨는지는 알 수 없다.
  아래 표의 "대체 모듈에 필요" 열은 함수 이름·역할로 추정한 **잠정 판단**이며, 함수별 교체 시험으로 확인한다.

| 분류 | 함수 | 대체 모듈에 필요 |
| --- | --- | --- |
| 그리기 | `BltDrawChars`(글자를 찍는 최하위 함수) | 필수. `WinDrawChars`가 아니라 그 아래를 바꾼다 |
| 글꼴 상태 | `FntSetFont`, `WinPopDrawState` | 필수(현재 글꼴에 맞는 한글 크기 선택) |
| 폭·줄바꿈 | `FntCharWidth`, `FntCharsWidth`, `FntCharsInWidth`, `FntLineWidth`, `FntWidthToOffset`, `FntWordWrap`, `FldWordWrap` | 필수 |
| 글자 경계(Intl) | `TxtByteAttr`, `TxtCharAttr`, `TxtCharSize`, `TxtGetPreviousChar`, `TxtGetNextChar`, `TxtGetChar`, `TxtSetNextChar`, `TxtCharBounds`, `TxtCharWidth`, `TxtCharIsValid` | 필수(커서·BS·선택) |
| 인코딩 정보 | `TxtCharEncoding`, `TxtStrEncoding`, `TxtEncodingName`(Intl), `FtrGet`, `IntlDispatch` | 필요(시스템이 다바이트 인코딩으로 인식) |
| 단어·비교·정렬 | `TxtWordBounds`, `TxtCompare`, `TxtCaselessCompare`, `TxtTransliterate`(Intl), `StrCompare`, `StrCaselessCompare`, `GetCharSortValue`, `GetCharAttr`, `GetCharCaselessValue` | 선택 |
| 화면 현지화 | `FrmInitForm`, `FrmDrawForm`, `MenuAddItem`, `DateToAscii`, `FontSelect` | 불필요(또는 나중) |
| CJKOS 자체 입력기 | `EvtGetEvent`, `FldHandleEvent`, `SysKeyboardDialog` | 불필요(이 프로젝트 키보드가 대신함) |
| CJKOS 자체 관리 | `SysAppLaunch`, `DmOpenDatabase`, `DmCloseDatabase`, `DmDeleteDatabase`, `DmGet1Resource`, `DmNextOpenResDatabase`, `DmMoveOpenDBContext`, `IntlGetRoutineAddress`(Intl) | 불필요(글꼴 DB·리소스 관리로 추정) |

초기 구현 후보: 글자 그리기, 문자열 폭 계산, 2바이트 글자 경계 처리에 관련된 트랩·Intl 선택자(잠정 후보 트랩 약 10개 + Intl 약 13개).
실제로 필요한 최소 범위는 함수 하나씩 교체 시험과 함수 간 의존성 확인으로 확정한다. 지금까지 확정된 것은 `BltDrawChars` 하나(2a~2c 실측)다.

#### 실측 2: CJKOS 한글 글꼴 크기·폭 (ClieDiag v20 Info, v21 Font 페이지)
| 쓰는 곳 | 줄 높이 | CJKOS가 그리는 한글 | 한 글자 폭 | 대체 모듈 계획 |
| --- | --- | --- | --- | --- |
| 저해상도 std / bold / large | 11 / 11 / 14 | 12px | 13 / 14 / 13 | 12×12 |
| 고해상도 small(HR) | 14 | 12px 그대로(선명) | 13 | 12×12 |
| 고해상도 std / bold | 22 | 약 8~9px를 2배로 늘림(투박) | 18 / 20 | **16×16 고해상도 그대로** |
| 고해상도 large | 28 | 12px를 2배로 늘림(투박) | 26 | **24×24 고해상도 그대로** |

폭은 CJKOS와 같게 맞춰 기존 앱의 줄바꿈·정렬을 유지하고, 고해상도 std·bold·large는 늘린 글꼴 대신 해당 크기 조합형 글꼴로 그린다.

- 교체 시점: 리셋 알림(`sysAppLaunchCmdSystemReset`) 또는 부팅 때 실행되는 SkWr 로더의 Open.
- 글꼴: **벌식 조합형으로 결정**(8×4×4 기준 조각 약 360개: 12×12 약 8.6KB, 16×16 약 11.5KB, 24×24 약 26KB, 11,172자). 16×16은 Neo둥근모(OFL) 활용 검토, 12·24는 직접 제작 또는 변환.
- 시험 앱 `bin/MnHangul.prc`(크리에이터 `MnHg`, 빌드 `tools/build-hangul.sh`): 같은 PRC 안의 패치 코드 리소스 `HgPt 0`(`src/hangul/patch.c`)을
  `BltDrawChars`(0xA378)에 끼우고 빼는 앱(`src/hangul/app.c`). `BltDrawChars` 인자는 ROM 호출부(0x50D8C, 0x2BC4B8) 역분석으로 7개(26바이트):
  상태 포인터, 사각형 포인터, 점 포인터, 문자열, 길이(16비트), 글꼴 구조체 포인터, 보조 포인터(의미는 2a 실측으로 확정).
  시험 기간에는 부팅 때 자동으로 켜지 않으므로 리셋 한 번이면 원래대로 돌아간다. **CJKOS를 끈 백업 복사본 세션에서만** 시험한다.
- 2a(통과 시험, v1~v2; v2는 고해상도 화면): 패치가 인자를 feature `MnHg`에 기록하고 원래 함수를 그대로 부른다. 화면이 바뀌지 않아야 정상.
  **실측(성공)**: 190~260회 호출 중 멈춤 없음. `WinDrawChars`와 HR `HRWinDrawChars` 모두 이 함수를 지난다
  (키보드 `한/A` = `C7 D1 2F 41` 잡힘). p2 = 그릴 사각형(창 기준). 기존 함수는 고해상도 모드에서도 저해상도 좌표(높이 11),
  HR 함수는 고해상도 좌표(높이 22)로 들어와 **두 좌표계가 섞인다**. 고해상도 모드에서는 p6 글꼴 포인터도 달라진다.
  MnHangul은 패치가 켜진 동안 DB 삭제 보호를 걸므로, 업데이트는 `Patch OFF`(또는 리셋) 뒤 설치한다.
- 2b(v3): `Box ON`이면 KS X 1001 2바이트 글자(앞·뒤 0xA1~0xFE)를 `[]`로 바꾼 사본을 원래 함수에 넘긴다.
  바이트 수가 같아 위치·좌표계는 원래 함수가 처리한다(좌표계를 몰라도 안전). 2바이트를 한 글자로 끊는지 확인한다.
  함께 글꼴 헤더(fontType, fRectHeight)·p3·p1을 기록해, 2c에서 직접 그릴 때 좌표계를 구분할 단서를 찾는다.
  **실측(성공)**: 메모 목록 `[]`×9·공백·`[]`×2, 키보드 `[]/A`, 짝 없는 1바이트는 그대로. 깨진 글자 끼임 없음, 멈춤 없음.
  좌표계: 기존 함수(고해상도 모드) 사각형 높이 11·글꼴 `0x9000`/22, HR 함수 사각형 높이 22·글꼴 `0x9000`/22.
  → **글꼴 높이 = 실제 픽셀 크기, 글꼴 높이 ÷ 사각형 높이 = 좌표 배율.** p3은 항상 0,0.
- 2c 방향(합의): 한글 한 글자마다 p6 머리를 본뜬 1글자 임시 글꼴을 만들어 원래 함수로 그린다(배율·클리핑·그리기 상태는 원래 함수가 처리).
  먼저 v4 Font 페이지로 `0x9000` 글꼴 구조를 덤프해 SDK FontType 구조와 같은지 확인한다.
  **실측**: ROM 고해상도 std 글꼴(`0x102AF2DE`) = fontType `9000`, 글자 9~255, 높이 22, ascent 18, descent 4, rowWords 152.
  폭 표 위치가 SDK 구조(위치표 항목 `끝−첫+3`개, '없는 글자' 칸 포함)와 정확히 일치. 'A' 폭 10.
- 2c(v6, 시험 무늬 단계를 건너뛰고 실제 글자로 바로 시험): 모드 `glyph`이면 높이 22 글꼴에서 한글을 16×16으로 그린다.
  글자 데이터는 **시험용** Neo둥근모 16×16 완성형 2,350자(75,200바이트, 리소스 `HgFn 0·1`,
  `tools/gen-hangul16.py`, 라이선스 `src/hangul/data/NEODGM-OFL.md`). 최종 모듈은 벌식 조합형으로 바꾼다.
  한글 폭 18(고해상도), 세로는 22칸의 3~18줄. 저해상도 글꼴은 `[]`로 남긴다(12×12 단계에서 처리).
  **실측(성공)**: MnHangul 견본(기존 함수, 배율 2) `한글 OK 가각`, 키보드(HR 함수, 배율 1) `한/A`,
  메모장 목록(굵은 글꼴, 높이 22) `클리에에서한글로 작성`이 CJKOS 없이 표시됨. 메모장 큰 글꼴(높이 22 아님)은 `[]`.
  → 저해상도 앱(메모장)도 Sony가 고해상도 글꼴로 그린다.
- v7: 글꼴 높이 16~40, 배율 1·2이면 16×16 글자를 그린다(세로 시작 줄 = ascent − 15). Sizes 페이지에
  (글꼴 높이, 사각형 높이)별 한글 호출 수를 세어 실제로 쓰이는 크기를 조사한다(24×24·12×12 제작 순서 근거).
  **실측**: 메모장 기본·굵게·크게 모두 한글 표시. 크기는 **22/11(×2, 기본·굵게)**, **30/15(×2, 크게)** 두 가지
  (키보드 HR 22/22 ×1은 앞서 확인). → 필요한 글자는 16×16(22칸)과 24×24(30칸). 12×12는 HR 작은 글꼴(14)용으로 나중.
- 3단계 조사(v8): 폭 함수 트랩 8개(`FntCharWidth`, `FntCharsWidth`, `FntCharsInWidth`, `FntLineWidth`, `FntWordWrap`,
  `FntWordWrapReverseNLines`, `FntWidthToOffset`, `FldWordWrap`)와 Intl 루틴 16개(선택자 1~9, 13~15, 19, 22, 23, 30)에
  18바이트 통과 코드(리소스 `HgCt 0`, `src/hangul/count.s`)를 끼워 호출 수만 센다. Intl은 `IntlSetRoutineAddress`(선택자 29)로 바꾼다.
  메모 입력·BS·커서 이동 때 실제로 불리는 함수를 실측해 교체 대상을 확정한다.
  **실측**(메모 입력·BS·커서 이동): 불림 = Intl `NextChar` 129, `PrevChar` 109, `SetNext` 82, `CharWid` 40, `CharAttr` 35,
  `CharBnds` 6 / 트랩 `CharsW` 605, `LineW` 220, `W2Offset` 34, `CharsInW` 26, `WordWrap` 11, `CharW` 4.
  0회 = `ByteAttr`, `CharXAttr`, `CharSize`, `GetChar`, `WordBnds`, `IsValid`, `CharEnc`, `StrEnc`, `TruncOfs`, `WWrapOfs`,
  `WWrapRevN`, `FldWWrap`. → 교체 대상 **Intl 6개 + 폭 트랩 5개**(`CharW`는 1바이트 전용이라 제외). 영문 ROM에서도 Intl 루틴은 디스패처를 거친다.
  CJKOS 없이 "가나다" 입력 시 바이트가 깨짐: 조합 중 고치기(BS + 새 글자)의 BS가 1바이트만 지우기 때문(`PrevChar` 1바이트 기준).
- 3a(v9): Intl 6개(`CharAttr`, `PrevChar`, `NextChar`, `SetNext`, `CharBnds`, `CharWid`)를 EUC-KR 판으로 교체(리소스 `HgIt 0`,
  `src/hangul/intl.c`). 2바이트 = 앞·뒤 0xA1~0xFE, 상위 바이트 구간은 구간 시작부터 두 바이트씩 짝. 한글 폭은 그리기와 같은 규칙
  (glyph 모드·글꼴 16~40 → 18픽셀 ÷ 좌표 배율, 배율 = 'W' 픽셀 폭 ÷ `FntCharWidth('W')`). 자모 행 0xA4(현대 자모 51자) 글자 추가(`HgFn 2`).
  폭 트랩 5개는 3b에서 교체한다.
- 진행 순서(디버그 먼저): ① 바뀐 함수 조사(**완료**, 실측 1) ② `BltDrawChars` 하나만 교체해 표시 시험(CJKOS 끈 복사본 세션)
  ③ 폭·글자 경계 ④ 글꼴 3벌(16 → 12 → 24).
