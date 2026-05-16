// id_us_1.c - User Services (minimal SDL3 port)
// Provides basic user interface functions: window drawing, text output,
// random numbers, command-line parsing, high scores.
// Most functions are stubs initially.

#include "id_us.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_in.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

extern byte fontcolor;
extern byte backcolor;

extern void Quit(char *error);

// -----------------------------------------------------------------------
// Public globals
// -----------------------------------------------------------------------

boolean  abortgame      = false;
boolean  loadedgame     = false;
boolean  NoWait         = false;
boolean  HighScoresDirty = false;
boolean  abortprogram   = false;
boolean  restartgame    = false;
boolean  Button0        = false;
boolean  Button1        = false;
boolean  CursorBad      = false;

// Forward declarations from id_vh.c
extern void VW_MeasurePropString(char *string, word *width, word *height);
extern void VW_DrawPropString(char *string);

// Function pointers for string measurement/drawing
void (*USL_MeasureString)(char *, word *, word *) = VW_MeasurePropString;
void (*USL_DrawString)(char *) = VW_DrawPropString;
int      CursorX        = 0;
int      CursorY        = 0;
SaveGame Games[MaxSaveGames];
HighScore Scores[MaxScores] =
{
    {"id software-'92",10000,1},
    {"Adrian Carmack",10000,1},
    {"John Carmack",10000,1},
    {"Kevin Cloud",10000,1},
    {"Tom Hall",10000,1},
    {"John Romero",10000,1},
    {"Jay Wilbur",10000,1},
};

// -----------------------------------------------------------------------
// Internal state
// -----------------------------------------------------------------------

// Original Wolfenstein 3-D random number table.  US_RndT walks this fixed
// 256-byte table by an index; recorded demos depend on the exact sequence,
// so this must match the original byte-for-byte (an LCG desyncs demos).
static const byte rndtable[256] = {
      0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66,
     74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36,
     95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188,
     52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224,
    149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242,
    145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0,
    175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235,
     25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113,
     94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75,
    136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196,
    135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113,
     80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241,
     24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224,
    145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95,
     28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226,
     71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36,
     17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106,
    197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136,
    120, 163, 236, 249
};
static int rndindex = 0;

// ========================================================================
// Startup / Shutdown
// ========================================================================

void US_Startup(void)
{
    // Initialize random table index
    US_InitRndT(true);

    // Clear save game slots
    memset(Games, 0, sizeof(Games));

    // Default window
    WindowX = 0;
    WindowY = 0;
    WindowW = MaxX;
    WindowH = MaxY;
    PrintX  = WindowX;
    PrintY  = WindowY;

    ingame         = false;
    abortgame      = false;
    loadedgame     = false;
    NoWait         = false;
    HighScoresDirty = false;
    abortprogram   = false;
    restartgame    = false;
    tedlevel       = 0;
    tedlevelnum    = 0;
}

void US_Setup(void)
{
    // Ensure default string measure/draw handlers are active.
    USL_MeasureString = VW_MeasurePropString;
    USL_DrawString = VW_DrawPropString;
}

void US_Shutdown(void)
{
    USL_MeasureString = VW_MeasurePropString;
    USL_DrawString = VW_DrawPropString;
}

// ========================================================================
// Random number generator (LCG)
// ========================================================================

void US_InitRndT(boolean randomize)
{
    // randomize: seed the index from the clock (normal play).
    // !randomize: index 0, the deterministic state demos were recorded with.
    if (randomize)
        rndindex = (int)(time(NULL) & 0xFF);
    else
        rndindex = 0;
}

int US_RndT(void)
{
    rndindex = (rndindex + 1) & 0xFF;
    return rndtable[rndindex];
}

// ========================================================================
// Load/Save hooks (stubs)
// ========================================================================

static boolean (*save_hook)(int)   = NULL;
static boolean (*load_hook)(int)   = NULL;
static void    (*reset_hook)(void) = NULL;

void US_SetLoadSaveHooks(boolean (*save)(int), boolean (*load)(int), void (*reset)(void))
{
    save_hook  = save;
    load_hook  = load;
    reset_hook = reset;
}

// ========================================================================
// Text screen (stubs)
// ========================================================================

void US_TextScreen(void)
{
    US_ClearWindow();
    VW_UpdateScreen();
}

void US_UpdateTextScreen(void)
{
    VW_UpdateScreen();
}

void US_FinishTextScreen(void)
{
    VW_UpdateScreen();
}

// ========================================================================
// Window drawing
// ========================================================================

void US_DrawWindow(int x, int y, int w, int h)
{
    int i, sx, sy, sw, sh, right, bottom;

    WindowX = x * 8;
    WindowY = y * 8;
    WindowW = w * 8;
    WindowH = h * 8;

    PrintX  = WindowX;
    PrintY  = WindowY;

    US_ClearWindow();
    sx = (x - 1) * 8;
    sy = (y - 1) * 8;
    sw = (w + 1) * 8;
    sh = (h + 1) * 8;

    right = sx + sw - 8;
    bottom = sy + sh;

    VWB_DrawTile8(sx, sy, 0);
    VWB_DrawTile8(sx, bottom, 5);
    for (i = sx + 8; i <= right; i += 8)
    {
        VWB_DrawTile8(i, sy, 1);
        VWB_DrawTile8(i, bottom, 6);
    }
    VWB_DrawTile8(right, sy, 2);
    VWB_DrawTile8(right, bottom, 7);

    for (i = sy + 8; i <= bottom - 8; i += 8)
    {
        VWB_DrawTile8(sx, i, 3);
        VWB_DrawTile8(right, i, 4);
    }
}

void US_CenterWindow(int w, int h)
{
    int x = (MaxX / 8 - w) / 2;
    int y = (MaxY / 8 - h) / 2;
    US_DrawWindow(x, y, w, h);
}

void US_SaveWindow(WindowRec *win)
{
    if (!win) return;
    win->x  = WindowX;
    win->y  = WindowY;
    win->w  = WindowW;
    win->h  = WindowH;
    win->px = PrintX;
    win->py = PrintY;
}

void US_RestoreWindow(WindowRec *win)
{
    if (!win) return;
    WindowX = win->x;
    WindowY = win->y;
    WindowW = win->w;
    WindowH = win->h;
    PrintX  = win->px;
    PrintY  = win->py;
}

void US_ClearWindow(void)
{
    VW_Bar(WindowX, WindowY, WindowW, WindowH, WHITE);
    PrintX = WindowX;
    PrintY = WindowY;
}

// ========================================================================
// Print routines (stubs)
// ========================================================================

void US_SetPrintRoutines(void (*measure)(char *, word *, word *), void (*draw)(char *))
{
    if (measure)
        USL_MeasureString = measure;
    else
        USL_MeasureString = VW_MeasurePropString;

    if (draw)
        USL_DrawString = draw;
    else
        USL_DrawString = VW_DrawPropString;
}

void US_PrintCentered(char *s)
{
    word w, h;
    USL_MeasureString(s, &w, &h);
    PrintX = WindowX + ((WindowW - w) / 2);
    US_Print(s);
}

void US_CPrint(char *s)
{
    char c, *se;
    word w, h;

    while (*s)
    {
        se = s;
        while ((c = *se) && (c != '\n'))
            se++;

        char linebuf[256];
        int len = (int)(se - s);
        if (len >= (int)sizeof(linebuf)) len = (int)sizeof(linebuf) - 1;
        memcpy(linebuf, s, len);
        linebuf[len] = '\0';

        USL_MeasureString(linebuf, &w, &h);
        PrintX = WindowX + ((WindowW - w) / 2);
        USL_DrawString(linebuf);

        s = se;
        if (c)
        {
            s++;
            PrintY += h;
        }
    }
}

void US_CPrintLine(char *s)
{
    US_CPrint(s);
}

void US_Print(char *s)
{
    char c, *se;
    word w, h;
    char linebuf[256];

    while (*s)
    {
        se = s;
        while ((c = *se) && (c != '\n'))
            se++;

        int len = (int)(se - s);
        if (len >= (int)sizeof(linebuf)) len = (int)sizeof(linebuf) - 1;
        memcpy(linebuf, s, len);
        linebuf[len] = '\0';

        USL_MeasureString(linebuf, &w, &h);
        USL_DrawString(linebuf);

        s = se;
        if (c)
        {
            s++;
            PrintX = WindowX;
            PrintY += h;
        }
        else
        {
            PrintX += w;
        }
    }
}

static void USL_XORICursor(int x, int y, char *s, word cursor)
{
    static boolean status;
    char buf[MaxString];
    word w, h;

    if (!s) return;

    strncpy(buf, s, sizeof(buf) - 1);
    {
        size_t cidx = (size_t)cursor;
        if (cidx > sizeof(buf) - 2)
            cidx = sizeof(buf) - 2;
        buf[cidx] = '\0';
    }

    USL_MeasureString(buf, &w, &h);
    px = x + (int)w - 1;
    py = y;

    if (status ^= 1)
        USL_DrawString("\x80");
    else
    {
        byte temp = fontcolor;
        fontcolor = backcolor;
        USL_DrawString("\x80");
        fontcolor = temp;
    }
}

void US_PrintUnsigned(longword n)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)n);
    US_Print(buf);
}

void US_PrintSigned(long n)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", n);
    US_Print(buf);
}

// ========================================================================
// Cursor (stubs)
// ========================================================================

void US_StartCursor(void)
{
    CursorBad = false;
}

void US_ShutCursor(void)
{
    CursorBad = false;
}

boolean US_UpdateCursor(void)
{
    return CursorBad;
}

// ========================================================================
// Line input (stub)
// ========================================================================

static size_t USL_StrLenBounded(const char *s, size_t maxlen)
{
    size_t len = 0;
    while (s && *s && len < maxlen)
    {
        ++len;
        ++s;
    }
    return len;
}

boolean US_LineInput(int x, int y, char *buf, char *def, boolean escok,
                     int maxchars, int maxwidth)
{
    boolean redraw;
    boolean cursorvis, cursormoved, done, result;
    ScanCode sc;
    char c;
    char s[MaxString];
    char olds[MaxString];
    word i;
    word cursor;
    word w, h;
    word len;
    byte temp;
    Uint64 lastBlink;

    if (def)
    {
        strncpy(s, def, sizeof(s) - 1);
        s[sizeof(s) - 1] = '\0';
    }
    else
    {
        s[0] = '\0';
    }

    olds[0] = '\0';
    cursor = (word)USL_StrLenBounded(s, sizeof(s) - 1);
    redraw = true;
    cursormoved = true;
    cursorvis = false;
    done = false;
    result = true;
    lastBlink = SDL_GetTicks();

    IN_StartAck();

    while (!done)
    {
        if (cursorvis)
            USL_XORICursor(x, y, s, cursor);

        sc = sc_None;
        c = 0;

        if (IN_CheckAck())
        {
            sc = LastScan;
            LastScan = sc_None;
            c = LastASCII;
            LastASCII = 0;
        }

        switch (sc)
        {
        case sc_LeftArrow:
            if (cursor)
                cursor--;
            c = 0;
            cursormoved = true;
            break;

        case sc_RightArrow:
            if (s[cursor])
                cursor++;
            c = 0;
            cursormoved = true;
            break;

        case sc_Home:
            cursor = 0;
            c = 0;
            cursormoved = true;
            break;

        case sc_End:
            cursor = (word)USL_StrLenBounded(s, sizeof(s) - 1);
            c = 0;
            cursormoved = true;
            break;

        case sc_Return:
            if (buf)
                strcpy(buf, s);
            result = true;
            done = true;
            c = 0;
            break;

        case sc_Escape:
            if (escok)
            {
                done = true;
                result = false;
            }
            c = 0;
            break;

        case sc_BackSpace:
            if (cursor)
            {
                memmove(s + cursor - 1, s + cursor, strlen(s + cursor) + 1);
                cursor--;
                redraw = true;
            }
            c = 0;
            cursormoved = true;
            break;

        case sc_Delete:
            if (s[cursor])
            {
                memmove(s + cursor, s + cursor + 1, strlen(s + cursor + 1) + 1);
                redraw = true;
            }
            c = 0;
            cursormoved = true;
            break;

        case 0x4c:
        case sc_UpArrow:
        case sc_DownArrow:
        case sc_PgUp:
        case sc_PgDn:
        case sc_Insert:
            c = 0;
            break;
        }

        if (!done && c)
        {
            len = (word)USL_StrLenBounded(s, sizeof(s) - 1);
            USL_MeasureString(s, &w, &h);

            if (isprint((unsigned char)c)
                && (len < MaxString - 1)
                && ((!maxchars) || (len < (word)maxchars))
                && ((!maxwidth) || (w < (word)maxwidth)))
            {
                for (i = len + 1; i > cursor; i--)
                    s[i] = s[i - 1];
                s[cursor++] = c;
                redraw = true;
            }
        }

        if (redraw)
        {
            px = x;
            py = y;
            temp = fontcolor;
            fontcolor = backcolor;
            USL_DrawString(olds);
            fontcolor = temp;

            strcpy(olds, s);
            px = x;
            py = y;
            USL_DrawString(s);
            redraw = false;
        }

    if (cursormoved)
    {
        cursorvis = false;
        lastBlink = SDL_GetTicks() - 250;
        cursormoved = false;
    }

    if (SDL_GetTicks() - lastBlink >= 500u)
    {
        lastBlink = SDL_GetTicks();
        cursorvis = (boolean)!cursorvis;
    }

        if (cursorvis)
            USL_XORICursor(x, y, s, cursor);

        VW_UpdateScreen();
        if (!done && !IN_CheckAck())
            SDL_Delay(10);
    }

    if (cursorvis)
        USL_XORICursor(x, y, s, cursor);
    if (!result)
    {
        px = x;
        py = y;
        USL_DrawString(olds);
    }
    VW_UpdateScreen();

    IN_ClearKeysDown();
    return result;
}

// ========================================================================
// Command-line parameter parsing
// ========================================================================

int US_CheckParm(char *parm, char **strings)
{
    int i;
    char cp, cs;
    char *p, *s;

    if (!parm || !strings)
        return -1;

    while (*parm && !isalpha((unsigned char)*parm))
        parm++;

    for (i = 0; strings[i] != NULL; i++)
    {
        p = parm;
        s = strings[i];
        cp = 0;
        cs = 0;

        for (;;)
        {
            cp = *p;
            cs = *s;

            if (!cp && !cs)
                return i;
            if (!cp || !cs)
                break;

            if (tolower((unsigned char)cp) != tolower((unsigned char)cs))
                break;

            p++;
            s++;
        }

        cp = *p;
        cs = *s;
    }

    return -1;
}

// ========================================================================
// User input (timed wait)
// ========================================================================

boolean US_UserInput(longword ticks)
{
    return IN_UserInput(ticks);
}

// ========================================================================
// High scores (stubs)
// ========================================================================

void US_CheckHighScore(long score, int other)
{
    HighScore myscore;
    word i, j;
    int n;

    myscore.score = score;
    myscore.completed = (word)other;
    myscore.episode = 0;
    myscore.name[0] = '\0';

    for (i = 0, n = -1; i < MaxScores; i++)
    {
        if ((myscore.score > Scores[i].score) || ((myscore.score == Scores[i].score) && (myscore.completed > Scores[i].completed)))
        {
            for (j = MaxScores - 1; j > i; j--)
                Scores[j] = Scores[j - 1];
            Scores[i] = myscore;
            n = i;
            break;
        }
    }

    if (n != -1)
    {
        PrintY = 76 + (16 * n);
        PrintX = 4 * 8;
        fontcolor = WHITE;
        backcolor = BLACK;
        US_LineInput(PrintX, PrintY, Scores[n].name, NULL, true, MaxHighName, 100);
        HighScoresDirty = true;
    }
}

void US_DisplayHighScores(int which)
{
    word i;
    word w, h;
    char buf[32];
    char level[8];
    WindowRec wr;

    (void)which;

    US_SaveWindow(&wr);
    US_CenterWindow(25, 12);
    US_ClearWindow();
    USL_MeasureString("High Scores", &w, &h);
    PrintX = WindowX + ((WindowW - w) / 2);
    US_Print("High Scores");

    PrintY += 12;
    for (i = 0; i < MaxScores; i++)
    {
        PrintX = WindowX + 8;
        PrintY += 12;
        US_Print(Scores[i].name[0] ? Scores[i].name : "-----");

        PrintX = WindowX + WindowW - 90;
        snprintf(level, sizeof(level), "E%u/L%u", (unsigned)Scores[i].episode + 1, (unsigned)Scores[i].completed);
        US_Print(level);

        PrintX = WindowX + WindowW - 24;
        snprintf(buf, sizeof(buf), "%ld", Scores[i].score);
        US_Print(buf);
    }

    VW_UpdateScreen();
    US_RestoreWindow(&wr);
}

void US_DisplaySaving(void)
{
    US_CenterWindow(16, 3);
    PrintX = WindowX;
    PrintY = WindowY + 8;
    US_Print("Saving...");
    VW_UpdateScreen();
}

// ========================================================================
// TED death (editor support)
// ========================================================================

void TEDDeath(void)
{
    Quit("TEDDeath: Unexpected TED editor call");
}
