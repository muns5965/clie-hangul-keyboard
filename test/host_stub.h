/* PC에서 skwr.c 조합 로직을 시험하기 위한 Palm OS 흉내 (tools/test-compose.sh) */
#include <stdint.h>
#include <stddef.h>
typedef uint8_t UInt8; typedef uint16_t UInt16; typedef uint32_t UInt32;
typedef int16_t Int16; typedef int16_t Coord; typedef uint16_t Err; typedef unsigned char Boolean;
typedef uint16_t WChar; typedef uint32_t LocalID; typedef char Char; typedef int FontID;
typedef void *WinHandle;
typedef struct { struct { Coord x, y; } topLeft, extent; } RectangleType;
#define true 1
#define false 0
#define errNone 0
#define chrBackspace 0x08
#define chrLineFeed 0x0A
#define stdFont 0
#define roundFrame 1
#define asm __asm__
#define SYS_TRAP(x)
#define sysLibTrapCustom 0
typedef uint16_t FrameType;
Err SysLibFind(const Char *name, UInt16 *ref);
Err FtrGet(UInt32 c, UInt16 n, UInt32 *v);
Err FtrSet(UInt32 c, UInt16 n, UInt32 v);
Err EvtEnqueueKey(WChar c, UInt16 k, UInt16 m);
Err SysCurAppDatabase(UInt16 *card, LocalID *id);
const UInt8 *KscBitsPtr(void);
const UInt8 *KscCumPtr(void);
static inline WinHandle WinSetDrawWindow(WinHandle h) { return h; }
static inline void WinGetWindowExtent(Coord *w, Coord *h) { *w = 160; *h = 65; }
static inline void WinEraseRectangle(RectangleType *r, UInt16 c) { (void)r; (void)c; }
static inline void WinInvertRectangle(RectangleType *r, UInt16 c) { (void)r; (void)c; }
static inline void WinDrawRectangleFrame(int f, RectangleType *r) { (void)f; (void)r; }
static inline void WinDrawChars(const Char *s, Int16 n, Coord x, Coord y) { (void)s; (void)n; (void)x; (void)y; }
static inline FontID FntSetFont(FontID f) { return f; }
static inline Int16 FntCharsWidth(const Char *s, Int16 n) { (void)s; return n * 6; }
static inline Int16 FntLineHeight(void) { return 11; }
static inline void RctSetRectangle(RectangleType *r, Coord x, Coord y, Coord w, Coord h)
{ r->topLeft.x = x; r->topLeft.y = y; r->extent.x = w; r->extent.y = h; }
static inline void RctInsetRectangle(RectangleType *r, Coord d)
{ r->topLeft.x += d; r->topLeft.y += d; r->extent.x -= 2 * d; r->extent.y -= 2 * d; }
