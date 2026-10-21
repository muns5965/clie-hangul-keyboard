/*
 * 조합 중 커서 이동 감지 (MnSkCore build 16~17에서 시작, MnSonyKbd v10과 공용) — 두 모듈이 각자 소스로 포함한다.
 *
 * 키를 누를 때 폼 초점 입력칸(FrmGetActiveForm·FrmGetFocus·FrmGetObjectPtr, 필드만)의 FldGetInsPtPosition을
 * "직전 키 때 위치 + 그 뒤 우리가 보낸 키로 생길 변화"와 비교한다. 다르거나 입력칸이 바뀌었으면 조합을 끝내야 한다.
 *   변화: 2바이트 글자 +2, 1바이트 +1, 조합 BS -2(우리 2바이트 글자를 지움). 크기를 모르는 키(앱에 그대로 넘긴 BS 등)
 *   뒤에는 비교하지 않는다(CtUnknown). 바이트 계산이므로 2바이트 글자 경계 처리(MnHangul·CJKOS)가 있어야 정확하다.
 * 앞 키로 넣은 키 이벤트는 다음 키 전에 앱이 처리했다고 본다.
 *
 * 기록(feature 크리에이터 cr, 번호 b부터 7칸):
 *   b+0 비교 수, b+1 다름 수, b+2 마지막 비교(31~16 기대, 15~0 실제 위치), b+3 마지막 비교(비트0 다름, 비트1 조합 중),
 *   b+4 직전 입력칸(FieldType*, 0 없음), b+5 직전 위치(비트31 = 기대 위치 모름), b+6 그 뒤 위치 변화(부호 있음)
 * 전역변수·문자열 상수 없음(코드 리소스용). MNSK_HOST_TEST(PC 조합 시험)에서는 아무 일도 하지 않는다.
 */
#ifndef CURSOR_TRACK_H
#define CURSOR_TRACK_H

#include "diag.h"   /* b+0~b+3(비교 수·결과)은 진단 기록, b+4~b+6은 동작용 */

#ifndef MNSK_HOST_TEST

static inline UInt32 CtGet(UInt32 cr, UInt16 n)
{
    UInt32 v = 0;

    FtrGet(cr, n, &v);
    return v;
}

/* 보낸 키 하나로 생길 커서 변화 누적 */
static inline void CtDelta(UInt32 cr, UInt16 b, UInt16 c)
{
    Int32 d = (Int32)CtGet(cr, b + 6);

    d += (c == chrBackspace) ? -2 : (c > 0xFF) ? 2 : 1;
    FtrSet(cr, b + 6, (UInt32)d);
}

/* 다음 비교 건너뜀(크기를 모르는 키를 앱에 넘김) */
static inline void CtUnknown(UInt32 cr, UInt16 b)
{
    FtrSet(cr, b + 5, CtGet(cr, b + 5) | 0x80000000UL);
}

/* 비교 다시 시작(키보드 새로 띄움 등) */
static inline void CtReset(UInt32 cr, UInt16 b)
{
    FtrSet(cr, b + 4, 0);
}

/* 지금 입력 초점이 있는 입력칸(폼의 필드만, 표 안 필드 등은 NULL) */
static inline FieldType *CtField(void)
{
    FormType *f = FrmGetActiveForm();
    UInt16    i;

    if (f == NULL)
        return NULL;
    i = FrmGetFocus(f);
    if (i == noFocus || FrmGetObjectType(f, i) != frmFieldObj)
        return NULL;
    return (FieldType *)FrmGetObjectPtr(f, i);
}

/* 키를 누를 때 부른다. 조합 중(composing)인데 커서가 옮겨졌거나 입력칸이 바뀌었으면 true(조합을 끝낼 것) */
static inline Boolean CtCheck(UInt32 cr, UInt16 b, Boolean composing)
{
    FieldType *fld = CtField();
    UInt32     pf = CtGet(cr, b + 4), pp = CtGet(cr, b + 5);
    UInt16     pos;
    Boolean    end = false;

    if (fld == NULL) {
        FtrSet(cr, b + 4, 0);
        return false;
    }
    pos = FldGetInsPtPosition(fld);
    if (pf != 0 && pf != (UInt32)fld && composing)
        end = true;   /* 다른 입력칸으로 옮김 */
    if (pf == (UInt32)fld && (pp & 0x80000000UL) == 0) {
        UInt16 e = (UInt16)((Int32)(pp & 0xFFFF) + (Int32)CtGet(cr, b + 6));

        MN_D(FtrSet(cr, b + 0, CtGet(cr, b + 0) + 1));
        if (e != pos) {
            MN_D(FtrSet(cr, b + 1, CtGet(cr, b + 1) + 1));
            if (composing)
                end = true;   /* 커서가 옮겨짐 — 옛 조합 글자를 고치지 않는다 */
        }
        MN_D(FtrSet(cr, b + 2, ((UInt32)e << 16) | pos));
        MN_D(FtrSet(cr, b + 3, (e != pos ? 1UL : 0) | (composing ? 2UL : 0)));
    }
    FtrSet(cr, b + 4, (UInt32)fld);
    FtrSet(cr, b + 5, pos);
    FtrSet(cr, b + 6, 0);
    return end;
}

#else   /* PC 조합 시험 */
#define CtDelta(cr, b, c)       ((void)0)
#define CtUnknown(cr, b)        ((void)0)
#define CtReset(cr, b)          ((void)0)
#define CtCheck(cr, b, comp)    (false)
#endif

#endif
