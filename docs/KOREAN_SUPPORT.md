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
- 교체 후보: 그리기(`WinDrawChars`, `WinEraseChars`, `WinInvertChars`, `FntCharsWidth`, `FntWordWrap` …),
  글자 경계(`TxtGetNextChar`, `TxtGetPrevChar`, `TxtCharBounds`, `TxtByteAttr` …), NR70V 고해상도 그리기 경로(HR 라이브러리).
- 교체 시점: 리셋 알림(`sysAppLaunchCmdSystemReset`) 또는 부팅 때 실행되는 SkWr 로더의 Open.
- 글꼴: ① 완성형 2,350자 비트맵(저해상도 12px 약 42KB, 고해상도 24px 약 170KB) ② 벌식 조합형(수 KB~10KB대, 11,172자). 자유 라이선스 글꼴 또는 자체 제작.
- 진행 순서(디버그 먼저): ① ClieDiag에 "바뀐 트랩 목록"(ROM 밖을 가리키는 트랩) 페이지 → CJKOS가 실제로 바꾼 함수 확인
  ② `WinDrawChars` 하나만 교체해 표시 시험(CJKOS 끈 복사본 세션) ③ 글자 경계 ④ 고해상도 경로·글꼴.
