/*
 * 한글 두벌식 조합 (MnSkCore src/skwr/skwr.c, MnSonyKbd src/sony/ime.c 공용) — 두 모듈이 각각 소스로 포함해 따로 빌드한다.
 *
 * 포함하기 전에 정할 것:
 *   - static inline void HcSend(UInt16 c)   조합 결과 키 하나 보내기(모듈마다 keyCode 표시가 다름)
 *   - HC_FN (선택)                          Compose·ComposeBack 선언 앞말. 기본 static inline
 *   - 파일 끝에 KscBits·KscCum 표(src/skwr/ksc_table.h)를 둔다(KscBitsPtr가 pc 상대로 찾음).
 * 조합 상태 st[0] = 초성<<24 | 중성<<16 | 종성<<8 | 화면에 나간 글자 수, st[1] = 조합 중인 앱 LocalID.
 * 어느 앱에서든 불리므로 전역변수·A5·재배치·문자열 상수 금지(각 빌드 스크립트가 검사).
 * 시험: tools/test-compose.sh(MnSkCore를 통해 이 파일을 PC에서 시험).
 */
#ifndef HANGUL_COMPOSE_H
#define HANGUL_COMPOSE_H

#ifndef HC_FN
#define HC_FN static inline
#endif

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

/* 조합 상태를 화면에 보낸다. 보낸 글자 수 반환 */
static inline UInt16 Emit(UInt8 cho, UInt8 jung, UInt8 jong)
{
    UInt16 n = 0;

    if (cho && jung) {
        UInt16 k = SylKsc(cho, jung, jong);

        if (k) {
            HcSend(k);
            return 1;
        }
    }
    if (cho)  { HcSend(KscJamo(cho));  n++; }
    if (jung) { HcSend(KscJamo(jung)); n++; }
    if (jong) { HcSend(KscJamo(jong)); n++; }
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

/* 자모 하나 입력 */
HC_FN void Compose(UInt32 *st, UInt8 o)
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
            HcSend(chrBackspace);
        shown = Emit(c, v, t);
    }
    if (split) {
        shown = Emit(nc, nv, 0);
        PutComp(st, nc, nv, 0, shown);
    } else
        PutComp(st, c, v, t, shown);
}

/* 조합 중 BS: 자모 하나 되돌리기. 조합 중이었으면 true(원래 BS는 버림) */
HC_FN Boolean ComposeBack(UInt32 *st)
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
        HcSend(chrBackspace);
    if (c || v || t) {
        shown = Emit(c, v, t);
        PutComp(st, c, v, t, shown);
    } else
        st[0] = 0;
    return true;
}

#endif
