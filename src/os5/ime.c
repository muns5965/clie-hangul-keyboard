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
 *   v17: 키보드 대화상자 아래 단추 줄의 가장 오른쪽 단추(Int'l) 오른쪽에 한/영 칸('한'/'A', OS 4 MnKbdHan과 같은 자리)을
 *     그리고, 누르면 한/영을 바꾼다 — 하드웨어 키보드가 없는 VZ90용(TG50·NX70V에서도 쓸 수 있음). 바꾼 뒤 키 글자는
 *     FrmUpdateForm(frmUpdateEvent)으로 대화상자가 다시 그리게 한다.
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

/* v17: 한/영 칸 — 키보드 대화상자(활성 폼·활성 창)의 아래 단추 줄(초점 입력칸 아래에서 가장 왼쪽 단추 = Done과 같은 y)의
 *   가장 오른쪽 단추(Int'l) 오른쪽. 창 밖으로 나가거나 다른 단추와 겹치면 0(칸 없음).
 *   VZ90 실측(표준 좌표): Done 5,126 · abc 49 · 123 69 · Int'l 89~107, 오른쪽 142,124·142,132에 작은 단추 둘. */
#define kTogW 14
#define kTogGap 4
#define kWinW 160
static int KbdToggleRect(void *f, struct NRect *t)
{
    struct NRect r;
    u16 fo, n, i;
    s16 top, row = -1, left = 0x7FFF, right = -1;

    if (!f || ((FormU16Fn)T12(T12_FrmGetFormId))(f) != kKeyboardFormId) return 0;
    if (((PtrFn)T8(T8_WinGetActiveWindow))() != ((FormPtrFn)T12(T12_FrmGetWindowHandle))(f)) return 0;
    fo = ((FormU16Fn)T12(T12_FrmGetFocus))(f);
    n = ((FormU16Fn)T12(T12_FrmGetNumberOfObjects))(f);
    if (fo >= n || ((FormIdxFn)T12(T12_FrmGetObjectType))(f, fo) != frmFieldObj) return 0;
    ((FormBoundsFn)T12(T12_FrmGetObjectBounds))(f, fo, &r);
    top = (s16)(r.y + r.h);
    for (i = 0; i < n; i++) {   /* 가장 왼쪽 단추(Done)의 y = 아래 단추 줄 */
        if (((FormIdxFn)T12(T12_FrmGetObjectType))(f, i) != frmControlObj) continue;
        ((FormBoundsFn)T12(T12_FrmGetObjectBounds))(f, i, &r);
        if (r.y >= top && r.x < left) { left = r.x; row = r.y; }
    }
    for (i = 0; i < n; i++) {
        if (((FormIdxFn)T12(T12_FrmGetObjectType))(f, i) != frmControlObj) continue;
        ((FormBoundsFn)T12(T12_FrmGetObjectBounds))(f, i, &r);
        if (r.y == row && r.x + r.w > right) {
            right = (s16)(r.x + r.w);
            t->y = r.y;
            t->h = r.h;
        }
    }
    if (right < 0 || right + kTogGap + kTogW > kWinW) return 0;
    t->x = (s16)(right + kTogGap);
    t->w = kTogW;
    for (i = 0; i < n; i++) {   /* 다른 단추와 겹치면 칸 없음 */
        if (((FormIdxFn)T12(T12_FrmGetObjectType))(f, i) != frmControlObj) continue;
        ((FormBoundsFn)T12(T12_FrmGetObjectBounds))(f, i, &r);
        if (r.x < t->x + t->w && t->x < r.x + r.w && r.y < t->y + t->h && t->y < r.y + r.h) return 0;
    }
    return 1;
}

static void KbdToggleDraw(u32 han)
{
    void *f = ((PtrFn)T12(T12_FrmGetActiveForm))(), *old;
    struct NRect r;
    u8 of;
    s16 ty;

    if (!KbdToggleRect(f, &r)) return;
    old = ((FormPtrFn)T8(T8_WinSetDrawWindow))(((FormPtrFn)T12(T12_FrmGetWindowHandle))(f));
    of = ((FontFn)T8(T8_FntSetFont))(0);
    ty = (s16)(r.y + (r.h - 11) / 2);
    ((EraseFn)T8(T8_WinEraseRectangle))(&r, 0);
    ((void (*)(u16, const struct NRect *))T8(T8_WinDrawRectangleFrame))(1, &r);   /* simpleFrame */
    if (han) {
        char t[2];

        t[0] = (char)0xC7;   /* 한 */
        t[1] = (char)0xD1;
        HgDrawChars(t, 2, (s16)(r.x + (r.w - HW) / 2), ty);
    } else {
        char a = 'A';
        s16 w = ((CharsWidthFn)ORIG(O_CharsWidth))(&a, 1);

        ((DrawCharsFn)ORIG(O_DrawChars))(&a, 1, (s16)(r.x + (r.w - w) / 2), ty);
    }
    ((FontFn)T8(T8_FntSetFont))(of);
    ((FormPtrFn)T8(T8_WinSetDrawWindow))(old);
}

/* v17: 대화상자의 shift 키 — 아래 단추 줄 바로 위 키 줄(24 이내)의 왼쪽 끝(x < 32). VZ90 실측: shift 0~31, y 103~115(창 기준) */
static int KbdShiftTap(const struct NEvent *ev)
{
    struct NRect r;

    if (!KbdToggleRect(((PtrFn)T12(T12_FrmGetActiveForm))(), &r)) return 0;
    return ev->screenX >= 0 && ev->screenX < 32 && ev->screenY < r.y && ev->screenY >= r.y - 24;
}

static int KbdToggleTap(const struct NEvent *ev)
{
    struct NRect r;

    if (!KbdToggleRect(((PtrFn)T12(T12_FrmGetActiveForm))(), &r)) return 0;
    return ev->screenX >= r.x && ev->screenX < r.x + r.w && ev->screenY >= r.y && ev->screenY < r.y + r.h;
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

/* '한' 표시: 활성 폼의 GSI 자리에 그리거나(han) 지운다. 메뉴·팝업이 위에 있으면 그리지 않음.
 *   v18: 키보드 대화상자에 한/영 칸을 그릴 수 있으면 그 칸이 상태를 보이므로 GSI '한'은 그리지 않는다(같은 정보가 두 번 보였음).
 *   칸을 그릴 수 없는 대화상자(창 밖·겹침)에서는 지금처럼 GSI에 그린다. */
static void Mark(u32 *mark, u32 han)
{
    void *f = ((PtrFn)T12(T12_FrmGetActiveForm))(), *w, *old;
    u16 n, i;
    struct NRect r;

    if (!f) return;
    w = ((FormPtrFn)T12(T12_FrmGetWindowHandle))(f);
    if (KbdToggleRect(f, &r)) {   /* v18: 한/영 칸이 상태를 보임 → GSI '한'은 그리지 않음 */
        /* v19: 이 대화상자 창에 우리가 그린 '한'이 남아 있으면(칸을 그릴 수 없던 때 그림) 그 자리만 지운다.
         *   다른 창(메모 화면 등)에 그린 표시는 그 창의 것이라 건드리지 않는다(대화상자 GSI의 shift 표시를 지우지 않게). */
        if ((*mark & 4) && CtxPtr()->markwin == (u32)w)
            han = 0;
        else {
            *mark &= ~4u;
            return;
        }
    }
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
        CtxPtr()->markwin = (u32)w;
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
    if (c->kbd) {   /* v17: 대화상자가 다 그린 뒤 한/영 칸 */
        c->kbd = 0;
        KbdToggleDraw(st[2]);
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
    if (ev->eType == frmOpenEvent || ev->eType == frmUpdateEvent || ev->eType == winEnterEvent ||
        ev->eType == penUpEvent || ev->eType == keyDownEvent)
        c->kbd = 1;
    if (ev->eType == frmOpenEvent && ev->chr == kKeyboardFormId)   /* chr 자리 = frmOpen.formID */
        c->kshift = 0;   /* v18: 앞 대화상자에서 shift만 누르고 닫은 경우 — 새 화면은 손으로 누른 shift 없음(v19: 키보드 대화상자만) */   /* v17: 키보드 대화상자면 다음에 한/영 칸을 (다시) 그림(아니면 KbdToggleRect가 0) */
    if (ev->eType == penDownEvent && KbdToggleTap(ev)) {   /* v17: 한/영 칸 */
        st[2] = !st[2];
        st[0] = 0;
        c->kshift = 0;
        c->mark |= 1;
        c->kbd = 1;
        ((void (*)(u16, u16))T12(T12_FrmUpdateForm))(kKeyboardFormId, 0x8000);   /* frmRedrawUpdateCode: 키 글자 다시 */
        LogC(0, ev, 3);
        ev->eType = nilEvent;
        return;
    }
    if (ev->eType == penDownEvent && st[2] && KbdShiftTap(ev))
        c->kshift = 1;   /* v17: 손으로 누른 shift(대화상자 상태가 대문자일 때만 쓰임 — 두 번 눌러 푼 경우는 소문자가 옴) */
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
    /* v17: 키보드 대화상자의 대문자(원시 키 없음)는 shift를 손으로 눌렀을 때만 그대로, 아니면(자동 대문자·cap) 기본 자모 */
    if (st[2] && st[3] == 0 && ch >= 'A' && ch <= 'Z' && !c->kshift && KbdToggleRect(((PtrFn)T12(T12_FrmGetActiveForm))(), &(struct NRect){0, 0, 0, 0}))
        ch |= 0x20;
    if (st[3] == 0 && ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')))
        c->kshift = 0;
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
