/*
 * v35: Sony 가상 키보드 두벌식 배치 (patch.c 오버레이·ime.c 입력 공용)
 *
 * 한글 모드 = 쉬프트 고정(⇪) 켜짐. 그때 Sony 키보드는
 *   보통      → 대문자(키 글자도 대문자)  → 기본 자모
 *   ⇪ + 쉬프트 → 소문자(키 글자도 소문자)  → 쌍자음 ㅃㅉㄸㄲㅆ, ㅒㅖ (나머지는 기본 자모)
 * (v33·v34 실측: ⇪ = g1000, ⇪+쉬프트 = 소문자, 받는 순간 g1000)
 *
 * 돌려주는 값: KS X 1001 호환 자모 둘째 바이트(0xA1~0xD3, 첫째 바이트 0xA4). 0 = 영문 글자 아님.
 * 상수 표 대신 switch(코드 리소스는 데이터 구역을 쓸 수 없다).
 */
#ifndef HG_DUBEOL_H
#define HG_DUBEOL_H

/* v54: Sony 가상 키보드 한글은 별도 모듈 MnSonyKbd(크리에이터 'MnSy', src/sony)로 분리. 기록 feature도 'MnSy' */
#define kSyCr 'MnSy'
#include "../common/diag.h"

#include "../common/dubeol_map.h"   /* Dubeol(): MnKbdHan과 공용 */

/* v39: Sony 가상 키보드 한글 모드를 누른 키로 미리 안다(v38 실측: 펜 누름이 앱 EvtGetEvent를 먼저 지나고,
 *   Sony는 그다음 키 글자를 다시 그린 뒤 GrfSetState를 부른다. ⇪+쉬프트와 ⇪ 끄기는 상태로 구별 불가).
 *   ⇪ 키: x 0~12, y 182~191 / 쉬프트 키: x 0~12, y 192~201 (저해상도 화면 좌표, v38 실측 6,186 · 7,195)
 *   kSyCr #143 = 한글 모드(1), #144 = 마지막 펜 누름이 ⇪ 키(1), #145 = ⇪ 누를 때의 다른 창 글자 그리기 수(#108)
 *   ⇪를 눌렀는데 키 글자 다시 그리기가 없으면(가상 키보드가 아님) 펜 뗄 때 되돌린다. */
#define kFtrHan      143
#define kFtrPenCaps  144
#define kFtrPenDraw  145
#define kFtrSonyCaps 150   /* v45: 자판 전체를 그릴 때 본 ⇪ 그림 = Sony 실제 상태(0x100 | 켜짐) */
#define kCapsKey(x, y)  ((x) >= 0 && (x) < 13 && (y) >= 182 && (y) < 192)
#define kKeyAreaEnd  213   /* v50: 가상 키보드 키 5줄 = y 160~212 (그 아래 abc/int'l/sym 탭, 상태 표시줄) */
/* v20 진단: sym 탭 오른쪽 빈칸 = 한/영 단추 후보(화면 사진으로 추정한 저해상도 화면 좌표, 펜 기록으로 확인).
 *   #210 다음 펜 칸, #211~#213 가상 실크 펜 누름 3개(x<<16 | y), #214 시험용 한글 표시(입력에는 아직 안 씀),
 *   #215 빈칸 누를 때의 다른 창 글자 그리기 수(#108, 그 뒤 다시 그리기가 있었는지 비교), #216 'sym' 그리기 수,
 *   #217·#218 그 사각형(x<<16 | y, 폭<<16 | 높이), #219 빈칸 누름 수 */
#define kHanTab(x, y)  ((x) >= 66 && (x) <= 134 && (y) >= 213 && (y) <= 224)
#define kFtrHanTab   214
#define kMarkX       143   /* 한/영 표시 자리(가상 실크 창, 고해상도) — v31: y 105 → 108(탭 글자 가운데에 맞춤) */
#define kMarkY       108

/* v46 진단: 판정 순간 기록 고리 kSyCr #160~#175(16칸), #176 다음 칸, #177 마지막 키 글자 기록 상태.
 *   칸 = 종류<<30 (1 키 글자, 2 ⇪ 그림) | d(#108 하위 12비트)<<18 | 글자<<10 | H<<9 | p<<7 | caps<<6 | temp<<2 |
 *        (그림: 판정에 씀<<1 | 반전) */
static inline void DecLog(UInt32 e)
{
#if MN_DIAG
    UInt32 k = 0;

    FtrGet(kSyCr, 176, &k);
    FtrSet(kSyCr, 160 + (UInt16)(k & 15), e);
    FtrSet(kSyCr, 176, k + 1);
#else
    (void)e;
#endif
}

/* v32: 한글 모드 = sym 오른쪽 한/영 단추 값(#214). ⇪(쉬프트 고정)는 더 이상 보지 않는다(⇪는 영문 대문자 고정만) */
static inline Boolean HanMode(void)
{
    UInt32 v = 0;

    FtrGet(kSyCr, kFtrHanTab, &v);
    return v != 0;
}

/* v34: PC 두벌식 규칙 — 쌍자음 = shift(자동 대문자 아님). ⇪는 보지 않는다.
 *   v33 실측: 대문자 글자로 판단하면 ⇪가 켜진 채면 거꾸로(⇪ 켜짐 = shift 없이 대문자). 첫 줄(` 1 2 …)은 ⇪와 상관없이 shift만 따르므로
 *   자판을 그릴 때 첫 키가 '~'이고 자동 대문자가 아니면 쌍자음 — #255(spatch.c). 영문 글자 키는 대·소문자와 상관없이 이 값대로.
 *   입력은 펜 누를 때(Sony가 다시 그리기 전, v38 실측) #255를 #256에 옮겨 두고 그 값대로 → 보이는 자모와 입력이 같다. */
#define kFtrShift    255
#define kFtrShiftIn  256
static inline Boolean IsLetter(UInt8 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline UInt8 ShiftCase(UInt8 c, UInt16 n)
{
    UInt32 dbl = 0;

    if (!IsLetter(c))
        return c;
    FtrGet(kSyCr, n, &dbl);
    /* v44: Dubeol()(dubeol_map.h)은 옛 ⇪ 규칙 — 대문자 = 기본 자모, 소문자 = 쌍자음. v32~v43은 반대로 넘겨
     *   shift 없이 쌍자음이 나왔음(MnKbdHan은 대·소문자를 뒤집어 넘김) */
    return dbl ? (UInt8)(c | 0x20) : (UInt8)(c & ~0x20);
}

#define HanLabelKey(c) ShiftCase((c), kFtrShift)
#define HanInKey(c)    ShiftCase((c), kFtrShiftIn)

#endif
