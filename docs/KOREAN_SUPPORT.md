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
- v60 / MnSonyKbd v8 (진단, 동작 변경 없음): 내부 구조 의존(ARCHITECTURE 10절 3단계)을 공개 함수로 바꿀 수 있는지 비교.
  MnHangul KDlg 7~9줄 = `FtrGet` 시스템 Feature 원시값(인코딩·인코딩 플래그·언어·국가·TSM 플래그, `!` = 오류)과 지금 `CJKOS?` 판정. 값의 변화가 실측되기 전에는 CJKOS 판정에 쓰지 않는다.
  MnSonyKbd 판정 기록 8~10줄 = 키 글자 후보마다 창 판정 세 가지(windowFlags 0x4000 / `WinGetBitmap` 비교 / SilkWareLib `GetScrWindow` 일치)의 표본·일치 수·마지막 불일치, 글꼴(`FntGetFontPtr()` == p6, `FntCharWidth`·`FntBaseLine` 대 폭 표·ascent)의 일치·불일치.
  `GetScrWindow`는 ROM 정적 분석으로 확인(라이브러리 이름 "Silk Ware Library", 디스패치 표 9번째 = custom + 4, 0x1035CF12: SilkExtn 전역 +172를 출력 인자에 쓰고 0 반환, 'inoT' 확인 실패면 0x7704). 앱 화면에서만 부른다.
  비교 상태: 순정(CJKOS 없음) / CJKOS 켬 / CJKOS 끔 / MnHangul 켬 / MnHangul 켬 → CJKOS 켬.
- v60 / v8 실측(2026-10-19, CJKOS 켬):
  - MnHangul KDlg: `CJKOS? running`(함수 4개 방식)인데 Feature는 `enc 3`(PalmLatin)·`flags 1`·`lang 0`(영어)·`ctry 23`·`tsm` 없음(오류) — CJKOS가 `FtrGet`을 바꾸지만 이 시스템 Feature들은 영문 그대로 돌려준다. **Feature 방식으로는 CJKOS를 감지할 수 없다** → 함수 4개 방식 유지.
  - MnSonyKbd(키 글자 후보 80개): 창 판정 windowFlags 0x4000 = `WinGetBitmap` 비교 80/80, 글꼴 `FntGetFontPtr()` = p6·`FntCharWidth` = 폭 표·`FntBaseLine` = ascent 80/80. `GetScrWindow`는 0(라이브러리 못 찾음 또는 실패)이라 비교에서 빠짐.
- v8 실측(2026-10-19, CJKOS 끔): 키 글자 후보 224·422·522개 모두 창 판정·글꼴 공개 함수 일치(불일치 0). CJKOS 켬·끔 두 상태 모두 일치 → 공개 함수로 바꿀 근거(바꾸기는 다음 판).
- v8 실측(2026-10-19): CJKOS 켬 → MnSonyKbd `ON`(blt below = 0x340AA0 CJKOS) → CJKOS 끔 → 메모장: `한A` 켜짐인데 자판 영문, 입력 영문. 진단: heal 키 글자 +1(⇪ 그림 때 복구, blt below = 0x3C3CAE MnHangul), 입력 복구 0, 판정 기록 `p0`만(펜 기록 없음 = 입력 함수 안 불림), `why6 H0`. 첫 화면 `Now: broken: tap ON` → `ON` = `re-attached 1` → 정상. → CJKOS를 끄면 **입력(EvtGetEvent) 연결도 빠지고**, 입력 복구는 앱 `ON`에서만 했다. ⇪ 그림(WinDrawBitmap) 연결은 남는다.
- v8 실측(2026-10-19): MnSonyKbd ON → CJKOS 켬(`ON` 안 누름) → 메모장: 자판 영문, 누른 키만 자모, 입력 `GA`(영문). CJKOS가 MnSonyKbd 위에 걸린 상태. 다른 패치가 위에 있으면 다시 잇지 않는 원칙이라 자동 처리 없음(아래 v9 진단으로 원인 조사).
- MnSonyKbd v9: ⇪ 그림 연결(`HgImBmp`)에서 키 글자 다음으로 입력 연결도 확인해, ROM이 맨 위면(빠짐) 다시 잇는다(`SyImHealOne(0)`, 그림 연결 자신은 건드리지 않음). CJKOS를 끈 뒤 키보드를 처음 그릴 때 `ON` 없이 복구.
  진단: 판정 기록 7줄 `get N ev 맨위/아래`(입력 함수 불린 수, ⇪ 그림 순간 EvtGetEvent 맨 위, MnSonyKbd 입력의 아래 주소), 9줄 `blt top`(⇪ 그림 순간 BltDrawChars 맨 위). CJKOS를 나중에 켠 경우 CJKOS가 아래(MnSonyKbd)를 부르는지 판단 자료. 불린 수만으로 CJKOS 내부 방식을 단정하지 않고, 위로 다시 끼우는 동작은 연결 안에 없음이 확인되기 전에는 하지 않는다.
- v45: ⇪ 펜 누름은 Sony 실제 상태(자판 전체 그리기의 ⇪ 그림, #150)의 반대로 정함. v44 실측: 값을 뒤집는 방식은 한 번 어긋나면 ⇪ 켜기·끄기 모두 1·2줄이 틀림.
- v9 실측(2026-10-19, 시험 A): MnHangul ON, CJKOS 켬 → MnSonyKbd `ON`(입력 아래 = 0x341D60 CJKOS) → CJKOS 끔 → MnSonyKbd를 열지 않고 메모장: 자판·입력 정상. 진단 heal `00010000`(입력 +1, ⇪ 그림 때 자동), 입력 아래 = ROM 0x100496FC, get 계속 증가. **CJKOS를 끈 뒤 `ON` 없이 입력 연결 복구 확인.** 이번 순서에서는 키 글자 연결이 CJKOS·MnHangul 아래(below ROM)라 빠지지 않음(키 글자 복구는 v3~v8에서 확인).
- v9 실측(2026-10-19, 시험 B): MnHangul·MnSonyKbd ON(키 글자 MnSonyKbd → MnHangul 0x3C2516, 입력 아래 ROM) → CJKOS 켬(`ON` 안 누름) → 메모장 자판 한글·입력 한글 정상. 진단: 입력 맨 위 0x341D60·키 글자 맨 위 0x340AA0 = CJKOS인데 입력 함수 176회, 키 글자 표본 94·덧그리기 38 → **CJKOS는 위에 걸려도 자기 아래(MnSonyKbd)를 부른다**(맨 위 주소와 호출 수를 함께 본 판단). CJKOS 끔 → 맨 위가 MnSonyKbd로 돌아오고 heal 변화 없음, 정상.
  → CJKOS 위로 다시 끼우면 두 번 호출·순환 위험이 있으므로 하지 않는다(자동 재설치 불필요).
  → 10:46의 "CJKOS 켬 → 자판·입력 영문"은 CJKOS를 나중에 켠 탓이 아니라, 그 전 CJKOS 끄기로 입력 연결이 빠진 채(v8, `Now: broken`) CJKOS를 켠 상태였다(10:43 진단 `p0`, 10:59 `Now: broken` → `re-attached 1`). v9는 CJKOS를 끈 뒤 첫 키보드에서 입력을 다시 이으므로 이 상태는 "CJKOS 끔 → 키보드 안 열고 → CJKOS 다시 켬"일 때만 남는다(그때는 `ON`).
- 한글 조합 공용화(MnSkCore build 15, MnSonyKbd 코드 변화 없음): `src/skwr/skwr.c`와 `src/sony/ime.c`에 복사되어 있던 조합 함수(KscJamo~SylKsc 같은 줄, Compose·ComposeBack 상태 저장만 다름)를 `src/common/hangul_compose.h` 하나로. 상태는 `st[0..1]`, 키 보내기는 모듈이 정하는 `HcSend`(MnSkCore keyCode 0 + 횟수 기록, MnSonyKbd keyCode 0x4D48). MnSkCore는 feature #kFtrComp·#kFtrCompApp를 읽어 넘기고 다시 저장하는 감싸기(SkCompose·SkComposeBack, 조합 중 아닐 때 BS는 직접 보냄). 확인: 조합 시험 41/41, MnSonyKbd 입력 코드 리소스(SyIm 0)가 바이트 단위로 같음. 실측(2026-10-19): MnSkCore build 15·MnSonyKbd v9 메모장에서 조합·BS 정상.
- MnSkCore build 16 / ClieDiag v23 (진단, 동작 변경 없음): 조합 중 커서 이동 감지 가능성. 키를 누를 때 폼 초점 입력칸(`FrmGetActiveForm`·`FrmGetFocus`·`FrmGetObjectPtr`, 필드만)과 `FldGetInsPtPosition`을 읽어, 직전 키 때 위치 + 그 뒤 보낸 키로 생길 변화(2바이트 글자 +2, 1바이트 +1, 조합 BS -2)와 비교. 조합 아닌 BS 뒤는 비교 안 함. 기록 #81 비교 수·#82 다름 수·#83 기대/실제·#84 다름·조합 중 표시. ClieDiag SkEv 6줄 `fld N bad N eE aA [X][C]`. MnHangul 등 2바이트 글자 경계 처리가 있어야 BS가 2바이트를 지운다.
- build 16 실측(2026-10-19, MnHangul ON 메모장): 커서를 옮기지 않은 입력·BS 14회 모두 기대=실제(`fld 14 bad 0`). 조합 중(`달`) 본문 앞쪽을 탭한 뒤 `ㅏ` → `fld 7 bad 1 e6 a2 X C`로 감지. 화면은 `다라나다달`(조합 고치기 BS·새 글자가 옮긴 자리에 들어가고 끝의 `달`은 남음 — 커서 앞에 글자가 있었다면 지워졌을 것).
- MnSkCore build 17: 조합 중인데 커서가 기대 위치와 다르거나 입력칸이 바뀌었으면 조합을 끝낸다(`Commit`, BS 안 보냄). 진단 기록은 유지.
  실측(2026-10-19, MnHangul ON 메모장): `가`+`달`(조합 중) → `가`와 `달` 사이로 커서 이동 → `ㅏ` = `가ㅏ달`(옮긴 자리에 ㅏ만, 다른 글자 그대로). 조합 중 커서 이동 뒤 BS = 일반 BS(커서 앞 `가`만 지움, 예전에는 `가` 자리에 `다`가 들어감).
- MnSkCore build 18 / MnSonyKbd v10: 커서 위치 비교를 `src/common/cursor_track.h`로 옮겨 두 키보드가 공용(MnSkCore 동작 같음). MnSonyKbd는 키를 받을 때 비교해 조합 중 어긋나면 조합 끝, 앱에 그대로 넘기는 키(영문 모드·명령 키·조합 아닌 BS·자모 아닌 글자)와 앱 화면 펜 누름 뒤에는 비교 안 함. 진단 화면 9줄 `fld N bad N eE aA [X][C]`(v9의 blt top 줄 자리).
- ClieDiag v24 (진단): Ftr 페이지 = RAM Feature 표 전체(`FtrGetByIndex`, MnSk·MnHg·MnSy·MnDg 제외, 한 줄에 `크리에이터 번호 값` 2개). CJKOS가 켜질 때만 등록하는 Feature가 있으면 MnHangul CJKOS 감지를 공개 함수 방식(함수 4개 ROM 밖 판정 대신)으로 바꿀 수 있는지 확인용. ClieDiag 시작 때 모으므로 CJKOS를 바꾼 뒤 ClieDiag를 다시 연다.
- ClieDiag v24 실측(2026-10-21): CJKOS 끔 RAM 254개(표시 55) → 켬 256개(57) → 끔 254개(55). 켤 때만 `dCJK 0 000F4A3E`·`dCJK 1968 000F4A3E`가 생기고 나머지는 같음. → CJKOS는 켜질 때 자기 크리에이터 `dCJK` Feature를 등록하고 끌 때 지운다(v60의 시스템 Feature `psys` 인코딩·언어는 그대로였음).
- MnHangul v61 / MnSonyKbd v11: CJKOS 감지 = `FtrGet('dCJK', 0)`(공개 함수, ARCHITECTURE 10절 3단계 → 1단계). 시험 기간에는 함수 4개 방식과 OR(안전 쪽), MnHangul KDlg 9줄 `now: CJKOS ftr yes/no / traps yes/no`로 두 결과 비교. MnSonyKbd 첫 화면은 Feature로 확인되면 `CJKOS`, 함수 4개만 맞으면 `CJKOS?`. 실측(2026-10-21): CJKOS 끔 `ftr no / traps no`·`none` → 켬 `ftr yes / traps yes`·`CJKOS` → 끔 `ftr no / traps no`·`none`, 두 방식 일치.
- MnHangul v62 / MnSonyKbd v12: CJKOS 감지는 `dCJK` Feature만(함수 4개 ROM 밖 판정 뺌). 화면 글귀 `CJKOS?` → `CJKOS`. MnHangul 진단 첫 화면 6줄 오른쪽에 `[Update]`(3초, 사용자 화면 `Update (hold 3s)`와 같음: 두 모듈 자동 시작 한 번 건너뛰고 리셋) 추가 — 예전에는 Debug를 켜면 Update 버튼이 없었음.
- MnHangul v63 / MnSonyKbd v13: 판 번호를 `kHgVer`·`kSyVer` 한 곳에 두고 모든 화면 제목이 그 값을 씀(외부 검토 지적: v61·v62의 MnHangul 사용자 첫 화면 제목이 `v60`으로 남아 있었음). MnSonyKbd 첫 화면 한글 표시 줄: CJKOS와 MnHangul이 둘 다 켜져 있으면 `CJKOS (+MnHangul)`(실제 그리기는 CJKOS).
- MnHangul v64 / MnSonyKbd v14: `All OFF`(MnHangul 사용자 첫 화면 4줄 `All OFF (for install)`, MnSonyKbd 3줄 `All OFF`) — 자기 끄기 → 다른 모듈 끄기를 두 바퀴(`src/common/mod_off.h`). 다른 모듈은 `SysAppLaunch(…, sysAppLaunchFlagNewGlobals, sysAppLaunchCmdCustomBase + 0x10)`으로 불러 그 앱이 직접 끔(MnHangul은 끄기에서 전역 gMsg를 쓰므로 새 전역 공간이 없으면 3을 돌려줌). 결과: `all off: ready to install` / `kept: turn CJKOS off, retry`(dCJK 있음) / `kept: use Update, then install` / `other app: launch failed`. 실측(2026-10-21): 켠 순서와 상관없이 `All OFF`로 두 모듈 모두 꺼지고 리셋 없이 설치됨(새 전역 공간으로 다른 앱 부르기 동작 확인).
- MnSonyKbd v15: 창·글꼴 공개 함수 전환 — 화면 밖 창 판정 `WinGetBitmap(그리는 창) != WinGetBitmap(화면 창)`(창 구조 +8 대신), 자모 가로 가운데 `FntCharWidth`(폭 표 계산 `CharW` 대신), 세로 위치 `FntBaseLine`(f[9] 대신). 근거 v8 실측(불일치 0). 높이 f[7](22 판정·배율·임시 글꼴 크기)은 그대로 두고 `FntCharHeight`와 비교(진단 10줄 `hbad`, #198·#199). 진단 8줄 `win … ok …`는 이제 a = 예전 판정, b = 지금 판정. 실측(2026-10-21, CJKOS 켬 포함): `win 446 ok 446`, `font 446 ptr 446 bad 0 hbad 0`, 덧그리기 342 — 예전·지금 판정과 글꼴 높이 모두 일치.
- MnSonyKbd v16: 글꼴 높이도 `FntCharHeight`(22 판정·배율·임시 글꼴 크기). 할 일을 마친 비교 진단(v8 창·글꼴 비교 `Diag`, 예전 판정 `SilkWinFlags`·`CharW`, SilkWareLib `GetScrWindow` 호출)은 뺌 — 비공개 함수 의존도 하나 줄어듦. 진단 화면 8줄은 `win/font: public API, overlay N`, 10줄은 빈 줄. 동작 경로에서 글꼴 구조를 읽는 곳은 `MakeTmpFont`(임시 글꼴의 종류·ascent·descent 복사)만. 실측(2026-10-21): 메모장 `한A`·⇪ 쌍자음 표시 정상.
- ClieDiag v25 (진단): Info 페이지에 Sony SysInfo(`FtrGet('SoNy', 1)` → Sony SDK `SonySystemFtr.h` 구조) 해석 — `extn`(소프트웨어 실크 0x10·하드웨어 키보드 0x40·조그 0x01·홀드 0x04), `libr`(HR 0x01·Silk 0x100), `status`, `jogType`. MnSonyKbd·Mn 가상 키보드를 켜기 전에 "소프트웨어 실크가 있는 기종인가"를 공개 Feature로 확인할 수 있는지 보는 용도. SDK 5.0의 `Vsk…`는 Silk 라이브러리 API 2판 이상용이라 NR70V(1판)에서는 쓰지 않음. 실측(2026-10-21, CloudpilotEmu NR70V): `SoNy#1` 있음, SysInfo rev 1, `extn 0000007F` → silk·kb·jog·hold 모두 Y, HRlib Y·SilkLib Y, status 8, jogType 2. (하드웨어 키보드 비트는 켜져 있으나 에뮬레이터는 터치 입력만 시험 가능)
- MnSonyKbd v17: `SonyOn`(`ON`·리셋 뒤 자동 시작) 앞에 Sony SysInfo 실크 비트 확인. 없으면 아무것도 바꾸지 않고 `no Sony virtual silk`(결과 0xEE, 자동 시작 기록 #10에도 남음). NR70V에서는 통과해야 하므로 평소처럼 켜지면 정상.
- SDK 확인(2026-10-21): NR70V용은 Sony CLIÉ SDK 3.0(`'rdwd'` Redwood). SDK 5.0 `SonySlkw.h`의 실크 플러그인 이벤트 번호가 Mn 가상 키보드 역분석 번호와 일치(정황, 구조 배치는 다름). Palm OS 4 공개 후보: `sysNotifyDeleteProtectedEvent`('-pdb', 보호된 DB 지우기 직전 알림 — 런처 삭제 때 자동 끄기 후보), `MemPtrHeapID`·`MemNumRAMHeaps`(ROM 판정 후보).
- ClieDiag v26 (진단): Info 끝에 ROM 판정 — `card0 rom/ram 크기, heaps RAM수/전체수`, 표본 주소(SysReset·BltDrawChars·EvtGetEvent·FntCharsWidth 맨 위, MemPtrNew 덩어리)마다 `MemPtrHeapID`·`MemHeapFlags`·판정(힙 번호 ≥ RAM 힙 수 → ROM). 고정 범위(0x10000000~0x107FFFFF)를 공개 함수로 바꿀 수 있는지 확인용. **실측(2026-10-21): ClieDiag 시작 때 Fatal Exception** → `MemPtrHeapID`는 할당된 메모리 덩어리 포인터용이라 코드 주소(ROM·패치 진입점)에는 쓸 수 없는 것으로 판단. ROM 판정은 고정 범위 유지(ARCHITECTURE 10절 3단계 그대로). ClieDiag v27에서 이 진단을 뺌.
- 진단·배포 빌드 분리(2026-10-21): `src/common/diag.h`(`MN_DIAG` 기본 1, `MN_D(x)`). 감싼 뒤 진단 빌드 코드 리소스 13개가 이전과 바이트 단위로 같음(동작 변화 없음 확인). 배포 빌드(`DIAG=0`, `build/release/`) 코드 크기: MnHangul 그리기 7028→4860·폭 3584→2256·Intl 1704→1204, MnSonyKbd 키 글자 3008→2488·입력 11888(12384에서), MnSkCore 29908→29736바이트. 배포 빌드 에뮬레이터 시험은 배포 시점에.
- MnHangul v65(진단): 지우기·HotSync 알림 수신 확인. 그리기 패치를 켤 때 `'-pdb'`(sysNotifyDeleteProtectedEvent)·`'hots'`(sysNotifySyncStartEvent) 등록, Patch OFF 때 해제(보호 해제 전). 받으면 횟수만(#160 '-pdb' 수, #161 그 DB 크리에이터, #162 'hots' 수, #163 등록 결과). 진단 첫 화면 11줄 `ntf r… -pdb N 크리에이터 hots N`. 끄는 동작 없음. 실측(2026-10-21): 에뮬레이터 덮어쓰기 설치는 `-pdb 0`(에뮬레이터가 Palm OS 밖에서 거부, 앱이 끼어들 수 없음), 런처 지우기는 `-pdb 2 MnHg`(한 번에 2번 옴, "Unable to delete MnHangul.").
- MnHangul v66: 런처 지우기 자동 끄기. '-pdb'가 크리에이터 MnHg DB(앱·24 글꼴)에 대해 오면 모두 맨 위인지 먼저 확인하고 All OFF와 같은 순서로 끈다(전역 없는 `Uninst*`, 버튼의 끄기도 같은 함수를 씀). 하나라도 위에 다른 패치가 있거나 Count가 켜져 있으면 아무것도 끄지 않는다(지우기 거부). 결과 #164(1 끔, 2 위에 다른 패치, 3 Count, 4 켜진 것 없음). 알림 등록은 '-pdb' 처리 중에 풀지 않고 앱 지우기 직전 'gbye'(sysNotifyCleanupEvent)에서 푼다. 업데이트 설치는 All OFF 그대로. 실측(2026-10-21): 켜진 채 런처 지우기 → 지워짐. 런처는 같은 크리에이터 DB를 모두 지우므로 24 글꼴(MnHgFont24, 크리에이터 MnHg)도 함께 지워짐 — 알림은 보호된 DB마다 1번(v65의 2번 = 앱 + 24 글꼴). 다시 설치 뒤 MnSonyKbd가 위에 있을 때 지우기 → `off2`, 거부되고 한글 표시 유지. 24 글꼴 다시 설치 → Patch ON → 정상.
- MnSonyKbd v18: 런처 지우기 자동 끄기(MnHangul v66과 같은 방식). 키 글자 패치를 켤 때 '-pdb' 등록, MnSy DB 알림이면 입력·키 글자 모두 풀 수 있는지(맨 위 또는 이미 빠짐) 확인 후 둘 다 끔, 하나라도 위에 다른 패치면 아무것도 안 끔. 'gbye'·OFF 때 등록 해제. 기록 #200~#203, 진단 화면 10줄 `ntf … -pdb N 크리에이터 offN`. 알림 경로 함수는 빌드 때 %a5 검사에 추가.
- MnHangul v67 / MnSonyKbd v19: 지우기 거부 때 맨 위 패치 판정(`src/common/top_who.h`: 맨 위 주소가 상대 모듈의 잠긴 코드 리소스 범위 안인지 Feature 코드 주소 + `MemHandleSize`로, 아니면 `dCJK` 있으면 CJKOS, 그 밖 알 수 없음). 다음에 앱을 열면 첫 화면 메시지 `can't delete: <누구> on top` 한 번(MnHangul #165·#166, MnSonyKbd #204·#205), 진단 줄 `off2H/S/C/?`. 빌드 %a5 검사는 함수의 마지막 `rts` 뒤(다음 함수의 문자열 상수)를 빼도록 고침(`rts`가 없으면 전체) — MnHangul도 알림 경로 함수 검사 추가.
- KbdScan v1 (진단, 다른 기종용, 크리에이터 MnKd, `src/kdiag/`, `tools/build-kdiag.sh`): N700C(일본어 OS 3.5) 등에서 한글 표시·입력 가능성 판단. 시작 때는 시스템 Feature(ROM 버전·인코딩·언어·국가·Intl·Notify·OEM)와 트랩 존재(BltDrawChars·WinPaintChars)만 읽음. `[Hook ON]`을 눌렀을 때만 SysKeyboardDialog(·V10)·WinDraw/PaintBitmap·WinDraw/PaintChars·BltDrawChars(세기만)·WinCopyRectangle·EvtGetEvent·FldInsert·EvtEnqueueKey를 기록만 하는 코드(KdHk 0)로 바꾸고, 키보드 화면이 열려 있는 동안의 호출·그림 크기·글자·키 이벤트·입력칸 삽입을 기록. 기종에 없는 함수는 걸지 않음. SDK 3.5 헤더 확인: `IntlSetRoutineAddress`·`sysNotifyDeleteProtectedEvent`는 없음(SDK 4부터) → OS 3.5에서는 MnHangul 글자 경계 처리·지우기 자동 끄기 방식을 그대로 쓸 수 없음.
  - KbdScan 실측(2026-10-21) N700C(`rom 3523000 enc 5 lang 6 ctry 13 oem sony ysmt`): 일본어 키보드 화면도 SysKeyboardDialog(kbdDefault)로 열림, 특수 키만 작은 그림(9~13×7~11), 키 글자·입력칸 글자는 WinDrawChars, 키는 EvtEnqueueKey → keyDown(FldInsert 0). 메뉴로 연 `abc`는 로마자 일본어 입력(FEP 밑줄), Graffiti `a`로 열면 영문(⇧ 표시). T600C(`rom 4103000 enc 3 lang 0 ctry 23 oem sony mdna`, 영문 4.1): 표준 영문 키보드, 키 글자 WinDrawChars, 키는 keyDown(`52 6b 73` — 첫 글자 R은 자동 대문자).
- 견본 메모(`tools/gen-memo-sample.py`, 완성형 2,350자만): T600C에서 MnHangul v67 수정 없이 한글 표시·글자 단위 BS·줄바꿈·키보드 화면 입력칸 표시 정상. N700C에서는 MnHangul 화면이 중간부터 그려지지 않음.
- MnHangul v68: 지원 기종 확인(`HgSupported`: ROM 4.0 이상 + 인코딩 PalmLatin 3, 공개 FtrGet). 아니면 Hangul ON·자동 시작(#95 = 0x10000000)·Intl·Count 켜기·고해상도 전환을 하지 않음.
- MnKbdHan v1 (진단, 크리에이터 MnKh, `src/kbdhan/`, `tools/build-kbdhan.sh`): Sony 가상 실크가 없는 OS 4 영문 기종(T600C 등)의 시스템 키보드 화면 한글. v1은 표시만 — SysKeyboardDialog가 열려 있는 동안 WinDrawChars의 영문 한 글자를, 한글 모드이면 두벌식 자모(0xA4 xx, MnHangul이 그림)로 바꿔 키 가운데에 그림. 두벌식 표는 `src/common/dubeol_map.h`로 옮겨 MnSonyKbd와 공용(MnSonyKbd 코드 리소스 바이트 단위 같음 확인). v1 실측(T600C): 자모 표시는 되지만 한 박자 늦음 — 키보드는 cap을 누르면 키 글자를 먼저 다시 그리고 Graffiti caps를 나중에 바꿈(첫 cap → 영문 대문자, 둘째 cap → 소문자 자리에 쌍자음). v2: 키보드 화면이 열려 있는 동안 EvtGetEvent에서 펜 누름이 cap 키(추정 x 0~22, y 107~121)이면 한글 모드를 바꾸고, 열 때는 Graffiti caps로 맞춤. 진단 줄에 펜 위치 3개. v2 실측: cap taps 0 — 펜·글자 좌표는 키보드 화면 창 기준(z~m 줄 글자 y 95)인데 화면 좌표로 잡았음. v3: cap 키 x 0~21, y 89~105. v3 실측: cap 판정은 맞으나 Palm 키보드는 cap 켜진 채 shift를 누르면 cap이 꺼짐 → v4: PC 두벌식 규칙(소문자 = 기본 자모, 대문자 = 쌍자음), 한/영은 Int'l 오른쪽 빈칸(x 110~131, y 121~141, 누름은 nilEvent로 삼킴)과 '한'/'A' 표시. 전환 직후 다시 그리기는 이번 화면에서 그린 키 글자 위치(최대 40)를 기억해 지우고 우리가 그림(키보드를 다시 그리게 할 공개 방법 없음). v4 실측: 키 글자 26개 기억 정상, '한'/'A' 표시는 키보드 화면 그리기에 덮임 → v5: 키 글자를 그린 뒤 첫 이벤트에 표시. v5 실측(T600C): A ↔ 한 전환, 소문자 = 기본 자모, shift = 쌍자음 정상. 표시 자리 지우기가 space 키 아래 테두리를 끊음 → v6: y 123부터 지움. v6 실측: 키보드 화면이 열린 동안 Edit 메뉴의 명령 글자(/U /X /C /P /S)도 자모로 그려짐(입력칸 한 글자 줄도 같은 위험) → v7: 그리는 창이 지금 폼 창이고 y 60~100(키 세 줄)일 때만 바꿈. 실측: shift 뒤 한/영 전환 때 쌍자음이 보였다가 기본 자모로 바뀜 — 키보드가 글자를 가운데 맞춤해 q·Q의 x가 달라 표에 한 키가 두 번 들어감 → v8: 같은 줄 x 4 이내면 같은 키로 덮어쓰고, 지우는 폭은 대·소문자 영문·자모 중 가장 넓은 것. v8 실측: 표시 정상, 새 메모에서 열면 자동 대문자(auto-shift)로 쌍자음 배치. v9: 두벌식 조합 입력(한글 모드, 키보드 화면 안, keyDown → hangul_compose.h, 결과는 EvtEnqueueKey keyCode 'MK', 입력칸 펜 누름·커서 이동·기호에서 조합 끝, BS는 자모 단위) + 한글 모드에서 열 때·전환 때 자동 대문자 풀기(GrfSetState). 빌드: 코드 리소스 %a5 검사를 `objdump | grep -q` 파이프 대신 파일로(pipefail에서 grep이 먼저 끝나면 SIGPIPE로 검사가 거짓 통과할 수 있음 — 다른 빌드 스크립트의 같은 패턴은 파일 방식으로 다시 검사해 지금 코드에 %a5 없음 확인). v9 실측(2026-10-21, T600C 영문 4.1 + MnHangul v68): 키보드 화면에서 두벌식 조합 입력 정상 — `대한민국`·`뽐뿌`(shift 쌍자음)·영문 섞어 쓰기, Done 뒤 메모 반영.
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
