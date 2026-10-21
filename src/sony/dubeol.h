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

/* 지금 쉬프트 고정(⇪)인가 = 한글 모드 */
static inline Boolean HanMode(void)
{
    Boolean caps = false, num = false, autoS = false;
    UInt16  temp = 0;

    GrfGetState(&caps, &num, &temp, &autoS);
    return caps;
}

#endif
