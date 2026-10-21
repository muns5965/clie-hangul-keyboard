/*
 * MnHangul v63 — CJKOS 대체 한글 모듈 시험 앱 (2a~2c 그리기, 글꼴 덤프·크기 조사, 3단계 호출 조사, 3a 글자 경계, 3b 폭)
 *
 * 같은 PRC 안의 패치 코드 리소스(HgPt 0, src/hangul/patch.c)를 시스템 트랩 BltDrawChars(0xA378)에 끼운다.
 *   켜기: 리소스 잠금 → 원래 함수 주소를 리소스 오프셋 0에 기록(DmWrite) → DB 삭제 보호
 *         → SysSetTrapAddress(0xA378, 리소스+4)
 *   끄기: 트랩이 아직 우리 진입점이면 원래 주소로 되돌림 → 보호 해제 → 잠금 해제
 * 시험 기간에는 부팅 때 자동으로 켜지 않는다. 문제가 생기면 리셋 한 번으로 원래대로 돌아간다.
 * 반드시 CJKOS를 끈 상태(같은 함수를 CJKOS도 바꿈)의 CloudpilotEmu 백업 복사본에서 시험한다.
 *
 * 화면: 상태, [Patch ON] [Patch OFF] [Box ON/OFF] [Clear counters], 패치 기록(feature 'MnHg', patch.c 참고).
 * Box: 패치가 켜진 동안 2바이트 한글을 "[]"로 바꿔 그린다(#22). Patch ON은 기록만 한다.
 * 위/아래 버튼 또는 화면 탭으로 다시 그린다(기록 새로고침).
 * v2: 고해상도 모드(ClieDiag v19와 같은 방식: HR 라이브러리 HRWinScreenMode 320x320, 끝날 때 복귀).
 *     맨 아래 메시지 줄이 화면 밖으로 잘리던 것 수정.
 * v3: Box 버튼, 글꼴(fontType/fRectHeight)·p3·p1 기록 표시(저해상도/고해상도 좌표 구분 단서).
 * v4: 아래 버튼(PageDown)으로 Font 페이지. 마지막 호출 p6(#3)과 마지막 한글 호출 p6(#18)의 글꼴 머리 13워드
 *     (fontType firstChar lastChar maxWidth kernMax nDescent fRectWidth fRectHeight owTLoc ascent descent
 *     leading rowWords)와 SDK 구조로 계산한 표 위치, 'A'·0xC7의 오프셋/폭 항목을 보인다(2c 임시 글꼴 설계용).
 *     위 버튼(PageUp)으로 Main 페이지.
 * v5: 메시지 줄이 맨 아래 기록 줄(12)과 겹치던 것 수정(빈 줄 1로 옮김). 맨 위 제목 줄을 탭해도 페이지 전환.
 * v6: Box 버튼 → Mode 버튼(off → box → glyph 순환, #22 = 0/1/2). Patch ON 때 글자 데이터 리소스 HgFn 0·1
 *     (Neo둥근모 16×16 완성형, 시험용)을 잠가 포인터를 #23·#24, 핸들을 #25·#26에 둔다(Patch OFF 때 해제).
 *     버튼 오른쪽에 한글 견본(기존 함수 WinDrawChars, 고해상도 모드 = 배율 2 경우)을 그린다.
 * v7: 패치가 글꼴 높이 16~40에서 한글을 그림. Sizes 페이지(#30~#37: 글꼴 높이·사각형 높이별 한글 호출 수).
 *     제목 줄 탭 = Main → Font → Sizes → Main, 아래 버튼 = 다음 페이지, 위 버튼 = Main.
 * v8: Calls 페이지(3단계 조사). 2번째 줄 탭 = Count ON/OFF. 켜면 폭 함수 트랩 8개와 Intl 루틴 16개에
 *     통과 코드(리소스 HgCt 0, src/hangul/count.s)를 끼워 호출 수만 센다(동작은 그대로).
 *     트랩은 SysSetTrapAddress, Intl 루틴은 IntlSetRoutineAddress(선택자 29)로 바꾼다.
 *     끄기: 아직 우리 코드를 가리키는 것만 원래대로. 기록 #40 핸들, #41 횟수 블록, #42 코드 주소.
 *     Main의 Clear counters가 횟수도 0으로 만든다. 페이지: Main → Font → Sizes → Calls.
 * v9: 3a 글자 경계. Main 5번째 줄 오른쪽 [Intl ON/OFF] = Intl 루틴 6개(CharAttr, PrevChar, NextChar, SetNext,
 *     CharBounds, CharWidth)를 리소스 HgIt 0(src/hangul/intl.c)의 EUC-KR 판으로 교체(IntlSetRoutineAddress).
 *     기록 #45 핸들, #46 코드 주소. 글자 데이터에 자모 행 HgFn 2 추가(#43 포인터, #44 핸들, Patch ON 때 잠금).
 * v10: 3b 폭. Main 4번째 줄 오른쪽 [Width ON/OFF] = 폭 함수 트랩 5개(FntCharsWidth, FntLineWidth, FntCharsInWidth,
 *     FntWordWrap, FntWidthToOffset)를 리소스 HgWd 0(src/hangul/width.c)으로 교체. 기록 #47 핸들, #48 코드 주소.
 *     첫 줄에 prot N = 지금 켜진 기능 수(각각 DB 삭제 보호 1개). 0이어야 MnHangul을 덮어써 설치할 수 있다.
 * v11: Width 페이지(폭 함수 진단: 함수별 호출 수·한글 경로 수, 마지막 한글 폭·배율·글꼴 높이, #50~#63).
 *     Main 3번째 줄 오른쪽 [All OFF] = Width → Intl → Count → Patch 순서로 모두 끈다(업데이트 설치 전).
 *     페이지: Main → Font → Sizes → Calls → Width.
 * v12: 한글 폭 수정(width.c·intl.c). 폭을 잴 때 저해상도 글꼴(높이 8~15)이면 9 — 그릴 때 2배 고해상도라서.
 * v13: 글자 데이터 다시 생성(tools/gen-hangul16.py: 글꼴 격자에서 직접 그림). 조각이 겹치는 곳의 두꺼운 획 수정.
 * v14: 완성형 글자 데이터(HgFn 0~2, 78KB)를 조합형 묶음으로 교체: HgJb 0 = Neo둥근모 16×16(14KB),
 *     HgJb 1 = 물마루 기반 12×12(23KB, 칸 높이 12~15에서 사용). tools/gen-johab.py가 11,172자 전부 원본과 같음을 확인.
 * v63: 판 번호를 kHgVer 한 곳에(v61·v62는 사용자 첫 화면 제목이 v60으로 남아 있었음).
 * v62: CJKOS 감지는 'dCJK' Feature만(함수 4개 방식 뺌, v61에서 일치 확인). 진단 첫 화면 6줄 오른쪽 [Update](3초) 추가.
 * v61: CJKOS 감지 = 'dCJK' Feature(FtrGet, 공개 함수). 시험 기간에는 함수 4개 방식과 OR, KDlg 9줄에 둘 다 표시.
 * v60 (진단, 동작 변경 없음): KDlg 7~9줄 = FtrGet 시스템 Feature 원시값(인코딩·인코딩 플래그·언어·국가·TSM 플래그)과 현재 CJKOS? 판정.
 * v59: 진단 첫 화면 제목의 옛 고정 글귀 "CJKOS OFF" 뺌(실제 상태는 사용자 화면 CJKOS? 표시).
 * v58: Hangul ON 반복 먹통 수정 — 켜짐 판정을 "연결 안에 있음"으로(BltFind), 이미 켜져 있으면 다시 끼우지 않고, 빠졌으면 맨 위에 다시 잇기.
 * v57: Update는 설치되어 있는 MnSonyKbd도 다음 리셋 한 번 자동 시작을 건너뛰게 한다(둘 다 한 번에 설치).
 * v56: CJKOS 동작 감지(CjkOn: CJKOS가 바꾸고 MnHangul은 안 바꾸는 함수 4개가 ROM 밖인가). 동작 중이면 Hangul ON·리셋 자동 시작 안 함, 첫 화면 표시.
 * v55: 첫 화면 7~8줄 = 역할(Hangul display, CJKOS replacement)과 MnSonyKbd 상태(없음/켜짐/꺼짐).
 * v54: Sony 가상 키보드 한글(입력 HgIm·한A HgIc·키 글자 오버레이·KDlg 판정 기록·Sony 설정)을 MnSonyKbd(src/sony)로 분리.
 *      MnHangul은 한글 표시(CJKOS 대체)만. 앱 설정 1번(Sony)은 쓰지 않는다.
 * v53: 첫 화면 [Update (hold 3s)] = 다음 리셋 한 번 자동 시작 건너뛰기(설정 3번) + 리셋 → 잠금 없이 바로 설치. 업데이트 = Update → 설치 → Hangul ON.
 * v52: 자모 오버레이는 ROM BltDrawChars(#152)로 직접 그림(CJKOS 아래에서 획 빠짐). KDlg 2줄 = blt below / jamo 주소.
 * v51 (시험): KDlg 10줄 [Sony only] = 그리기 패치(글자 모드 off) + 자모 오버레이 + Sony 입력, Intl·Width 없음 — CJKOS와 같이 쓰기 시험.
 * v50: 펜 표시 3은 키 영역(y<213)만, v49 영문 규칙은 키 누름(3) 동안 적용 안 함(⇪+쉬프트 입력 뒤 qwerty 줄 영문 문제).
 * v49: 소문자 키 글자 + ⇪ 꺼짐이면 영문(키보드 처음 띄울 때 1~2줄 쌍자음 문제).
 * v48: 가상 키보드 일반 키 누름(펜 표시 3) 동안은 v44 영문 내림을 하지 않음(누른 키만 영문으로 남던 문제).
 * v47: ⇪ 그림은 직전 키 글자가 2줄 끝(\ 또는 |)일 때만 판정에 씀(눌림 표시 반전 그림이 앱보다 먼저 그려져 기준값이 틀어졌음).
 * v46 (진단): KDlg 7~10줄 = 판정 순간 기록(키 글자 판정 값이 바뀔 때, ⇪ 그림마다). 2줄만 틀리는 원인 확인용.
 * v45: ⇪ 펜 누름은 한글 모드를 뒤집지 않고 'Sony 실제 상태(#150, 자판 전체 그리기의 ⇪ 그림)의 반대'로 정함.
 * v44: ⇪ 판정 보완 — ⇪ 누른 직후(키 글자 그리기 전) 눌림 표시 그림은 판정에서 뺌, 그리는 순간 ⇪ 꺼짐 + 일시 쉬프트이고
 *      마지막 펜이 ⇪가 아니면 영문(새 메모), 앱 화면 누름도 '마지막 펜 ⇪ 아님'.
 * v43: Sony ⇪ 키 그림을 '한A'로(HgIc 0·1) 바꿔 그리고, 그 그림(보통/반전)으로 한글 모드 판정(새 메모에서 표시는 한글·입력은 A 문제).
 *      Sony 한글 코드(HgIm 0)가 WinDrawBitmap도 바꾼다. KDlg 11줄 ic = ⇪ 그림 횟수 @ 그때 글자 그리기 수, d = 지금 수.
 * v42: 사용자 화면 이름 — Hangul ON / Hangul OFF / Start at reset / Sony keyboard, 한 줄에 하나씩(길어서).
 *      Start at reset(앱 설정 0번)·Sony keyboard(1번)는 저장한 적 없으면 ON(처음 설치 → Hangul ON 한 번이면 끝).
 * v41: 기본 Main은 사용자용(Patch ON·All OFF, Auto·Sony, Soft Reset, 사용법, [ ] Debug).
 *      Debug 체크(앱 설정 2번)일 때만 지금까지의 진단 Main과 진단 페이지(제목줄 탭·위아래 버튼).
 * v40: Main 줄 6 오른쪽 [Sony: ON/off] — Sony 가상 키보드 한글(오버레이 + 입력)을 첫 화면에서 설정(앱 설정 1번, 저장형).
 *      Patch ON·Auto(리셋 뒤 자동 시작)가 이 설정이면 Sony 한글도 켠다(InstallAll 16~19비트).
 * v39: 오버레이 한글 판정을 ⇪ 키 펜 누름으로(ime.c가 #143 한글 모드 전환, patch.c가 사용). ⇪ 끄기 표시 늦음 해결.
 *      KDlg 11줄 H = 한글 모드, p = 마지막 펜이 ⇪(1·2).
 * v38: SetSt에 가상 실크 영역 펜 누름·뗌(앱이 받는 순간) 기록 — ⇪·쉬프트 중 무엇을 눌렀는지 미리 알기 위함.
 * v37: 진단 SetSt 페이지(KDlg 다음) — Hooks가 GrfSetState·GsiSetShiftState·WinCopyRectangle(가상 실크 창)도 기록.
 *      키 글자 다시 그리기와 상태 변화의 순서, 화면 복사 여부 확인용(오버레이 한 박자 늦음 해결).
 * v36: 오버레이 판정 보완 — 키 글자 대·소문자와 Graffiti 상태를 함께 봄(⇪를 켜자마자 자판 전체가 자모).
 *      KDlg 6~9줄: 그리는 순간 판정 입력 변화 기록(⇪+쉬프트 표시 판정용).
 * v35: Sony 가상 키보드 한글 — KDlg [Sony Hangul]이 오버레이와 입력(HgIm 0, EvtGetEvent)을 함께 켠다.
 *      쉬프트 고정(⇪) = 한글 모드. 대문자 = 기본 자모, ⇪+쉬프트(소문자) = 쌍자음·ㅒㅖ. 조합은 MnSkCore와 같은 방식.
 *      ⇪ 꺼지면 오버레이·변환 없음(영문). All OFF가 함께 끈다.
 * v34: KDlg 진단 — 글자 키만 최근 4개, 넣는 순간(add)과 받는 순간(get)의 Graffiti 상태를 나란히(⇪+쉬프트 쌍자음과 영문 소문자 구별용).
 * v33: KDlg 진단 — EvtAddEventToQueue keyDown 최근 4개를 한 줄에 하나씩, 넣는 순간 Graffiti 상태(g = caps num temp auto).
 * v32: KDlg 진단에 EvtGetEvent keyDown(글자·modifiers·keyCode·Graffiti caps/num/temp/auto) 최근 4개 표시,
 *      EvtAddEventToQueue keyDown 기록. %c 표시 문제(Palm StrPrintF) 없앰.
 * v31: EvtEnqueueKey 최근 8개 기록을 kbd.c에 실제 반영(v30은 앱만 바뀌어 표시가 깨졌음), 오버레이 조건 완화
 *      (비트맵 폭 검사 뺌, 사각형 높이 22·11 아니면 배율 1), 걸러진 이유 표시(줄 11).
 * v30: KDlg 페이지 [Overlay] - Sony 가상 키보드 키 글자(영문)를 두벌식 자모로 바꿔 그림(patch.c, #126, #127).
 *      표시만 바꾸고 입력은 아직 영문. EvtEnqueueKey 기록은 최근 8개(부른 모듈 표시).
 * v29: KDlg 진단에 가상 실크(활성 창 아닌 창)에 찍히는 글자 기록(patch.c, #108~#121)과
 *      EvtEnqueueKey 호출 기록(HgKd 2, #122~#125) 추가. Sony 가상 키보드 한글 오버레이·변환 가능성 판단용.
 * v28: 진단 KDlg 페이지 — SysKeyboardDialog(·V10)를 기록만 하는 코드(HgKd 0)로 교체해 화면 키보드가 어디서 불리는지 확인.
 *      켜기/끄기는 이 페이지에서만(Patch ON·Auto와 무관), All OFF가 함께 끈다.
 * v27: 리셋 버튼을 누르는 동안 폭 고정("Hold... 3s"), boot 줄에서 bdc 주소를 빼 a(결과) 값이 다 보이게
 * v26: Main 줄 6에 [Soft Reset (hold 3s)] — 3초 누르면 SysReset(), 중간에 떼면 취소. last len 줄은 뺐다(calls 줄이 7로)
 * v25: 부팅 자동 시작 — [Auto] 설정(앱 설정 'MnHg' 0, 리셋 후에도 유지)이 on이면 리셋 알림 때 Patch ON과 같은 설치
 *      (패치·글꼴 12/16 잠금, 24 폰트 파일 연결, Mode glyph, Intl 6, Width 5). 결과 #95. 리셋 알림 중에는 앱 전역변수·상수 표·
 *      문자열 상수를 쓸 수 없으므로 설치 함수(Inst*)는 결과 코드만 돌려주고 표 대신 switch를 쓴다. 비상: 위 버튼 + 리셋(알림 없음).
 * v24(진단): 리셋 알림(sysAppLaunchCmdSystemReset) 수신 기록만 — #96 횟수, #97 tick, #98 그때 BltDrawChars 트랩 주소,
 *      #99 = hardReset<<24 | createDefaults<<16 | 자기 DB 열기 성공(1/0). 메인 12줄에 표시. 동작 변경 없음(자동 설치는 다음 판).
 * v23: 큰 글자 칸 26 → 24(글자 사이 약 5 → 3픽셀). Patch ON이 이미 켜져 있어도 추가 글꼴 연결을 다시 시도하고,
 *      결과를 메시지로(f24 ok / f24: none / open fail / no res / bad fmt).
 * v22: 추가 글꼴 MnHgFont24.prc(타입 HgFt, 리소스 HgJb 2 = 24×24 조합형 묶음 판 2)가 있으면 Patch ON 때 잠가 연결(#92 주소,
 *      #93 핸들, #94 DB LocalID, DB 보호). 메모 큰 글자(그릴 때 30/15 ×2, 폭 잴 때 높이 15)에 24×24, 칸 폭 26(저해상도 13).
 *      Patch OFF·All OFF 때 잠금·보호를 풀어 글꼴만 바꿔 설치할 수 있게 한다.
 * v21: Sizes 아래에 글꼴 높이/번호(0 std 1 bold 2 large)별 횟수 — 폭 잴 때(#84~#87)·그릴 때(#88~#91).
 * v20: Kbd 페이지가 키보드 14단계 기록 위치(#64~#79, 다음 칸 #80)를 읽음(#60은 로더가 써서 옮김).
 * v19: Patch ON 한 번에 Mode glyph(off였으면) + Intl ON + Width ON (리셋 후 설정이 지워지므로).
 * v18: Kbd 페이지 — 키보드(MnSk 13단계) 최근 이벤트 16개(종류, event[14], 처리 후 hires, 앞 이벤트와 tick 차). 맨 윗줄 탭 = 지우기.
 * v17: 저해상도 글꼴 폭 — 현재 그리기 창이 offscreen(windowFlags 0x4000)이면 13(12×12 ×1), 아니면 9(16×16 ×2).
 * v16: Win 페이지 — 한글 폭 잴 때·그릴 때의 현재 그리기 창 4개(주소, 비트맵 폭·rowBytes, windowFlags, 창 폭,
 *      본 경우 W=폭 잼 1=그림×1 2=그림×2 ?=기타, #72~#83). 폭 잴 때 ×1/×2 구별 단서 찾기(src/hangul/winlog.h).
 * v15: 11칸에도 12×12(한글은 위 11줄 안). Sizes 페이지에 크기별 마지막으로 그린 앱 크리에이터(#64~#71; #40~#48은 핸들이라 피함).
 */

#include <PalmOS.h>

#define kHgVer "v63"   /* 판 번호는 여기 한 곳만 고친다(모든 화면 제목이 이 값을 씀) */
#define HgTitle(rest) WinDrawInvertedChars("MnHangul " kHgVer rest, sizeof("MnHangul " kHgVer rest) - 1, 0, 0)

#define kHgCreator    'MnHg'
#define kPatchRes     'HgPt'
#define kTrapBlt      0xA378
#define kFtrHandle    20
#define kFtrEntry     21
#define kFtrMax       19
#define kFtrBox       22      /* 모드: 0 off, 1 box, 2 glyph */
#define kFtrGlyph0    23
#define kFtrGlyph1    24
#define kFtrGlyphH0   25
#define kFtrGlyphH1   26
#define kGlyphN       2        /* v14: HgJb 0 = 조합형 16×16(Neo둥근모), HgJb 1 = 조합형 12×12 */
#define kFtrIntlH     45
#define kFtrIntlC     46
#define kIntlRes      'HgIt'
#define kIntlN        6
#define kFtrWidthH    47
#define kFtrWidthC    48
#define kWidthRes     'HgWd'
/* v28: 키보드 대화상자 진단(HgKd 0) — #100 핸들, #101 코드, #102~#105 기록(kbd.c) */
#define kFtrKbdH      100
#define kFtrKbdC      101
#define kKbdRes       'HgKd'
#define kKbdN         8   /* v37: + GrfSetState, GsiSetShiftState, WinCopyRectangle.  v29: + EvtEnqueueKey, v32: + EvtGetEvent, EvtAddEventToQueue */
#define kFtrKbdBuf    107 /* v29: EvtEnqueueKey 기록 버퍼(동적 힙, 소유자 0) */
#define kKbdBufSz     480 /* v37: + 상태 변화·화면 복사 16개.  v33: EvtAddEventToQueue도 8개.  v30: EvtEnqueueKey 8개, v32: + EvtGetEvent keyDown 8개, EvtAddEventToQueue (kbd.c 참고) */
#define kWidthN       5
/* v25: 리셋 알림(전역 없음)에서도 쓰므로 상수 표 대신 switch */
static UInt16 WidthTrap(UInt16 i)
{
    switch (i) {
    case 0:  return sysTrapFntCharsWidth;
    case 1:  return sysTrapFntLineWidth;
    case 2:  return sysTrapFntCharsInWidth;
    case 3:  return sysTrapFntWordWrap;
    default: return sysTrapFntWidthToOffset;
    }
}

static UInt16 IntlSel(UInt16 i)
{
    switch (i) {
    case 0:  return intlTxtCharAttr;
    case 1:  return intlTxtGetPreviousChar;
    case 2:  return intlTxtGetNextChar;
    case 3:  return intlTxtSetNextChar;
    case 4:  return intlTxtCharBounds;
    default: return intlTxtCharWidth;
    }
}
#define kGlyphPtrFtr(i) (23 + (i))
#define kGlyphHFtr(i)   (25 + (i))
#define kFtrGlyphCnt  27
#define kFtrRatio     28
#define kGlyphRes     'HgJb'

#define kLineH        11
#define kRowOn        2
#define kRowOff       3
#define kRowBox       4
#define kRowClear     5
#define kRowReset     6   /* v26: Soft Reset (hold 3s) */
#define kUpdX         112 /* v62: 진단 첫 화면 6줄 오른쪽 [Update] */
#define kResetHoldSec 3

static Char   gMsg[40] = "-";
static UInt16 gPage = 0;   /* 0 Main, 1 Font, 2 Sizes, 3 Calls, 4 Width, 5 Win, 6 Kbd, 7 KDlg */
#define kPages 9

/* 3단계 호출 조사 대상 (count.s의 StubN 순서와 같다): 0~7 트랩, 8~23 Intl 선택자 */
#define kCountN      24
#define kCountTraps  8
#define kCountRes    'HgCt'
#define kFtrCountH   40
#define kFtrCountP   41
#define kFtrCountC   42
static const UInt16 kCountId[kCountN] = {
    sysTrapFntCharWidth, sysTrapFntCharsWidth, sysTrapFntCharsInWidth, sysTrapFntLineWidth,
    sysTrapFntWordWrap, sysTrapFntWordWrapReverseNLines, sysTrapFntWidthToOffset, sysTrapFldWordWrap,
    intlTxtByteAttr, intlTxtCharAttr, intlTxtCharXAttr, intlTxtCharSize,
    intlTxtGetPreviousChar, intlTxtGetNextChar, intlTxtGetChar, intlTxtSetNextChar,
    intlTxtCharBounds, intlTxtWordBounds, intlTxtCharEncoding, intlTxtStrEncoding,
    intlTxtCharIsValid, intlTxtCharWidth, intlTxtGetTruncationOffset, intlTxtGetWordWrapOffset
};
static const Char *kCountName[kCountN] = {
    "CharW", "CharsW", "CharsInW", "LineW", "WordWrap", "WWrapRevN", "W2Offset", "FldWWrap",
    "ByteAttr", "CharAttr", "CharXAttr", "CharSize", "PrevChar", "NextChar", "GetChar", "SetNext",
    "CharBnds", "WordBnds", "CharEnc", "StrEnc", "IsValid", "CharWid", "TruncOfs", "WWrapOfs"
};

/* 고해상도 모드 (소니 헤더 없이 필요한 트랩만 선언, sysLibTrapCustom + 순번) */
Err HROpen(UInt16 ref)  SYS_TRAP(sysLibTrapOpen);
Err HRClose(UInt16 ref) SYS_TRAP(sysLibTrapClose);
Err HRWinScreenMode(UInt16 ref, WinScreenModeOperation op, UInt32 *w, UInt32 *h,
                    UInt32 *depth, Boolean *color) SYS_TRAP(sysLibTrapCustom + 48);
static UInt16 gHrRef = 0xFFFF;

static void StartHires(void)
{
    UInt16 ref;
    UInt32 w = 320, h = 320;

    if (SysLibFind("Sony HR Library", &ref) != errNone || HROpen(ref) != errNone)
        return;
    if (HRWinScreenMode(ref, winScreenModeSet, &w, &h, NULL, NULL) != errNone) {
        HRClose(ref);
        return;
    }
    gHrRef = ref;
}

static void StopHires(void)
{
    if (gHrRef == 0xFFFF)
        return;
    HRWinScreenMode(gHrRef, winScreenModeSetToDefaults, NULL, NULL, NULL, NULL);
    HRClose(gHrRef);
    gHrRef = 0xFFFF;
}

static UInt32 GetF(UInt16 n)
{
    UInt32 v = 0;

    if (FtrGet(kHgCreator, n, &v) != errNone)
        v = 0;
    return v;
}

static void Row(UInt16 row, const Char *s)
{
    WinDrawChars(s, StrLen(s), 2, (row + 1) * kLineH);
}

static void Button(UInt16 row, const Char *s)
{
    RectangleType r;

    RctSetRectangle(&r, 3, (row + 1) * kLineH + 1, FntCharsWidth(s, StrLen(s)) + 6, kLineH - 2);
    WinDrawChars(s, StrLen(s), 6, (row + 1) * kLineH);
    WinDrawRectangleFrame(roundFrame, &r);
}

/* 앱 DB(card, id) */
static Boolean AppDB(UInt16 *card, LocalID *id)
{
    DmSearchStateType st;

    return DmGetNextDatabaseByTypeCreator(true, &st, sysFileTApplication, kHgCreator, true, card, id) == errNone;
}

/* v58: 그리기 패치가 연결 안에 있는가. 트랩에서 시작해 아는 패치(MnSonyKbd 'MnSy' #1, 아래 주소 = 진입점 - 4)를 따라 내려간다.
 *   1 = MnHangul을 만남, 0 = 못 만나고 ROM에 닿음(빠짐, 예: CJKOS를 켰다 끄면 ROM으로 되돌림), 2 = 모르는 패치라 알 수 없음.
 *   전역 없음(리셋 알림에서 부름) */
static UInt16 BltFind(UInt32 ours)
{
    UInt32 t = (UInt32)SysGetTrapAddress(kTrapBlt), e;
    UInt16 n;

    for (n = 0; n < 8; n++) {
        if (t == ours)
            return 1;
        if (t >= 0x10000000UL && t < 0x10800000UL)
            return 0;
        e = 0;
        FtrGet('MnSy', 1, &e);
        if (e == 0 || t != e)
            return 2;
        t = *(const UInt32 *)(e - 4);
    }
    return 2;
}

/* v58: 켜져 있음 = 연결 안에 있음(맨 위가 아니어도). v57까지는 '트랩이 바로 MnHangul'만 켜짐으로 봐서,
 *   MnSonyKbd가 위에 있을 때 Hangul ON을 다시 누르면 자기를 한 번 더 끼워 순환(무한 호출) → 먹통 */
static Boolean Installed(void)
{
    UInt32 e = GetF(kFtrEntry);

    return e != 0 && BltFind(e) != 0;
}

/* 글자 데이터 HgFn 0·1 잠금(앱이 끝나도 DB 보호 중이라 주소 유지) */
static UInt16 Lock24(void);
static UInt32 InstallAll(void);
static UInt16 AutoGet(void);
static UInt16 DebugGet(void);
static void   DrawUserMain(void);
static void   DrawCheck(UInt16 row, const Char *label, Boolean on);

/* v61: CJKOS 동작 감지. CJKOS는 켜질 때 크리에이터 'dCJK' Feature(#0, #1968)를 등록하고 끄면 지운다
 *   (ClieDiag v24 Ftr 페이지 실측 2026-10-21: 끔 254개 → 켬 256개 → 끔 254개, 나머지 같음). 공개 함수 FtrGet 하나로 판정.
 *   v62: v61 시험에서 v56의 함수 4개 방식(FntSetFont·FrmDrawForm·FldHandleEvent·StrCompare가 ROM 밖인가)과
 *   끔·켬·끔 모두 같아 함수 4개 방식은 뺐다. 리셋 알림에서도 부르므로 전역 없음 */
static Boolean CjkOn(void)
{
    UInt32 v = 0;

    return FtrGet('dCJK', 0, &v) == errNone;
}

static void LockGlyphs(void)
{
    UInt16 i;

    for (i = 0; i < kGlyphN; i++) {
        MemHandle h = DmGetResource(kGlyphRes, i);

        FtrSet(kHgCreator, kGlyphHFtr(i), (UInt32)h);
        FtrSet(kHgCreator, kGlyphPtrFtr(i), (h != NULL) ? (UInt32)MemHandleLock(h) : 0);
    }
    Lock24();
}

/* v22: 추가 글꼴(HgFt/MnHg) 리소스 HgJb 2를 잠가 #92에. DB는 닫고 잠금만 유지, 지워지지 않게 보호 */
#define kFtrF24P   92
#define kFtrF24H   93
#define kFtrF24Id  94
/* 결과: 0 연결됨(또는 이미), 1 글꼴 DB 없음, 2 열기 실패, 3 리소스 없음, 4 형식 다름 */
static UInt16 Lock24(void)
{
    DmSearchStateType st;
    UInt16            card = 0;
    LocalID           id = 0;
    DmOpenRef         db;
    MemHandle         h;
    const UInt8      *p;

    if (GetF(kFtrF24P) != 0)
        return 0;
    if (DmGetNextDatabaseByTypeCreator(true, &st, 'HgFt', kHgCreator, true, &card, &id) != errNone || id == 0)
        return 1;
    db = DmOpenDatabase(card, id, dmModeReadOnly);
    if (db == NULL)
        return 2;
    h = DmGet1Resource(kGlyphRes, 2);
    p = (h != NULL) ? MemHandleLock(h) : NULL;
    DmCloseDatabase(db);
    if (p == NULL)
        return 3;
    if (p[0] != 'H' || p[1] != 'J' || p[2] != 2 || p[3] != 24) {   /* 형식이 다르면 쓰지 않음 */
        MemHandleUnlock(h);
        return 4;
    }
    DmDatabaseProtect(card, id, true);
    FtrSet(kHgCreator, kFtrF24H, (UInt32)h);
    FtrSet(kHgCreator, kFtrF24Id, (UInt32)id);
    FtrSet(kHgCreator, kFtrF24P, (UInt32)p);
    return 0;
}

static void Unlock24(void)
{
    UInt32 h = GetF(kFtrF24H), id = GetF(kFtrF24Id);

    FtrSet(kHgCreator, kFtrF24P, 0);
    FtrSet(kHgCreator, kFtrF24H, 0);
    FtrSet(kHgCreator, kFtrF24Id, 0);
    if (h != 0)
        MemHandleUnlock((MemHandle)h);
    if (id != 0)
        DmDatabaseProtect(0, (LocalID)id, false);
}

static void UnlockGlyphs(void)
{
    UInt16 i;

    Unlock24();
    for (i = 0; i < kGlyphN; i++) {
        UInt32 h = GetF(kGlyphHFtr(i));

        FtrSet(kHgCreator, kGlyphPtrFtr(i), 0);
        FtrSet(kHgCreator, kGlyphHFtr(i), 0);
        if (h != 0) {
            MemHandleUnlock((MemHandle)h);
            DmReleaseResource((MemHandle)h);
        }
    }
}

/* 3a: Intl 루틴 6개 교체 */
/* 결과: 0 켬, 1 이미 켜짐, 2 리소스 없음 (전역 없음) */
static UInt16 InstIntl(void)
{
    MemHandle h;
    UInt8    *code;
    UInt16    i, card;
    LocalID   id;

    if (GetF(kFtrIntlC) != 0)
        return 1;
    h = DmGetResource(kIntlRes, 0);
    if (h == NULL)
        return 2;
    code = MemHandleLock(h);
    for (i = 0; i < kIntlN; i++) {
        void *o = IntlGetRoutineAddress(IntlSel(i));

        DmWrite(code, 24 + 4 * i, &o, 4);
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, true);
    FtrSet(kHgCreator, kFtrIntlH, (UInt32)h);
    FtrSet(kHgCreator, kFtrIntlC, (UInt32)code);
    for (i = 0; i < kIntlN; i++)
        IntlSetRoutineAddress(IntlSel(i), code + ((UInt32 *)code)[i]);
    return 0;
}

static void IntlOn(void)
{
    UInt16 r = InstIntl();

    StrCopy(gMsg, r == 0 ? "intl ON (6)" : r == 1 ? "intl already ON" : "intl resource missing");
}

static void IntlOff(void)
{
    UInt8  *code = (UInt8 *)GetF(kFtrIntlC);
    UInt16  i, kept = 0, card;
    LocalID id;

    if (code == NULL) {
        StrCopy(gMsg, "intl not ON");
        return;
    }
    for (i = 0; i < kIntlN; i++) {
        UInt8 *entry = code + ((UInt32 *)code)[i];

        if ((UInt8 *)IntlGetRoutineAddress(IntlSel(i)) == entry)
            IntlSetRoutineAddress(IntlSel(i), (void *)((UInt32 *)code)[6 + i]);
        else
            kept++;
    }
    if (kept != 0) {
        StrPrintF(gMsg, "intl: %u changed by other, kept", kept);
        return;
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, false);
    MemHandleUnlock((MemHandle)GetF(kFtrIntlH));
    DmReleaseResource((MemHandle)GetF(kFtrIntlH));
    FtrSet(kHgCreator, kFtrIntlH, 0);
    FtrSet(kHgCreator, kFtrIntlC, 0);
    StrCopy(gMsg, "intl OFF");
}

/* 3b: 폭 함수 트랩 5개 교체 */
/* 결과: 0 켬, 1 이미 켜짐, 2 리소스 없음 (전역 없음) */
static UInt16 InstWidth(void)
{
    MemHandle h;
    UInt8    *code;
    UInt16    i, card;
    LocalID   id;

    if (GetF(kFtrWidthC) != 0)
        return 1;
    h = DmGetResource(kWidthRes, 0);
    if (h == NULL)
        return 2;
    code = MemHandleLock(h);
    for (i = 0; i < kWidthN; i++) {
        void *o = SysGetTrapAddress(WidthTrap(i));

        DmWrite(code, 20 + 4 * i, &o, 4);
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, true);
    FtrSet(kHgCreator, kFtrWidthH, (UInt32)h);
    FtrSet(kHgCreator, kFtrWidthC, (UInt32)code);
    for (i = 0; i < kWidthN; i++)
        SysSetTrapAddress(WidthTrap(i), code + ((UInt32 *)code)[i]);
    return 0;
}

static void WidthOn(void)
{
    UInt16 r = InstWidth();

    StrCopy(gMsg, r == 0 ? "width ON (5)" : r == 1 ? "width already ON" : "width resource missing");
}

static void WidthOff(void)
{
    UInt8  *code = (UInt8 *)GetF(kFtrWidthC);
    UInt16  i, kept = 0, card;
    LocalID id;

    if (code == NULL) {
        StrCopy(gMsg, "width not ON");
        return;
    }
    for (i = 0; i < kWidthN; i++) {
        UInt8 *entry = code + ((UInt32 *)code)[i];

        if ((UInt8 *)SysGetTrapAddress(WidthTrap(i)) == entry)
            SysSetTrapAddress(WidthTrap(i), (void *)((UInt32 *)code)[5 + i]);
        else
            kept++;
    }
    if (kept != 0) {
        StrPrintF(gMsg, "width: %u changed by other, kept", kept);
        return;
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, false);
    MemHandleUnlock((MemHandle)GetF(kFtrWidthH));
    DmReleaseResource((MemHandle)GetF(kFtrWidthH));
    FtrSet(kHgCreator, kFtrWidthH, 0);
    FtrSet(kHgCreator, kFtrWidthC, 0);
    StrCopy(gMsg, "width OFF");
}

/* v28: SysKeyboardDialog 진단 (호출만 기록하고 원래 함수로) */
static UInt16 KbdTrap(UInt16 i)
{
    switch (i) {
    case 0:  return sysTrapSysKeyboardDialog;
    case 1:  return sysTrapSysKeyboardDialogV10;
    case 2:  return sysTrapEvtEnqueueKey;
    case 3:  return sysTrapEvtGetEvent;
    case 4:  return sysTrapEvtAddEventToQueue;
    case 5:  return sysTrapGrfSetState;
    case 6:  return sysTrapGsiSetShiftState;
    default: return sysTrapWinCopyRectangle;
    }
}

static void KbdOn(void)
{
    MemHandle h;
    UInt8    *code;
    UInt16    i, card;
    LocalID   id;

    if (GetF(kFtrKbdC) != 0) {
        StrCopy(gMsg, "kbd hook already ON");
        return;
    }
    h = DmGetResource(kKbdRes, 0);
    if (h == NULL) {
        StrCopy(gMsg, "kbd resource missing");
        return;
    }
    code = MemHandleLock(h);
    for (i = 0; i < kKbdN; i++) {
        void *o = SysGetTrapAddress(KbdTrap(i));

        DmWrite(code, 32 + 4 * i, &o, 4);
    }
    {
        UInt32 *buf = MemPtrNew(kKbdBufSz);

        if (buf != NULL) {
            MemSet(buf, kKbdBufSz, 0);
            MemPtrSetOwner(buf, 0);
        }
        DmWrite(code, 64, &buf, 4);
        FtrSet(kHgCreator, kFtrKbdBuf, (UInt32)buf);
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, true);
    FtrSet(kHgCreator, kFtrKbdH, (UInt32)h);
    FtrSet(kHgCreator, kFtrKbdC, (UInt32)code);
    for (i = 0; i < kKbdN; i++)
        SysSetTrapAddress(KbdTrap(i), code + ((UInt32 *)code)[i]);
    StrCopy(gMsg, "hooks ON (log only)");
}

static void KbdOff(void)
{
    UInt8  *code = (UInt8 *)GetF(kFtrKbdC);
    UInt16  i, kept = 0, card;
    LocalID id;

    if (code == NULL) {
        StrCopy(gMsg, "kbd hook not ON");
        return;
    }
    for (i = 0; i < kKbdN; i++) {
        UInt8 *entry = code + ((UInt32 *)code)[i];

        if ((UInt8 *)SysGetTrapAddress(KbdTrap(i)) == entry)
            SysSetTrapAddress(KbdTrap(i), (void *)((UInt32 *)code)[kKbdN + i]);
        else
            kept++;
    }
    if (kept != 0) {
        StrPrintF(gMsg, "kbd: %u changed by other, kept", kept);
        return;
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, false);
    {
        void *buf = (void *)GetF(kFtrKbdBuf), *z = NULL;

        DmWrite(code, 64, &z, 4);
        if (buf != NULL)
            MemPtrFree(buf);
        FtrSet(kHgCreator, kFtrKbdBuf, 0);
    }
    MemHandleUnlock((MemHandle)GetF(kFtrKbdH));
    DmReleaseResource((MemHandle)GetF(kFtrKbdH));
    FtrSet(kHgCreator, kFtrKbdH, 0);
    FtrSet(kHgCreator, kFtrKbdC, 0);
    StrCopy(gMsg, "kbd hook OFF");
}

static void ClearKbdDlg(void)
{
    UInt16 n;

    UInt32 *buf = (UInt32 *)GetF(kFtrKbdBuf);

    for (n = 102; n <= 121; n++)
        if (n != 106 && n != kFtrKbdBuf)
            FtrSet(kHgCreator, n, 0);
    if (buf != NULL)
        MemSet(buf, kKbdBufSz, 0);
    FtrSet(kHgCreator, 127, 0);
    FtrSet(kHgCreator, 128, 0);
    FtrSet(kHgCreator, 129, 0);
    for (n = 134; n <= 142; n++)
        FtrSet(kHgCreator, n, 0);
    for (n = 160; n <= 177; n++)   /* v46 */
        FtrSet(kHgCreator, n, 0);
}

/* 켜진 기능 수 = 걸려 있는 DB 삭제 보호 수 */
static UInt16 ProtectCount(void)
{
    return (GetF(kFtrEntry) ? 1 : 0) + (GetF(kFtrCountC) ? 1 : 0) +
           (GetF(kFtrIntlC) ? 1 : 0) + (GetF(kFtrWidthC) ? 1 : 0) + (GetF(kFtrKbdC) ? 1 : 0);
}

static void PatchOff(void);
static void CountOff(void);

/* 모두 끄기: 켠 반대 순서 */
static void AllOff(void)
{
    if (GetF(kFtrKbdC) != 0)
        KbdOff();
    if (GetF(kFtrWidthC) != 0)
        WidthOff();
    if (GetF(kFtrIntlC) != 0)
        IntlOff();
    if (GetF(kFtrCountC) != 0)
        CountOff();
    if (GetF(kFtrEntry) != 0)
        PatchOff();
    if (ProtectCount() == 0)
        StrCopy(gMsg, "all off: ready to install");
    else   /* v53: 다른 패치가 위에 있어 못 푼 것 — Update 리셋 */
        StrCopy(gMsg, "kept: use Update, then install");
}

/* v11 폭 진단 */
static void DrawWidthPage(void)
{
    static const Char *name[6] = { "CharsW", "LineW", "CharsInW", "WordWrap", "W2Offset", "CharWid" };
    RectangleType r;
    Char          s[48];
    UInt32        v;
    UInt16        i;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    HgTitle(" Width [tap: Win]");
    StrPrintF(s, "width %s intl %s  (calls / hangul)", GetF(kFtrWidthC) ? "ON" : "off",
              GetF(kFtrIntlC) ? "ON" : "off");
    Row(0, s);
    for (i = 0; i < 6; i++) {
        StrPrintF(s, "%s  %lu / %lu", name[i], GetF(50 + i), GetF(56 + i));
        Row(1 + i, s);
    }
    v = GetF(62);
    StrPrintF(s, "last hangul w %u  x%u  font h %u", (UInt16)(v >> 16), (UInt16)((v >> 8) & 0xFF),
              (UInt16)(v & 0xFF));
    Row(8, s);
    v = GetF(63);
    StrPrintF(s, "last: %s len %u -> %u", name[(v >> 24) < 6 ? (v >> 24) : 0], (UInt16)((v >> 12) & 0xFFF),
              (UInt16)(v & 0xFFF));
    Row(9, s);
    Row(11, "Clear counters (Main) resets");
}

static void ButtonAt(UInt16 x, UInt16 row, const Char *s)
{
    RectangleType r;

    RctSetRectangle(&r, x, (row + 1) * kLineH + 1, FntCharsWidth(s, StrLen(s)) + 6, kLineH - 2);
    WinDrawChars(s, StrLen(s), x + 3, (row + 1) * kLineH);
    WinDrawRectangleFrame(roundFrame, &r);
}


/* v19: Patch ON = 그리기 패치 + Mode glyph(off였으면) + Intl + Width 한 번에 */
static void PatchOn(void)
{
    Char m[48];

    static const Char *pm[4] = { "ON", "already ON", "patch res missing", "DmWrite fail" };
    static const Char *f24[5] = { "f24 ok", "f24: none", "f24: open fail", "f24: no res", "f24: bad fmt" };
    UInt32 r;
    UInt16 p, f;

    if (CjkOn()) {   /* v56: CJKOS와 같은 함수를 바꾸므로 켜지 않는다 */
        StrCopy(gMsg, "CJKOS running: turn it off first");
        return;
    }
    r = InstallAll();     /* v23: 이미 켜져 있어도 추가 글꼴 연결을 다시 시도 */
    p = (UInt16)(r >> 12) & 15;
    f = (UInt16)(r >> 8) & 15;

    StrCopy(m, pm[p < 4 ? p : 3]);
    if (f < 5) {
        StrCat(m, " ");
        StrCat(m, f24[f]);
    }
    m[sizeof gMsg - 1] = 0;
    StrCopy(gMsg, m);
}

/* 결과: 0 켬, 1 이미 켜짐, 2 리소스 없음, 3 쓰기 실패 (전역 없음) */
static UInt16 InstPatch(void)
{
    MemHandle h;
    UInt8    *p;
    UInt32    orig;
    UInt16    card;
    LocalID   id;

    if (GetF(kFtrEntry) != 0) {   /* v58: 이미 잠가 둔 패치 */
        UInt32 e = GetF(kFtrEntry), cur;

        if (BltFind(e) != 0)
            return 1;             /* 연결 안에 있음(위에 다른 패치가 있어도) — 다시 끼우지 않는다 */
        cur = (UInt32)SysGetTrapAddress(kTrapBlt);   /* 빠져 있음이 확인됨 → 맨 위에 다시 잇기 */
        DmWrite((void *)(e - 4), 0, &cur, 4);
        SysSetTrapAddress(kTrapBlt, (void *)e);
        return 0;
    }
    h = DmGetResource(kPatchRes, 0);
    if (h == NULL)
        return 2;
    p = MemHandleLock(h);
    orig = (UInt32)SysGetTrapAddress(kTrapBlt);
    if (DmWrite(p, 0, &orig, sizeof(orig)) != errNone) {
        MemHandleUnlock(h);
        DmReleaseResource(h);
        return 3;
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, true);
    LockGlyphs();
    FtrSet(kHgCreator, kFtrHandle, (UInt32)h);
    FtrSet(kHgCreator, kFtrEntry, (UInt32)(p + 4));
    SysSetTrapAddress(kTrapBlt, p + 4);
    return 0;
}

/* v25: Patch ON 전체(패치 + 24 글꼴 다시 연결 + Mode glyph + Intl + Width). 전역 없음.
 * 결과 = 패치 << 12 | 24 글꼴 << 8 | Intl << 4 | Width (각 함수의 결과 코드) */
static UInt32 InstallAll(void)
{
    UInt16 p = InstPatch(), f = 9, it, wd;

    if (GetF(kFtrEntry) != 0)
        f = Lock24();
    if (GetF(kFtrBox) == 0)
        FtrSet(kHgCreator, kFtrBox, 2);
    it = InstIntl();
    wd = InstWidth();
    return ((UInt32)p << 12) | ((UInt32)f << 8) | ((UInt32)it << 4) | wd;
}

static void PatchOff(void)
{
    UInt32    e = GetF(kFtrEntry), h = GetF(kFtrHandle);
    UInt32    orig;
    UInt16    card;
    LocalID   id;

    if (e == 0) {
        StrCopy(gMsg, "not ON");
        return;
    }
    if ((UInt32)SysGetTrapAddress(kTrapBlt) != e) {
        StrCopy(gMsg, "trap changed by other: kept");
        return;
    }
    orig = *(const UInt32 *)(e - 4);
    SysSetTrapAddress(kTrapBlt, (void *)orig);
    UnlockGlyphs();
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, false);
    if (h != 0) {
        MemHandleUnlock((MemHandle)h);
        DmReleaseResource((MemHandle)h);
    }
    FtrSet(kHgCreator, kFtrEntry, 0);
    FtrSet(kHgCreator, kFtrHandle, 0);
    StrPrintF(gMsg, "OFF orig=%lx", orig);
}

static void ModeText(Char *o, UInt32 m)
{
    StrCopy(o, (m == 2) ? "glyph" : (m == 1) ? "box" : "off");
}

static void ToggleBox(void)
{
    UInt32 m = (GetF(kFtrBox) + 1) % 3;

    FtrSet(kHgCreator, kFtrBox, m);
    ModeText(gMsg, m);
    StrCat(gMsg, Installed() ? " mode" : " mode (needs Patch ON)");
}

static void *GetRoutine(UInt16 i)
{
    return (i < kCountTraps) ? SysGetTrapAddress(kCountId[i]) : IntlGetRoutineAddress(kCountId[i]);
}

static void SetRoutine(UInt16 i, void *a)
{
    if (i < kCountTraps)
        SysSetTrapAddress(kCountId[i], a);
    else
        IntlSetRoutineAddress(kCountId[i], a);
}

static void CountOn(void)
{
    MemHandle h;
    UInt8    *code;
    UInt32   *cnt;
    UInt16    i, card, n = 0;
    LocalID   id;

    if (GetF(kFtrCountC) != 0) {
        StrCopy(gMsg, "count already ON");
        return;
    }
    h = DmGetResource(kCountRes, 0);
    if (h == NULL) {
        StrCopy(gMsg, "count resource missing");
        return;
    }
    cnt = MemPtrNew(kCountN * 4);
    if (cnt == NULL) {
        DmReleaseResource(h);
        StrCopy(gMsg, "no memory");
        return;
    }
    MemSet(cnt, kCountN * 4, 0);
    MemPtrSetOwner(cnt, 0);   /* 앱이 끝나도 남긴다 */
    code = MemHandleLock(h);
    DmWrite(code, 0, &cnt, 4);
    for (i = 0; i < kCountN; i++) {
        void *o = GetRoutine(i);

        DmWrite(code, 4 + 4 * i, &o, 4);
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, true);
    FtrSet(kHgCreator, kFtrCountH, (UInt32)h);
    FtrSet(kHgCreator, kFtrCountP, (UInt32)cnt);
    FtrSet(kHgCreator, kFtrCountC, (UInt32)code);
    for (i = 0; i < kCountN; i++)
        if (*(UInt32 *)(code + 4 + 4 * i) != 0) {   /* 원래 주소가 없는 것은 건너뜀 */
            SetRoutine(i, code + 100 + 18 * i);
            n++;
        }
    StrPrintF(gMsg, "count ON (%u)", n);
}

static void CountOff(void)
{
    UInt8  *code = (UInt8 *)GetF(kFtrCountC);
    UInt16  i, kept = 0, card;
    LocalID id;

    if (code == NULL) {
        StrCopy(gMsg, "count not ON");
        return;
    }
    for (i = 0; i < kCountN; i++) {
        UInt8 *stub = code + 100 + 18 * i;

        if ((UInt8 *)GetRoutine(i) == stub)
            SetRoutine(i, (void *)*(UInt32 *)(code + 4 + 4 * i));
        else if (*(UInt32 *)(code + 4 + 4 * i) != 0)
            kept++;
    }
    if (kept != 0) {   /* 다른 것이 위에 끼워져 있음: 코드를 풀면 안 된다 */
        StrPrintF(gMsg, "count: %u changed by other, kept", kept);
        return;
    }
    if (AppDB(&card, &id))
        DmDatabaseProtect(card, id, false);
    MemHandleUnlock((MemHandle)GetF(kFtrCountH));
    DmReleaseResource((MemHandle)GetF(kFtrCountH));
    MemPtrFree((void *)GetF(kFtrCountP));
    FtrSet(kHgCreator, kFtrCountH, 0);
    FtrSet(kHgCreator, kFtrCountP, 0);
    FtrSet(kHgCreator, kFtrCountC, 0);
    StrCopy(gMsg, "count OFF");
}

static void DrawCallsPage(void)
{
    RectangleType r;
    Char          s[48];
    UInt32       *cnt = (UInt32 *)GetF(kFtrCountP);
    UInt16        i;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    HgTitle(" Calls [tap: Width]");
    StrPrintF(s, "[tap] count %s: %s", GetF(kFtrCountC) ? "ON" : "off", gMsg);
    Row(0, s);
    for (i = 0; i < kCountN; i++) {   /* 줄 1~12, 두 칸씩 */
        StrPrintF(s, "%s %lu", kCountName[i], (cnt != NULL) ? cnt[i] : 0UL);
        WinDrawChars(s, StrLen(s), (i & 1) ? 82 : 2, (1 + i / 2) * kLineH + kLineH);
    }
}

static void ClearCounters(void)
{
    UInt16 i;

    for (i = 0; i <= kFtrMax; i++)
        FtrUnregister(kHgCreator, i);   /* #20~#26(핸들·진입점·모드·글자 데이터)은 지우지 않는다 */
    FtrUnregister(kHgCreator, kFtrGlyphCnt);
    FtrUnregister(kHgCreator, kFtrRatio);
    for (i = 30; i <= 37; i++)
        FtrUnregister(kHgCreator, i);
    for (i = 64; i <= 71; i++)
        FtrUnregister(kHgCreator, i);
    if (GetF(kFtrCountP) != 0)
        MemSet((void *)GetF(kFtrCountP), kCountN * 4, 0);
    for (i = 50; i <= 63; i++)
        FtrUnregister(kHgCreator, i);
    for (i = 72; i <= 91; i++)   /* v16 창 진단, v21 글꼴 번호 */
        FtrUnregister(kHgCreator, i);
    StrCopy(gMsg, "counters cleared");
}

static void Hex(Char *o, UInt32 v)
{
    StrPrintF(o, "%lx", v);
}

/* x<<16|y → "x,y" (부호 있음) */
static void XY(Char *o, UInt32 v)
{
    if (v == 0xFFFFFFFFUL)
        StrCopy(o, "null");
    else
        StrPrintF(o, "%d,%d", (Int16)(v >> 16), (Int16)(v & 0xFFFF));
}

/* w<<16|h → "wxh" */
static void WH(Char *o, UInt32 v)
{
    if (v == 0xFFFFFFFFUL)
        StrCopy(o, "null");
    else
        StrPrintF(o, "%dx%d", (Int16)(v >> 16), (Int16)(v & 0xFFFF));
}

/* fontType<<16|fRectHeight → "type/h" */
static void Font(Char *o, UInt32 v)
{
    StrPrintF(o, "%x/%d", (UInt16)(v >> 16), (Int16)(v & 0xFFFF));
}

/* 읽어도 되는 주소인지: ROM(0x10xxxxxx) 또는 RAM(16MB 아래), 짝수 */
static Boolean Readable(UInt32 a)
{
    return a != 0 && (a & 1) == 0 && (a < 0x01000000UL || (a >> 24) == 0x10);
}

/* 글꼴 하나를 6줄로: 머리 13워드, SDK FontType 구조로 계산한 표 위치, 'A'·0xC7 항목 */
static void DumpFont(UInt16 row, const Char *name, UInt32 a)
{
    const Int16 *w = (const Int16 *)a;
    Char         s[48];
    UInt32       bmp, loc, owt, exp;
    Int16        first, last;
    UInt16       k;

    StrPrintF(s, "%s @%lx", name, a);
    Row(row, s);
    if (!Readable(a)) {
        Row(row + 1, "  (none)");
        return;
    }
    for (k = 0; k < 3; k++) {
        UInt16 i = k * 5;

        if (k < 2)
            StrPrintF(s, "%d: %x %x %x %x %x", i, w[i], w[i + 1], w[i + 2], w[i + 3], w[i + 4]);
        else
            StrPrintF(s, "%d: %x %x %x", i, w[i], w[i + 1], w[i + 2]);
        Row(row + 1 + k, s);
    }
    first = w[1];
    last = w[2];
    bmp = (UInt32)(UInt16)w[12] * 2 * (UInt16)w[7];
    loc = 26 + bmp;
    owt = 16 + (UInt32)(UInt16)w[8] * 2;
    exp = loc + (UInt32)(last - first + 2) * 2;
    StrPrintF(s, "bmp %lu loc %lu owt %lu/%lu", bmp, loc, owt, exp);
    Row(row + 4, s);
    if (owt < 0x8000UL && last >= first && last - first < 512) {
        UInt16 ea = 0xFFFF, ec = 0xFFFF;

        if (0x41 >= first && 0x41 <= last)
            ea = *(const UInt16 *)(a + owt + (0x41 - first) * 2);
        if (0xC7 >= first && 0xC7 <= last)
            ec = *(const UInt16 *)(a + owt + (0xC7 - first) * 2);
        StrPrintF(s, "ow A %x  C7 %x", ea, ec);
        Row(row + 5, s);
    }
}

static void DrawFontPage(void)
{
    RectangleType r;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    HgTitle(" Font [tap: Sizes]");
    DumpFont(0, "last p6", GetF(3));
    DumpFont(6, "hi p6", GetF(18));
}

/* v21: 4칸(#base~+3, 번호<<24|높이<<16|횟수)을 두 줄에 */
static void FidRows(UInt16 row, const Char *tag, UInt16 base)
{
    Char   s[48], t[16];
    UInt16 i;

    for (i = 0; i < 4; i += 2) {
        UInt16 k;

        StrCopy(s, i == 0 ? tag : "");
        StrCat(s, i == 0 ? " h/fid:" : "   ");
        for (k = i; k < i + 2; k++) {
            UInt32 v = GetF(base + k);

            if (v == 0)
                continue;
            StrPrintF(t, " %u/%u x%lu", (UInt16)((v >> 16) & 0xFF), (UInt16)(v >> 24), v & 0xFFFF);
            StrCat(s, t);
        }
        Row(row + i / 2, s);
    }
}

/* v7 크기 조사 표 */
static void DrawSizesPage(void)
{
    RectangleType r;
    Char          s[48];
    UInt16        i, row = 1;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    HgTitle(" Sizes [tap: Calls]");
    Row(0, "hangul calls by font h / rect h");
    for (i = 30; i <= 37; i++) {
        UInt32 v = GetF(i);
        UInt16 fh, rh;

        if (v == 0 || row > 7)   /* v21: 8~11줄은 글꼴 번호 표 */
            continue;
        fh = (UInt16)(v >> 24);
        rh = (UInt16)((v >> 16) & 0xFF);
        {
            UInt32 c = GetF(i + 34);   /* v15: 마지막으로 그린 앱 크리에이터 */
            Char   cr[5];

            cr[0] = (Char)(c >> 24); cr[1] = (Char)(c >> 16); cr[2] = (Char)(c >> 8); cr[3] = (Char)c; cr[4] = 0;
            if (c == 0)
                StrCopy(cr, "-");
            StrPrintF(s, "font %u rect %u x%s : %lu  %s", fh, rh,
                      (fh == rh) ? "1" : (fh == rh * 2) ? "2" : "?", v & 0xFFFF, cr);
        }
        Row(row++, s);
    }
    if (row == 1)
        Row(1, "(none yet)");
    /* v21: 글꼴 높이/번호(0 std 1 bold 2 large)별 횟수 */
    FidRows(8, "width", 84);
    FidRows(10, "draw", 88);
}

/* v16 창 진단(#72~#83): 한글 폭 잴 때(W)·그릴 때(1 = ×1, 2 = ×2)의 현재 그리기 창 */
static void DrawWinPage(void)
{
    RectangleType r;
    Char          s[48], seen[5];
    UInt16        i, row = 1;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    HgTitle(" Win [tap: Kbd]");
    Row(0, "draw window: W=width 1=x1 2=x2 ?=other");
    for (i = 0; i < 4; i++) {
        UInt32 w = GetF(72 + i), b = GetF(76 + i), f = GetF(80 + i);
        UInt16 n = 0;

        if (w == 0)
            continue;
        if (f & 1) seen[n++] = 'W';
        if (f & 2) seen[n++] = '1';
        if (f & 4) seen[n++] = '2';
        if (f & 8) seen[n++] = '?';
        seen[n] = 0;
        StrPrintF(s, "%lx  seen %s", w, seen);
        Row(row++, s);
        StrPrintF(s, "  bmp %u rb %u win %u fl %x",
                  (UInt16)(b >> 16), (UInt16)b, (UInt16)((f >> 4) & 0xFFF), (UInt16)(f >> 16));
        Row(row++, s);
    }
    if (row == 1)
        Row(1, "(none yet)");
    Row(10, "Clear counters, open app, type hangul");
}

/* v18 키보드 이벤트 순서(v20: MnSk #64~#79 고리, #80 다음 칸) */
#define kSkCr 'MnSk'
static UInt32 GetSk(UInt16 n)
{
    UInt32 v = 0;

    FtrGet(kSkCr, n, &v);
    return v;
}

static void ClearKbdLog(void)
{
    UInt16 i;

    for (i = 64; i <= 80; i++)
        FtrUnregister(kSkCr, i);
}

static void DrawKbdPage(void)
{
    static const Char *nm[10] = { "e0", "Opn", "e2", "Sta", "Stp", "Drw", "Dsp", "Dn", "e8", "Up" };
    RectangleType r;
    Char          s[32], en[4];
    UInt32        idx = GetSk(80), prev = 0;
    UInt16        k, n = 0;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    HgTitle(" Kbd [tap: KDlg]");
    StrPrintF(s, "kbd build %lu  events %lu", GetSk(55) & 0xFFFF, idx);
    Row(0, s);
    Row(1, "evt ev14>hires +ticks (old->new)");
    for (k = 0; k < 16; k++) {
        UInt32 v = GetSk(64 + (UInt16)((idx + k) % 16)), dt;
        UInt16 e;

        if (v == 0)
            continue;
        e = (UInt16)((v >> 8) & 0xFF);
        if (e < 10)
            StrCopy(en, nm[e]);
        else
            StrPrintF(en, "%u", e);
        dt = (n == 0) ? 0 : (UInt16)((v >> 16) - (prev >> 16));
        prev = v;
        StrPrintF(s, "%s %u>%u +%lu", en, (UInt16)((v >> 4) & 0xF), (UInt16)(v & 1), dt);
        WinDrawChars(s, StrLen(s), 2 + (n / 8) * 80, (3 + n % 8) * kLineH);
        n++;
    }
    if (n == 0)
        Row(2, "(none) open memo, press keys");
    Row(11, "tap row 0 = clear kbd log");
}

/* v37: Sony 키보드 상태 변화(GrfSetState·GsiSetShiftState)와 가상 실크 창 복사(WinCopyRectangle) 기록 페이지.
 * 한 줄 = 한 기록(오래된 것부터 최근 11개). d = 그 순간까지 다른 창 글자 그리기 수(#108) */
static void DrawSetStPage(void);

/* v28: 화면 키보드(SysKeyboardDialog) 진단, v29: 가상 실크 글자 그리기·EvtEnqueueKey 진단 */
static const Char *RomMod(UInt32 a) __attribute__((unused));
static const Char *RomMod(UInt32 a)
{
    if (a >= 0x1032A546UL && a < 0x1032A546UL + 27778)
        return "Silk";
    if (a >= 0x102CBC0CUL && a < 0x102CE4C8UL)
        return "KbdX";
    if (a >= 0x10000000UL && a < 0x10800000UL)
        return "ROM";
    return a ? "RAM" : "-";
}

static void DrawKbdDlgPage(void)
{
    static const Char *tn[3] = { "alpha", "number", "accent" };
    RectangleType r;
    Char          s[48], c[5], ring[33];
    UInt32        v = GetF(104), cr = GetF(105), w;
    UInt16        t = (UInt16)((v >> 16) & 0xFF), i;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    HgTitle(" KDlg [tap: SetSt]");
    Button(0, GetF(kFtrKbdC) ? "Hooks: ON" : "Hooks: off");
    ButtonAt(100, 0, "Clear");
    WinDrawInvertedChars(gMsg, StrLen(gMsg), 0, 2 * kLineH);
    c[0] = (Char)(cr >> 24); c[1] = (Char)(cr >> 16); c[2] = (Char)(cr >> 8); c[3] = (Char)cr; c[4] = 0;
    /* v52: 줄 2 = 그리기 패치 아래 함수(ROM 0x10…, 다른 패치면 RAM) */
    StrPrintF(s, "blt below %lx", GetF(kFtrEntry) ? *(const UInt32 *)(GetF(kFtrEntry) - 4) : 0UL);
    Row(2, s);
    (void)tn; (void)t; (void)c; (void)cr;
    w = GetF(111);
    StrPrintF(s, "draw %lu flags %x h %u", GetF(108), (UInt16)w, (UInt16)GetF(112));
    Row(3, s);
    for (i = 0; i < 32; i++) {
        UInt32 pos = GetF(113), k = (pos >= 32 ? pos : 32) - 32 + i;   /* 오래된 것부터 */
        UInt8  b;

        if (pos < 32)
            k = i;
        b = (UInt8)(GetF(114 + (UInt16)((k % 32) / 4)) >> (24 - 8 * (k % 4)));
        ring[i] = (b >= 0x20 && b < 0x7F) ? (Char)b : '.';
        if (k >= pos)
            ring[i] = ' ';
    }
    ring[32] = 0;
    Row(4, ring);
    {
        const UInt32 *kb = (const UInt32 *)GetF(kFtrKbdBuf);

        {
            UInt32 g = 0;

            if (kb != NULL && kb[18] != 0)
                g = kb[21 + 2 * (UInt16)((kb[19] + 7) & 7)];   /* 마지막 get */
            StrPrintF(s, "enq %lu add %lu get %lu g%x", kb ? kb[0] : 0, kb ? kb[36] : 0, kb ? kb[18] : 0, (UInt16)g);
            Row(5, s);
        }
    }
    {   /* v60 진단(동작 변경 없음): CJKOS 감지를 공개 함수(FtrGet 시스템 Feature)로 바꿀 수 있는지 원시값 비교.
         *   값 뒤 !는 FtrGet 오류. 인코딩 3 = PalmLatin, 67 = KSC5601, 68 = CP949 / 언어 62 = 한국어 / TSM 플래그 1 = FEP 있음 */
        static const UInt16 num[5] = { sysFtrNumEncoding, sysFtrNumCharEncodingFlags, sysFtrNumLanguage,
                                       sysFtrNumCountry, tsmFtrNumFlags };
        UInt32 v[5];
        Err    e[5];
        UInt16 k;

        for (k = 0; k < 5; k++) {
            v[k] = 0;
            e[k] = FtrGet(k == 4 ? tsmFtrCreator : sysFtrCreator, num[k], &v[k]);
        }
        StrPrintF(s, "enc %lu%s flags %lx%s", v[0], e[0] ? "!" : "", v[1], e[1] ? "!" : "");
        Row(7, s);
        StrPrintF(s, "lang %lu%s ctry %lu%s tsm %lx%s", v[2], e[2] ? "!" : "", v[3], e[3] ? "!" : "", v[4], e[4] ? "!" : "");
        Row(8, s);
        StrPrintF(s, "now: CJKOS %s (dCJK feature)", CjkOn() ? "running" : "no");   /* v62 */
        Row(9, s);
    }
}

static void Draw(void)
{
    RectangleType r;
    Char          s[48], a[16], b[16], c[16];

    if (gPage == 1) {
        DrawFontPage();
        return;
    }
    if (gPage == 2) {
        DrawSizesPage();
        return;
    }
    if (gPage == 3) {
        DrawCallsPage();
        return;
    }
    if (gPage == 4) {
        DrawWidthPage();
        return;
    }
    if (gPage == 5) {
        DrawWinPage();
        return;
    }
    if (gPage == 6) {
        DrawKbdPage();
        return;
    }
    if (gPage == 8) {
        DrawSetStPage();
        return;
    }
    if (gPage == 7) {
        DrawKbdDlgPage();
        return;
    }
    if (!DebugGet()) {   /* v41: 기본 화면은 사용자용 버튼만 */
        DrawUserMain();
        return;
    }
    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    HgTitle(" [tap: Font]");

    ModeText(a, GetF(kFtrBox));
    StrPrintF(s, "patch %s  %s  font %s%s  prot %u%s", Installed() ? "ON" : "off", a,
              (GetF(kFtrGlyph0) && GetF(kFtrGlyph1)) ? "ok" : "-", GetF(kFtrF24P) ? "+24" : "", ProtectCount(),
              ProtectCount() ? "" : " ok");
    Row(0, s);
    Button(kRowOn, "Patch ON");
    Button(kRowOff, "Patch OFF");
    StrPrintF(s, "Mode: %s", a);
    Button(kRowBox, s);
    Button(kRowClear, "Clear counters");
    ButtonAt(84, kRowClear, GetF(kFtrIntlC) ? "Intl: ON" : "Intl: off");
    /* 한글 견본: 한글 OK 가각 (EUC-KR) */
    ButtonAt(84, kRowOn, AutoGet() ? "Auto: ON" : "Auto: off");   /* v25 */
    ButtonAt(84, kRowOff, "All OFF");
    ButtonAt(84, kRowBox, GetF(kFtrWidthC) ? "Width: ON" : "Width: off");

    Button(kRowReset, "Soft Reset (hold 3s)");   /* v26 */
    ButtonAt(kUpdX, kRowReset, "Update");          /* v62: 진단 화면에서도 업데이트(3초) */
    StrPrintF(s, "calls %lu hi %lu drawn %lu 2c %lu x%lu", GetF(0), GetF(9), GetF(19),
              GetF(kFtrGlyphCnt), GetF(kFtrRatio));
    Row(7, s);   /* v26: last len 줄 대신 */
    XY(a, GetF(4));
    WH(b, GetF(5));
    XY(c, GetF(6));
    StrPrintF(s, "rect %s %s  p3 %s", a, b, c);
    Row(8, s);
    Font(a, GetF(14));
    Hex(b, GetF(7));
    StrPrintF(s, "font %s  p1 %s", a, b);
    Row(9, s);
    Hex(b, GetF(11));
    StrPrintF(s, "hi: len %lu  %s", GetF(10), b);
    Row(10, s);
    XY(a, GetF(12));
    WH(b, GetF(13));
    XY(c, GetF(16));
    StrPrintF(s, "hi rect %s %s  p3 %s", a, b, c);
    Row(11, s);
    /* v24: 리셋 알림 진단(#96~#99) */
    /* v41: 줄 12 = [v] Debug + boot 요약(n 리셋 알림 수, a 자동 설치 결과). 자세한 t·h·db는 뺌 */
    DrawCheck(12, "Debug", true);
    StrPrintF(s, "boot n%lu a%lx", GetF(96), GetF(95));
    WinDrawChars(s, StrLen(s), 60, 13 * kLineH);

    WinDrawInvertedChars(gMsg, StrLen(gMsg), 0, 2 * kLineH);   /* 빈 줄 1 (맨 아래는 줄 12까지 참) */
}

/* v26: 3초 동안 누르고 있으면 SysReset(), 중간에 떼면 취소 (ClieDiag와 같은 방식) */
static void SkipSet(UInt16 v);
/* v57: Update = 설치되어 있는 다른 모듈(MnSonyKbd)도 다음 리셋 한 번 자동 시작 건너뛰기(앱 설정 3번).
 *   설치되어 있지 않으면 아무것도 저장하지 않는다(하나만 쓰는 경우는 그대로) */
static void SkipOther(UInt32 cr)
{
    DmSearchStateType st;
    UInt16            card, v = 1;
    LocalID           id;

    if (DmGetNextDatabaseByTypeCreator(true, &st, sysFileTApplication, cr, true, &card, &id) == errNone)
        PrefSetAppPreferences(cr, 3, 1, &v, sizeof v, true);
}

static void HoldResetAt(UInt16 x0, UInt16 row, const Char *label, Boolean upd);

static void HoldReset(UInt16 row)
{
    HoldResetAt(0, row, "Soft Reset (hold 3s)", false);
}

static void HoldResetAs(UInt16 row, const Char *label, Boolean upd)
{
    HoldResetAt(0, row, label, upd);
}

/* v53: upd = 다음 리셋 한 번 자동 시작 건너뛰기(업데이트 설치용)를 저장하고 리셋. v62: x0 = 버튼 왼쪽 위치 */
static void HoldResetAt(UInt16 x0, UInt16 row, const Char *label, Boolean upd)
{
    UInt32        t0 = TimGetTicks(), tps = SysTicksPerSecond();
    Int16         x, y, shown = -1, left;
    Boolean       down = true;
    RectangleType r, b;
    Char          s[24];
    /* v27: 버튼 폭은 처음 글자 기준으로 고정 */
    UInt16        w = FntCharsWidth(label, StrLen(label)) + 6;

    RctSetRectangle(&r, x0, (row + 1) * kLineH, w + 6, kLineH);
    RctSetRectangle(&b, x0 + 3, (row + 1) * kLineH + 1, w, kLineH - 2);
    while (down) {
        left = (Int16)(kResetHoldSec - (TimGetTicks() - t0) / tps);
        if (left <= 0)
            StrCopy(s, "Resetting...");
        else
            StrPrintF(s, "Hold... %ds", left);
        if (left != shown || left <= 0) {
            WinEraseRectangle(&r, 0);
            WinDrawChars(s, StrLen(s), x0 + 6, (row + 1) * kLineH);
            WinDrawRectangleFrame(roundFrame, &b);
            WinInvertRectangle(&b, 0);
            shown = left;
        }
        if (left <= 0) {
            if (upd) {
                SkipSet(1);
                SkipOther('MnSy');
            }
            SysReset();
            return;
        }
        SysTaskDelay(tps / 20);
        EvtGetPen(&x, &y, &down);
    }
    StrCopy(gMsg, "reset cancelled");
}

static void DrawSetStPage(void)
{
    RectangleType r;
    Char          s[48];
    const UInt32 *kb = (const UInt32 *)GetF(kFtrKbdBuf);
    UInt32        cnt, k;
    Int16         j;
    UInt16        row = 1;

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    HgTitle(" SetSt [tap: Main]");
    if (kb == NULL) {
        Row(0, "Hooks off (KDlg page)");
        return;
    }
    cnt = kb[54];
    k = kb[55];
    StrPrintF(s, "n %lu  draw now %lu", cnt, GetF(108));
    Row(0, s);
    for (j = 10; j >= 0; j--) {
        UInt16 e;
        UInt32 a, x1, x2, x3;

        if ((UInt32)j >= cnt)
            continue;
        e = (UInt16)((k - 1 - (UInt32)j) & 15);
        a = kb[56 + 4 * e];
        x1 = kb[57 + 4 * e];
        x2 = kb[58 + 4 * e];
        x3 = kb[59 + 4 * e];
        switch ((UInt16)(a >> 28)) {
        case 1:
            StrPrintF(s, "Grf c%u n%u u%u d%lu %lx", (UInt16)(x1 >> 16), (UInt16)((x1 >> 8) & 0xFF),
                      (UInt16)(x1 & 0xFF), a & 0x0FFFFFFFUL, x3);
            break;
        case 2:
            StrPrintF(s, "Gsi L%x t%u d%lu %lx", (UInt16)(x1 >> 16), (UInt16)x1, a & 0x0FFFFFFFUL, x3);
            break;
        case 5:
        case 6:
            StrPrintF(s, "%s %d,%d d%lu", (a >> 28) == 5 ? "Pen dn" : "Pen up", (Int16)(x1 >> 16), (Int16)x1,
                      a & 0x0FFFFFFFUL);
            break;
        default:
            StrPrintF(s, "%s %d,%d %dx%d >%d,%d d%lu", (a >> 28) == 3 ? "Cp>" : "Cp<", (Int16)(x1 >> 16),
                      (Int16)x1, (Int16)(x2 >> 16), (Int16)x2, (Int16)(x3 >> 16), (Int16)x3, a & 0x0FFFFFFFUL);
            break;
        }
        Row(row++, s);
    }
}

/* v25: 자동 시작 설정(앱 설정 'MnHg' 0번, 저장형) */
/* v41: Debug 체크(앱 설정 2번, 저장형). 꺼져 있으면 Main은 사용자 버튼만, 진단 페이지로 넘어가지 않음 */
static UInt16 DebugGet(void)
{
    UInt16 v = 0, size = sizeof v;

    if (PrefGetAppPreferences(kHgCreator, 2, &v, &size, true) == noPreferenceFound || size != sizeof v)
        v = 0;
    return v;
}

static void DebugSet(UInt16 v)
{
    PrefSetAppPreferences(kHgCreator, 2, 1, &v, sizeof v, true);
}

/* 네모 체크박스 + 이름 */
static void DrawCheck(UInt16 row, const Char *label, Boolean on)
{
    RectangleType r;

    RctSetRectangle(&r, 4, (row + 1) * kLineH + 2, 7, 7);
    WinDrawRectangleFrame(rectangleFrame, &r);
    if (on) {
        WinDrawLine(5, (row + 1) * kLineH + 5, 7, (row + 1) * kLineH + 8);
        WinDrawLine(7, (row + 1) * kLineH + 8, 10, (row + 1) * kLineH + 2);
    }
    WinDrawChars(label, StrLen(label), 15, (row + 1) * kLineH);
}

/* v41: 사용자용 Main — Patch ON·All OFF, Auto·Sony, Soft Reset, 사용법, [ ] Debug */
#define kURowOn    2
#define kURowAuto  3
#define kURowReset 5
#define kURowUpd   6   /* v53 */
static void DrawUserMain(void)
{
    RectangleType r;
    Char          s[48];

    RctSetRectangle(&r, 0, 0, 160, 160);
    WinEraseRectangle(&r, 0);
    HgTitle("");
    StrPrintF(s, "Hangul %s  Reset %s  CJKOS %s", Installed() ? "ON" : "off", AutoGet() ? "ON" : "off",
              CjkOn() ? "running" : "no");
    Row(0, s);
    Button(kURowOn, "Hangul ON");
    ButtonAt(84, kURowOn, "Hangul OFF");
    Button(kURowAuto, AutoGet() ? "Start at reset: ON" : "Start at reset: off");
    Button(kURowReset, "Soft Reset (hold 3s)");
    Button(kURowUpd, "Update (hold 3s)");   /* v53 */
    Row(7, "Hangul display, CJKOS replacement");
    {   /* v55: Sony 가상 키보드 한글(MnSonyKbd) 상태 */
        UInt32            on = 0;
        DmSearchStateType st;
        UInt16            card;
        LocalID           id;

        FtrGet('MnSy', 1, &on);
        Row(8, DmGetNextDatabaseByTypeCreator(true, &st, sysFileTApplication, 'MnSy', true, &card, &id) != errNone ?
                   "Sony kbd Hangul: MnSonyKbd (none)" :
                   on ? "Sony kbd Hangul: MnSonyKbd ON" : "Sony kbd Hangul: MnSonyKbd off");
    }
    Row(10, "Update: [Update], install, ON");
    DrawCheck(12, "Debug", false);
    WinDrawInvertedChars(gMsg, StrLen(gMsg), 0, 2 * kLineH);
}

static UInt16 AutoGet(void)
{
    UInt16 v = 0, size = sizeof v;

    if (PrefGetAppPreferences(kHgCreator, 0, &v, &size, true) == noPreferenceFound || size != sizeof v)
        v = 1;   /* v42: 저장한 적 없으면 ON */
    return v;
}

static void AutoSet(UInt16 v)
{
    PrefSetAppPreferences(kHgCreator, 0, 1, &v, sizeof v, true);
}

/* v53: 앱 설정 3번 = 다음 리셋 한 번 자동 시작 건너뛰기(Update 버튼). 리셋 알림에서 읽으므로 전역 없음 */
static UInt16 SkipGet(void)
{
    UInt16 v = 0, size = sizeof v;

    if (PrefGetAppPreferences(kHgCreator, 3, &v, &size, true) == noPreferenceFound || size != sizeof v)
        v = 0;
    return v;
}

static void SkipSet(UInt16 v)
{
    PrefSetAppPreferences(kHgCreator, 3, 1, &v, sizeof v, true);
}

/* v24 진단: 리셋 알림 기록. 이 호출에서는 앱 전역변수·문자열 상수(데이터 구역)를 쓸 수 없으므로 상수와 지역변수만 쓴다 */
static void ResetDiag(MemPtr pb)
{
    UInt32    n = 0;
    UInt32    flags = 0;
    DmOpenRef db;

    FtrGet(kHgCreator, 96, &n);
    FtrSet(kHgCreator, 96, n + 1);
    FtrSet(kHgCreator, 97, TimGetTicks());
    FtrSet(kHgCreator, 98, (UInt32)SysGetTrapAddress(kTrapBlt));
    if (pb != NULL)
        flags = ((UInt32)(((const UInt8 *)pb)[0] ? 1 : 0) << 24) | ((UInt32)(((const UInt8 *)pb)[1] ? 1 : 0) << 16);
    db = DmOpenDatabaseByTypeCreator('appl', kHgCreator, dmModeReadOnly);
    if (db != NULL) {
        flags |= 1;
        DmCloseDatabase(db);
    }
    FtrSet(kHgCreator, 99, flags);
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    EventType e;

    if (cmd == sysAppLaunchCmdSystemReset) {
        ResetDiag(cmdPBP);
        if (SkipGet()) {   /* v53: Update 버튼 리셋 — 이번 한 번 켜지 않음(잠금 없음 → 바로 설치 가능) */
            SkipSet(0);
            FtrSet(kHgCreator, 95, 0x40000000UL);
        } else if (AutoGet() && CjkOn())   /* v56: CJKOS가 먼저 걸려 있으면 건너뜀 */
            FtrSet(kHgCreator, 95, 0x20000000UL);
        else if (AutoGet())   /* v25: 자동 시작 */
            FtrSet(kHgCreator, 95, 0x80000000UL | InstallAll());
        return 0;
    }
    if (cmd != sysAppLaunchCmdNormalLaunch)
        return 0;

    FtrSet(kHgCreator, 106, 1);   /* v29: MnHangul 실행 중 — 다른 창 글자 기록 제외 */
    StartHires();
    Draw();
    do {
        EvtGetEvent(&e, evtWaitForever);
        if (e.eType == penDownEvent && e.screenY >= 0 && e.screenY < 160) {
            Int16 row = e.screenY / kLineH - 1;

            if (!DebugGet()) {   /* v41: 사용자용 Main */
                gPage = 0;
                if (row == kURowOn && e.screenX >= 84)
                    AllOff();
                else if (row == kURowOn)
                    PatchOn();
                else if (row == kURowAuto) {
                    AutoSet(AutoGet() ? 0 : 1);
                    StrCopy(gMsg, AutoGet() ? "start at reset: ON" : "start at reset: off");
                } else if (row == kURowReset && e.screenX < 108)
                    HoldReset(kURowReset);
                else if (row == kURowUpd && e.screenX < 90)   /* v53 */
                    HoldResetAs(kURowUpd, "Update (hold 3s)", true);
                else if (row == 12 && e.screenX < 60) {
                    DebugSet(1);
                    StrCopy(gMsg, "debug pages: tap title");
                }
            } else if (e.screenY < kLineH)
                gPage = (gPage + 1) % kPages;
            else if (gPage == 0 && row == 12 && e.screenX < 60) {
                DebugSet(0);
                StrCopy(gMsg, "-");
            }
            else if (gPage == 6) {
                if (row == 0)
                    ClearKbdLog();
            } else if (gPage == 7) {
                if (row == 0 && e.screenX >= 100)
                    ClearKbdDlg();
                else if (row == 0) {
                    if (GetF(kFtrKbdC) != 0)
                        KbdOff();
                    else
                        KbdOn();
                }
            } else if (gPage == 3) {
                if (row == 0) {
                    if (GetF(kFtrCountC) != 0)
                        CountOff();
                    else
                        CountOn();
                }
            } else if (gPage != 0)
                ;
            else if (row == kRowOn && e.screenX >= 84) {
                AutoSet(AutoGet() ? 0 : 1);
                StrCopy(gMsg, AutoGet() ? "auto ON: install at reset" : "auto off");
            } else if (row == kRowOn)
                PatchOn();
            else if (row == kRowOff && e.screenX >= 84)
                AllOff();
            else if (row == kRowOff)
                PatchOff();
            else if (row == kRowBox && e.screenX >= 84) {
                if (GetF(kFtrWidthC) != 0)
                    WidthOff();
                else
                    WidthOn();
            } else if (row == kRowBox)
                ToggleBox();
            else if (row == kRowClear && e.screenX >= 84) {
                if (GetF(kFtrIntlC) != 0)
                    IntlOff();
                else
                    IntlOn();
            } else if (row == kRowClear)
                ClearCounters();
            else if (row == kRowReset && e.screenX < 108)
                HoldReset(kRowReset);
            else if (row == kRowReset && e.screenX >= kUpdX - 3)
                HoldResetAt(kUpdX - 3, kRowReset, "Update", true);   /* v62 */
            Draw();
            continue;
        }
        if (!SysHandleEvent(&e) && e.eType == keyDownEvent &&
            (e.data.keyDown.chr == vchrPageUp || e.data.keyDown.chr == vchrPageDown) && DebugGet()) {
            gPage = (e.data.keyDown.chr == vchrPageDown) ? (gPage + 1) % kPages : 0;
            Draw();
        }
    } while (e.eType != appStopEvent);
    StopHires();
    FtrSet(kHgCreator, 106, 0);
    return 0;
}
