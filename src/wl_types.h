// wl_types.h - Modern replacements for DOS/Borland types
#ifndef WL_TYPES_H
#define WL_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Original type mappings
typedef uint8_t  byte;
typedef uint16_t word;
typedef uint32_t longword;
typedef byte    *Ptr;

// Remove far/_seg/huge - just plain pointers
// memptr was "void _seg *" in original - segment pointer for MM system
typedef void    *memptr;

#ifndef nil
#define nil ((void *)0)
#endif

// The original used an enum {false, true} for boolean
// but also used it as an int in many places. Use int for compat.
#undef boolean
typedef int boolean;

#ifndef true
#define true  1
#endif
#ifndef false
#define false 0
#endif

typedef struct {
    int x, y;
} Point;

typedef struct {
    Point ul, lr;
} Rect;

// VGA constants - updated for linear (non-planar) framebuffer
#define SCREENSEG      0xA000
#define SCREENWIDTH    320   // bytes per scanline (1 byte per pixel in linear mode)
#define MAXSCANLINES   200
#define CHARWIDTH      8
#define TILEWIDTH      16    // 16 bytes per tile width (16 pixels, 1 byte each)

// Display dimensions
#define VL_VGA_WIDTH   320
#define VL_VGA_HEIGHT  200

#endif
