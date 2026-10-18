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
| CJKOS 자체 입력기 | `EvtGetEvent`, `FldHandleEvent`, `SysKeyboardDialog` | 불필요(이 프로젝트 키보드가 대신함). `SysKeyboardDialog`는 실측 3 참고 |
| CJKOS 자체 관리 | `SysAppLaunch`, `DmOpenDatabase`, `DmCloseDatabase`, `DmDeleteDatabase`, `DmGet1Resource`, `DmNextOpenResDatabase`, `DmMoveOpenDBContext`, `IntlGetRoutineAddress`(Intl) | 불필요(글꼴 DB·리소스 관리로 추정) |

초기 구현 후보: 글자 그리기, 문자열 폭 계산, 2바이트 글자 경계 처리에 관련된 트랩·Intl 선택자(잠정 후보 트랩 약 10개 + Intl 약 13개).
실제로 필요한 최소 범위는 함수 하나씩 교체 시험과 함수 간 의존성 확인으로 확정한다. 지금까지 확정된 것은 `BltDrawChars` 하나(2a~2c 실측)다.

#### 실측 3: 소프트 키보드 경로 (MnHangul v28 KDlg, 2026-10-18)
- **방법**: `SysKeyboardDialog`·`SysKeyboardDialogV10`을 호출만 기록하고 원래 함수로 넘기는 코드로 교체(CJKOS OFF).
- **관찰**: 메뉴 → Keyboard(`default`), 가상 실크 Graffiti 영역의 ⓐ(`alpha`)·①로 여는 Palm 표준 **소프트 키보드**는 모두 `SysKeyboardDialog`로 열린다
  (메모장, 포커스 필드 있음). `V10`은 0회. 가상 실크의 ⌨ 아이콘으로 여는 Sony **가상 키보드**는 이 함수를 거치지 않는다.
  소프트 키보드 창의 편집칸에서도 MnHangul 그리기·폭 패치로 한글이 정상 표시된다.
- **판단**: CJKOS 화면 키보드는 이 함수를 교체하는 방식으로 본다(트랩 교체라 리셋 불필요).
- **결정**: 한글 소프트 키보드는 만들지 않는다. 창이 앱 화면 대부분을 가리고(복사본 칸에 입력 후 `Done`),
  실크 키보드(MnSk)의 리셋은 로더 처음 설치 때 1회뿐이라 얻는 것이 적다. 가상 실크가 없는 기종을 지원할 때 다시 검토한다.

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
  **실측(성공)**: "가나다라" 입력 → 바이트 정상 저장(글꼴을 바꿔 줄 전체를 다시 그리면 정상 표시).
  바로 입력하는 줄은 다음 글자가 앞 글자 오른쪽을 덮고, 커서가 글자마다 여백·글자 위로 들쭉날쭉함
  → 원인: 폭 함수가 한글을 두 바이트의 라틴-1 폭(글자마다 다름)으로 계산.
- 3b(v10): 폭 트랩 5개(`FntCharsWidth`, `FntLineWidth`, `FntCharsInWidth`, `FntWordWrap`, `FntWidthToOffset`)를
  리소스 `HgWd 0`(`src/hangul/width.c`)으로 교체. 처리 범위에 상위 바이트가 없으면 원래 함수 그대로(영문 동작 불변).
  한글 폭은 그리기·`CharWid`와 같은 규칙. 줄바꿈은 공백 뒤 우선, 공백이 없으면 글자 경계.
  MnHangul 첫 줄 `prot N` = 켜진 기능 수(각각 DB 삭제 보호 1개), 0이어야 덮어써 설치 가능.
  **실측(v10~v11)**: 바로 입력하면 여전히 겹침. v11 진단: 메모 칸은 교체한 폭 함수를 거친다(`CharsW` 226·`LineW` 61·
  `WordWrap` 15회 한글 경로). 그러나 **폭을 잴 때 현재 글꼴은 저해상도(높이 11)**, 그릴 때만 Sony가 고해상도 글꼴(22, ×2)로 바꿔 넘김
  → 폭을 `[]` 폭 6으로 계산, 그리기는 9 간격.
- v12: 폭 잴 때 글꼴 높이 8~15이고 glyph 모드면 한글 폭 9(= 18 ÷ 2). 순수 저해상도 화면(그릴 때도 `[]`)은 이 기기에서 해당 없음.
  **실측(성공, 2026-10-16)**: CJKOS 없이 메모장에서 `고노도로`, `가나다라` 바로 입력 시 겹침 없음, 글자 간격 일정.
  긴 한글 줄이 화면 오른쪽 끝에서 글자 경계로 자동 줄바꿈. Width 페이지 `last hangul w 9 x2 font h 11`,
  한글 경로 호출 `CharsW` 445·`LineW` 462·`WordWrap` 191·`W2Offset` 6. → **CJKOS 대체의 핵심 경로(그리기·글자 경계·폭) 동작 확인.**
  탭으로 커서 이동(글자 사이에 정확히 섬), BS 글자 단위 삭제도 정상.
- v13: 일부 한글 획이 두껍게 나오던 문제 수정. 원인: PIL(FreeType)로 그리면 초·중·종성 조각이 겹치는 곳에 한 줄이 더 칠해짐
  (완성형 2,350자 중 약 20%). Neo둥근모는 1픽셀 = 64단위 격자의 사각형 윤곽이므로 윤곽을 직접 풀어 nonzero 규칙으로 칠한다.
  **실측**: 획이 고르게(깨끗하게) 나옴. 글자 크기는 아직 세 글꼴 모두 16×16(굵게·24×24 미구현).
  참고: Neo둥근모 한글은 초·중·종성 조각(`cho_*`, `jung_*`, `jong_*`)을 조합하는 구조라, 조합형 글꼴(D 단계)의 16×16 조각 출처로 쓸 수 있다.
- v14(조합형 전환): 완성형 시험 데이터(78KB)를 **조합형 글꼴 묶음** 두 개로 교체(`tools/gen-johab.py`, 형식은 그 파일 머리말).
  | 묶음 | 원본 | 방식 | 조각 | 예외 | 특수 글자 | 크기 | 쓰는 칸 |
  | --- | --- | --- | --- | --- | --- | --- | --- |
  | `HgJb 0` (`HgJ16.bin`) | Neo둥근모 1.600 (OFL) | 8×4×4 | 344 | 0 | 0 | 14KB(자모 포함) | 높이 16~40 |
  | `HgJb 1` (`HgJ12.bin`) | 물마루 v1.0 (OFL, 예약 이름 미사용) | 10×6×4 | 457(추가 조각 포함) | 1,125 | 22 | 23KB | 높이 12~15 |
  벌 고르기 규칙: 초성 벌 = (중성, 받침 유무), 중성 벌 = (초성, 받침 유무), 종성 벌 = 중성. 규칙과 다른 음절은 예외표(조각 번호),
  손으로 그린 음절은 특수 글자표. 생성 때 **11,172자 전부 원본 글꼴과 1픽셀도 다르지 않음**을 확인하고,
  CI(`tools/test-johab.sh`)가 기기 코드(`patch.c` Compose)와 생성기 결과를 2,350자 + 자모 94자로 비교한다.
  16×16 화면 결과는 v13 완성형 데이터와 같음(2,350자 비교). MnHangul.prc 95KB → 57KB.
  조합형이라 11,172자를 그릴 수 있지만, 저장 코드가 EUC-KR(KS X 1001)이라 `똠`·`펲` 같은 음절은 저장할 수 없다
  (표시하려면 CP949 확장 완성형 지원이 필요 — 별도 단계).
- v14 크기 조사(여러 앱): `22/11 ×2` 117, **`11/11 ×1` 439**(저해상도 글꼴을 1:1로 그리는 화면, `[]`로 나옴), `22/22 ×1` 41(키보드),
  `30/15 ×2` 75, `28/14 ×2` 7, `14/14 ×1` 7(HR 작은 글꼴 → 12×12 사용). 12×12 한글은 2,350자 모두 위 11줄 안에 있음.
- v15: 칸 높이 11~15 → 12×12(11칸은 맨 위부터, 12번째 줄 버림). Sizes 페이지에 크기별 마지막 앱 크리에이터(#64~#71).
- v16(진단): 일정·주소록·할일·메모를 열면 키보드가 먼저 저해상도(160×160, 11/11 ×1)로 그렸다가 hires(22/22)로 다시 그린다(실크 이벤트의 hires 값이 처음엔 0). 11/11 ×1의 대부분은 이 저해상도 프레임. 12×12 자모는 위 11줄·왼쪽 11칸만 써서 11칸에 잘리지 않는다. 남은 문제는 폭(잴 때 9, 그릴 때 13): 폭 잴 때 글꼴은 ×1·×2 모두 저해상도 11이라 구별 불가 → 현재 그리기 창(winlog.h, Win 페이지 #72~#83)으로 구별되는지 조사.
- v16 결과: 폭 잴 때·그릴 때 창 비교. 비트맵·창 폭은 모두 160(rb 320)으로 같고, windowFlags만 다름 — 키보드 실크 창 C000(offscreen, W·×1), 일반 화면 창 1A00·0000(W·×2).
- v17: 저해상도 글꼴(8~15) 한글 폭 = 그리기 창이 offscreen이면 13(12×12 ×1), 아니면 9(16×16 ×2). width.c·intl.c 같은 규칙(winlog.h WinOffscreen).
- v18: Kbd 페이지(키보드 13단계 이벤트 순서 진단, docs/DEVELOPMENT_HISTORY.md 13단계).
- v19: Patch ON 한 번에 Mode glyph(off였으면) + Intl ON + Width ON.
- v20: Kbd 페이지가 키보드 14단계 기록 위치(#64~#80)를 읽음.
- v21(진단): 글꼴 높이/번호(FntGetFont)별 횟수 — 폭 잴 때 #84~#87, 그릴 때 #88~#91 (Sizes 8~11줄). 큰 글자(24×24) 폭 구별용.
- v21 결과: 폭 잴 때 일반 11/0, 볼드 11/1, 큰 글자 15/7(largeBold), HR 22/8. 그릴 때 일반 22/8, 볼드 22/9, 큰 글자 30/15 → 큰 글자는 폭 잴 때 높이 15로 구별된다.
- v22: 추가 글꼴 MnHgFont24.prc(HgFt/MnHg, HgJb 2, 묶음 판 2 = 한 줄 4바이트)를 Patch ON 때 연결(#92~#94, DB 보호). 그릴 때 28 이상 → 24×24(칸 26), 폭 잴 때 저해상도 14 이상 → 13·hires 28 이상 → 26/배율. patch.c 그림 줄 32비트화. 글꼴만 업데이트: All OFF → MnHgFont24.prc 설치 → Patch ON.
- v23: 큰 글자 칸 26 → 24(칸 여백 없이 글꼴 자체 여백만, 글자 사이 약 5 → 3픽셀, 저해상도 폭 12). Patch ON이 이미 켜져 있어도 추가 글꼴 연결 재시도 + 결과 메시지(f24 ok / none / open fail / no res / bad fmt).
- v24(진단): 리셋 알림(sysAppLaunchCmdSystemReset) 수신 기록(#96 횟수, #97 tick, #98 BltDrawChars 주소, #99 hard·defaults·DB 열기). 동작 변경 없음.
- v24 결과: 리셋 알림 수신(n1, 부팅 후 약 3.4초), 자기 DB 열기 가능, 그때 BltDrawChars는 ROM 주소(0x1008359A).
- v25: 부팅 자동 시작. [Auto] 설정(앱 설정, 리셋 후 유지)이 on이면 리셋 알림 때 Patch ON과 같은 설치(결과 #95). 설치 함수는 전역·상수 표 없이 동작(빌드 때 %a5 검사). 비상: 위 버튼 + 리셋.
- v26: Main 줄 6에 [Soft Reset (hold 3s)] (3초 누르면 리셋, 떼면 취소). 자동 시작 시험을 MnHangul 안에서 끝낼 수 있다. 줄 7은 calls 줄(이전 last len 줄 삭제).
- v27: 리셋 버튼을 누르는 동안 폭 고정(`Hold... 3s`), `boot` 줄에서 bdc 주소를 빼 `a` 값이 다 보이게.
- v28 (진단): KDlg 페이지 — `SysKeyboardDialog`·`SysKeyboardDialogV10`을 기록만 하는 코드(HgKd 0)로 교체. 호출 수, 마지막 키보드 종류, 포커스 필드 유무, 호출한 앱을 기록. CJKOS 화면 키보드처럼 리셋 없는 한글 키보드를 띄울 수 있는지 판단용.
- v47: ⇪ 그림은 직전 키 글자가 2줄 끝(\\ 또는 |)일 때만 판정에 씀. v46 실측: ⇪ 누를 때 Sony가 눌림 표시로 반전 그림을 앱이 펜을 받기 전에 그려 기준값(S)이 켜짐으로 틀어짐 → 2줄 영문.
- v48: 가상 키보드 일반 키를 누른 동안(펜 표시 #144 = 3)은 v44 영문 내림을 하지 않음. v47 실측: 한글 모드에서 키를 누르면 Sony가 잠깐 ⇪ 꺼짐 + 일시 쉬프트(c0 t3)로 바꿔, 그 순간 다시 그린 누른 키만 영문으로 남음(LR667·LW674 H0 c0 t3 → 바로 c1). 앱 화면 누름(새 메모)은 0이라 v44 그대로.
- v49: 키 글자가 소문자이고 그리는 순간 ⇪ 꺼짐(c0)이면 영문. v48 실측: 키보드를 처음 띄울 때 이전 한글 모드(H=1)가 남아 숫자·qwerty 줄 소문자가 쌍자음으로 그려지고 ⇪ 그림(회색) 뒤부터 영문. ⇪+쉬프트는 그리는 순간 c1이라 영향 없음.
- v50: 펜 표시 3(키 누름)은 키 영역(저해상도 y 160~212)만, 아래 탭·상태 표시줄은 0. v49 규칙은 3일 때 적용 안 함. v49 실측: ⇪+쉬프트에서 ㅃ 입력 → 누른 키 다시 그릴 때 c0 소문자로 H=0 → 쉬프트 풀린 다시 그리기(c0 t0 대문자)의 qwerty 줄 영문, ⇪ 그림 뒤부터 자모.
- v51 (시험): CJKOS 세션 실측(v50) — Sony 한글 입력(HgIm)만 켜도 두벌식 조합 입력·한A 전환 정상(겹침 없음). 키 글자 자모 표시까지 CJKOS와 같이 쓰도록 KDlg [Sony only]: 그리기 패치는 걸되 글자 모드 off(2바이트 한글은 원래 함수 = CJKOS), Intl·Width 없음.
- v52: v51 실측(CJKOS + Sony only) — 입력 정상, 키 글자 자모는 세로획이 빠진 모양. 자모 임시 글꼴을 "패치 아래 함수"(CJKOS)로 그리면 CJKOS가 다시 처리하는 것으로 보고, 오버레이만 ROM BltDrawChars(0x1008359A, 시작 바이트 4E56FFF0 48E71E30 확인 후)로 직접 그림. KDlg 2줄에 아래 함수·자모 주소.
- v53: 업데이트 단순화 — 첫 화면 `Update (hold 3s)`: 앱 설정 3번(다음 리셋 한 번 자동 시작 건너뛰기)을 켜고 리셋. 리셋 알림에서 건너뛰고(#95 = 0x40000000) 설정을 지운다. CJKOS 훅이 위에 있어 `Hangul OFF`로 못 풀 때도 같은 순서(Update → 설치 → Hangul ON).
- v54: 세 모듈로 분리 — 한글 표시(MnHangul), Mn 가상 키보드(MnSkPlugin+MnSkCore), Sony 가상 키보드 한글(새 모듈 MnSonyKbd, 크리에이터 MnSy, src/sony).
  MnSonyKbd = SyPt 0(spatch.c: BltDrawChars, 가상 실크 창 영문 키 글자만 자모로, 나머지는 아래 함수로) + SyIm 0(ime.c, MnHangul HgIm 그대로·기록 feature만 'MnSy')
  + SyIc 0·1('한A') + SyJm 0(자모 그림만, tools/gen-sony-jamo.py로 HgJ16 자모표에서 뽑음). 한/영 판정(HanLabel)은 v50과 같다.
  CJKOS 세션 실측(v53 `Sony only`): 자모·입력·쌍자음 정상(본문은 CJKOS 글꼴) → 설치 B(CJKOS + MnSonyKbd)의 근거. MnHangul에서는 Sony 관련 코드·설정·KDlg 판정 기록을 뺐다.
- v55 / MnSonyKbd v2: 첫 화면 정리 — MnSonyKbd는 쓰는 것만(ON/OFF, Start at reset, Update, '한A' 그림 사용법, 제목 누르면 판정 기록), 역할 줄 "Hangul kbd on Sony virtual kbd". MnHangul은 역할 줄과 MnSonyKbd 상태 줄.
- MnSonyKbd v3: v2 실측 — CJKOS + MnSonyKbd ON → CJKOS 끄기 → MnHangul ON 하면 `한A`·입력은 되는데 키 글자 자모가 안 나옴. 판정 기록: ⇪ 그림은 계속 그려지고(I735 반복) 키 글자 그리기 수(#108)는 멈춤, blt below = 0x340AA0(CJKOS). CJKOS가 꺼질 때 BltDrawChars를 자기가 저장한 ROM 주소로 되돌려 그 위의 MnSonyKbd가 빠진 것. 수정: src/sony/chain.h — 트랩에서 아는 패치(MnHangul #21)를 따라 내려가 MnSonyKbd를 못 만나고 ROM에 닿으면(빠짐 확인) 맨 위에 다시 끼움. ⇪ 그림을 그릴 때(ime.c) 자동으로, 앱 `ON`으로도(입력·그림 연결 포함). `Now:`는 실제 연결 상태(ON / ON (other patch above) / broken).
- v56 / MnSonyKbd v4: CJKOS 동작 감지 — 실측 1의 CJKOS 교체 함수 중 MnHangul·MnSonyKbd가 바꾸지 않는 FntSetFont·FrmDrawForm·FldHandleEvent·StrCompare가 하나라도 ROM 밖이면 "CJKOS?". MnHangul은 그때 Hangul ON·리셋 자동 시작을 하지 않는다(리셋 순간 CJKOS가 나중에 걸리면 못 알아챔). MnSonyKbd 첫 화면에 한글 표시 모듈 표시.
- v57 / MnSonyKbd v5: 실측 — MnHangul `Update` 뒤 MnSonyKbd는 자기 자동 시작으로 잠겨 설치 실패. `Update`는 설치되어 있는 다른 모듈의 앱 설정 3번(건너뛰기)도 켠다(설치 안 된 모듈에는 저장하지 않음).
- MnSonyKbd v6: v5 실측 — CJKOS 끄고 MnHangul ON 뒤 ⇪: 1번째 영문 키 글자(복구가 ⇪ 그림 때라 앞 줄은 패치 없이 그려지고 #151이 없어 기준값 S 안 바뀜), 2번째(끄기) 쌍자음 표시(S 옛 값으로 H=1), 3번째 정상. 수정: 가상 실크 펜 누름 때(Sony가 그리기 전, v38 실측 순서) 먼저 복구, 그림 때 복구했으면 그 그림을 실제 상태로 씀.
- MnSonyKbd v7: v6 실측 — 복구 뒤 첫 ⇪에서 qwerty 줄만 영문(`한A` 파랑, 아래 줄 자모). 펜 누름 때 복구는 됐지만 ⇪ 판정이 옛 실제 상태(#150)의 반대라 H=0으로 시작. 수정: 복구 때 #150을 모름으로, 모를 때는 그 순간 Graffiti ⇪ 상태의 반대(Sony는 그린 뒤 상태를 바꾸므로 펜 누름 순간 = Sony 현재 상태).
- v58: 실측 — MnSonyKbd가 위에 있을 때 MnHangul `Hangul ON`을 다시 누르면 먹통. 원인: 켜짐 판정이 "트랩 = MnHangul 진입점"이라 아니라고 보고 다시 설치 → 아래 주소에 맨 위(MnSonyKbd)를 적어 MnHangul → MnSonyKbd → MnHangul 순환. 수정: 연결을 따라 내려가 찾기(BltFind, MnSonyKbd chain.h와 같은 방법). 연결 안에 있으면 그대로, ROM까지 없으면 맨 위에 다시 잇기.
- 실측(2026-10-18, v58 + MnSonyKbd v7): MnHangul 켬 → CJKOS 켬 → 끔 뒤 ClieDiag Trap 페이지 — ROM 밖 트랩 8개(폭 5개 0x3C0F60~0x3C15A0, BltDrawChars, EvtGetEvent·WinDrawBitmap = MnSonyKbd)와 Intl 6개(0x39DCxx~)만 남고 CJKOS 것(트랩 33·Intl 18)은 사라짐. → CJKOS는 꺼질 때 **자기가 켜질 때 저장한 주소**로 되돌린다. 먼저 켜진 모듈(MnHangul)은 연결이 유지되고, CJKOS보다 나중에 켠 모듈(MnSonyKbd v2 실측)만 빠진다. 외부 검토가 지적한 Intl·Width 유실은 이 순서에서 재현되지 않아 복구 코드는 넣지 않는다.
- 실측(2026-10-18): CJKOS를 끈 뒤 MnHangul로 "가나다" BS·커서 이동 글자 단위 정상. MnSonyKbd v7: CJKOS 켬(MnSonyKbd ON) → CJKOS 끔 → ⇪ 켬·끔·켬 세 번 모두 자판 전체가 맞게 그려짐(v5·v6 문제 해소).
- 실측(2026-10-18): MnHangul 켬 → CJKOS 켬(둘 다 켜짐)에서 메모장 한글 입력 전후 MnHangul 진단 — 그리기 호출 수 calls 4799 → 5045(늘어남), 2바이트 글자 호출 수 hi 442 → 442(그대로). → 연결 맨 위의 CJKOS가 한글을 직접 그리고 아래(MnHangul)에는 한글 아닌 글자만 넘긴다. 둘 다 켜진 동안 한글 표시는 CJKOS 몫이고 MnHangul은 한글에 관여하지 않는다(Intl·Width도 같은 구조로 추정, 미확인). 이때 메모 "세종시"의 글자 사이가 벌어져 보인 원인은 미확인.
- v45: ⇪ 펜 누름은 Sony 실제 상태(자판 전체 그리기의 ⇪ 그림, #150)의 반대로 정함. v44 실측: 값을 뒤집는 방식은 한 번 어긋나면 ⇪ 켜기·끄기 모두 1·2줄이 틀림.
- v44: ⇪ 판정 보완(눌림 표시 그림 무시, ⇪ 꺼짐 + 일시 쉬프트 → 영문). v43 실측: Sony는 줄 순서로 그려 ⇪ 그림이 3줄 맨 앞 — 1·2줄은 그 전에 그려짐. ⇪ 누를 때 눌림 표시로 보통 그림을 먼저 한 번 그림.
- v43: Sony ⇪ 키 그림(ROM SilkExtn Tbmp 2400 보통·2500 반전, 8비트 글자 0·바탕 0xD8/0x39)을 `한A`(22×18, Galmuri11 OFL)로 바꿔 그리고, 그 그림으로 한글 모드 판정(WinDrawBitmap). v42 실측: 새 메모에서 Sony가 쉬프트 고정을 풀었는데 표시는 한글로 남고 입력은 A.
- v42: 사용자 화면 이름 Hangul ON/OFF, Start at reset, Sony keyboard(한 줄씩). Start at reset·Sony keyboard는 저장한 적 없으면 ON.
- v41: 기본 Main은 사용자용(Patch ON·All OFF, Auto·Sony, Soft Reset, 사용법, [ ] Debug), 진단 Main·페이지는 Debug 체크 때만.
- v40: Main 줄 6 오른쪽 `Sony: ON/off` — Sony 한글을 첫 화면에서 설정(저장형), Patch ON·Auto가 함께 켬. v39 실측: ⇪·쉬프트·⇪ 끄기 표시 정상, 새 메모에서도 유지, 긴밤·달가·값읽·와의왜·가 나 가 입력 정상.
- v39: 오버레이 한글 모드를 ⇪ 키 펜 누름으로 판정(#143). v38 실측: 펜 누름(⇪ 6,186 · 쉬프트 7,195)이 키 글자 다시 그리기보다 먼저 앱 EvtGetEvent를 지남.
- v38 (진단): SetSt에 가상 실크 영역 펜 누름·뗌 기록. v37 실측: Sony는 키 글자 약 47개를 다 그린 뒤 SilkExtn(0xFB2)이 GrfSetState 호출, ⇪+쉬프트와 ⇪ 끄기 모두 GrfSetState(c0 u0)로 같음, 화면 복사 없음(가상 실크 창에 그리면 바로 보임).
- v37 (진단): SetSt 페이지 — GrfSetState·GsiSetShiftState·가상 실크 창 WinCopyRectangle 기록(키 글자 다시 그리기와 상태 변화 순서). v36 실측: 그리는 순간 상태는 직전 상태(보통→⇪와 보통→쉬프트 모두 U g0000, ⇪→쉬프트와 ⇪ 끄기 모두 L g1000).
- v36: 오버레이 판정을 키 글자 대·소문자 + Graffiti 상태로(⇪ 상태가 다시 그리기보다 늦게 바뀜). v35 실측: 입력 정상(R K → 가), ⇪ 직후 전체 다시 그리기는 영문으로 남고 누른 키만 자모.
- v35: Sony 가상 키보드 한글 입력. KDlg `Sony Hangul`이 오버레이 + 입력 변환(HgIm 0, EvtGetEvent)을 함께 켬. 쉬프트 고정(⇪) = 한글 모드, 대문자 = 기본 자모, ⇪+쉬프트(소문자) = 쌍자음·ㅒㅖ. 조합은 MnSkCore와 같은 방식(BS + 완성형 2바이트, 우리 키는 keyCode 0x4D48 표시). v34 실측: ⇪+쉬프트 소문자는 받는 순간 g1000, 영문 소문자는 g0000.
- v34 (진단): 글자 키 add·get 순간 Graffiti 상태 나란히. v33 실측: ⇪ 상태는 g1000(첫 글자도 자동 쉬프트 없음), ⇪+쉬프트는 소문자 0061 g0000, 영문 모드의 쉬프트는 자동 쉬프트와 같은 g0031.
- v33 (진단): EvtAddEventToQueue keyDown 최근 8개 + 넣는 순간 Graffiti 상태. v32 실측: Sony 가상 키보드 글자는 SilkExtn이 EvtAddEventToQueue로 넣음(modifiers 0).
- v32 (진단): EvtGetEvent keyDown(글자·modifiers·keyCode·Graffiti caps/num/temp/auto) 최근 기록, EvtAddEventToQueue keyDown 기록. v31 실측: Sony 가상 키보드 글자는 EvtEnqueueKey로 오지 않음(0x0207·홈 0x0108만), 오버레이는 동작(fh 22 rh 22).
- v31: v30의 EvtEnqueueKey 8개 기록이 kbd.c에 반영되지 않아 표시가 깨졌던 것 수정. 오버레이 조건 완화(비트맵 폭 검사 뺌, 사각형 높이 22·11 아니면 배율 1), 걸러진 이유(#128) 표시.
- v30: KDlg `Overlay` — Sony 가상 키보드 키 글자(영문 한 글자, 가상 실크 창·글꼴 높이 22)를 두벌식 자모로 바꿔 그림. 표시만, 입력은 영문. EvtEnqueueKey 기록 최근 8개.
- v29 (진단): Sony 가상 키보드 한글 오버레이·입력 변환 가능성 판단용. 활성 창이 아닌 창(가상 실크 등)에 찍히는 글자 기록(그리기 패치, #108~#121), `EvtEnqueueKey` 호출 기록(글자·modifier·부른 곳, 인터럽트에서도 불릴 수 있어 동적 힙 버퍼에 직접 기록).
- 실측(CloudpilotEmu): Auto ON + Soft reset → 메모장 한글, 상태 `patch ON glyph font ok+24`, `boot n1 t340 h0 db1`. No extensions 리셋 → `patch off`, `boot n0` (비상 탈출 확인). Auto off + Soft reset → 설치 안 함.
- 24 낱자모(gen-johab24.py): 글자마다 ① 조각 JSON jamo → ② hangul24-jamo.json 손그림(ㅅㅆㅈㅉㅊㅎ) → ③ 지금 조각으로 만들기(자음 초성 1벌, 겹받침 종성 1벌, 모음 중성 0벌, 가운데 맞춤). 예전 16×16 자동 확대는 쓰지 않음.
  남은 문제: 11/11 ×1 칸은 폭을 잴 때 글꼴(11)이 2배 그리기 칸과 같아 폭 9로 계산됨(실제 12) → 앱 확인 후 구분 방법 결정.
- 진행 순서(디버그 먼저): ① 바뀐 함수 조사(**완료**, 실측 1) ② `BltDrawChars` 하나만 교체해 표시 시험(CJKOS 끈 복사본 세션)
  ③ 폭·글자 경계 ④ 글꼴 3벌(16 → 12 → 24).
