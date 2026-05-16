// id_ca.c - Cache Manager (SDL3 port)
// Handles loading and decompressing graphics, maps, and audio from the
// WL6 data files (VGAGRAPH, VGAHEAD, VGADICT, GAMEMAPS, MAPHEAD,
// AUDIOHED, AUDIOT).
//
// Huffman decompression + RLEW expansion for graphics chunks.
// RLEW expansion for map data.

#include "id_ca.h"
#include "id_mm.h"
#include "id_pm.h"
#include "id_vl.h"
#include "id_vh.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void Quit(char *error);

// -----------------------------------------------------------------------
// Public globals
// -----------------------------------------------------------------------

char      audioname[13] = "AUDIOT.WL6";
byte      *tinf         = NULL;
int       mapon         = 0;
word      *mapsegs[MAPPLANES]    = { NULL, NULL };
maptype   *mapheaderseg[NUMMAPS];
byte      *audiosegs[NUMSNDCHUNKS];
void      *grsegs[NUMCHUNKS];
size_t     grsegs_size[NUMCHUNKS]; // actual size of each cached chunk
byte      *grneeded     = NULL;
byte      ca_levelbit   = 1;
byte      ca_levelnum   = 0;
char      *titleptr[8]  = { NULL };
longword  *grstarts     = NULL;
longword  *audiostarts  = NULL;
void      (*drawcachebox)(void)            = NULL;
void      (*updatecachebox)(word, word)    = NULL;
void      (*finishcachebox)(void)          = NULL;

// -----------------------------------------------------------------------
// Internal file handles and state
// -----------------------------------------------------------------------

static FILE *ca_graphfile = NULL;  // VGAGRAPH.WL6
static FILE *ca_mapfile   = NULL;  // GAMEMAPS.WL6
static FILE *ca_audiofile = NULL;  // AUDIOT.WL6
static FILE *ca_debugfile = NULL;  // DEBUG.TXT

static longword ca_rlew_tag = 0;   // RLEW tag from MAPHEAD.WL6
static const longword CA_GR_SPARSE_OFFSET = 0x00FFFFFFu;
static const longword CA_GR_EOF_OFFSET = 0xFFFFFFFFu;

static FILE *CA_GetFarFile(void)
{
    if (ca_debugfile) return ca_debugfile;
    if (ca_graphfile) return ca_graphfile;
    if (ca_mapfile) return ca_mapfile;
    if (ca_audiofile) return ca_audiofile;
    return NULL;
}

// ========================================================================
// RLEW expansion (used by both maps and graphics)
// ========================================================================

void CA_RLEWexpand(word *source, word *dest, longword length, word rlewtag)
{
    // length is expanded size in bytes; fill dest until it's full.
    longword count = length / 2;
    longword dst_idx = 0;
    word *src_ptr = source;

    while (dst_idx < count) {
        word val = *src_ptr++;

        if (val == rlewtag) {
            word run_count = *src_ptr++;
            word run_value = *src_ptr++;
            while (run_count > 0 && dst_idx < count) {
                dest[dst_idx++] = run_value;
                run_count--;
            }
        } else {
            dest[dst_idx++] = val;
        }
    }
}

// ========================================================================
// Carmack decompression (used for WL6 map planes)
// ========================================================================

#define NEARTAG 0xA7
#define FARTAG  0xA8

static void CA_CarmackExpand(const byte *source, size_t src_len,
                              byte *dest, size_t dest_len)
{
    size_t src_pos = 0, dst_pos = 0;
    size_t words_left = dest_len / 2;

    while (words_left > 0) {
        if (src_pos + 2 > src_len) break;

        word ch = source[src_pos] | ((word)source[src_pos + 1] << 8);
        word tag = ch >> 8;
        word count = ch & 0xFF;
        src_pos += 2;

        if (tag == NEARTAG) {
            if (!count) {
                // literal word containing the tag byte
                if (src_pos >= src_len) break;
                ch |= source[src_pos++];
                if (dst_pos + 2 > dest_len) break;
                dest[dst_pos++] = (byte)(ch & 0xFF);
                dest[dst_pos++] = (byte)((ch >> 8) & 0xFF);
                --words_left;
                continue;
            }

            if (src_pos >= src_len) break;
            size_t offset = (size_t)source[src_pos++] * 2u;
            if (offset > dst_pos) break;
            size_t cp = dst_pos - offset;
            words_left -= count;
            while (count-- > 0 && dst_pos + 1 < dest_len) {
                dest[dst_pos++] = dest[cp++];
                if (dst_pos < dest_len) dest[dst_pos++] = dest[cp++];
            }
        } else if (tag == FARTAG) {
            if (!count) {
                // literal word containing the tag byte
                if (src_pos >= src_len) break;
                ch |= source[src_pos++];
                if (dst_pos + 2 > dest_len) break;
                dest[dst_pos++] = (byte)(ch & 0xFF);
                dest[dst_pos++] = (byte)((ch >> 8) & 0xFF);
                --words_left;
                continue;
            }

            if (src_pos + 1 >= src_len) break;
            size_t cp = ((size_t)source[src_pos] | (size_t)source[src_pos + 1] << 8) * 2u;
            src_pos += 2;
            words_left -= count;
            while (count-- > 0 && dst_pos + 1 < dest_len && cp + 1 < dest_len) {
                dest[dst_pos++] = dest[cp++];
                if (dst_pos < dest_len) dest[dst_pos++] = dest[cp++];
            }
        } else {
            if (dst_pos + 2 > dest_len) break;
            dest[dst_pos++] = (byte)(ch & 0xFF);
            dest[dst_pos++] = (byte)((ch >> 8) & 0xFF);
            --words_left;
        }
    }
}

void CA_RLEWCompress(word *source, longword length, word *dest, word rlewtag)
{
    longword count = length / 2;
    longword src_idx = 0;
    longword dst_idx = 0;

    while (src_idx < count) {
        word val = source[src_idx];

        // Count consecutive identical words
        longword run = 1;
        while (src_idx + run < count && source[src_idx + run] == val && run < 0xFFFF) {
            run++;
        }

        if (run > 3 || (run >= 2 && val == rlewtag)) {
            dest[dst_idx++] = rlewtag;
            dest[dst_idx++] = (word)run;
            dest[dst_idx++] = val;
            src_idx += run;
        } else {
            // Emit literal (even if it equals rlewtag, short runs are fine)
            dest[dst_idx++] = val;
            src_idx++;
        }
    }
}

// ========================================================================
// Huffman decompression
// ========================================================================

// The Huffman dictionary (VGADICT.WL6) contains 254 entries.
// Each entry is 4 bytes: two int16_t node indices (left, right child).
// Nodes 0-254 are internal nodes; leaves are implicit (0-255 byte values).
// Node index >= 256 means "emit byte (index - 256)".
// The tree root is node 254.

#define HUFF_TREE_SIZE 255  // nodes 0..254

// Raw Huffman tree from VGADICT.WL6:
//   - value < 256  : LEAF node, output this byte value
//   - value >= 256 : INTERNAL node, follow to node index (value - 256)
static word huff_raw[HUFF_TREE_SIZE][2];
static boolean huff_loaded = false;

static void CA_LoadHuffman(void)
{
    if (huff_loaded) return;

    const char *dict_paths[] = {
        "assets/wl6/VGADICT.WL6",
        "VGADICT.WL6",
        NULL
    };

    FILE *f = NULL;
    int i;
    for (i = 0; dict_paths[i] != NULL; i++) {
        f = fopen(dict_paths[i], "rb");
        if (f) break;
    }

    if (!f) {
        Quit("CA_LoadHuffman: Cannot open VGADICT.WL6");
    }

    // Read 255 entries of 4 bytes each
    byte raw[4];
    for (i = 0; i < HUFF_TREE_SIZE; i++) {
        if (fread(raw, 1, 4, f) != 4) {
            fclose(f);
            Quit("CA_LoadHuffman: VGADICT.WL6 too short");
        }
        huff_raw[i][0] = (word)(raw[0] | (raw[1] << 8));
        huff_raw[i][1] = (word)(raw[2] | (raw[3] << 8));
    }
    fclose(f);
    huff_loaded = true;
}

static longword CA_ReadLE32(const byte *p)
{
    return (longword)p[0]
         | ((longword)p[1] << 8)
         | ((longword)p[2] << 16)
         | ((longword)p[3] << 24);
}

static word CA_ReadLE16(const byte *p)
{
    return (word)(p[0] | (p[1] << 8));
}

static longword CA_CalcImplicitGrChunkSize(int chunk)
{
    if (chunk < STARTTILE8 || chunk >= STARTEXTERNS)
        return 0;

    if (chunk < STARTTILE8M)
        return 64u * (longword)NUMTILE8;
    if (chunk < STARTTILE16)
        return 128u * (longword)NUMTILE8M;
    if (chunk < STARTTILE16M)
        return 64u * 4;
    if (chunk < STARTTILE32)
        return 128u * 4;
    if (chunk < STARTTILE32M)
        return 64u * 16;

    return 128u * 16;
}

// Decompress Huffman-compressed data (exact Wolf4SDL algorithm).
// Tree convention: value < 256 = leaf byte, value >= 256 = internal node (index = value-256)
static longword CA_HuffExpand(byte *source, byte *dest, longword exp_length)
{
    byte *end = dest + exp_length;
    int head = HUFF_TREE_SIZE - 1;  // root = node 254
    byte val = *source++;
    byte mask = 1;
    int node = head;

    while (1) {
        word nodeval;
        if (!(val & mask))
            nodeval = huff_raw[node][0];
        else
            nodeval = huff_raw[node][1];

        if (mask == 0x80) {
            val = *source++;
            mask = 1;
        } else {
            mask <<= 1;
        }

        if (nodeval < 256) {
            *dest++ = (byte)nodeval;
            node = head;
            if (dest >= end) break;
        } else {
            node = nodeval - 256;
        }
    }

    return exp_length;
}

// ========================================================================
// File I/O helpers
// ========================================================================

void CA_FarRead(byte *dest, longword length)
{
    if (!dest || length == 0) return;

    FILE *f = CA_GetFarFile();
    if (!f) {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "CA_FarRead: no file available, zero-filling %lu bytes", (unsigned long)length);
        memset(dest, 0, (size_t)length);
        return;
    }

    if (length > 0xffffl) {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "CA_FarRead: truncating read >64K (%lu)", (unsigned long)length);
        length = 0xffffu;
    }

    size_t got = fread(dest, 1, (size_t)length, f);
    if (got < (size_t)length) {
        memset(dest + got, 0, (size_t)length - got);
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "CA_FarRead: short read %zu/%lu", got, (unsigned long)length);
    }
}

void CA_FarWrite(byte *source, longword length)
{
    if (!source || length == 0) return;

    if (!ca_debugfile) {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "CA_FarWrite: no debug file open, dropping %lu bytes", (unsigned long)length);
        return;
    }

    if (length > 0xffffl) {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "CA_FarWrite: truncating write >64K (%lu)", (unsigned long)length);
        length = 0xffffu;
    }

    size_t wrote = fwrite(source, 1, (size_t)length, ca_debugfile);
    if (wrote < (size_t)length) {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "CA_FarWrite: short write %zu/%lu", wrote, (unsigned long)length);
    }
}

boolean CA_ReadFile(char *filename, memptr *ptr, longword *length)
{
    FILE *f = fopen(filename, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return false;
    }

    *length = (longword)sz;
    MM_GetPtr(ptr, (unsigned long)sz);
    if (!*ptr) {
        fclose(f);
        return false;
    }

    size_t rd = fread(*ptr, 1, (size_t)sz, f);
    fclose(f);

    return (longword)rd == *length;
}

boolean CA_LoadFile(char *filename, memptr *ptr, longword *length)
{
    return CA_ReadFile(filename, ptr, length);
}

boolean CA_WriteFile(char *filename, byte *ptr, longword length)
{
    FILE *f = fopen(filename, "wb");
    if (!f) return false;

    size_t wr = fwrite(ptr, 1, (size_t)length, f);
    fclose(f);

    return (longword)wr == length;
}

// ========================================================================
// Startup / Shutdown
// ========================================================================

void CA_Startup(void)
{
    int i;

    // Initialize grsegs, audiosegs, mapsegs, mapheaderseg
    memset(grsegs, 0, sizeof(grsegs));
    memset(audiosegs, 0, sizeof(audiosegs));
    memset(mapheaderseg, 0, sizeof(mapheaderseg));
    for (i = 0; i < MAPPLANES; i++) {
        mapsegs[i] = NULL;
    }

    // Allocate grneeded array (one byte per graphics chunk)
    grneeded = (byte *)calloc(NUMCHUNKS, 1);
    if (!grneeded) {
        Quit("CA_Startup: Out of memory for grneeded");
    }

    // --- Load VGAHEAD.WL6 (offset table for VGAGRAPH) ---
    {
        const char *paths[] = {
            "assets/wl6/VGAHEAD.WL6",
            "VGAHEAD.WL6",
            NULL
        };
        FILE *f = NULL;
        for (i = 0; paths[i]; i++) {
            f = fopen(paths[i], "rb");
            if (f) break;
        }
        if (!f) {
            Quit("CA_Startup: Cannot open VGAHEAD.WL6");
        }

        // VGAHEAD contains 3-byte (24-bit) little-endian offsets
        fseek(f, 0, SEEK_END);
        long filesize = ftell(f);
        fseek(f, 0, SEEK_SET);
        size_t count = (size_t)(filesize / 3);
        if (count < 2) {
            fclose(f);
            Quit("CA_Startup: VGAHEAD.WL6 too small");
        }
        grstarts = (longword *)malloc((count + 1) * sizeof(longword));
        if (!grstarts) {
            fclose(f);
            Quit("CA_Startup: Out of memory for grstarts");
        }
        // Read 3-byte offsets and convert to 32-bit
        for (size_t grstart_idx = 0; grstart_idx < count; grstart_idx++) {
            byte b[3];
            if (fread(b, 1, 3, f) != 3) {
                fclose(f);
                Quit("CA_Startup: Failed to read VGAHEAD.WL6");
            }
            longword value = (longword)b[0] | ((longword)b[1] << 8) | ((longword)b[2] << 16);
            if (value == CA_GR_SPARSE_OFFSET) {
                grstarts[grstart_idx] = CA_GR_SPARSE_OFFSET;
            } else {
                grstarts[grstart_idx] = value;
            }
        }
        // Sentinel: one past the last entry for size calculation
        grstarts[count] = CA_GR_EOF_OFFSET;
        fclose(f);
    }

    // --- Load VGADICT.WL6 (Huffman dictionary) ---
    CA_LoadHuffman();

    // --- Open VGAGRAPH.WL6 ---
    {
        const char *paths[] = {
            "assets/wl6/VGAGRAPH.WL6",
            "VGAGRAPH.WL6",
            NULL
        };
        for (i = 0; paths[i]; i++) {
            ca_graphfile = fopen(paths[i], "rb");
            if (ca_graphfile) break;
        }
        if (!ca_graphfile) {
            Quit("CA_Startup: Cannot open VGAGRAPH.WL6");
        }
    }

    // --- Load pictable from STRUCTPIC (chunk 0) ---
    // Copy to its own allocation so it survives cache purges.
    {
        extern pictabletype *pictable;
        CA_CacheGrChunk(STRUCTPIC);
        if (grsegs[STRUCTPIC]) {
            size_t pt_size = NUMPICS * sizeof(pictabletype);
            pictable = (pictabletype *)malloc(pt_size);
            if (pictable) {
                memcpy(pictable, grsegs[STRUCTPIC], pt_size);
            }
            free(grsegs[STRUCTPIC]);
            grsegs[STRUCTPIC] = NULL;
            grsegs_size[STRUCTPIC] = 0;
        }
    }

    // --- Load palette from GAMEPAL.BIN ---
    // Original Wolf3D stored the 768-byte VGA palette in GAMEPAL.BIN,
    // linked as a Borland OBJ (GAMEPAL.OBJ) into the DOS executable.
    // Values are VGA 6-bit (0-63 range).
    {
        extern byte gamepal[256 * 3];
        const char *paths[] = {
            "assets/wl6/GAMEPAL.BIN",
            "GAMEPAL.BIN",
            NULL
        };
        FILE *f = NULL;
        for (i = 0; paths[i]; i++) {
            f = fopen(paths[i], "rb");
            if (f) break;
        }
        if (f) {
            size_t n = fread(gamepal, 1, 768, f);
            fclose(f);
            if (n == 768) {
                // Palette loaded successfully
            } else {
            }
        } else {
            SDL_Log("CA_Startup: GAMEPAL.BIN not found, palette defaults to zero");
        }
    }

    // --- Open GAMEMAPS.WL6 ---
    {
        const char *paths[] = {
            "assets/wl6/GAMEMAPS.WL6",
            "GAMEMAPS.WL6",
            NULL
        };
        for (i = 0; paths[i]; i++) {
            ca_mapfile = fopen(paths[i], "rb");
            if (ca_mapfile) break;
        }
        // Map file is optional (game can run without maps loaded initially)
    }

    // --- Load MAPHEAD.WL6 (RLEW tag + map offsets), then load all map headers ---
    {
        const char *paths[] = {
            "assets/wl6/MAPHEAD.WL6",
            "MAPHEAD.WL6",
            NULL
        };
        FILE *f = NULL;
        for (i = 0; paths[i]; i++) {
            f = fopen(paths[i], "rb");
            if (f) break;
        }
        if (!f) {
            Quit("CA_Startup: Cannot open MAPHEAD.WL6");
        }

        fseek(f, 0, SEEK_END);
        long filesize = ftell(f);
        if (filesize < 2) {
            fclose(f);
            Quit("CA_Startup: MAPHEAD.WL6 too small");
        }
        fseek(f, 0, SEEK_SET);

        if (tinf) {
            free(tinf);
            tinf = NULL;
        }
        tinf = (byte *)malloc((size_t)filesize);
        if (!tinf) {
            fclose(f);
            Quit("CA_Startup: Out of memory for MAPHEAD");
        }
        if (fread(tinf, 1, (size_t)filesize, f) != (size_t)filesize) {
            free(tinf);
            tinf = NULL;
            fclose(f);
            Quit("CA_Startup: Failed to read MAPHEAD.WL6");
        }
        fclose(f);

        ca_rlew_tag = (longword)CA_ReadLE16(tinf);

        if (ca_mapfile) {
            const byte *map_offsets = tinf + 2;
            longword header_entry_count = (filesize - 2) / 4;
            for (i = 0; i < NUMMAPS; i++) {
                if (i >= header_entry_count) {
                    mapheaderseg[i] = NULL;
                    continue;
                }

                longword map_offset = CA_ReadLE32(map_offsets + i * 4);
                if (map_offset == 0 || map_offset == 0xFFFFFFFFu) {
                    mapheaderseg[i] = NULL;
                    continue;
                }

                mapheaderseg[i] = (maptype *)malloc(sizeof(maptype));
                if (!mapheaderseg[i]) {
                    Quit("CA_Startup: Out of memory for map headers");
                }

                if (fseek(ca_mapfile, (long)map_offset, SEEK_SET) != 0) {
                    free(mapheaderseg[i]);
                    mapheaderseg[i] = NULL;
                    continue;
                }
                if (fread(mapheaderseg[i], sizeof(maptype), 1, ca_mapfile) != 1) {
                    free(mapheaderseg[i]);
                    mapheaderseg[i] = NULL;
                }
            }
        }
    }

    // --- Load AUDIOHED.WL6 (offset table for AUDIOT) ---
    {
        const char *paths[] = {
            "assets/wl6/AUDIOHED.WL6",
            "AUDIOHED.WL6",
            NULL
        };
        FILE *f = NULL;
        for (i = 0; paths[i]; i++) {
            f = fopen(paths[i], "rb");
            if (f) break;
        }
        if (f) {
            // Count entries by file size
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            size_t num_entries = (size_t)fsize / sizeof(longword);

            audiostarts = (longword *)malloc(num_entries * sizeof(longword));
            if (audiostarts) {
                fread(audiostarts, sizeof(longword), num_entries, f);
            }
            fclose(f);
        }
    }

    // --- Open AUDIOT.WL6 ---
    {
        const char *paths[] = {
            "assets/wl6/AUDIOT.WL6",
            "AUDIOT.WL6",
            NULL
        };
        for (i = 0; paths[i]; i++) {
            ca_audiofile = fopen(paths[i], "rb");
            if (ca_audiofile) break;
        }
        // Audio file is optional
    }
}

void CA_Shutdown(void)
{
    int i;

    // Free graphics segments
    for (i = 0; i < NUMCHUNKS; i++) {
        if (grsegs[i]) {
            free(grsegs[i]);
            grsegs[i] = NULL;
            grsegs_size[i] = 0;
        }
    }

    // Free audio segments
    for (i = 0; i < NUMSNDCHUNKS; i++) {
        if (audiosegs[i]) {
            free(audiosegs[i]);
            audiosegs[i] = NULL;
        }
    }

    // Free map segments
    for (i = 0; i < MAPPLANES; i++) {
        if (mapsegs[i]) {
            free(mapsegs[i]);
            mapsegs[i] = NULL;
        }
    }

    // Free map headers
    for (i = 0; i < NUMMAPS; i++) {
        if (mapheaderseg[i]) {
            free(mapheaderseg[i]);
            mapheaderseg[i] = NULL;
        }
    }

    if (grstarts) {
        free(grstarts);
        grstarts = NULL;
    }
    if (audiostarts) {
        free(audiostarts);
        audiostarts = NULL;
    }
    if (grneeded) {
        free(grneeded);
        grneeded = NULL;
    }
    if (tinf) {
        free(tinf);
        tinf = NULL;
    }

    if (ca_graphfile) { fclose(ca_graphfile); ca_graphfile = NULL; }
    if (ca_mapfile)   { fclose(ca_mapfile);   ca_mapfile = NULL; }
    if (ca_audiofile) { fclose(ca_audiofile);  ca_audiofile = NULL; }
}

// ========================================================================
// Graphics caching
// ========================================================================

void CA_SetGrPurge(void)
{
    // No-op: modern OS handles memory management
}

void CA_CacheGrChunk(int chunk)
{
    if (chunk < 0 || chunk >= NUMCHUNKS) return;
    if (grsegs[chunk]) return; // already cached

    if (!ca_graphfile || !grstarts) return;

    if (grstarts[chunk] == CA_GR_SPARSE_OFFSET) return; // sparse tile
    longword offset = grstarts[chunk];

    // Find next non-sparse chunk for length calculation
    longword next_offset = 0;
    int next;
    for (next = chunk + 1; next < NUMCHUNKS; next++) {
        next_offset = grstarts[next];
        if (next_offset != CA_GR_SPARSE_OFFSET) break;
    }
    if (next == NUMCHUNKS) {
        long file_end;
        fseek(ca_graphfile, 0, SEEK_END);
        file_end = ftell(ca_graphfile);
        fseek(ca_graphfile, (long)offset, SEEK_SET);
        if (file_end < 0) {
            return;
        }
        next_offset = (longword)file_end;
    }

    longword total_length = next_offset - offset;
    if (total_length <= 0) return;

    // Read entire chunk from VGAGRAPH
    byte *raw_data = (byte *)malloc((size_t)total_length);
    if (!raw_data) return;

    fseek(ca_graphfile, (long)offset, SEEK_SET);
    if (fread(raw_data, 1, (size_t)total_length, ca_graphfile) != total_length) {
        free(raw_data);
        return;
    }

    // Most chunks are implicit-size for tile ranges; explicit-size chunks
    // include a 4-byte decompressed length before huffman data.
    longword expanded = CA_CalcImplicitGrChunkSize(chunk);
    byte *huff_source = raw_data;

    if (!expanded) {
        if (total_length < 4) {
            free(raw_data);
            return;
        }
        expanded = (longword)raw_data[0]
                  | ((longword)raw_data[1] << 8)
                  | ((longword)raw_data[2] << 16)
                  | ((longword)raw_data[3] << 24);
        huff_source = raw_data + 4;
    }

    if (expanded == 0) {
        free(raw_data);
        return;
    }

    // Allocate output buffer and Huffman-decompress
    byte *dest = (byte *)malloc((size_t)expanded);
    if (!dest) {
        free(raw_data);
        return;
    }

    CA_HuffExpand(huff_source, dest, expanded);
    free(raw_data);

    grsegs[chunk] = dest;
    grsegs_size[chunk] = (size_t)expanded;
}

void CA_CacheScreen(int chunk)
{
    // Full-screen pics (TITLEPIC, etc.) are stored as 4-plane planar VGA
    // data: four 16000-byte planes, where plane p holds the pixels with
    // (x & 3) == p. De-plane it back into the linear framebuffer.
    CA_CacheGrChunk(chunk);
    if (grsegs[chunk] && grsegs_size[chunk] >= 64000) {
        extern byte *VL_GetFramebuffer(void);
        byte *fb = VL_GetFramebuffer() + bufferofs;
        byte *src = grsegs[chunk];
        for (int plane = 0; plane < 4; plane++) {
            for (int y = 0; y < 200; y++) {
                byte *dst = fb + y * (int)linewidth + plane;
                for (int c = 0; c < 80; c++) {
                    *dst = *src++;
                    dst += 4;
                }
            }
        }
    }
}

// ========================================================================
// Audio caching
// ========================================================================

void CA_CacheAudioChunk(int chunk)
{
    if (chunk < 0 || chunk >= NUMSNDCHUNKS) return;
    if (audiosegs[chunk]) return; // already cached

    if (!ca_audiofile || !audiostarts) return;

    longword offset = audiostarts[chunk];
    longword next_offset = audiostarts[chunk + 1];
    longword length = next_offset - offset;

    if (length == 0) return;

    audiosegs[chunk] = (byte *)malloc((size_t)length);
    if (!audiosegs[chunk]) return;

    fseek(ca_audiofile, (long)offset, SEEK_SET);
    fread(audiosegs[chunk], 1, (size_t)length, ca_audiofile);
}

void CA_LoadAllSounds(void)
{
    int i;
    for (i = STARTPCSOUNDS; i < STARTADLIBSOUNDS; i++) {
        if (i < NUMSNDCHUNKS) {
            CA_CacheAudioChunk(i);
        }
    }
}

// ========================================================================
// Map caching
// ========================================================================

void CA_CacheMap(int mapnum)
{
    if (mapnum < 0 || mapnum >= NUMMAPS) return;
    if (!ca_mapfile || !mapheaderseg[mapnum]) return;

    maptype *mh = mapheaderseg[mapnum];

    // Allocate and decompress each plane
    int plane;
    for (plane = 0; plane < MAPPLANES; plane++) {
        longword plane_start = mh->planestart[plane];
        word     plane_len   = mh->planelength[plane];

        if (plane_len == 0 || plane_start == 0) {
            if (mapsegs[plane]) { free(mapsegs[plane]); mapsegs[plane] = NULL; }
            continue;
        }

        // Allocate expanded map plane (64x64 = 4096 words)
        size_t map_plane_size = 64 * 64 * sizeof(word);
        if (mapsegs[plane]) {
            free(mapsegs[plane]);
        }
        mapsegs[plane] = (word *)malloc(map_plane_size);
        if (!mapsegs[plane]) continue;

        memset(mapsegs[plane], 0, map_plane_size);

        // Read compressed plane data
        byte *comp_data = (byte *)malloc(plane_len);
        if (!comp_data) continue;

        // planestart values are absolute file offsets in GAMEMAPS
        if (fseek(ca_mapfile, (long)plane_start, SEEK_SET) != 0) {
            free(comp_data);
            continue;
        }
        size_t rd = fread(comp_data, 1, plane_len, ca_mapfile);
        if (rd == plane_len) {
            // WL6 maps are Carmack-compressed, then RLEW-compressed.
            // Decompression chain:
            // 1. First word of plane data = Carmack expanded length (bytes)
            // 2. Carmack expand (skip first word) -> intermediate buffer
            // 3. First word of intermediate = RLEW expanded length (bytes)
            // 4. RLEW expand (skip first word) -> final map data

            word carmack_expanded = comp_data[0] | (comp_data[1] << 8);

            byte *carmack_out = (byte *)malloc(carmack_expanded > 0 ? carmack_expanded : 1);
            if (!carmack_out) { free(comp_data); continue; }

            CA_CarmackExpand(comp_data + 2, plane_len - 2,
                              carmack_out, carmack_expanded);

            // First word of Carmack output is the RLEW expanded length
            // (informational, we use MAPSIZE*MAPSIZE instead)
            // Skip it and RLEW expand the rest
            CA_RLEWexpand((word *)(carmack_out + 2), mapsegs[plane],
                          (longword)map_plane_size, (word)ca_rlew_tag);

            free(carmack_out);
        }
        free(comp_data);
    }
}

// ========================================================================
// Level / purge management
// ========================================================================

void CA_UpLevel(void)
{
    ca_levelnum++;
    if (ca_levelnum > 7) ca_levelnum = 7;
    ca_levelbit = (byte)(1 << ca_levelnum);
}

void CA_DownLevel(void)
{
    CA_SetAllPurge();
    if (ca_levelnum > 0) ca_levelnum--;
    ca_levelbit = (byte)(1 << ca_levelnum);
}

void CA_SetAllPurge(void)
{
    // Mark all graphics chunks as purgable
    int i;
    for (i = 0; i < NUMCHUNKS; i++) {
        if (grsegs[i] && !(grneeded[i] & ca_levelbit)) {
            free(grsegs[i]);
            grsegs[i] = NULL;
        }
    }
}

void CA_ClearMarks(void)
{
    if (grneeded) {
        memset(grneeded, 0, NUMCHUNKS);
    }
}

void CA_ClearAllMarks(void)
{
    CA_ClearMarks();
    // Free all cached graphics
    int i;
    for (i = 0; i < NUMCHUNKS; i++) {
        if (grsegs[i]) {
            free(grsegs[i]);
            grsegs[i] = NULL;
            grsegs_size[i] = 0;
        }
    }
}

void CA_CacheMarks(void)
{
    int i;
    for (i = 0; i < NUMCHUNKS; i++) {
        if (grneeded[i] & ca_levelbit) {
            CA_CacheGrChunk(i);
        }
    }
}

// ========================================================================
// Debug (stubs)
// ========================================================================

void CA_OpenDebug(void)
{
    if (ca_debugfile)
    {
        fclose(ca_debugfile);
        ca_debugfile = NULL;
    }

    ca_debugfile = fopen("DEBUG.TXT", "wb");
    if (!ca_debugfile)
        return;

    // keep it simple and always visible in editor/test logs
    setvbuf(ca_debugfile, NULL, _IONBF, 0);
}

void CA_CloseDebug(void)
{
    if (ca_debugfile)
    {
        fclose(ca_debugfile);
        ca_debugfile = NULL;
    }
}

