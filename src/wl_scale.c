// WL_SCALE.C - Sprite scaling system
// Ported from original WOLFSRC/WL_SCALE.C to C11/SDL3
//
// Changes from original:
//   - All self-modifying x86 compiled scaling code (BuildCompScale) removed
//   - ScaleLine (inline assembly) replaced with pure C pixel-by-pixel scaling
//   - VGA planar Mode X drawing replaced with VL_Plot calls
//   - All far/_seg/huge qualifiers removed
//   - Memory manager (MM_GetPtr/MM_FreePtr) scalers replaced with malloc/free
//   - Compiled scaler table replaced with simple width lookup arrays
//   - Sprite column data format preserved: 0xFF = transparent pixel

#include "id_heads.h"
#include "wl_def.h"

/*
=============================================================================

                          GLOBALS

=============================================================================
*/

// Scaling tables: for each possible scale height, store a precomputed
// width-per-source-column and a lookup table that maps destination pixels
// back to source pixels.

// scaledirectory[scale] points to a malloc'd block containing first the
// width[64] array (one byte per source column: how many screen pixels wide
// this column paints), followed by an array of 64 pointers to scale-mapping
// tables (one per source column). Each scale-mapping table maps destination
// pixel index (0..height-1) to source pixel index (0..63).
// If a column has zero width, its mapping pointer is NULL.

typedef struct {
    int     width[64];          // screen pixels per source column
    byte   *rowmap[64];         // rowmap[col][dest_y] = source row, or NULL
} scaleentry_t;

static scaleentry_t *scaledirectory[MAXSCALEHEIGHT + 1];

int     maxscale, maxscaleshl2;

boolean insetupscaling;

/*
=============================================================================

                          LOCALS

=============================================================================
*/

static int stepbytwo;

//===========================================================================

/*
==========================
=
= SetupScaling
=
= Initializes the scaling lookup tables for all heights up to
= maxscaleheight. The original generated self-modifying x86 machine code;
= we generate simple C lookup tables instead.
=
==========================
*/

void SetupScaling(int maxscaleheight)
{
    int i, col;

    insetupscaling = true;

    maxscaleheight /= 2;     // one scaler every two pixels

    maxscale = maxscaleheight - 1;
    maxscaleshl2 = maxscale << 2;

//
// free up old scalers
//
    for (i = 1; i < MAXSCALEHEIGHT; i++)
    {
        if (scaledirectory[i])
        {
            // Free the rowmap sub-arrays
            scaleentry_t *se = scaledirectory[i];
            for (col = 0; col < 64; col++)
            {
                if (se->rowmap[col])
                    free(se->rowmap[col]);
            }
            free(se);
            scaledirectory[i] = NULL;
        }
        if (i >= stepbytwo)
            i += 2;
    }
    memset(scaledirectory, 0, sizeof(scaledirectory));

    stepbytwo = viewheight / 2;   // save space by double stepping

//
// build the scale tables
//
    for (i = 1; i <= maxscaleheight; i++)
    {
        int height = i * 2;
        int toppix = (viewheight - height) / 2;
        long step = ((long)height << 16) / 64;
        long fix;

        scaleentry_t *se = (scaleentry_t *)calloc(1, sizeof(scaleentry_t));
        if (!se)
            Quit("SetupScaling: Out of memory");

        for (col = 0; col < 64; col++)
        {
            int startpix, endpix;

            fix = (long)col * step;
            startpix = fix >> 16;
            fix += step;
            endpix = fix >> 16;

            if (endpix > startpix)
                se->width[col] = endpix - startpix;
            else
                se->width[col] = 0;
        }

        // Build row mapping tables for columns that have nonzero width
        for (col = 0; col < 64; col++)
        {
            if (se->width[col] == 0)
            {
                se->rowmap[col] = NULL;
                continue;
            }

            // Allocate a mapping table for this column:
            // maps screen Y (relative to top of sprite) to source row 0..63
            byte *map = (byte *)malloc(height);
            if (!map)
                Quit("SetupScaling: Out of memory for rowmap");

            // For each destination pixel, find which source pixel maps to it
            for (int dest = 0; dest < height; dest++)
            {
                // Inverse mapping: which source pixel lands at this dest?
                // src -> dest: dest = (src * height) / 64  (approximately)
                // dest -> src: src = (dest * 64) / height
                int src = (dest * 64) / height;
                if (src < 0) src = 0;
                if (src > 63) src = 63;
                map[dest] = (byte)src;
            }

            se->rowmap[col] = map;
        }

        scaledirectory[i] = se;

        // Double-step: share the same table for i+1 and i+2
        if (i >= stepbytwo)
        {
            scaledirectory[i + 1] = se;
            scaledirectory[i + 2] = se;
            i += 2;
        }
    }

    scaledirectory[0] = scaledirectory[1];

    insetupscaling = false;
}


/*
=======================
=
= ScaleLine
=
= Draws a single vertical column of sprite data at screen position slinex,
= using the given sprite data and scale entry.
=
= coldata: pointer into the sprite's column pixel data
=          (0xFF = transparent, other values = color index)
= scale:   the scaleentry_t for this sprite height
= col:     source column index (0..63)
= x:       screen X position
= height:  total sprite height in pixels
=
=======================
*/

static void ScaleLine(const byte *coldata, scaleentry_t *scale,
                      int col, int x, int height)
{
    int toppix = (viewheight - height) / 2;
    byte *rowmap = scale->rowmap[col];
    int y;

    if (!rowmap || !coldata)
        return;

    // Draw each pixel in the scaled column
    for (y = 0; y < height; y++)
    {
        int screeny = toppix + y;
        if (screeny < 0 || screeny >= viewheight)
            continue;

        byte srcrow = rowmap[y];
        byte pixel = coldata[srcrow];

        if (pixel != 0xFF)       // 0xFF = transparent
            VL_Plot(x, screeny, pixel);
    }
}


/*
=======================
=
= ReadSpriteColumn
=
= Reads a column of pixel data from a t_compshape sprite.
= Returns a malloc'd buffer of 64 bytes containing the column pixel data,
= or NULL if the column has no data. Caller must free().
=
= The sprite column data is organized as segments of non-transparent pixels.
= Each segment has:
//   word endpix    - end pixel *2 (0 terminates the column)
//   word topdelta  - top of segment (relative to sprite top, 0-63)
//   word length    - number of pixels in segment data following
// Then length bytes of pixel data follow.
// Then the next segment starts.
//
========================
*/

static byte *ReadSpriteColumn(t_compshape *shape, int col)
{
    byte *pixels;
    unsigned dataofs;
    const word *seg;
    const byte *linesrc;

    pixels = (byte *)malloc(64);
    if (!pixels)
        Quit("ReadSpriteColumn: Out of memory");

    memset(pixels, 0xFF, 64);  // all transparent

    if (col < (int)shape->leftpix || col > (int)shape->rightpix)
        return pixels;   // column outside sprite bounds

    dataofs = shape->dataofs[col - shape->leftpix];
    seg = (const word *)(((const byte *)shape) + dataofs);
    linesrc = (const byte *)shape;

    // Each segment header is 3 words (6 bytes):
    //   seg[0] = endpix * 2   (0 = end of column)
    //   seg[1] = top           (byte offset into shape for pixel data)
    //   seg[2] = startpix * 2
    while (1)
    {
        unsigned endpix_x2  = seg[0];
        unsigned top         = seg[1];
        unsigned startpix_x2 = seg[2];
        seg += 3;  // advance past 6-byte header

        if ((endpix_x2 >> 1) == 0)
            break;  // end of column

        int start = startpix_x2 >> 1;
        int end   = endpix_x2 >> 1;

        for (int i = start; i < end && i < 64; i++)
        {
            if (i >= 0 && top + i < PMPageSize)
                pixels[i] = linesrc[top + i];
        }
    }

    return pixels;
}


/*
=======================
=
= ScaleShape
=
= Draws a scaled sprite at xcenter, with the given height.
= Performs wall occlusion checking against wallheight[].
=
= each vertical line of the sprite has a pointer to column data in
= the t_compshape structure via the dataofs[] array.
=
========================
*/

void ScaleShape(int xcenter, int shapenum, unsigned height)
{
    t_compshape  *shape;
    scaleentry_t *comptable;
    unsigned      scale;
    int           srcx, stopx;          // signed: leftpix may be 0, srcx
                                        // decrements past it before the
                                        // loop test (unsigned would wrap
                                        // and index width[] out of bounds)
    boolean       leftvis, rightvis;
    int           slinex, slinewidth;

    shape = (t_compshape *)PM_GetSpritePage(shapenum);
    if (!shape) return;

    scale = height >> 3;    // low three bits are fractional
    if (!scale || scale > (unsigned)maxscale)
        return;             // too close or far away
    comptable = scaledirectory[scale];
    if (!comptable)
        return;

//
// scale to the left (from pixel 31 to shape->leftpix)
//
    srcx = 32;
    slinex = xcenter;
    stopx = shape->leftpix;

    while (--srcx >= stopx && slinex > 0)
    {
        slinewidth = comptable->width[srcx];
        if (!slinewidth)
            continue;

        if (slinewidth == 1)
        {
            slinex--;
            if (slinex < viewwidth)
            {
                if (wallheight[slinex] >= height)
                    continue;   // obscured by closer wall

                // Draw this column
                byte *coldata = ReadSpriteColumn(shape, srcx);
                ScaleLine(coldata, comptable, srcx, slinex, scale * 2);
                free(coldata);
            }
            continue;
        }

        //
        // handle multi pixel lines
        //
        if (slinex > viewwidth)
        {
            slinex -= slinewidth;
            slinewidth = viewwidth - slinex;
            if (slinewidth < 1)
                continue;   // still off the right side
        }
        else
        {
            if (slinewidth > slinex)
                slinewidth = slinex;
            slinex -= slinewidth;
        }

        leftvis = (wallheight[slinex] < height);
        rightvis = (wallheight[slinex + slinewidth - 1] < height);

        if (leftvis)
        {
            if (rightvis)
            {
                // Fully visible - draw all columns
                for (int c = 0; c < slinewidth; c++)
                {
                    byte *coldata = ReadSpriteColumn(shape, srcx);
                    ScaleLine(coldata, comptable, srcx, slinex + c, scale * 2);
                    free(coldata);
                }
            }
            else
            {
                while (wallheight[slinex + slinewidth - 1] >= height)
                    slinewidth--;
                for (int c = 0; c < slinewidth; c++)
                {
                    byte *coldata = ReadSpriteColumn(shape, srcx);
                    ScaleLine(coldata, comptable, srcx, slinex + c, scale * 2);
                    free(coldata);
                }
            }
        }
        else
        {
            if (!rightvis)
                continue;   // totally obscured

            while (wallheight[slinex] >= height)
            {
                slinex++;
                slinewidth--;
            }
            for (int c = 0; c < slinewidth; c++)
            {
                byte *coldata = ReadSpriteColumn(shape, srcx);
                ScaleLine(coldata, comptable, srcx, slinex + c, scale * 2);
                free(coldata);
            }
            break;  // the rest of the shape is gone
        }
    }


//
// scale to the right
//
    slinex = xcenter;
    stopx = shape->rightpix;
    if (shape->leftpix < 31)
        srcx = 31;
    else
        srcx = shape->leftpix - 1;
    slinewidth = 0;

    while (++srcx <= stopx && (slinex += slinewidth) < viewwidth)
    {
        slinewidth = comptable->width[srcx];
        if (!slinewidth)
            continue;

        if (slinewidth == 1)
        {
            if (slinex >= 0 && wallheight[slinex] < height)
            {
                byte *coldata = ReadSpriteColumn(shape, srcx);
                ScaleLine(coldata, comptable, srcx, slinex, scale * 2);
                free(coldata);
            }
            continue;
        }

        //
        // handle multi pixel lines
        //
        if (slinex < 0)
        {
            if (slinewidth <= (unsigned)(-slinex))
                continue;   // still off the left edge

            slinewidth += slinex;
            slinex = 0;
        }
        else
        {
            if (slinex + slinewidth > viewwidth)
                slinewidth = viewwidth - slinex;
        }

        leftvis = (wallheight[slinex] < height);
        rightvis = (wallheight[slinex + slinewidth - 1] < height);

        if (leftvis)
        {
            if (rightvis)
            {
                for (int c = 0; c < slinewidth; c++)
                {
                    byte *coldata = ReadSpriteColumn(shape, srcx);
                    ScaleLine(coldata, comptable, srcx, slinex + c, scale * 2);
                    free(coldata);
                }
            }
            else
            {
                while (wallheight[slinex + slinewidth - 1] >= height)
                    slinewidth--;
                for (int c = 0; c < slinewidth; c++)
                {
                    byte *coldata = ReadSpriteColumn(shape, srcx);
                    ScaleLine(coldata, comptable, srcx, slinex + c, scale * 2);
                    free(coldata);
                }
                break;  // the rest of the shape is gone
            }
        }
        else
        {
            if (rightvis)
            {
                while (wallheight[slinex] >= height)
                {
                    slinex++;
                    slinewidth--;
                }
                for (int c = 0; c < slinewidth; c++)
                {
                    byte *coldata = ReadSpriteColumn(shape, srcx);
                    ScaleLine(coldata, comptable, srcx, slinex + c, scale * 2);
                    free(coldata);
                }
            }
            else
                continue;   // totally obscured
        }
    }
}



/*
=======================
=
= SimpleScaleShape
=
= NO CLIPPING against walls, height in pixels.
= Draws a sprite scaled to the given height, centered at xcenter.
= Used for status bar face, death cam, etc.
=
========================
*/

void SimpleScaleShape(int xcenter, int shapenum, unsigned height)
{
    t_compshape  *shape;
    scaleentry_t *comptable;
    unsigned      scale;
    int           srcx, stopx;          // signed: see ScaleShape note
    int           slinex, slinewidth;

    shape = (t_compshape *)PM_GetSpritePage(shapenum);
    if (!shape) return;

    scale = height >> 1;
    if (!scale || scale > (unsigned)maxscale)
        return;
    comptable = scaledirectory[scale];
    if (!comptable)
        return;

//
// scale to the left (from pixel 31 to shape->leftpix)
//
    srcx = 32;
    slinex = xcenter;
    stopx = shape->leftpix;

    while (--srcx >= stopx)
    {
        slinewidth = comptable->width[srcx];
        if (!slinewidth)
            continue;

        slinex -= slinewidth;
        for (int c = 0; c < slinewidth; c++)
        {
            byte *coldata = ReadSpriteColumn(shape, srcx);
            ScaleLine(coldata, comptable, srcx, slinex + c, scale * 2);
            free(coldata);
        }
    }


//
// scale to the right
//
    slinex = xcenter;
    stopx = shape->rightpix;
    if (shape->leftpix < 31)
    {
        srcx = 31;
    }
    else
    {
        srcx = shape->leftpix - 1;
    }
    slinewidth = 0;

    while (++srcx <= stopx)
    {
        slinewidth = comptable->width[srcx];
        if (!slinewidth)
            continue;

        for (int c = 0; c < slinewidth; c++)
        {
            byte *coldata = ReadSpriteColumn(shape, srcx);
            ScaleLine(coldata, comptable, srcx, slinex + c, scale * 2);
            free(coldata);
        }
        slinex += slinewidth;
    }
}
