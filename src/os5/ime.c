/*
 * MnHangul5 입력 — 표 8 EvtGetEvent(0x824)를 바꿔 TG50 키보드의 영문 글자를 두벌식으로 조합한다.
 * 68k 앱(PACE 연결 함수 경유)과 ARM 앱의 EvtGetEvent가 모두 여기로 온다.
 *
 *   - TG50 키는 keyDown 3개로 온다: 0x1720(누름, keyCode = 키) → 글자(SysHandleEvent가 만듦) → 0x1721(뗌).
 *   - 한/영 = Shift+Space. 글자 이벤트에는 Shift 표시가 없어(Space 0x20, modifiers 0) 바로 앞 0x1720의 keyCode 0x8420으로 판단.
 *   - 한글 모드: 글자 키는 nilEvent로 바꾸고, 조합 결과를 BS + 완성형 2바이트로 EvtEnqueueKey(keyCode kImMark).
 *     소문자 = 기본 자모, 대문자(Shift) = 쌍자음·ㅒ·ㅖ. 조합은 OS 4와 같은 src/common/hangul_compose.h.
 *   - '한' 표시: 입력칸 화면의 그래피티 상태 표시(GSI) 자리에 그린다(GSI가 없는 화면은 표시 없음).
 *     앱이 다 그리고 다음 이벤트를 받으러 올 때(원래 함수 부르기 전) 그린다.
 */
#include "os5.h"

#define kImMark 0x4D48   /* 우리가 넣은 키의 keyCode ('MH', OS 4 MnSonyKbd와 같음) */
#define kKeyShiftSpace 0x8420
#define nilEvent 0
#define penDownEvent 1
#define penUpEvent 2
#define winEnterEvent 3
#define keyDownEvent 4
#define frmOpenEvent 24
#define frmUpdateEvent 26
#define commandKeyMask 0x0008
#define chrBackspace 0x08
#define frmGraffitiStateObj 11

/* hangul_compose.h가 쓰는 Palm 형식 */
typedef u8 UInt8;
typedef u16 UInt16;
typedef u32 UInt32;
typedef s16 Int16;
typedef u32 LocalID;
#define true 1
#define false 0
/* 조합 상태의 앱 구분은 하지 않음(앱이 바뀌어도 펜 누름·글자 아닌 키에서 조합이 끝난다) */
#define SysCurAppDatabase(cardP, idP) (*(idP) = 0, *(cardP) = 0)

struct NEvent {          /* ARM EventType (68k와 같은 오프셋, 리틀엔디언) */
    u16 eType;
    u8 penDown, tapCount;
    s16 screenX, screenY;
    u16 chr, keyCode, modifiers;
};
struct NRect { s16 x, y, w, h; };
typedef void (*GetEventFn)(struct NEvent *, s32);
typedef u16 (*EnqueueKeyFn)(u16, u16, u16);
typedef void *(*PtrFn)(void);
typedef void *(*FormPtrFn)(void *);
typedef u16 (*FormU16Fn)(void *);
typedef u16 (*FormIdxFn)(void *, u16);
typedef void (*FormBoundsFn)(void *, u16, struct NRect *);
typedef void (*EraseFn)(const struct NRect *, u16);
typedef u8 (*FontFn)(u8);

/* st[0..1] 조합(hangul_compose.h), st[2] 한글 모드, st[3] Shift+Space 대기, st[4] 넣은 키 수, st[5] 앱에 간 수,
   st[6] 미룬 횟수, st[7] 불린 수(진단) */
static inline void HcSend(u16 c)
{
    u32 *st = CtxPtr()->st;
    if (((EnqueueKeyFn)T8(T8_EvtEnqueueKey))(c, kImMark, 0) == 0) st[4]++;
}
#define MNSK_HOST_TEST 1   /* 표는 .text 어셈블리 대신 ctx->ksc */
static inline const u8 *KscBitsPtr(void) { return CtxPtr()->ksc; }
static inline const u8 *KscCumPtr(void) { return CtxPtr()->ksc + 1398; }
#define HC_FN static
#include "../common/hangul_compose.h"
#include "../common/dubeol_map.h"

#if MN_DIAG
/* 최근 키 24개: 글자<<16 | modifiers<<8 | 한글 모드<<7 | 처리(1 우리 키, 2 원시 0x1720(글자 자리 = keyCode), 3 한/영,
   4 미룸, 5 그대로, 6 글자 아님, 7 조합) */
static void LogC(u16 chr, const struct NEvent *ev, u32 act)
{
    struct Ctx *c = CtxPtr();
    c->ring[c->rpos % 24] = ((u32)chr << 16) | ((ev->modifiers & 0xff) << 8) | ((c->st[2] & 1) << 7) | act;
    c->rpos++;
}
#else
#define LogC(chr, ev, act) ((void)0)
#endif
#define Log(ev, a) LogC((ev)->chr, ev, a)

/* '한' 표시: 활성 폼의 GSI 자리에 그리거나(han) 지운다. 메뉴·팝업이 위에 있으면 그리지 않음 */
static void Mark(u32 *mark, u32 han)
{
    void *f = ((PtrFn)T12(T12_FrmGetActiveForm))(), *w, *old;
    u16 n, i;
    struct NRect r;

    if (!f) return;
    w = ((FormPtrFn)T12(T12_FrmGetWindowHandle))(f);
    if (!w || ((PtrFn)T8(T8_WinGetActiveWindow))() != w) return;
    n = ((FormU16Fn)T12(T12_FrmGetNumberOfObjects))(f);
    for (i = 0; i < n; i++)
        if (((FormIdxFn)T12(T12_FrmGetObjectType))(f, i) == frmGraffitiStateObj) break;
    if (i == n) return;
    ((FormBoundsFn)T12(T12_FrmGetObjectBounds))(f, i, &r);
    old = ((FormPtrFn)T8(T8_WinSetDrawWindow))(w);
    if (han) {
        char t[2];
        u8 of;

        t[0] = (char)0xC7;   /* 한 (KSC). 스택에 만듦: 상수 데이터 금지 */
        t[1] = (char)0xD1;
        of = ((FontFn)T8(T8_FntSetFont))(0);   /* 보통 글꼴(16×16)으로: 앱 글꼴이 크면 24×24가 GSI 칸을 넘음 */
        ((EraseFn)T8(T8_WinEraseRectangle))(&r, 0);
        HgDrawChars(t, 2, r.x + r.w - HW, r.y);
        ((FontFn)T8(T8_FntSetFont))(of);
        *mark |= 4;
    } else if (*mark & 4) {
        ((EraseFn)T8(T8_WinEraseRectangle))(&r, 0);
        *mark &= ~4u;
    }
    ((FormPtrFn)T8(T8_WinSetDrawWindow))(old);
}

void HgGetEvent(struct NEvent *ev, s32 timeout)
{
    struct Ctx *c = CtxPtr();
    u32 *st = c->st;
    u16 ch;
    u8 j;

    if (c->idle) {   /* OFF(표를 되돌릴 수 없을 때): '한'만 지우고 원래 함수로 */
        if (c->mark & 4)
            Mark(&c->mark, 0);
        c->mark = 0;
        st[0] = 0;
        ((GetEventFn)ORIG(O_GetEvent))(ev, timeout);
        return;
    }
    if (c->mark & 1) {
        c->mark &= ~1u;
        Mark(&c->mark, st[2]);
    }
    ((GetEventFn)ORIG(O_GetEvent))(ev, timeout);
#if MN_DIAG
    st[7]++;
#endif
    if (!ev) return;
    if ((st[2] || (c->mark & 4)) &&
        (ev->eType == keyDownEvent || ev->eType == penUpEvent || ev->eType == winEnterEvent ||
         ev->eType == frmOpenEvent || ev->eType == frmUpdateEvent))
        c->mark |= 1;   /* 앱이 다시 그렸을 수 있음 → 다음에 '한' 다시 그림 */
    if (ev->eType == penDownEvent) { st[0] = 0; return; }   /* 커서가 옮겨질 수 있음: 조합 끝 */
    if (ev->eType != keyDownEvent) return;
    ch = ev->chr;
    if (ev->keyCode == kImMark) { st[5]++; Log(ev, 1); return; }
    if (ch == 0x1720 || ch == 0x1721 || ch == 0x012D) {   /* 원시 키 이벤트: SysHandleEvent가 글자로 바꿈 */
        if (ch == 0x1720) {
            st[3] = (ev->keyCode == kKeyShiftSpace);
            LogC(ev->keyCode, ev, 2);
        }
        return;
    }
    if (ch == ' ' && st[3]) {   /* Shift+Space: 한/영 */
        st[3] = 0;
        st[2] = !st[2];
        st[0] = 0;
        c->mark |= 1;
        Log(ev, 3);
        ev->eType = nilEvent;
        return;
    }
    st[3] = 0;
    if (st[4] != st[5] && st[6] < 20) {   /* 앞 키로 넣은 BS·글자가 아직 앱에 안 감: 이 키를 큐 뒤로 미룸 */
        ((EnqueueKeyFn)T8(T8_EvtEnqueueKey))(ch, ev->keyCode, ev->modifiers);
        st[6]++;
        Log(ev, 4);
        ev->eType = nilEvent;
        return;
    }
    st[5] = st[4];
    st[6] = 0;
    if ((ev->modifiers & commandKeyMask) || !st[2]) {
        st[0] = 0;
        Log(ev, 5);
        return;
    }
    if (ch == chrBackspace) {   /* 조합 중이면 자모 하나 되돌림 */
        if (ComposeBack(st)) ev->eType = nilEvent;
        return;
    }
    j = ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) ? Dubeol((u8)(ch ^ 0x20)) : 0;
    if (j == 0) {
        st[0] = 0;
        Log(ev, 6);
        return;
    }
    Compose(st, (u8)(j - 0xA0));
    Log(ev, 7);
    ev->eType = nilEvent;
}
