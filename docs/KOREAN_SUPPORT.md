# 한글 지원과 CJKOS

> 검토 문서. 아래 대체안은 **검토안**이며 구현·검증된 구조가 아니다. (기준 커밋 `117932a`)

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
CJKOS를 켰을 때 ROM 밖을 가리키는 시스템 트랩 33개·Intl 선택자 18개, **CJKOS를 끄면 0개**.
즉 아래는 모두 CJKOS가 바꾼 것이다(이 기기에는 다른 RAM 패치가 없다). 코드는 `0x0034xxxx`(트랩 31·Intl 18)와 `0x000F3xxx`(`FtrGet`, `IntlDispatch`) 두 곳.

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

최소 교체 범위(검토): 트랩 약 10개 + Intl 약 13개. CJKOS가 실제로 한 방식이 이것이라는 뜻이며, 같은 범위로 같은 호환성을 얻는지는 구현 후 검증한다.

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
- 2a(통과 시험, v1): 패치가 인자를 feature `MnHg`에 기록하고 원래 함수를 그대로 부른다. 화면이 바뀌지 않아야 정상.
- 진행 순서(디버그 먼저): ① 바뀐 함수 조사(**완료**, 실측 1) ② `BltDrawChars` 하나만 교체해 표시 시험(CJKOS 끈 복사본 세션)
  ③ 폭·글자 경계 ④ 글꼴 3벌(16 → 12 → 24).
