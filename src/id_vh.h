// id_vh.h - Video Handlers header (SDL3 port)
#ifndef ID_VH_H
#define ID_VH_H

#include "wl_types.h"

// VGA color indices used throughout the game
#define BLACK         0
#define DARKGRAY     37
#define MEDIUMGRAY  109
#define WHITE       144
#define RED         173
#define DARKRED     174
#define GREEN        33
#define LIGHTGREEN   34
#define BLUE         37
#define YELLOW      160
#define F_WHITE     144
#define F_BLACK       0
#define F_FIRSTCOLOR 144
#define F_LASTCOLOR  159

void VW_Startup(void);
void VW_Shutdown(void);
void VW_MeasurePropString(char *string, word *width, word *height);
void VW_DrawPropString(char *string);
void VW_DrawTile8(int x, int y, int tile);
void VW_DrawTile8M(int x, int y, int tile);
void VW_DrawTile16(int x, int y, int tile);
void VW_DrawTile16M(int x, int y, int tile);
void VW_DrawTile32(int x, int y, int tile);
void VW_DrawTile32M(int x, int y, int tile);
void VW_DrawPic(int x, int y, int picnum);
void VW_Bar(int x, int y, int width, int height, int color);
void VW_Plot(int x, int y, int color);
void VW_Hlin(int x1, int x2, int y, int color);
void VW_Vlin(int y1, int y2, int x, int color);
void VW_DrawSprite(int x, int y, int sprite);
void VW_MaskBlock(byte *mask, int x, int y, int width, int height);
void VW_UpdateScreen(void);

// VWB_* double-buffer wrappers
void VWB_Bar(int x, int y, int w, int h, int color);
void VWB_Plot(int x, int y, int color);
void VWB_Hlin(int x1, int x2, int y, int color);
void VWB_Vlin(int y1, int y2, int x, int color);
void VWB_DrawPic(int x, int y, int picnum);
void VWB_DrawPropString(char *str);
void VWB_DrawTile8(int x, int y, int tile);
void VWB_DrawTile8M(int x, int y, int tile);
void VWB_DrawTile16(int x, int y, int tile);
void VWB_DrawTile16M(int x, int y, int tile);
void VWB_DrawSprite(int x, int y, int sprite);
void VWB_UpdateScreen(void);

// Additional VW_* functions
void VW_FadeIn(void);
void VW_FadeOut(void);
void VW_WaitVBL(int vbls);
void VW_ScreenToScreen(unsigned src, unsigned dst, int w, int h);

// Latch and fizzle fade
void LatchDrawPic(int x, int y, int picnum);
void FizzleFade(unsigned src, unsigned dst, int width, int height, int steps, boolean abortable);
void LoadLatchMem(void);

// Extension string
extern char extension[5];

extern word FontColor, BackColor;
extern int  WindowX, WindowY, WindowW, WindowH;
extern int  PrintX, PrintY;

// Font structure for proportional text rendering
typedef struct {
    word height;
    word location[256];
    byte width[256];
} fontstruct;

// Picture table structure (on-disk format uses 16-bit fields, matching DOS Borland int)
typedef struct {
    int16_t width, height;
} pictabletype;

extern pictabletype *pictable;

// Game palette (256 RGB entries, 768 bytes)
extern byte gamepal[256 * 3];

// Alias px/py for compatibility
#define px PrintX
#define py PrintY

#endif
