// id_vh.c - Video Handlers (minimal SDL3 port)
// Wrappers around VL_* primitives. Provides the higher-level drawing
// API that the game logic uses (pics, bars, sprites, text).

#include "id_vh.h"
#include "id_vl.h"
#include "id_ca.h"
#include "id_mm.h"
#include "wl_def.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern void Quit(char *error);

// -----------------------------------------------------------------------
// Public globals
// -----------------------------------------------------------------------

word FontColor = WHITE;
word BackColor = BLACK;
int  WindowX   = 0;
int  WindowY   = 0;
int  WindowW   = 320;
int  WindowH   = 200;
int  PrintX    = 0;
int  PrintY    = 0;

// Additional globals declared in headers
byte fontcolor = 0;
byte backcolor = 0;
int  fontnumber = 0;
byte gamepal[256 * 3] = {0};
pictabletype *pictable = NULL;

// ========================================================================
// Startup / Shutdown
// ========================================================================

void VW_Startup(void)
{
    VL_Startup();
}

void VW_Shutdown(void)
{
    VL_Shutdown();
}

// ========================================================================
// Text rendering
// ========================================================================

void VW_MeasurePropString(char *string, word *width, word *height)
{
    fontstruct *font = (fontstruct *)grsegs[STARTFONT + fontnumber];
    if (!font) { if (width) *width = 0; if (height) *height = 0; return; }

    if (height) *height = font->height;
    word w = 0;
    while (*string)
        w += font->width[(byte)*string++];
    if (width) *width = w;
}

void VW_DrawPropString(char *string)
{
    fontstruct *font = (fontstruct *)grsegs[STARTFONT + fontnumber];
    if (!font) return;

    int height = font->height;
    byte *fb = VL_GetFramebuffer();

    while (*string)
    {
        byte ch = (byte)*string++;
        int width = font->width[ch];
        if (width == 0) continue;

        byte *source = ((byte *)font) + font->location[ch];
        for (int col = 0; col < width; col++)
        {
            for (int row = 0; row < height; row++)
            {
                if (source[row * width + col])
                {
                    int sx = PrintX + col;
                    int sy = PrintY + row;
                    if (sx >= 0 && sx < VL_VGA_WIDTH && sy >= 0 && sy < VL_VGA_HEIGHT)
                        fb[bufferofs + sy * linewidth + sx] = fontcolor;
                }
            }
        }
        PrintX += width;
    }
}

// ========================================================================
// Tile and sprite drawing
// ========================================================================

static void VW_DrawRawTile(int x, int y, int tile, int chunk, int width, int height, int count)
{
    byte *pixels;
    size_t tile_bytes;

    if (tile < 0)
        return;
    if (count > 0 && tile >= count)
        return;
    if (chunk < 0 || chunk >= NUMCHUNKS)
        return;

    CA_CacheGrChunk(chunk);
    pixels = (byte *)grsegs[chunk];
    if (!pixels)
        return;

    tile_bytes = (size_t)width * (size_t)height;
    if (tile_bytes == 0)
        return;

    if (grsegs_size[chunk] && (size_t)tile * tile_bytes + tile_bytes > grsegs_size[chunk])
        return;

    pixels += (size_t)tile * tile_bytes;
    byte *frame = VL_GetFramebuffer();

    for (int row = 0; row < height; row++) {
        int py = y + row;
        if (py < 0 || py >= (int)VL_VGA_HEIGHT)
            continue;

        for (int col = 0; col < width; col++) {
            int px = x + col;
            if (px < 0 || px >= (int)VL_VGA_WIDTH)
                continue;

            frame[bufferofs + ylookup[py] + (unsigned)px] = pixels[row * width + col];
        }
    }
}

void VW_DrawTile8(int x, int y, int tile)
{
    VW_DrawRawTile(x, y, tile, STARTTILE8, 8, 8, NUMTILE8);
}

void VW_DrawTile8M(int x, int y, int tile)
{
    VW_DrawRawTile(x, y, tile, STARTTILE8M, 8, 8, NUMTILE8M);
}

void VW_DrawTile16(int x, int y, int tile)
{
    VW_DrawRawTile(x, y, tile, STARTTILE16, 16, 16, NUMTILE16);
}

void VW_DrawTile16M(int x, int y, int tile)
{
    VW_DrawRawTile(x, y, tile, STARTTILE16M, 16, 16, NUMTILE16M);
}

void VW_DrawTile32(int x, int y, int tile)
{
    VW_DrawRawTile(x, y, tile, STARTTILE16M, 32, 32, NUMTILE32);
}

void VW_DrawTile32M(int x, int y, int tile)
{
    VW_DrawRawTile(x, y, tile, STARTTILE16M, 32, 32, NUMTILE32M);
}

// ========================================================================
// Picture drawing
// ========================================================================

void VW_DrawPic(int x, int y, int picnum)
{
    CA_CacheGrChunk(picnum);

    if (!grsegs[picnum]) return;

    byte *data = (byte *)grsegs[picnum];
    if (!data) return;

    if (!pictable || picnum < STARTPICS || picnum >= STARTPICS + NUMPICS)
        return;

    int idx = picnum - STARTPICS;
    int pt_w = pictable[idx].width;
    int pt_h = pictable[idx].height;
    if (pt_w <= 0 || pt_h <= 0 || pt_w > 320 || pt_h > 200)
        return;
    if ((unsigned)pt_w * (unsigned)pt_h > grsegs_size[picnum])
        return;

    // Wolfenstein 3-D VGAGRAPH pics are stored as 4-plane planar VGA data:
    // the chunk is width*height bytes, ordered plane by plane, where pixel
    // column i belongs to plane (i & 3). De-plane it back to a linear block.
    // (Equivalent to Wolf4SDL's VL_MemToScreen.)
    byte *fb = VL_GetFramebuffer();
    byte *ptr = data;
    for (int plane = 0; plane < 4; plane++) {
        for (int row = 0; row < pt_h; row++) {
            int sy = y + row;
            for (int col = plane; col < pt_w; col += 4) {
                byte color = *ptr++;
                int sx = x + col;
                if (sx >= 0 && sx < VL_VGA_WIDTH && sy >= 0 && sy < VL_VGA_HEIGHT)
                    fb[bufferofs + sy * linewidth + sx] = color;
            }
        }
    }
}

// ========================================================================
// Drawing primitives (delegates to VL_*)
// ========================================================================

void VW_Bar(int x, int y, int width, int height, int color)
{
    VL_Bar((unsigned)x, (unsigned)y, (unsigned)width, (unsigned)height, (byte)color);
}

void VW_Plot(int x, int y, int color)
{
    VL_Plot((unsigned)x, (unsigned)y, (byte)color);
}

// Original Wolf3D convention: VW_Hlin(x1, x2, y) draws an inclusive
// horizontal span; VW_Vlin(y1, y2, x) draws an inclusive vertical span.
void VW_Hlin(int x1, int x2, int y, int color)
{
    VL_Hlin((unsigned)x1, (unsigned)y, (unsigned)(x2 - x1 + 1), (byte)color);
}

void VW_Vlin(int y1, int y2, int x, int color)
{
    VL_Vlin((unsigned)x, (unsigned)y1, (unsigned)(y2 - y1 + 1), (byte)color);
}

// ========================================================================
// Sprite drawing (stub)
// ========================================================================

void VW_DrawSprite(int x, int y, int sprite)
{
    byte *chunk;
    byte *data;
    size_t span;
    int width;
    int height;

    if (sprite < 0 || sprite >= NUMCHUNKS)
        return;

    CA_CacheGrChunk(sprite);
    chunk = (byte *)grsegs[sprite];
    if (!chunk)
        return;

    if (grsegs_size[sprite] < 4)
        return;

    width = (int)chunk[0] | ((int)chunk[1] << 8);
    height = (int)chunk[2] | ((int)chunk[3] << 8);
    if (width <= 0 || height <= 0)
        return;

    span = (size_t)4 + (size_t)width * (size_t)height;
    if (span > grsegs_size[sprite])
        return;

    data = chunk + 4;
    VL_MaskedToScreen(data, y * (int)linewidth + x, width, height);
}

// ========================================================================
// Masked block blit (stub)
// ========================================================================

void VW_MaskBlock(byte *mask, int x, int y, int width, int height)
{
    if (!mask || width <= 0 || height <= 0)
        return;
    VL_MaskedToScreen(mask, y * (int)linewidth + x, width, height);
}

// ========================================================================
// Screen update
// ========================================================================

void VW_UpdateScreen(void)
{
    VL_ScreenToScreen(bufferofs, displayofs, 320, 200);
    VL_Present();
}

// ========================================================================
// VWB_* double-buffer wrappers (in SDL3 port, just call VW_* directly)
// ========================================================================

void VWB_Bar(int x, int y, int w, int h, int color) { VW_Bar(x, y, w, h, color); }
void VWB_Plot(int x, int y, int color) { VW_Plot(x, y, color); }
void VWB_Hlin(int x1, int x2, int y, int color) { VW_Hlin(x1, x2, y, color); }
void VWB_Vlin(int y1, int y2, int x, int color) { VW_Vlin(y1, y2, x, color); }
void VWB_DrawPic(int x, int y, int picnum) { VW_DrawPic(x, y, picnum); }
void VWB_DrawPropString(char *str) { VW_DrawPropString(str); }
void VWB_DrawTile8(int x, int y, int tile) { VW_DrawTile8(x, y, tile); }
void VWB_DrawTile8M(int x, int y, int tile) { VW_DrawTile8M(x, y, tile); }
void VWB_DrawTile16(int x, int y, int tile) { VW_DrawTile16(x, y, tile); }
void VWB_DrawTile16M(int x, int y, int tile) { VW_DrawTile16M(x, y, tile); }
void VWB_DrawSprite(int x, int y, int sprite) { VW_DrawSprite(x, y, sprite); }
void VWB_UpdateScreen(void) { VW_UpdateScreen(); }

// ========================================================================
// Additional VW_* functions
// ========================================================================

void VW_FadeIn(void) { VL_FadeIn(0, 255, gamepal, 30); }
void VW_FadeOut(void) { VL_FadeOut(0, 255, 0, 0, 0, 30); }
void VW_WaitVBL(int vbls) { VL_WaitVBL(vbls); }
void VW_ScreenToScreen(unsigned src, unsigned dst, int w, int h) { VL_ScreenToScreen(src, dst, w, h); }

// ========================================================================
// Latch memory and fizzle fade
// ========================================================================

void LatchDrawPic(int x, int y, int picnum)
{
    // Status-bar / latch pic coordinates are given in 8-pixel units (the
    // original drew them via byte offsets into planar VGA memory). Number
    // and face pics step x by 1 per 8-pixel-wide pic, so scale to pixels.
    VW_DrawPic(x * 8, y, picnum);
}

void FizzleFade(unsigned src, unsigned dst, int width, int height, int steps, boolean abortable)
{
    // Simple fade - just copy and present
    VL_ScreenToScreen(src, dst, width, height);
    VL_Present();
    (void)steps; (void)abortable;
}

void LoadLatchMem(void)
{
    int i,start,end;
    byte *src;
    unsigned destoff;

    //
    // tile 8s
    //
    latchpics[0] = (unsigned)freelatch;
    CA_CacheGrChunk(STARTTILE8);
    src = (byte *)grsegs[STARTTILE8];
    destoff = (unsigned)freelatch;

    for (i=0;i<NUMTILE8 && src;i++)
    {
        VL_MemToLatch(src,8,8,(int)destoff);
        src += 64;
        destoff += 16;
    }
    UNCACHEGRCHUNK(STARTTILE8);

    //
    // pics
    //
    start = LATCHPICS_LUMP_START;
    end = LATCHPICS_LUMP_END;

    for (i=start;i<=end;i++)
    {
        int idx = 2 + i - start;
        if (idx >= NUMLATCHPICS)
        {
            break;
        }
        latchpics[idx] = destoff;
        CA_CacheGrChunk(i);
        src = (byte *)grsegs[i];
        if (src && pictable && i >= STARTPICS && (i - STARTPICS) < NUMPICS)
        {
            unsigned width = (unsigned)pictable[i - STARTPICS].width;
            unsigned height = (unsigned)pictable[i - STARTPICS].height;
            if (width && height)
            {
                VL_MemToLatch(src,(int)width,(int)height,(int)destoff);
                destoff += (width / 4u) * height;
            }
        }
        UNCACHEGRCHUNK(i);
    }
}

// ========================================================================
// Extension string for game data files
// ========================================================================

char extension[5] = "WL6";
