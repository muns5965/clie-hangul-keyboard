/* skwr.c 를 PC용으로 묶는 시험 하네스 (tools/test-compose.sh) */
#define MNSK_HOST_TEST
#include "../src/skwr/skwr.c"
#include <string.h>

static UInt32 gFtr[64];
static unsigned char gText[4096];
static int gLen;
static unsigned char gBits[1397];
static unsigned char gCum[88];

Err FtrGet(UInt32 c, UInt16 n, UInt32 *v) { (void)c; *v = gFtr[n]; return 0; }
Err FtrSet(UInt32 c, UInt16 n, UInt32 v) { (void)c; gFtr[n] = v; return 0; }
Err SysCurAppDatabase(UInt16 *card, LocalID *id) { *card = 0; *id = 0x1234; return 0; }
Err SysLibFind(const Char *n, UInt16 *r) { (void)n; *r = 0xFFFF; return 1; }
void HRWinDrawChars(UInt16 a, const Char *s, Int16 l, Coord x, Coord y) { (void)a; (void)s; (void)l; (void)x; (void)y; }
void HRWinDrawRectangleFrame(UInt16 a, FrameType f, RectangleType *r) { (void)a; (void)f; (void)r; }
void HRWinEraseRectangle(UInt16 a, RectangleType *r, UInt16 c) { (void)a; (void)r; (void)c; }
void HRWinGetWindowExtent(UInt16 a, Coord *w, Coord *h) { (void)a; *w = 320; *h = 130; }
void HRWinInvertRectangle(UInt16 a, RectangleType *r, UInt16 c) { (void)a; (void)r; (void)c; }
FontID HRFntSetFont(UInt16 a, FontID f) { (void)a; return f; }
void HRWinDrawLine(UInt16 a, Coord x1, Coord y1, Coord x2, Coord y2) { (void)a; (void)x1; (void)y1; (void)x2; (void)y2; }
static const UInt8 gNoJamo[104] = { 0 };
const UInt8 *JamoOffPtr(void) { return gNoJamo; }
const UInt8 *JamoSegPtr(void) { return gNoJamo; }
const UInt8 *KscBitsPtr(void) { return gBits; }
const UInt8 *KscCumPtr(void) { return gCum; }

/* 메모 필드 흉내: 2바이트 한글은 한 글자로 지운다 */
Err EvtEnqueueKey(WChar c, UInt16 k, UInt16 m)
{
    (void)k; (void)m;
    if (c == chrBackspace) {
        if (gLen == 0) return 0;
        /* 앞에서부터 글자 경계를 따라가 마지막 글자 길이를 구한다 */
        { int i = 0, last = 0;
          while (i < gLen) { last = i; i += (gText[i] >= 0xA1) ? 2 : 1; }
          gLen = last; }
    } else if (c > 0xFF) {
        gText[gLen++] = c >> 8; gText[gLen++] = c & 0xFF;
    } else
        gText[gLen++] = (unsigned char)c;
    gText[gLen] = 0;
    return 0;
}

void T_Load(const unsigned char *bits, const unsigned char *cum)
{ memcpy(gBits, bits, sizeof gBits); memcpy(gCum, cum, sizeof gCum); }
void T_Reset(void) { memset(gFtr, 0, sizeof gFtr); gLen = 0; gText[0] = 0; }
void T_Key(int k) { if (DoKey((UInt16)k)) { /* 다시 그리기 없음 */ } }
const char *T_Text(void) { return (const char *)gText; }
