// id_pm.c - Page Manager (file-based, SDL3 port)
// Reads VSWAP.WL6 pages from disk with a simple malloc-based cache.
// Replaces the original EMS/XMS/disk swapping page manager.

#include "id_pm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void Quit(char *error);

// -----------------------------------------------------------------------
// Public globals
// -----------------------------------------------------------------------

boolean        XMSPresent   = false;
boolean        EMSPresent   = false;
word           XMSPagesAvail = 0;
word           EMSPagesAvail = 0;
word           ChunksInFile  = 0;
word           PMSpriteStart = 0;
word           PMSoundStart  = 0;
word           NumDigi       = 0;
word          *DigiList      = NULL;
PageListStruct *PMPages      = NULL;
char           PageFileName[13] = "VSWAP.WL6";

// -----------------------------------------------------------------------
// Internal state
// -----------------------------------------------------------------------

static FILE          *pm_file    = NULL;
static longword      *pm_offsets = NULL;
static word          *pm_lengths = NULL;
static byte          **pm_cache  = NULL;   // per-page cached buffer
static word          pm_numpages = 0;

// Maximum number of pages we will cache simultaneously
#define PM_MAX_CACHE_PAGES 512

// ========================================================================
// Startup / Shutdown
// ========================================================================

void PM_Startup(void)
{
    char path[256];

    // Try assets/wl6/ subdirectory first, then current directory
    snprintf(path, sizeof(path), "assets/wl6/%s", PageFileName);
    pm_file = fopen(path, "rb");
    if (!pm_file) {
        pm_file = fopen(PageFileName, "rb");
    }
    if (!pm_file) {
        Quit("PM_Startup: Cannot open VSWAP file");
    }

    // Read header: 3 uint16_t values (Wolf3D VSWAP format)
    //   [0] = ChunksInFile (number of data pages)
    //   [1] = PMSpriteStart (index of first sprite page)
    //   [2] = PMSoundStart  (index of first sound page)
    word header[3];
    if (fread(header, sizeof(word), 3, pm_file) != 3) {
        Quit("PM_Startup: Failed to read VSWAP header");
    }

    ChunksInFile = header[0];
    if (ChunksInFile == 0 || ChunksInFile > 4096) {
        Quit("PM_Startup: Invalid VSWAP page count");
    }
    pm_numpages = ChunksInFile;
    PMSpriteStart = header[1];
    PMSoundStart  = header[2];

    // Read offset table: ChunksInFile uint32_t values
    size_t offset_count = (size_t)ChunksInFile;
    pm_offsets = (longword *)malloc(offset_count * sizeof(longword));
    if (!pm_offsets) {
        Quit("PM_Startup: Out of memory for offset table");
    }
    if (fread(pm_offsets, sizeof(longword), offset_count, pm_file) != offset_count) {
        Quit("PM_Startup: Failed to read offset table");
    }

    // Read length table: ChunksInFile uint16_t values
    pm_lengths = (word *)malloc((size_t)ChunksInFile * sizeof(word));
    if (!pm_lengths) {
        Quit("PM_Startup: Out of memory for length table");
    }
    if (fread(pm_lengths, sizeof(word), (size_t)ChunksInFile, pm_file) != ChunksInFile) {
        Quit("PM_Startup: Failed to read length table");
    }

    word i;

    // Allocate page list array
    PMPages = (PageListStruct *)calloc((size_t)ChunksInFile, sizeof(PageListStruct));
    if (!PMPages) {
        Quit("PM_Startup: Out of memory for page list");
    }

    // Fill page list from offset/length tables
    for (i = 0; i < ChunksInFile; i++) {
        PMPages[i].offset  = pm_offsets[i];
        PMPages[i].length  = pm_lengths[i];
        PMPages[i].xmsPage = -1;
        PMPages[i].emsPage = -1;
        PMPages[i].mainPage = -1;
        PMPages[i].locked  = pml_Unlocked;
        PMPages[i].lastHit = 0;
    }

    // Allocate per-page cache pointers (NULL = not loaded)
    pm_cache = (byte **)calloc((size_t)ChunksInFile, sizeof(byte *));
    if (!pm_cache) {
        Quit("PM_Startup: Out of memory for page cache");
    }
}

void PM_Shutdown(void)
{
    if (pm_cache) {
        word i;
        for (i = 0; i < pm_numpages; i++) {
            if (pm_cache[i]) {
                free(pm_cache[i]);
                pm_cache[i] = NULL;
            }
        }
        free(pm_cache);
        pm_cache = NULL;
    }
    if (pm_offsets) {
        free(pm_offsets);
        pm_offsets = NULL;
    }
    if (pm_lengths) {
        free(pm_lengths);
        pm_lengths = NULL;
    }
    if (PMPages) {
        free(PMPages);
        PMPages = NULL;
    }
    if (pm_file) {
        fclose(pm_file);
        pm_file = NULL;
    }
    pm_numpages = 0;
}

void PM_Reset(void)
{
    // Evict all unlocked pages from cache
    if (pm_cache) {
        word i;
        for (i = 0; i < pm_numpages; i++) {
            if (pm_cache[i] && PMPages[i].locked == pml_Unlocked) {
                free(pm_cache[i]);
                pm_cache[i] = NULL;
                PMPages[i].mainPage = -1;
            }
        }
    }
}

// ========================================================================
// Page access
// ========================================================================

memptr PM_GetPage(int page)
{
    if (page < 0 || (word)page >= pm_numpages) {
        return NULL;
    }

    // Already cached
    if (pm_cache && pm_cache[page]) {
        PMPages[page].lastHit = (longword)page; // simple hit marker
        return pm_cache[page];
    }

    // Not cached: load from disk
    if (!pm_file) {
        return NULL;
    }

    word len = PMPages[page].length;
    if (len == 0) {
        // Empty page (separator or unused)
        return NULL;
    }

    // Clamp to PMPageSize for safety
    size_t alloc_size = PMPageSize;
    if (len > PMPageSize) {
        alloc_size = (size_t)len;
    }

    byte *buf = (byte *)malloc(alloc_size);
    if (!buf) {
        return NULL;
    }
    memset(buf, 0, alloc_size);

    // Seek to the page offset in the file
    longword file_offset = PMPages[page].offset;
    if (fseek(pm_file, (long)file_offset, SEEK_SET) != 0) {
        free(buf);
        return NULL;
    }

    // Read the page data
    size_t bytes_read = fread(buf, 1, (size_t)len, pm_file);
    if (bytes_read != (size_t)len) {
        // Partial read is okay for the last page; zero-fill already done
    }

    // Store in cache
    pm_cache[page] = buf;
    PMPages[page].mainPage = page;

    return buf;
}

memptr PM_GetPageAddress(int page)
{
    if (page < 0 || (word)page >= pm_numpages) {
        return NULL;
    }
    if (pm_cache) {
        return pm_cache[page];
    }
    return NULL;
}

// ========================================================================
// Page locking / purge
// ========================================================================

void PM_SetPageLock(int page, PMLockType lock)
{
    if (page < 0 || (word)page >= pm_numpages) {
        return;
    }
    PMPages[page].locked = lock;
}

void PM_SetMainPurge(int purge)
{
    (void)purge;
    // No-op: modern OS handles memory management
}

void PM_SetMainMemPurge(int purge)
{
    (void)purge;
    // No-op: modern OS handles memory management
}

void PM_CheckMainMem(void)
{
    // No-op: no memory pressure management needed
}

void PM_NextFrame(void)
{
    // No-op: in original this triggered EMS/XMS page aging
}

void PM_Preload(boolean (*update)(word, word))
{
    // Preload all pages, calling update callback periodically
    word i;
    for (i = 0; i < pm_numpages; i++) {
        PM_GetPage((int)i);
        if (update && (i % 16 == 0)) {
            if (!update(i, pm_numpages)) {
                break;
            }
        }
    }
}
