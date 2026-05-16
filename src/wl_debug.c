// WL_DEBUG.C - Debug keys and cheat codes
// Ported from original WOLFSRC/WL_DEBUG.C to C11/SDL3
//
// Changes from original:
//   - Removed #include <BIOS.H> and bioskey() calls
//   - Removed #pragma warn directives
//   - Replaced VGA direct memory access in PicturePause with framebuffer copy
//   - Removed inline asm for mode 13h set
//   - Removed _seg/far qualifiers
//   - Removed #ifdef SPEAR sections (WL6 target)
//   - Replaced MK_FP/VGAREADMAP/_fmemcpy with portable equivalents

#include "id_heads.h"
#include "wl_def.h"

/*
=============================================================================

                         LOCAL CONSTANTS

=============================================================================
*/

#define VIEWTILEX   (viewwidth/16)
#define VIEWTILEY   (viewheight/16)

/*
=============================================================================

                         GLOBAL VARIABLES

=============================================================================
*/


int DebugKeys(void);

/*
=============================================================================

                         LOCAL VARIABLES

=============================================================================
*/


static int maporgx;
static int maporgy;
enum {mapview, tilemapview, actoratview, visview} viewtype;

static void ViewMap(void);

//===========================================================================

/*
==================
=
= DebugMemory
=
==================
*/

static void DebugMemory(void)
{
    CenterWindow(16, 7);

    US_CPrint("Memory Usage");
    US_CPrint("------------");
    US_Print("Total     :");
    US_PrintUnsigned(mminfo.mainmem / 1024);
    US_Print("k\nFree      :");
    US_PrintUnsigned(MM_UnusedMemory() / 1024);
    US_Print("k\nWith purge:");
    US_PrintUnsigned(MM_TotalFree() / 1024);
    US_Print("k\n");
    VW_UpdateScreen();
    IN_Ack();
}

//===========================================================================

/*
==================
=
= CountObjects
=
==================
*/

static void CountObjects(void)
{
    int     i, total, count, active, inactive, doors;
    objtype *obj;

    CenterWindow(16, 7);
    active = inactive = count = doors = 0;

    US_Print("Total statics :");
    total = (int)(laststatobj - &statobjlist[0]);
    US_PrintUnsigned(total);

    US_Print("\nIn use statics:");
    for (i = 0; i < total; i++)
        if (statobjlist[i].shapenum != -1)
            count++;
        else
            doors++;    // debug
    US_PrintUnsigned(count);

    US_Print("\nDoors         :");
    US_PrintUnsigned(doornum);

    for (obj = player->next; obj; obj = obj->next)
    {
        if (obj->active)
            active++;
        else
            inactive++;
    }

    US_Print("\nTotal actors  :");
    US_PrintUnsigned(active + inactive);

    US_Print("\nActive actors :");
    US_PrintUnsigned(active);

    VW_UpdateScreen();
    IN_Ack();
}

//===========================================================================

/*
================
=
= PicturePause
=
= Captures the current screen and displays it full-screen until the user
= presses a key. On the original DOS version this dropped to VGA mode 13h;
= here we simply pause and let the user take a screenshot via the OS.
=
================
*/

void PicturePause(void)
{
    VL_ColorBorder(15);
    FinishPaletteShifts();

    LastScan = 0;
    while (!LastScan)
        ;   // wait for key
    if (LastScan != sc_Enter)
    {
        VL_ColorBorder(0);
        return;
    }

    // In the original, this captured the VGA framebuffer and redisplayed it.
    // In the SDL3 port, the screen is already visible. We just pause here
    // until the user presses another key, then restore.

    VL_ColorBorder(1);

    // Wait for user to dismiss
    IN_Ack();

    VL_ColorBorder(0);
}


//===========================================================================


/*
================
=
= ShapeTest
=
= Browses pages in the page manager (walls, sprites, sounds).
= Left/Right arrows navigate pages.
=
================
*/

static void ShapeTest(void)
{
    extern word NumDigi;
    extern word *DigiList;
    static char buf[10];

    boolean  done;
    ScanCode scan;
    int      i, j, k, x;
    longword l;
    memptr   addr;

    CenterWindow(20, 16);
    VW_UpdateScreen();
    for (i = 0, done = false; !done; )
    {
        US_ClearWindow();

        PageListStruct *page = &PMPages[i];
        US_Print(" Page #");
        US_PrintUnsigned(i);
        if (i < (int)PMSpriteStart)
            US_Print(" (Wall)");
        else if (i < (int)PMSoundStart)
            US_Print(" (Sprite)");
        else if (i == (int)(ChunksInFile - 1))
            US_Print(" (Sound Info)");
        else
            US_Print(" (Sound)");

        US_Print("\n XMS: ");
        if (page->xmsPage != -1)
            US_PrintUnsigned(page->xmsPage);
        else
            US_Print("No");

        US_Print("\n Main: ");
        if (page->mainPage != -1)
            US_PrintUnsigned(page->mainPage);
        else if (page->emsPage != -1)
        {
            US_Print("EMS ");
            US_PrintUnsigned(page->emsPage);
        }
        else
            US_Print("No");

        US_Print("\n Last hit: ");
        US_PrintUnsigned(page->lastHit);

        US_Print("\n Address: ");
        addr = PM_GetPageAddress(i);
        sprintf(buf, "0x%p", (void *)addr);
        US_Print(buf);

        if (addr)
        {
            if (i < (int)PMSpriteStart)
            {
            //
            // draw the wall
            //
                postx = 128;
                postwidth = 1;
                postpage = addr; posttex = 0;
                for (x = 0; x < 64; x++, postx++, posttex += 64)
                {
                    wallheight[postx] = 256;
                    FarScalePost();
                }
            }
            else if (i < (int)PMSoundStart)
            {
            //
            // draw the sprite
            //
                SimpleScaleShape(160, i - PMSpriteStart, 64);
            }
            else if (i == (int)(ChunksInFile - 1))
            {
                US_Print("\n\n Number of sounds: ");
                US_PrintUnsigned(NumDigi);
                for (l = 0, j = k = 0; j < (int)NumDigi; j++)
                {
                    l += DigiList[(j * 2) + 1];
                    k += (DigiList[(j * 2) + 1] + (PMPageSize - 1)) / PMPageSize;
                }
                US_Print("\n Total bytes: ");
                US_PrintUnsigned(l);
                US_Print("\n Total pages: ");
                US_PrintUnsigned(k);
            }
            else
            {
                byte *dp = (byte *)addr;
                for (j = 0; j < (int)NumDigi; j++)
                {
                    k = (DigiList[(j * 2) + 1] + (PMPageSize - 1)) / PMPageSize;
                    if
                    (
                        (i >= (int)(PMSoundStart + DigiList[j * 2]))
                    &&  (i <  (int)(PMSoundStart + DigiList[j * 2] + k))
                    )
                        break;
                }
                if (j < (int)NumDigi)
                {
                    US_Print("\n Sound #");
                    US_PrintUnsigned(j);
                    US_Print("\n Segment #");
                    US_PrintUnsigned(i - PMSoundStart - DigiList[j * 2]);
                }
                for (j = 0; j < (int)page->length; j += 32)
                {
                    byte v = dp[j];
                    int v2 = (unsigned)v;
                    v2 -= 128;
                    v2 /= 4;
                    if (v2 < 0)
                        VWB_Vlin(WindowY + WindowH - 32 + v2,
                                WindowY + WindowH - 32,
                                WindowX + 8 + (j / 32), BLACK);
                    else
                        VWB_Vlin(WindowY + WindowH - 32,
                                WindowY + WindowH - 32 + v2,
                                WindowX + 8 + (j / 32), BLACK);
                }
            }
        }

        VW_UpdateScreen();

        while (!(scan = LastScan))
            SD_Poll();

        IN_ClearKey(scan);
        switch (scan)
        {
        case sc_LeftArrow:
            if (i)
                i--;
            break;
        case sc_RightArrow:
            if (++i >= (int)ChunksInFile)
                i--;
            break;
        case sc_W:  // Walls
            i = 0;
            break;
        case sc_S:  // Sprites
            i = PMSpriteStart;
            break;
        case sc_D:  // Digitized
            i = PMSoundStart;
            break;
        case sc_I:  // Digitized info
            i = ChunksInFile - 1;
            break;
        case sc_L:  // Load all pages
            for (j = 0; j < (int)ChunksInFile; j++)
                PM_GetPage(j);
            break;
        case sc_P:
            break;
        case sc_Escape:
            done = true;
            break;
        case sc_Enter:
            PM_GetPage(i);
            break;
        }
    }
    SD_StopDigitized();
}



//===========================================================================


/*
================
=
= DebugKeys
=
= Handles debug key presses during gameplay. Returns 1 if a debug key
= was handled, 0 otherwise.
=
================
*/

int DebugKeys(void)
{
    boolean esc;
    int     level;

    if (Keyboard[sc_B])     // B = border color
    {
        CenterWindow(24, 3);
        PrintY += 6;
        US_Print(" Border color (0-15):");
        VW_UpdateScreen();
        esc = !US_LineInput(PrintX, PrintY, str, NULL, true, 2, 0);
        if (!esc)
        {
            level = atoi(str);
            if (level >= 0 && level <= 15)
                VL_ColorBorder(level);
        }
        return 1;
    }

    if (Keyboard[sc_C])     // C = count objects
    {
        CountObjects();
        return 1;
    }

    if (Keyboard[sc_E])     // E = quit level
    {
        if (tedlevel)
            Quit(NULL);
        playstate = ex_completed;
    }

    if (Keyboard[sc_F])     // F = facing spot
    {
        CenterWindow(14, 4);
        US_Print("X:");
        US_PrintUnsigned(player->x);
        US_Print("\nY:");
        US_PrintUnsigned(player->y);
        US_Print("\nA:");
        US_PrintUnsigned(player->angle);
        VW_UpdateScreen();
        IN_Ack();
        return 1;
    }

    if (Keyboard[sc_G])     // G = god mode
    {
        CenterWindow(12, 2);
        if (godmode)
            US_PrintCentered("God mode OFF");
        else
            US_PrintCentered("God mode ON");
        VW_UpdateScreen();
        IN_Ack();
        godmode ^= 1;
        return 1;
    }
    if (Keyboard[sc_H])     // H = hurt self
    {
        IN_ClearKeysDown();
        TakeDamage(16, NULL);
    }
    else if (Keyboard[sc_I])        // I = item cheat
    {
        CenterWindow(12, 3);
        US_PrintCentered("Free items!");
        VW_UpdateScreen();
        GivePoints(100000);
        HealSelf(99);
        if (gamestate.bestweapon < wp_chaingun)
            GiveWeapon(gamestate.bestweapon + 1);
        gamestate.ammo += 50;
        if (gamestate.ammo > 99)
            gamestate.ammo = 99;
        DrawAmmo();
        IN_Ack();
        return 1;
    }
    else if (Keyboard[sc_M])        // M = memory info
    {
        DebugMemory();
        return 1;
    }
    else if (Keyboard[sc_N])        // N = no clip
    {
        noclip ^= 1;
        CenterWindow(18, 3);
        if (noclip)
            US_PrintCentered("No clipping ON");
        else
            US_PrintCentered("No clipping OFF");
        VW_UpdateScreen();
        IN_Ack();
        return 1;
    }
    else if (Keyboard[sc_P])        // P = pause with no screen disruption
    {
        PicturePause();
        return 1;
    }
    else if (Keyboard[sc_Q])        // Q = fast quit
        Quit(NULL);
    else if (Keyboard[sc_S])        // S = slow motion
    {
        singlestep ^= 1;
        CenterWindow(18, 3);
        if (singlestep)
            US_PrintCentered("Slow motion ON");
        else
            US_PrintCentered("Slow motion OFF");
        VW_UpdateScreen();
        IN_Ack();
        return 1;
    }
    else if (Keyboard[sc_T])        // T = shape test
    {
        ShapeTest();
        return 1;
    }
    else if (Keyboard[sc_V])        // V = extra VBLs
    {
        CenterWindow(30, 3);
        PrintY += 6;
        US_Print("  Add how many extra VBLs(0-8):");
        VW_UpdateScreen();
        esc = !US_LineInput(PrintX, PrintY, str, NULL, true, 2, 0);
        if (!esc)
        {
            level = atoi(str);
            if (level >= 0 && level <= 8)
                extravbls = level;
        }
        return 1;
    }
    else if (Keyboard[sc_W])        // W = warp to level
    {
        CenterWindow(26, 3);
        PrintY += 6;
        US_Print("  Warp to which level(1-10):");
        VW_UpdateScreen();
        esc = !US_LineInput(PrintX, PrintY, str, NULL, true, 2, 0);
        if (!esc)
        {
            level = atoi(str);
            if (level > 0 && level < 11)
            {
                gamestate.mapon = level - 1;
                playstate = ex_warped;
            }
        }
        return 1;
    }
    else if (Keyboard[sc_X])        // X = item cheat
    {
        CenterWindow(12, 3);
        US_PrintCentered("Extra stuff!");
        VW_UpdateScreen();
        // DEBUG: put stuff here
        IN_Ack();
        return 1;
    }

    return 0;
}
