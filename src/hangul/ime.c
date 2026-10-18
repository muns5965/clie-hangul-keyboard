/*
 * MnHangul Sony 가상 키보드 한글 입력 (리소스 HgIm 0) — v35
 *
 * EvtGetEvent를 바꿔, 앱이 받는 영문 글자 키를 두벌식 자모로 바꾸고 조합한다.
 *   - 한글 모드 = 받는 순간 쉬프트 고정(⇪) 켜짐 (v34 실측: ⇪ = g1000, ⇪+쉬프트 소문자도 받는 순간 g1000,
 *     영문 소문자는 g0000). 배치는 dubeol.h(오버레이와 같은 표).
 *   - 원래 키 이벤트는 nilEvent로 바꾸고, 조합 결과는 BS + 완성형 2바이트 글자로 EvtEnqueueKey
 *     (MnSkCore와 같은 방식). 우리가 넣은 키는 keyCode kImMark로 표시해 다시 바꾸지 않는다.
 *   - 조합 끝: 글자 아닌 키, ⇪ 꺼짐, 명령 키(홈 등), 앱 영역 펜 누름(y < 160), 다른 앱.
 *   - BS: 조합 중이면 자모 하나씩 되돌림(MnSkCore와 같음), 아니면 그대로.
 * 조합 함수(KscJamo ~ SylKsc, Compose, ComposeBack)는 src/skwr/skwr.c(MnSkCore)에서 복사했다.
 *   고칠 때는 두 곳을 같이 고친다.
 *
 * v43: WinDrawBitmap도 바꾼다. SilkExtn이 ⇪ 키 그림(ROM Tbmp 2400 보통 / 2500 반전 = 쉬프트 고정 켜짐)을 그리면
 *   (1) 그 그림으로 한글 모드(#143)를 정하고(Sony 키보드의 실제 ⇪ 상태 — 새 메모 등 시스템이 바꿔도 맞음),
 *   (2) 대신 '한A' 그림(MnHangul HgIc 0·1, 22×18, 2픽셀 왼쪽)을 그린다.
 *   기록: #146 = ⇪ 그림 그린 순간의 다른 창 글자 그리기 수(#108, 키 글자보다 먼저인지 확인용), #147 = 횟수
 *
 * 리소스 배치: 오프셋 0·4 = 진입점 오프셋(EvtGetEvent, WinDrawBitmap), 8·12 = 원래 주소, 16 = 상태 버퍼 주소,
 *   20·24 = ROM ⇪ 그림 주소(보통·반전), 28·32 = 대신 그릴 그림 주소(보통·켜짐) — 모두 앱이 DmWrite.
 * 상태 버퍼(동적 힙, 소유자 0): [0] 조합(초성<<24 | 중성<<16 | 종성<<8 | 화면에 나간 글자 수),
 *   [1] 조합 중인 앱 LocalID, [2] 바꾼 키 수, [3] 마지막 바꾼 키(원래 글자<<16 | 자모 바이트)
 * 어느 앱에서든 불리므로 전역변수·A5·재배치·문자열 상수 금지(빌드 스크립트가 검사).
 */

#include <PalmOS.h>

#define kImMark 0x4D48   /* 우리가 넣은 키의 keyCode ('MH') */

void HgImGet(EventType *event, Int32 timeout);
void HgImBmp(BitmapPtr bmp, Coord x, Coord y);

asm(".text\n.even\nImBase:\n"
    " .long HgImGet-ImBase, HgImBmp-ImBase\n"
    "ImOrig:\n .long 0,0\nImBuf:\n .long 0\nImIcon:\n .long 0,0,0,0\n");

typedef void (*GetFn)(EventType *, Int32);
typedef void (*BmpFn)(BitmapPtr, Coord, Coord);

static inline const UInt32 *Icons(void)
{
    const UInt32 *p;

    asm("lea ImIcon(%%pc),%0" : "=a" (p));
    return p;
}

static inline BmpFn OrigBmp(void)
{
    const BmpFn *p;

    asm("lea ImOrig+4(%%pc),%0" : "=a" (p));
    return *p;
}

static inline UInt32 *St(void)
{
    UInt32 * const *bp;

    asm("lea ImBuf(%%pc),%0" : "=a" (bp));
    return *bp;
}

static inline GetFn OrigGet(void)
{
    const GetFn *p;

    asm("lea ImOrig(%%pc),%0" : "=a" (p));
    return *p;
}

#include "dubeol.h"

/* ---------------- 한글 조합 ---------------- */
/* 자모는 호환 자모 번호 o = 유니코드 - 0x3130 (1~51) 로 다룬다 */
#define O(u)          ((UInt8)((u) - 0x3130))
#define P(a, b)       (((UInt16)O(a) << 8) | O(b))

static inline UInt16 KscJamo(UInt8 o)
{
    return (UInt16)(0xA4A1 + o - 1);
}

static inline Boolean IsVowel(UInt8 o)
{
    return o >= O(0x314F);
}

/* 초성 번호 0~18, 초성 불가면 -1 */
static inline Int16 ChoIdx(UInt8 o)
{
    switch (o) {
    case O(0x3131): return 0;   case O(0x3132): return 1;   case O(0x3134): return 2;
    case O(0x3137): return 3;   case O(0x3138): return 4;   case O(0x3139): return 5;
    case O(0x3141): return 6;   case O(0x3142): return 7;   case O(0x3143): return 8;
    case O(0x3145): return 9;   case O(0x3146): return 10;  case O(0x3147): return 11;
    case O(0x3148): return 12;  case O(0x3149): return 13;  case O(0x314A): return 14;
    case O(0x314B): return 15;  case O(0x314C): return 16;  case O(0x314D): return 17;
    case O(0x314E): return 18;
    default:        return -1;
    }
}

/* 종성 번호 1~27, 종성 불가면 0 (ㄸ ㅃ ㅉ 은 종성 불가) */
static inline UInt16 JongIdx(UInt8 o)
{
    if (o == 0 || o > O(0x314E))
        return 0;
    switch (o) {
    case O(0x3138): case O(0x3143): case O(0x3149):
        return 0;
    }
    /* 호환 자모 순서에서 ㄸ ㅃ ㅉ 만 빼면 종성 순서(ㄱ=1 ... ㅎ=27)와 같다 */
    {
        UInt16 n = o;

        if (o > O(0x3138)) n--;
        if (o > O(0x3143)) n--;
        if (o > O(0x3149)) n--;
        return n;
    }
}

/* 모음 + 모음 (복합모음, 단모음 연타). 안 되면 0 */
static inline UInt8 VowelPlus(UInt8 a, UInt8 b)
{
    switch (((UInt16)a << 8) | b) {
    case P(0x3157, 0x314F): return O(0x3158);   /* ㅗ+ㅏ ㅘ */
    case P(0x3157, 0x3150): return O(0x3159);   /* ㅗ+ㅐ ㅙ */
    case P(0x3157, 0x3163): return O(0x315A);   /* ㅗ+ㅣ ㅚ */
    case P(0x315C, 0x3153): return O(0x315D);   /* ㅜ+ㅓ ㅝ */
    case P(0x315C, 0x3154): return O(0x315E);   /* ㅜ+ㅔ ㅞ */
    case P(0x315C, 0x3163): return O(0x315F);   /* ㅜ+ㅣ ㅟ */
    case P(0x3161, 0x3163): return O(0x3162);   /* ㅡ+ㅣ ㅢ */
    case P(0x314F, 0x314F): return O(0x3151);   /* ㅏㅏ ㅑ */
    case P(0x3153, 0x3153): return O(0x3155);   /* ㅓㅓ ㅕ */
    case P(0x3157, 0x3157): return O(0x315B);   /* ㅗㅗ ㅛ */
    case P(0x315C, 0x315C): return O(0x3160);   /* ㅜㅜ ㅠ */
    case P(0x3150, 0x3150): return O(0x3152);   /* ㅐㅐ ㅒ */
    case P(0x3154, 0x3154): return O(0x3156);   /* ㅔㅔ ㅖ */
    default:                return 0;
    }
}

/* 모음 되돌리기 (BS). 기본 모음이면 0 */
static inline UInt8 VowelBase(UInt8 v)
{
    switch (v) {
    case O(0x3158): case O(0x3159): case O(0x315A): case O(0x315B): return O(0x3157);
    case O(0x315D): case O(0x315E): case O(0x315F): case O(0x3160): return O(0x315C);
    case O(0x3162): return O(0x3161);
    case O(0x3151): return O(0x314F);
    case O(0x3155): return O(0x3153);
    case O(0x3152): return O(0x3150);
    case O(0x3156): return O(0x3154);
    default:        return 0;
    }
}

/* 겹받침 만들기. 안 되면 0 */
static inline UInt8 JongPlus(UInt8 a, UInt8 b)
{
    switch (((UInt16)a << 8) | b) {
    case P(0x3131, 0x3145): return O(0x3133);   /* ㄳ */
    case P(0x3134, 0x3148): return O(0x3135);   /* ㄵ */
    case P(0x3134, 0x314E): return O(0x3136);   /* ㄶ */
    case P(0x3139, 0x3131): return O(0x313A);   /* ㄺ */
    case P(0x3139, 0x3141): return O(0x313B);   /* ㄻ */
    case P(0x3139, 0x3142): return O(0x313C);   /* ㄼ */
    case P(0x3139, 0x3145): return O(0x313D);   /* ㄽ */
    case P(0x3139, 0x314C): return O(0x313E);   /* ㄾ */
    case P(0x3139, 0x314D): return O(0x313F);   /* ㄿ */
    case P(0x3139, 0x314E): return O(0x3140);   /* ㅀ */
    case P(0x3142, 0x3145): return O(0x3144);   /* ㅄ */
    default:                return 0;
    }
}

/* 겹받침의 앞 자음(홑받침이면 0), 뒤 자음(홑받침이면 자기 자신) */
static inline UInt8 JongFirst(UInt8 t)
{
    switch (t) {
    case O(0x3133): return O(0x3131);
    case O(0x3135): case O(0x3136): return O(0x3134);
    case O(0x313A): case O(0x313B): case O(0x313C): case O(0x313D):
    case O(0x313E): case O(0x313F): case O(0x3140): return O(0x3139);
    case O(0x3144): return O(0x3142);
    default:        return 0;
    }
}

static inline UInt8 JongSecond(UInt8 t)
{
    switch (t) {
    case O(0x3133): case O(0x313D): case O(0x3144): return O(0x3145);
    case O(0x3135): return O(0x3148);
    case O(0x3136): case O(0x3140): return O(0x314E);
    case O(0x313A): return O(0x3131);
    case O(0x313B): return O(0x3141);
    case O(0x313C): return O(0x3142);
    case O(0x313E): return O(0x314C);
    case O(0x313F): return O(0x314D);
    default:        return t;
    }
}

#ifndef MNSK_HOST_TEST
/* 표(KscBits, KscCum)는 파일 끝에서 .text에 넣는다(진입 함수가 code 0 맨 앞이어야 함) */
static inline const UInt8 *KscBitsPtr(void)
{
    const UInt8 *p;

    asm("lea KscBits(%%pc),%0" : "=a" (p));
    return p;
}

static inline const UInt8 *KscCumPtr(void)
{
    const UInt8 *p;

    asm("lea KscCum(%%pc),%0" : "=a" (p));
    return p;
}
#endif

static inline UInt16 Pop8(UInt8 b)
{
    UInt16 n = 0;

    while (b) {
        n += b & 1;
        b >>= 1;
    }
    return n;
}

/* 초성·중성·종성 → KSC5601 코드, 완성형에 없으면 0 */
static inline UInt16 SylKsc(UInt8 cho, UInt8 jung, UInt8 jong)
{
    Int16        ci = ChoIdx(cho);
    UInt16       ti = 0, i, r, b;
    const UInt8 *tb, *cum;

    if (ci < 0 || jung == 0 || !IsVowel(jung))
        return 0;
    if (jong != 0) {
        ti = JongIdx(jong);
        if (ti == 0)
            return 0;
    }
    i = ((UInt16)ci * 21 + (jung - O(0x314F))) * 28 + ti;
    tb = KscBitsPtr();
    if (!(tb[i >> 3] & (0x80 >> (i & 7))))
        return 0;
    cum = KscCumPtr();
    r = ((UInt16)cum[(i >> 8) * 2] << 8) | cum[(i >> 8) * 2 + 1];
    for (b = (i >> 8) << 5; b < (i >> 3); b++)
        r += Pop8(tb[b]);
    r += Pop8(tb[i >> 3] & (UInt8)~(0xFF >> (i & 7)));
    return ((UInt16)(0xB0 + r / 94) << 8) | (0xA1 + r % 94);
}


static inline void SendKey(UInt16 c)
{
    EvtEnqueueKey(c, kImMark, 0);
}

/* 조합 상태를 화면에 보낸다. 보낸 글자 수 반환 */
static inline UInt16 Emit(UInt8 cho, UInt8 jung, UInt8 jong)
{
    UInt16 n = 0;

    if (cho && jung) {
        UInt16 k = SylKsc(cho, jung, jong);

        if (k) {
            SendKey(k);
            return 1;
        }
    }
    if (cho)  { SendKey(KscJamo(cho));  n++; }
    if (jung) { SendKey(KscJamo(jung)); n++; }
    if (jong) { SendKey(KscJamo(jong)); n++; }
    return n;
}

static inline LocalID CurApp(void)
{
    UInt16  card = 0;
    LocalID id = 0;

    SysCurAppDatabase(&card, &id);
    return id;
}

static inline UInt32 GetComp(UInt32 *st)
{
    if (st[0] != 0 && st[1] != (UInt32)CurApp())
        st[0] = 0;
    return st[0];
}

static inline void PutComp(UInt32 *st, UInt8 c, UInt8 v, UInt8 t, UInt16 shown)
{
    st[0] = ((UInt32)c << 24) | ((UInt32)v << 16) | ((UInt32)t << 8) | (UInt8)shown;
    st[1] = (UInt32)CurApp();
}

/* 자모 하나 입력 (MnSkCore Compose와 같음) */
static void Compose(UInt32 *st, UInt8 o)
{
    UInt32  s = GetComp(st);
    UInt8   c0 = (UInt8)(s >> 24), v0 = (UInt8)(s >> 16), t0 = (UInt8)(s >> 8);
    UInt16  shown = (UInt8)s;
    UInt8   c = c0, v = v0, t = t0, nc = 0, nv = 0, x;
    Boolean split = false;

    if (shown == 0)
        c = v = t = 0;

    if (!IsVowel(o)) {
        if (c && v && !t && SylKsc(c, v, o))
            t = o;
        else if (t && (x = JongPlus(t, o)) != 0 && SylKsc(c, v, x))
            t = x;
        else {
            split = true;
            nc = o;
        }
    } else {
        if (c && !v) {
            if (SylKsc(c, o, 0))
                v = o;
            else {
                split = true;
                nv = o;
            }
        } else if (v && !t) {
            x = VowelPlus(v, o);
            if (x && (!c || SylKsc(c, x, 0)))
                v = x;
            else {
                split = true;
                nv = o;
            }
        } else if (t) {
            UInt8 keep = JongFirst(t), mv = JongSecond(t);

            split = true;
            if (SylKsc(mv, o, 0) && (keep == 0 || SylKsc(c, v, keep))) {
                t = keep;
                nc = mv;
            }
            nv = o;
        } else {
            split = true;
            nv = o;
        }
    }

    if (shown != 0 && (c != c0 || v != v0 || t != t0)) {
        UInt16 i;

        for (i = 0; i < shown; i++)
            SendKey(chrBackspace);
        shown = Emit(c, v, t);
    }
    if (split) {
        shown = Emit(nc, nv, 0);
        PutComp(st, nc, nv, 0, shown);
    } else
        PutComp(st, c, v, t, shown);
}

/* 조합 중 BS: 자모 하나 되돌리기. 조합 중이었으면 true(원래 BS는 버림) */
static Boolean ComposeBack(UInt32 *st)
{
    UInt32 s = GetComp(st);
    UInt8  c = (UInt8)(s >> 24), v = (UInt8)(s >> 16), t = (UInt8)(s >> 8);
    UInt16 shown = (UInt8)s, i;

    if (shown == 0) {
        st[0] = 0;
        return false;
    }
    if (t)
        t = JongFirst(t);
    else if (v)
        v = VowelBase(v);
    else
        c = 0;
    for (i = 0; i < shown; i++)
        SendKey(chrBackspace);
    if (c || v || t) {
        shown = Emit(c, v, t);
        PutComp(st, c, v, t, shown);
    } else
        st[0] = 0;
    return true;
}

void HgImGet(EventType *event, Int32 timeout)
{
    UInt32 *st;
    WChar   ch;
    UInt8   j;

    OrigGet()(event, timeout);
    st = St();
    if (st == NULL || event == NULL)
        return;
    if (event->eType == penDownEvent) {          /* 앱 영역을 누르면 커서가 옮겨질 수 있어 조합 끝 */
        UInt32 v = 0;

        if (event->screenY < 160) {
            st[0] = 0;
            FtrSet('MnHg', kFtrPenCaps, 0);   /* v44: 앱 화면 누름도 '마지막 펜이 ⇪ 아님' */
        } else if (kCapsKey(event->screenX, event->screenY)) {   /* v39: ⇪ → 한글 모드 전환 */
            UInt32 sc = 0;

            /* v45: Sony 실제 상태(마지막 자판 전체 그리기의 ⇪ 그림)의 반대로. 모르면 지금 값을 뒤집음 */
            FtrGet('MnHg', kFtrSonyCaps, &sc);
            FtrGet('MnHg', kFtrHan, &v);
            FtrSet('MnHg', kFtrHan, (sc & 0x100) ? !(sc & 1) : (v ? 0 : 1));
            FtrSet('MnHg', kFtrPenCaps, 1);
            v = 0;
            FtrGet('MnHg', 108, &v);
            FtrSet('MnHg', kFtrPenDraw, v);
        } else
            FtrSet('MnHg', kFtrPenCaps, 0);
        return;
    }
    if (event->eType == penUpEvent) {            /* v39: ⇪ 자리를 눌렀는데 키 글자 다시 그리기가 없었으면 되돌림 */
        UInt32 pc = 0, d0 = 0, d1 = 0, v = 0;

        FtrGet('MnHg', kFtrPenCaps, &pc);
        if (pc == 1) {
            FtrGet('MnHg', kFtrPenDraw, &d0);
            FtrGet('MnHg', 108, &d1);
            if (d1 - d0 < 20) {   /* v45: Sony 실제 상태로 되돌림(모르면 다시 뒤집음) */
                UInt32 sc = 0;

                FtrGet('MnHg', kFtrSonyCaps, &sc);
                FtrGet('MnHg', kFtrHan, &v);
                FtrSet('MnHg', kFtrHan, (sc & 0x100) ? (sc & 1) : (v ? 0 : 1));
            }
            FtrSet('MnHg', kFtrPenCaps, 2);   /* 처리함(다시 맞추기 막음은 다음 펜 누름까지 유지) */
        }
        return;
    }
    if (event->eType != keyDownEvent || event->data.keyDown.keyCode == kImMark)
        return;
    ch = event->data.keyDown.chr;
    if ((event->data.keyDown.modifiers & commandKeyMask) != 0 || !HanMode()) {
        st[0] = 0;
        return;
    }
    if (ch == chrBackspace) {
        if (ComposeBack(st))
            event->eType = nilEvent;
        return;
    }
    j = (ch < 0x80) ? Dubeol((UInt8)ch) : 0;
    if (j == 0) {
        st[0] = 0;
        return;
    }
    st[2]++;
    st[3] = ((UInt32)ch << 16) | j;
    Compose(st, (UInt8)(j - 0xA0));
    event->eType = nilEvent;
}

/* v43: ⇪ 키 그림 → 한글 모드 판정 + '한A' 그림 */
void HgImBmp(BitmapPtr bmp, Coord x, Coord y)
{
    const UInt32 *ic = Icons();

    if (bmp != NULL && ic[0] != 0 && ((UInt32)bmp == ic[0] || (UInt32)bmp == ic[1])) {
        UInt16 on = (UInt32)bmp == ic[1] ? 1 : 0;
        UInt32 v = 0, pc = 0, d0 = 0;

        /* v44: ⇪를 누른 직후 키 글자를 그리기 전의 그림은 눌린 키 표시(옛 상태) → 판정에 쓰지 않음 */
        FtrGet('MnHg', kFtrPenCaps, &pc);
        FtrGet('MnHg', kFtrPenDraw, &d0);
        FtrGet('MnHg', 108, &v);
        if (pc == 0 || v - d0 >= 5) {
            FtrSet('MnHg', kFtrHan, on);
            FtrSet('MnHg', kFtrSonyCaps, 0x100 | on);   /* v45 */
        }
        DecLog((2UL << 30) | ((v & 0xFFF) << 18) | ((pc & 3) << 7) |   /* v46 진단 */
               ((pc == 0 || v - d0 >= 5) ? 2 : 0) | on);
        FtrSet('MnHg', 177, 0);   /* 그림 다음 키 글자는 다시 기록 */
        FtrSet('MnHg', 146, v);
        v = 0;
        FtrGet('MnHg', 147, &v);
        FtrSet('MnHg', 147, v + 1);
        if (ic[2 + on] != 0) {
            OrigBmp()((BitmapPtr)ic[2 + on], x - 2, y);
            return;
        }
    }
    OrigBmp()(bmp, x, y);
}

#include "../skwr/ksc_table.h"
