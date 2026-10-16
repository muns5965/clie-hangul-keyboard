/*
 * v16 진단: 한글 폭을 잴 때·그릴 때의 현재 그리기 창 정보 (patch.c·width.c·intl.c 공용)
 *
 * 목적: 폭 잴 때 글꼴은 늘 저해상도(높이 11)라서, 그릴 때 배율 2(메모 std 22/11)인지
 *       배율 1(키보드 저해상도 프레임 11/11)인지 구별이 안 된다. 창(WindowType) 정보로 구별되는지 본다.
 *
 * 창 4칸(서로 다른 창 주소 순서대로):
 *   #72~#75 창 주소
 *   #76~#79 비트맵 폭 << 16 | 비트맵 rowBytes
 *   #80~#83 windowFlags << 16 | 창 폭(12비트) << 4 | 본 경우(1 폭 잼, 2 그림 ×1, 4 그림 ×2, 8 그림 기타)
 * WindowType(Palm OS 4): +8 windowFlags, +10 windowBounds(x,y,w,h), +26 bitmapP
 * BitmapType: +0 width, +4 rowBytes
 */
#ifndef HG_WINLOG_H
#define HG_WINLOG_H

#define kWinLog   72
#define kWinSeenW 1
#define kWinSeen1 2
#define kWinSeen2 4
#define kWinSeenX 8

/* v17: 현재 그리기 창이 화면 밖(offscreen, windowFlags 0x4000)인가.
 * v16 실측: 키보드 실크 창 C000(offscreen) = Sony가 hires로 바꾸지 않아 ×1로 그림,
 *          일반 화면 창 1A00·0000 = ×2. 저해상도 글꼴(높이 8~15)에서만 쓴다 */
#define kWinOffscreen 0x4000
#define kCell12W      13      /* patch.c kCell12과 같음(12×12 글자 칸, ×1) */

static inline Boolean WinOffscreen(void)
{
    const UInt8 *w = (const UInt8 *)WinGetDrawWindow();

    return w != NULL && (*(const UInt16 *)(w + 8) & kWinOffscreen) != 0;
}

static inline void WinLog(UInt16 seen)
{
    const UInt8 *w = (const UInt8 *)WinGetDrawWindow();
    UInt16       i;

    if (w == NULL)
        return;
    for (i = 0; i < 4; i++) {
        UInt32 k = 0, old = 0;

        FtrGet(kHgCreator, kWinLog + i, &k);
        if (k != 0 && k != (UInt32)w)
            continue;
        {
            const UInt8 *bm = *(const UInt8 * const *)(w + 26);
            UInt16       bw = 0xFFFF, rb = 0xFFFF;
            UInt16       flags = *(const UInt16 *)(w + 8);
            UInt16       ww = *(const UInt16 *)(w + 14);

            if (bm != NULL) {
                bw = *(const UInt16 *)bm;
                rb = *(const UInt16 *)(bm + 4);
            }
            FtrGet(kHgCreator, kWinLog + 8 + i, &old);
            FtrSet(kHgCreator, kWinLog + i, (UInt32)w);
            FtrSet(kHgCreator, kWinLog + 4 + i, ((UInt32)bw << 16) | rb);
            FtrSet(kHgCreator, kWinLog + 8 + i,
                   ((UInt32)flags << 16) | ((UInt32)(ww & 0xFFF) << 4) | ((old | seen) & 0xF));
        }
        return;
    }
}

/* v21 진단: 글꼴 높이·글꼴 번호(FntGetFont: 0 std, 1 bold, 2 large …)별 횟수.
 * 4칸, 칸 = 번호 << 24 | 높이 << 16 | 횟수. 폭 잴 때 #84~#87, 그릴 때(높이 = 그리는 글꼴) #88~#91 */
#define kFidW 84
#define kFidD 88

static inline void FidLog(UInt16 base, UInt16 h)
{
    UInt32 key = ((UInt32)(FntGetFont() & 0xFF) << 24) | ((UInt32)(h & 0xFF) << 16);
    UInt16 i;

    for (i = 0; i < 4; i++) {
        UInt32 v = 0;

        FtrGet(kHgCreator, base + i, &v);
        if (v == 0 || (v & 0xFFFF0000UL) == key) {
            if ((v & 0xFFFF) != 0xFFFF)
                FtrSet(kHgCreator, base + i, key | ((v & 0xFFFF) + 1));
            return;
        }
    }
}

#endif
