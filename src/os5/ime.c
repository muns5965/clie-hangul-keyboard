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
 *   - 펜 누름은 커서를 옮길 수 있어 조합을 끝낸다. v15: 시스템 키보드 대화상자(Edit → Keyboard, 폼 32000)의 키 영역
 *     (초점 입력칸 아래 ~ 그 아래 단추 줄 위)을 누른 것은 키 입력이므로 조합을 이어 간다(KbdKeyTap).
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
#define frmFieldObj 0
#define frmControlObj 1
#define kKeyboardFormId 32000   /* 시스템 키보드 대화상자(TG50 실측: 제목 Keyboard, 초점 = 입력칸) */

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

/* st[0..1] 조합(hangul_compose.h), st[2] 한글 모드, st[3] 바로 앞 원시 keyCode, st[4] 넣은 키 수, st[5] 앱에 간 수,
   st[6] 미룬 횟수, st[7] 불린 수(진단) */
static inline void HcSend(u16 c)
{
    struct Ctx *x = CtxPtr();
    if (((EnqueueKeyFn)T8(T8_EvtEnqueueKey))(c, kImMark, 0) == 0) x->st[4]++;
    else x->qfail++;   /* 큐가 가득 참: 화면과 조합 상태가 어긋나므로 이 키 처리 뒤 조합을 끝낸다(v5) */
}
/* v15: 펜 누름이 시스템 키보드 대화상자의 키 영역인가. 대화상자가 활성 폼이고 그 창이 활성 창(메뉴 등이 위에 없음)이며,
 *   누른 곳(창 기준 좌표 = 객체 좌표)이 초점 입력칸의 아래 끝 이상이고 그 아래 단추(Done·abc·123 등) 중 가장 위 단추의 윗변 미만.
 *   경계는 폼 객체 위치에서 계산한다(기종마다 크기가 다를 수 있음). 하나라도 아니면 0 = 지금처럼 조합 끝. */
static int KbdKeyTap(const struct NEvent *ev)
{
    void *f = ((PtrFn)T12(T12_FrmGetActiveForm))();
    struct NRect r;
    u16 fo, n, i;
    s16 top, bottom = 0x7FFF;

    if (!f || ((FormU16Fn)T12(T12_FrmGetFormId))(f) != kKeyboardFormId) return 0;
    if (((PtrFn)T8(T8_WinGetActiveWindow))() != ((FormPtrFn)T12(T12_FrmGetWindowHandle))(f)) return 0;
    fo = ((FormU16Fn)T12(T12_FrmGetFocus))(f);
    n = ((FormU16Fn)T12(T12_FrmGetNumberOfObjects))(f);
    if (fo >= n || ((FormIdxFn)T12(T12_FrmGetObjectType))(f, fo) != frmFieldObj) return 0;
    ((FormBoundsFn)T12(T12_FrmGetObjectBounds))(f, fo, &r);
    top = (s16)(r.y + r.h);
    if (ev->screenY < top) return 0;
    for (i = 0; i < n; i++) {
        if (((FormIdxFn)T12(T12_FrmGetObjectType))(f, i) != frmControlObj) continue;
        ((FormBoundsFn)T12(T12_FrmGetObjectBounds))(f, i, &r);
        if (r.y >= top && r.y < bottom) bottom = r.y;
    }
    return ev->screenY < bottom;
}

/* 조합 함수를 부른 뒤: 결과 키를 하나라도 못 넣었으면 조합 끝(다음 키부터 새 글자) */
static inline void EndIfQFail(u32 *st, u32 before)
{
    if (CtxPtr()->qfail != before) st[0] = 0;
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
    if (ev->eType == penDownEvent) {   /* 커서가 옮겨질 수 있음: 조합 끝(v15: 키보드 대화상자의 키는 이어 감) */
        if (!KbdKeyTap(ev)) st[0] = 0;
        return;
    }
    if (ev->eType != keyDownEvent) return;
    ch = ev->chr;
    if (ev->keyCode == kImMark) { st[5]++; Log(ev, 1); return; }
    if (ch == 0x1720 || ch == 0x1721 || ch == 0x012D) {   /* 원시 키 이벤트: SysHandleEvent가 글자로 바꿈 */
        if (ch == 0x1720) {
            st[3] = ev->keyCode;   /* 원시 keyCode: Shift 있으면 0x84xx (v7) */
            LogC(ev->keyCode, ev, 2);
        }
        return;
    }
    if (ch == ' ' && st[3] == kKeyShiftSpace) {   /* Shift+Space: 한/영 */
        st[3] = 0;
        st[2] = !st[2];
        st[0] = 0;
        c->mark |= 1;
        Log(ev, 3);
        ev->eType = nilEvent;
        return;
    }
    /* v7: 자동 대문자(Address 이름 칸 첫 글자 등) — 원시 keyCode에 Shift가 없는데 대문자가 오면
     * 한글 모드에서는 소문자로 본다(아니면 두벌식 Shift 규칙으로 된소리가 됨). 미루기 전에 고쳐 둔다. */
    if (st[2] && ch >= 'A' && ch <= 'Z' && st[3] != 0 && (st[3] & 0xFF00) == 0)
        ch |= 0x20;
    st[3] = 0;
    /* 앞 키로 넣은 BS·글자가 아직 앱에 안 감: 이 키를 큐 뒤로 미룸.
     * v5: 큐에 못 넣으면(가득 참) 미루지 않고 그대로 처리 — 키가 사라지는 것보다 순서가 바뀌는 쪽이 낫다 */
    if (st[4] != st[5] && st[6] < 20 &&
        ((EnqueueKeyFn)T8(T8_EvtEnqueueKey))(ch, ev->keyCode, ev->modifiers) == 0) {
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
        u32 qf = c->qfail;

        if (ComposeBack(st)) ev->eType = nilEvent;
        EndIfQFail(st, qf);
        return;
    }
    j = ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) ? Dubeol((u8)(ch ^ 0x20)) : 0;
    if (j == 0) {
        st[0] = 0;
        Log(ev, 6);
        return;
    }
    {
        u32 qf = c->qfail;

        Compose(st, (u8)(j - 0xA0));
        EndIfQFail(st, qf);
    }
    Log(ev, 7);
    ev->eType = nilEvent;
}
