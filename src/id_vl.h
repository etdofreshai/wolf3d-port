// id_vl.h - Video Layer (SDL3 port)
// Replaces VGA hardware access with SDL3 texture-based rendering
#ifndef ID_VL_H
#define ID_VL_H

#include "wl_types.h"

// VGA page addresses (used as buffer indices)
#ifndef PAGE1START
#define PAGE1START   0
#endif

#ifndef PAGE2START
#define PAGE2START   (SCREENWIDTH * MAXSCANLINES)
#endif

#ifndef PAGE3START
#define PAGE3START   (PAGE2START + SCREENWIDTH * MAXSCANLINES)
#endif

#ifndef FREESTART
#define FREESTART    (PAGE3START + SCREENWIDTH * MAXSCANLINES)
#endif

extern unsigned bufferofs;
extern unsigned displayofs, pelpan;
extern unsigned linewidth;
extern unsigned ylookup[MAXSCANLINES];
extern boolean  screenfaded;
extern unsigned bordercolor;

// Init/shutdown
void VL_Startup(void);
void VL_Shutdown(void);

// Mode setting
void VL_SetVGAPlaneMode(void);
void VL_SetTextMode(void);
void VL_DePlaneVGA(void);
void VL_ClearVideo(byte color);

// Display control
void VL_SetLineWidth(unsigned width);
void VL_SetSplitScreen(int linenum);
void VL_WaitVBL(int vbls);
void VL_CrtcStart(int crtc);
void VL_SetScreen(unsigned crtc, int pel);

// Palette
void VL_FillPalette(byte red, byte green, byte blue);
void VL_SetColor(int color, byte red, byte green, byte blue);
void VL_GetColor(int color, byte *red, byte *green, byte *blue);
void VL_SetPalette(byte *palette);
void VL_GetPalette(byte *palette);
void VL_FadeOut(int start, int end, int red, int green, int blue, int steps);
void VL_FadeIn(int start, int end, byte *palette, int steps);
void VL_ColorBorder(int color);
void VL_TestPaletteSet(void);

// Drawing primitives
void VL_Plot(unsigned x, unsigned y, byte color);
void VL_Hlin(unsigned x, unsigned y, unsigned width, byte color);
void VL_Vlin(unsigned x, unsigned y, unsigned height, byte color);
void VL_Bar(unsigned x, unsigned y, unsigned width, unsigned height, byte color);

// Fast framebuffer access for rendering hot paths
byte *VL_GetFramebuffer(void);
void VL_PlotFast(unsigned offset, byte color);

// Image transfer
void VL_MungePic(byte *source, unsigned width, unsigned height);
void VL_DrawPicBare(int x, int y, byte *pic, unsigned width, unsigned height);
void VL_MemToLatch(byte *source, int width, int height, int dest);
void VL_ScreenToScreen(unsigned source, unsigned dest, int width, int height);
void VL_MemToScreen(byte *source, int dest, int width, int height);
void VL_MaskedToScreen(byte *source, int dest, int width, int height);

// Text/font
void VL_DrawTile8String(char *str, byte *tile8ptr, int scan);
void VL_DrawLatch8String(char *str, int tile8ptr, int scan);
void VL_SizeTile8String(char *str, int *width, int *height);
void VL_DrawPropString(char *str, int basex);
void VL_SizePropString(char *str, int *width, int *height);

// Platform-specific (SDL3)
void VL_Present(void);
void VL_SetPaletteImmediate(byte *palette);
int VL_Screenshot(const char *filename);
void VL_ResetScreenshotState(void);
void VL_ArmScreenshotCapture(void);
void VL_ArmScreenshotCaptureFromFrame(int initialFrame);

#endif
