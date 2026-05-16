// id_heads.h - Master include header (SDL3 port)
// Replaces the original ID_HEADS.H with modern C equivalents.
#ifndef ID_HEADS_H
#define ID_HEADS_H

// Modern C headers replacing DOS headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <strings.h>
#endif

#include "wl_config.h"
#include "wl_types.h"

// Asset definition headers
#include "gfxv_wl6.h"
#include "audiowl6.h"
#include "mapswl6.h"

// Engine headers
#include "id_mm.h"
#include "id_pm.h"
#include "id_ca.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_in.h"
#include "id_sd.h"
#include "id_us.h"

#define GREXT "VGA"

#define PORTTILESWIDE    20
#define PORTTILESHIGH    13
#define UPDATEWIDE       PORTTILESWIDE
#define UPDATEHIGH       PORTTILESHIGH
#define MAXTICS          10
#define DEMOTICS         4
#define UPDATETERMINATE  0x0301

extern unsigned mapwidth, mapheight, tics;
extern boolean  compatability;

extern byte     *updateptr;
extern unsigned uwidthtable[UPDATEHIGH];
extern unsigned blockstarts[UPDATEWIDE * UPDATEHIGH];

extern byte     fontcolor, backcolor;
extern int      fontnumber;

#define SETFONTCOLOR(f, b) { fontcolor = (f); backcolor = (b); }

static inline int wl_stricmp(const char *a, const char *b)
{
#ifdef _WIN32
    return _stricmp(a, b);
#else
    return strcasecmp(a, b);
#endif
}

#ifndef _WIN32
static inline char *wl_itoa(int value, char *str, int base)
{
    char out[35];
    char *p = out + sizeof(out) - 1;
    unsigned int n;
    size_t len;
    const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (!str || base < 2 || base > 36)
        return NULL;

    *p-- = 0;

    if (value == 0)
    {
        *p-- = '0';
        memmove(str, p + 1, (out + sizeof(out)) - (p + 1));
        return str;
    }

    if (value < 0 && base == 10)
    {
        n = (unsigned int)(-(long long)value);
        *p-- = '-';
    }
    else
    {
        n = (unsigned int)value;
    }

    while (n)
    {
        *p-- = digits[n % base];
        n /= base;
    }

    ++p;
    len = (out + sizeof(out)) - p;
    memmove(str, p, len);
    return str;
}

#define itoa(value, str, base) wl_itoa((value), (str), (base))

static inline char *wl_ltoa(long value, char *str, int base)
{
    char out[35];
    char *p = out + sizeof(out) - 1;
    unsigned long n;
    size_t len;
    const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (!str || base < 2 || base > 36)
        return NULL;

    *p-- = 0;

    if (value == 0)
    {
        *p-- = '0';
        memmove(str, p + 1, (out + sizeof(out)) - (p + 1));
        return str;
    }

    if (value < 0 && base == 10)
    {
        n = (unsigned long)(-(long long)value);
        *p-- = '-';
    }
    else
    {
        n = (unsigned long)value;
    }

    while (n)
    {
        *p-- = digits[n % base];
        n /= base;
    }

    ++p;
    len = (out + sizeof(out)) - p;
    memmove(str, p, len);
    return str;
}

static inline char *wl_ultoa(unsigned long value, char *str, int base)
{
    char out[35];
    char *p = out + sizeof(out) - 1;
    size_t len;
    const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (!str || base < 2 || base > 36)
        return NULL;

    *p-- = 0;

    if (value == 0)
    {
        *p-- = '0';
        memmove(str, p + 1, (out + sizeof(out)) - (p + 1));
        return str;
    }

    while (value)
    {
        *p-- = digits[value % base];
        value /= base;
    }

    ++p;
    len = (out + sizeof(out)) - p;
    memmove(str, p, len);
    return str;
}

#define ltoa(value, str, base) wl_ltoa((value), (str), (base))
#define ultoa(value, str, base) wl_ultoa((value), (str), (base))
#endif

void Quit(char *error);

// File extension for game data
extern char extension[5];

#endif
