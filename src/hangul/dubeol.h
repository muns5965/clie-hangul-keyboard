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

static inline UInt8 Dubeol(UInt8 c)
{
    switch (c) {   /* ⇪ + 쉬프트(소문자) 때만 쌍자음 */
    case 'q': return 0xB3;  case 'w': return 0xB9;  case 'e': return 0xA8;  case 'r': return 0xA2;
    case 't': return 0xB6;  case 'o': return 0xC2;  case 'p': return 0xC6;
    }
    if (c >= 'a' && c <= 'z')
        c = (UInt8)(c - 'a' + 'A');
    switch (c) {
    case 'Q': return 0xB2;  case 'W': return 0xB8;  case 'E': return 0xA7;  case 'R': return 0xA1;
    case 'T': return 0xB5;  case 'Y': return 0xCB;  case 'U': return 0xC5;  case 'I': return 0xC1;
    case 'O': return 0xC0;  case 'P': return 0xC4;
    case 'A': return 0xB1;  case 'S': return 0xA4;  case 'D': return 0xB7;  case 'F': return 0xA9;
    case 'G': return 0xBE;  case 'H': return 0xC7;  case 'J': return 0xC3;  case 'K': return 0xBF;
    case 'L': return 0xD3;
    case 'Z': return 0xBB;  case 'X': return 0xBC;  case 'C': return 0xBA;  case 'V': return 0xBD;
    case 'B': return 0xD0;  case 'N': return 0xCC;  case 'M': return 0xD1;
    }
    return 0;
}

/* v39: Sony 가상 키보드 한글 모드를 누른 키로 미리 안다(v38 실측: 펜 누름이 앱 EvtGetEvent를 먼저 지나고,
 *   Sony는 그다음 키 글자를 다시 그린 뒤 GrfSetState를 부른다. ⇪+쉬프트와 ⇪ 끄기는 상태로 구별 불가).
 *   ⇪ 키: x 0~12, y 182~191 / 쉬프트 키: x 0~12, y 192~201 (저해상도 화면 좌표, v38 실측 6,186 · 7,195)
 *   'MnHg' #143 = 한글 모드(1), #144 = 마지막 펜 누름이 ⇪ 키(1), #145 = ⇪ 누를 때의 다른 창 글자 그리기 수(#108)
 *   ⇪를 눌렀는데 키 글자 다시 그리기가 없으면(가상 키보드가 아님) 펜 뗄 때 되돌린다. */
#define kFtrHan      143
#define kFtrPenCaps  144
#define kFtrPenDraw  145
#define kCapsKey(x, y)  ((x) >= 0 && (x) < 13 && (y) >= 182 && (y) < 192)

/* 지금 쉬프트 고정(⇪)인가 = 한글 모드 */
static inline Boolean HanMode(void)
{
    Boolean caps = false, num = false, autoS = false;
    UInt16  temp = 0;

    GrfGetState(&caps, &num, &temp, &autoS);
    return caps;
}

#endif
