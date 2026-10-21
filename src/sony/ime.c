/*
 * MnSonyKbd Sony 가상 키보드 한글 입력 (리소스 SyIm 0) — MnHangul v35에서 시작, v54에 MnSonyKbd로 분리
 *
 * EvtGetEvent를 바꿔, 앱이 받는 영문 글자 키를 두벌식 자모로 바꾸고 조합한다.
 *   - 한글 모드 = 받는 순간 쉬프트 고정(⇪) 켜짐 (v34 실측: ⇪ = g1000, ⇪+쉬프트 소문자도 받는 순간 g1000,
 *     영문 소문자는 g0000). 배치는 dubeol.h(오버레이와 같은 표).
 *   - 원래 키 이벤트는 nilEvent로 바꾸고, 조합 결과는 BS + 완성형 2바이트 글자로 EvtEnqueueKey
 *     (MnSkCore와 같은 방식). 우리가 넣은 키는 keyCode kImMark로 표시해 다시 바꾸지 않는다.
 *   - 조합 끝: 글자 아닌 키, ⇪ 꺼짐, 명령 키(홈 등), 앱 영역 펜 누름(y < 160), 다른 앱.
 *   - BS: 조합 중이면 자모 하나씩 되돌림(MnSkCore와 같음), 아니면 그대로.
 * 조합 함수(KscJamo ~ SylKsc, Compose, ComposeBack)는 src/common/hangul_compose.h(MnSkCore와 공용).
 *
 * v43: WinDrawBitmap도 바꾼다. SilkExtn이 ⇪ 키 그림(ROM Tbmp 2400 보통 / 2500 반전 = 쉬프트 고정 켜짐)을 그리면
 *   (1) 그 그림으로 한글 모드(#143)를 정하고(Sony 키보드의 실제 ⇪ 상태 — 새 메모 등 시스템이 바꿔도 맞음),
 *   (2) 대신 '한A' 그림(SyIc 0·1, 22×18, 2픽셀 왼쪽)을 그린다.
 *   기록: #146 = ⇪ 그림 그린 순간의 다른 창 글자 그리기 수(#108, 키 글자보다 먼저인지 확인용), #147 = 횟수
 * MnSonyKbd v10: 조합 중 커서 이동 감지(src/common/cursor_track.h, MnSkCore와 공용) — 키를 받을 때 입력칸·커서가
 *   기대와 다르면 조합 끝. 기록 #191~#197.
 * MnSonyKbd v9: ⇪ 그림 때 입력 연결(EvtGetEvent)이 빠졌으면 다시 잇는다(CJKOS 끈 뒤 ON 없이 복구).
 *   진단: #188 입력 함수 불린 수, #189·#190 ⇪ 그림 순간의 EvtGetEvent·BltDrawChars 맨 위 주소
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
    "ImOrig:\n .long 0,0\nImBuf:\n .long 0\nImIcon:\n .long 0,0,0,0,0,0\n");

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
#include "chain.h"

/* ---------------- 한글 조합 (src/common/hangul_compose.h, MnSkCore와 공용) ---------------- */
#define kSyFtrFld 191   /* #191~#197 조합 중 커서 이동 감지(src/common/cursor_track.h, MnSonyKbd v10) */
#include "../common/cursor_track.h"

static inline void HcSend(UInt16 c)
{
    EvtEnqueueKey(c, kImMark, 0);
    CtDelta(kSyCr, kSyFtrFld, c);
}

#define HC_FN static
#include "../common/hangul_compose.h"

void HgImGet(EventType *event, Int32 timeout)
{
    UInt32 *st;
    WChar   ch;
    UInt8   j;

    OrigGet()(event, timeout);
#if MN_DIAG
    {   /* v9 진단: 입력 함수가 불린 횟수(CJKOS가 위에 있을 때 아래로 부르는지 확인용) */
        UInt32 n = 0;

        FtrGet(kSyCr, 188, &n);
        FtrSet(kSyCr, 188, n + 1);
    }
#endif
    st = St();
    if (st == NULL || event == NULL)
        return;
    if (event->eType == penDownEvent) {          /* 앱 영역을 누르면 커서가 옮겨질 수 있어 조합 끝 */
        UInt32 v = 0;

        FtrGet(kSyCr, 250, &v);   /* v27 진단: 한/영 누름 뒤 첫 펜 누름 2개(위치 상관없이) */
        if (v != 0) {
            FtrSet(kSyCr, v == 2 ? 248 : 249, ((UInt32)(UInt16)event->screenX << 16) | (UInt16)event->screenY);
            FtrSet(kSyCr, 250, v - 1);
        }
        v = 0;

        if (event->screenY >= 160) {
            SyBltHeal();   /* MnSonyKbd v6: 가상 실크를 누르면 Sony가 다시 그리기 전에 키 글자 연결부터 확인·복구 */
            {   /* v20 진단: 가상 실크 펜 누름 3개 */
                UInt32 k = 0;

                FtrGet(kSyCr, 210, &k);
                if (k > 2)
                    k = 0;
                FtrSet(kSyCr, 211 + (UInt16)k, ((UInt32)(UInt16)event->screenX << 16) | (UInt16)event->screenY);
                FtrSet(kSyCr, 210, k + 1 > 2 ? 0 : k + 1);
            }
            if (kHanTab(event->screenX, event->screenY)) {   /* v20 진단: 한/영 단추 후보 — 시험 표시만 바꾸고 Sony에는 넘기지 않음 */
                FtrGet(kSyCr, kFtrHanTab, &v);
                FtrSet(kSyCr, kFtrHanTab, v ? 0 : 1);
                v = 0;
                FtrGet(kSyCr, 219, &v);
                FtrSet(kSyCr, 219, v + 1);
                v = 0;
                FtrGet(kSyCr, 108, &v);
                FtrSet(kSyCr, 215, v);
                /* v27 진단: v26 실측 — 일시 쉬프트를 바꿔도 다시 그리지 않음(u0). 이번 누름은 버리고, shift 키 두 번 누름 펜 점을
                 *   고해상도 좌표(12,392)로 넣어 본다. 누른 뒤 첫 펜 누름 2개를 위치 상관없이 #248·#249에 기록(#250 = 남은 칸 수) */
                event->eType = nilEvent;
                {
                    PointType pt;
                    Err       e1;

                    pt.x = 12; pt.y = 392;
                    e1 = EvtEnqueuePenPoint(&pt);
                    pt.x = -1; pt.y = -1;
                    EvtEnqueuePenPoint(&pt);
                    pt.x = 12; pt.y = 392;
                    EvtEnqueuePenPoint(&pt);
                    pt.x = -1; pt.y = -1;
                    EvtEnqueuePenPoint(&pt);
                    FtrSet(kSyCr, 248, 0);
                    FtrSet(kSyCr, 249, 0);
                    FtrSet(kSyCr, 250, 2);
                    FtrSet(kSyCr, 243, e1);
                }
                return;
            }
        }
        if (event->screenY < 160) {
            st[0] = 0;
            CtUnknown(kSyCr, kSyFtrFld);     /* v10: 앱 화면 누름은 커서를 옮길 수 있음(조합은 이미 끝) */
            FtrSet(kSyCr, kFtrPenCaps, 0);   /* v44: 앱 화면 누름도 '마지막 펜이 ⇪ 아님' */
        } else if (kCapsKey(event->screenX, event->screenY)) {   /* v39: ⇪ → 한글 모드 전환 */
            UInt32 sc = 0;

            /* v45: Sony 실제 상태(마지막 자판 전체 그리기의 ⇪ 그림)의 반대로. 모르면 지금 값을 뒤집음 */
            FtrGet(kSyCr, kFtrSonyCaps, &sc);
            FtrGet(kSyCr, kFtrHan, &v);
            if (sc & 0x100)
                FtrSet(kSyCr, kFtrHan, !(sc & 1));
            else {   /* v7: 모르면(시작 직후·연결 복구 직후) 지금 Graffiti ⇪ 상태의 반대(그리기 전이라 지금 상태 = Sony 상태) */
                Boolean caps = false, num = false, autoS = false;
                UInt16  temp = 0;

                GrfGetState(&caps, &num, &temp, &autoS);
                FtrSet(kSyCr, kFtrHan, caps ? 0 : 1);
            }
            FtrSet(kSyCr, kFtrPenCaps, 1);
            v = 0;
            FtrGet(kSyCr, 108, &v);
            FtrSet(kSyCr, kFtrPenDraw, v);
        } else   /* v48: 가상 키보드 일반 키 = 3(키 누름 중 순간 상태로 영문 내림 안 함)
                  * v50: 키 영역(y < 213)만 3. 아래 탭·상태 표시줄(키보드 띄우는 버튼 등)은 0 */
            FtrSet(kSyCr, kFtrPenCaps, event->screenY < kKeyAreaEnd ? 3 : 0);
        return;
    }
    if ((event->eType == penUpEvent || event->eType == penMoveEvent) && kHanTab(event->screenX, event->screenY)) {
        event->eType = nilEvent;                 /* v25: 한/영 자리 누름은 Sony에 넘기지 않음 */
        return;
    }
    if (event->eType == penUpEvent) {            /* v39: ⇪ 자리를 눌렀는데 키 글자 다시 그리기가 없었으면 되돌림 */
        UInt32 pc = 0, d0 = 0, d1 = 0, v = 0;

        FtrGet(kSyCr, kFtrPenCaps, &pc);
        if (pc == 1) {
            FtrGet(kSyCr, kFtrPenDraw, &d0);
            FtrGet(kSyCr, 108, &d1);
            if (d1 - d0 < 20) {   /* v45: Sony 실제 상태로 되돌림(모르면 다시 뒤집음) */
                UInt32 sc = 0;

                FtrGet(kSyCr, kFtrSonyCaps, &sc);
                FtrGet(kSyCr, kFtrHan, &v);
                FtrSet(kSyCr, kFtrHan, (sc & 0x100) ? (sc & 1) : (v ? 0 : 1));
            }
            FtrSet(kSyCr, kFtrPenCaps, 2);   /* 처리함(다시 맞추기 막음은 다음 펜 누름까지 유지) */
        }
        return;
    }
    if (event->eType != keyDownEvent || event->data.keyDown.keyCode == kImMark)
        return;
    ch = event->data.keyDown.chr;
    /* v10: 조합 중인데 커서가 옮겨졌거나 입력칸이 바뀌었으면(펜 없이 앱이 옮긴 경우 등) 조합 끝 */
    if (CtCheck(kSyCr, kSyFtrFld, (GetComp(st) & 0xFF) != 0))
        st[0] = 0;
    if ((event->data.keyDown.modifiers & commandKeyMask) != 0 || !HanMode()) {
        st[0] = 0;
        CtUnknown(kSyCr, kSyFtrFld);   /* 앱에 그대로 넘기는 키: 커서 변화 모름 */
        return;
    }
    if (ch == chrBackspace) {
        if (ComposeBack(st))
            event->eType = nilEvent;
        else
            CtUnknown(kSyCr, kSyFtrFld);
        return;
    }
    j = (ch < 0x80) ? Dubeol((UInt8)ch) : 0;
    if (j == 0) {
        st[0] = 0;
        CtUnknown(kSyCr, kSyFtrFld);
        return;
    }
    st[2]++;
    st[3] = ((UInt32)ch << 16) | j;
    Compose(st, (UInt8)(j - 0xA0));
    event->eType = nilEvent;
}

/* v43: ⇪ 키 그림 → 한글 모드 판정 + '한A' 그림 */
/* v21 진단: 가상 실크(화면 밖 창) 그림 기록 — 탭 줄(abc·int'l·sym)이 그림인지, 어디에 그리는지 */
static void BmpLog(BitmapPtr bmp, Coord x, Coord y)
{
    if (bmp != NULL) {
        WinHandle w = WinGetDrawWindow();

        if (w != NULL && WinGetBitmap(w) != WinGetBitmap(WinGetDisplayWindow())) {
            UInt32 k = 0, n = 0;
            Coord  bw = 0, bh = 0;

            BmpGetDimensions(bmp, &bw, &bh, NULL);
            FtrGet(kSyCr, 220, &k);
            k &= 3;
            FtrSet(kSyCr, 221 + 3 * (UInt16)k, (UInt32)bmp);
            FtrSet(kSyCr, 222 + 3 * (UInt16)k, ((UInt32)(UInt16)bw << 16) | (UInt16)bh);
            FtrSet(kSyCr, 223 + 3 * (UInt16)k, ((UInt32)(UInt16)x << 16) | (UInt16)y);
            FtrSet(kSyCr, 220, (k + 1) & 3);
            FtrGet(kSyCr, 233, &n);
            FtrSet(kSyCr, 233, n + 1);
        }
    }
}

void HgImBmp(BitmapPtr bmp, Coord x, Coord y)
{
    const UInt32 *ic = Icons();

    BmpLog(bmp, x, y);   /* v21 진단 */
    if (0 && bmp != NULL && ic[4] != 0 && ic[5] != 0) {   /* v22: 320×130은 자판이 아니라 Graffiti 판 그림이었음(v22 실측) → 끔 */
        WinHandle w = WinGetDrawWindow();
        Coord     bw = 0, bh = 0;

        BmpGetDimensions(bmp, &bw, &bh, NULL);
        if (bw == 320 && bh == 130 && w != NULL && WinGetBitmap(w) != WinGetBitmap(WinGetDisplayWindow())) {
            UInt32 v = 0;

            OrigBmp()(bmp, x, y);
            FtrGet(kSyCr, kFtrHanTab, &v);
            OrigBmp()((BitmapPtr)ic[v ? 4 : 5], x + 136, y + 108);
            return;
        }
    }

    if (bmp != NULL && ic[0] != 0 && ((UInt32)bmp == ic[0] || (UInt32)bmp == ic[1])) {
        UInt16 on = (UInt32)bmp == ic[1] ? 1 : 0;
        UInt32 v = 0, pc = 0, d0 = 0, last = 0;
        Boolean use, healed;

        /* MnSonyKbd v3: 키 글자 패치가 연결에서 빠졌으면(CJKOS 끄기 등) 다시 잇는다.
         * v6: 방금 이었으면 앞 키 글자 기록(#151)이 없으니 이 그림을 실제 상태로 쓴다(v5 실측: 기준값 S가 옛 값이라
         *     다음 ⇪에서 영문인데 쌍자음 표시) */
        healed = SyBltHeal();
        /* v9: CJKOS를 끄면 입력(EvtGetEvent) 연결도 빠진다(v8 실측: 키 글자만 복구되고 Now: broken, ON = re-attached 1).
         *     이 그림 연결은 CJKOS를 꺼도 남으므로 여기서 입력만 다시 잇는다(ROM이 맨 위일 때만, 그림 연결 자신은 건드리지 않음) */
        SyImHealOne(0);
        /* v9 진단: 이 순간의 맨 위 주소(입력·키 글자) */
#if MN_DIAG
        FtrSet(kSyCr, 189, (UInt32)SysGetTrapAddress(sysTrapEvtGetEvent));
        FtrSet(kSyCr, 190, (UInt32)SysGetTrapAddress(kSyTrapBlt));
#endif

        /* v47: 자판 전체 그리기에서 ⇪ 그림은 2줄 끝 키(\ 또는 |) 바로 다음(v46 실측). 그때만 Sony 실제 상태로 쓴다.
         *      ⇪ 누를 때의 눌림 표시(반전 그림, 앱이 펜을 받기 전에 그려짐)는 판정에 쓰지 않는다 */
        FtrGet(kSyCr, kFtrPenCaps, &pc);
        FtrGet(kSyCr, kFtrPenDraw, &d0);
        FtrGet(kSyCr, 108, &v);
        FtrGet(kSyCr, 151, &last);
        use = healed || (last == '\\' || last == '|');
        if (use) {
            FtrSet(kSyCr, kFtrHan, on);
            FtrSet(kSyCr, kFtrSonyCaps, 0x100 | on);   /* v45 */
        }
#if MN_DIAG
        DecLog((2UL << 30) | ((v & 0xFFF) << 18) | ((pc & 3) << 7) |   /* v46 진단 */
               (use ? 2 : 0) | on);
        FtrSet(kSyCr, 177, 0);   /* 그림 다음 키 글자는 다시 기록 */
        FtrSet(kSyCr, 146, v);
        v = 0;
        FtrGet(kSyCr, 147, &v);
        FtrSet(kSyCr, 147, v + 1);
#endif
        if (ic[2 + on] != 0) {
            OrigBmp()((BitmapPtr)ic[2 + on], x - 2, y);
            return;
        }
    }
    OrigBmp()(bmp, x, y);
}

#include "../skwr/ksc_table.h"
