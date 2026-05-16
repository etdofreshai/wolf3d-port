// wl_inter.c - Intermission screens, level completion (SDL3 port)
// Ported from original WOLFSRC/WL_INTER.C
// Target: Wolfenstein 3D (WL6) only, flat memory model, C11

#include "id_heads.h"
#include "wl_def.h"


//==========================================================================

/*
==================
=
= ClearSplitVWB
=
==================
*/

void ClearSplitVWB(void)
{
    memset(update, 0, sizeof(update));
    WindowX = 0;
    WindowY = 0;
    WindowW = 320;
    WindowH = 160;
}


//==========================================================================

/*
==================
=
= Victory
=
==================
*/

void Victory(void)
{
    long sec;
    int i, min, kr, sr, tr, x;
    char tempstr[8];

#define RATIOX  6
#define RATIOY  14
#define TIMEX   14
#define TIMEY   8

    StartCPMusic(URAHERO_MUS);
    ClearSplitVWB();
    CacheLump(LEVELEND_LUMP_START, LEVELEND_LUMP_END);
    CA_CacheGrChunk(STARTFONT);

    CA_CacheGrChunk(C_TIMECODEPIC);

    VWB_Bar(0, 0, 320, 200 - STATUSLINES, 127);

    Write(18, 2, STR_YOUWIN);

    Write(TIMEX, TIMEY - 2, STR_TOTALTIME);

    Write(12, RATIOY - 2, "averages");

    Write(RATIOX + 8, RATIOY,      STR_RATKILL);
    Write(RATIOX + 4, RATIOY + 2,  STR_RATSECRET);
    Write(RATIOX,     RATIOY + 4,  STR_RATTREASURE);

    VWB_DrawPic(8, 4, L_BJWINSPIC);

    for (kr = sr = tr = sec = i = 0; i < 8; i++)
    {
        sec += LevelRatios[i].time;
        kr += LevelRatios[i].kill;
        sr += LevelRatios[i].secret;
        tr += LevelRatios[i].treasure;
    }

    kr /= 8;
    sr /= 8;
    tr /= 8;

    min = sec / 60;
    sec %= 60;

    if (min > 99)
        min = sec = 99;

    i = TIMEX * 8 + 1;
    VWB_DrawPic(i, TIMEY * 8, L_NUM0PIC + (min / 10));
    i += 2 * 8;
    VWB_DrawPic(i, TIMEY * 8, L_NUM0PIC + (min % 10));
    i += 2 * 8;
    Write(i / 8, TIMEY, ":");
    i += 1 * 8;
    VWB_DrawPic(i, TIMEY * 8, L_NUM0PIC + (sec / 10));
    i += 2 * 8;
    VWB_DrawPic(i, TIMEY * 8, L_NUM0PIC + (sec % 10));
    VW_UpdateScreen();

    itoa(kr, tempstr, 10);
    x = RATIOX + 24 - strlen(tempstr) * 2;
    Write(x, RATIOY, tempstr);

    itoa(sr, tempstr, 10);
    x = RATIOX + 24 - strlen(tempstr) * 2;
    Write(x, RATIOY + 2, tempstr);

    itoa(tr, tempstr, 10);
    x = RATIOX + 24 - strlen(tempstr) * 2;
    Write(x, RATIOY + 4, tempstr);

    //
    // TOTAL TIME VERIFICATION CODE
    //
    if (gamestate.difficulty >= gd_medium)
    {
        VWB_DrawPic(30 * 8, TIMEY * 8, C_TIMECODEPIC);
        fontnumber = 0;
        fontcolor = READHCOLOR;
        PrintX = 30 * 8 - 3;
        PrintY = TIMEY * 8 + 8;
        PrintX += 4;
        tempstr[0] = (((min / 10) ^ (min % 10)) ^ 0xa) + 'A';
        tempstr[1] = (((sec / 10) ^ (sec % 10)) ^ 0xa) + 'A';
        tempstr[2] = (tempstr[0] ^ tempstr[1]) + 'A';
        tempstr[3] = 0;
        US_Print(tempstr);
    }

    fontnumber = 1;

    VW_UpdateScreen();
    VW_FadeIn();

    IN_Ack();

    VW_FadeOut();

    UNCACHEGRCHUNK(C_TIMECODEPIC);
    UnCacheLump(LEVELEND_LUMP_START, LEVELEND_LUMP_END);

    EndText();
}


//==========================================================================

/*
==================
=
= PG13
=
==================
*/

void PG13(void)
{
    VW_FadeOut();
    VWB_Bar(0, 0, 320, 200, 0x82);

    CA_CacheGrChunk(PG13PIC);
    VWB_DrawPic(216, 110, PG13PIC);
    VW_UpdateScreen();

    UNCACHEGRCHUNK(PG13PIC);

    VW_FadeIn();
    IN_UserInput(TickBase * 7);

    VW_FadeOut();
}


//==========================================================================

void Write(int x, int y, char *string)
{
    int alpha[] = {L_NUM0PIC, L_NUM1PIC, L_NUM2PIC, L_NUM3PIC, L_NUM4PIC, L_NUM5PIC,
        L_NUM6PIC, L_NUM7PIC, L_NUM8PIC, L_NUM9PIC, L_COLONPIC, 0, 0, 0, 0, 0, 0, L_APIC, L_BPIC,
        L_CPIC, L_DPIC, L_EPIC, L_FPIC, L_GPIC, L_HPIC, L_IPIC, L_JPIC, L_KPIC,
        L_LPIC, L_MPIC, L_NPIC, L_OPIC, L_PPIC, L_QPIC, L_RPIC, L_SPIC, L_TPIC,
        L_UPIC, L_VPIC, L_WPIC, L_XPIC, L_YPIC, L_ZPIC};

    int i, ox, nx, ny;
    char ch;

    ox = nx = x * 8;
    ny = y * 8;
    for (i = 0; i < (int)strlen(string); i++)
        if (string[i] == '\n')
        {
            nx = ox;
            ny += 16;
        }
        else
        {
            ch = string[i];
            if (ch >= 'a')
                ch -= ('a' - 'A');
            ch -= '0';

            switch (string[i])
            {
                case '!':
                    VWB_DrawPic(nx, ny, L_EXPOINTPIC);
                    nx += 8;
                    continue;

                case '\'':
                    VWB_DrawPic(nx, ny, L_APOSTROPHEPIC);
                    nx += 8;
                    continue;

                case ' ': break;
                case 0x3a: // ':'
                    VWB_DrawPic(nx, ny, L_COLONPIC);
                    nx += 8;
                    continue;

                case '%':
                    VWB_DrawPic(nx, ny, L_PERCENTPIC);
                    break;

                default:
                    if (ch >= 0 && ch < 43)
                        VWB_DrawPic(nx, ny, alpha[ch]);
                    break;
            }
            nx += 16;
        }
}


//
// Breathe Mr. BJ!!!
//
void BJ_Breathe(void)
{
    static int which = 0, max = 10;
    int pics[2] = {L_GUYPIC, L_GUY2PIC};

    if (TimeCount > max)
    {
        which ^= 1;
        VWB_DrawPic(0, 16, pics[which]);
        VW_UpdateScreen();
        TimeCount = 0;
        max = 35;
    }
}



/*
==================
=
= LevelCompleted
=
= Entered with the screen faded out
= Still in split screen mode with the status bar
=
= Exit with the screen faded out
=
==================
*/

LRstruct LevelRatios[8];

void LevelCompleted(void)
{
    #define VBLWAIT      30
    #define PAR_AMOUNT   500
    #define PERCENT100AMT 10000
    typedef struct {
        float time;
        char timestr[6];
    } times;

    int x, i, min, sec, ratio, kr, sr, tr;
    unsigned temp;
    char tempstr[10];
    long bonus, timeleft = 0;
    times parTimes[] =
    {
        //
        // Episode One Par Times
        //
        {1.5,  "01:30"},
        {2,    "02:00"},
        {2,    "02:00"},
        {3.5,  "03:30"},
        {3,    "03:00"},
        {3,    "03:00"},
        {2.5,  "02:30"},
        {2.5,  "02:30"},
        {0,    "??:??"},   // Boss level
        {0,    "??:??"},   // Secret level

        //
        // Episode Two Par Times
        //
        {1.5,  "01:30"},
        {3.5,  "03:30"},
        {3,    "03:00"},
        {2,    "02:00"},
        {4,    "04:00"},
        {6,    "06:00"},
        {1,    "01:00"},
        {3,    "03:00"},
        {0,    "??:??"},
        {0,    "??:??"},

        //
        // Episode Three Par Times
        //
        {1.5,  "01:30"},
        {1.5,  "01:30"},
        {2.5,  "02:30"},
        {2.5,  "02:30"},
        {3.5,  "03:30"},
        {2.5,  "02:30"},
        {2,    "02:00"},
        {6,    "06:00"},
        {0,    "??:??"},
        {0,    "??:??"},

        //
        // Episode Four Par Times
        //
        {2,    "02:00"},
        {2,    "02:00"},
        {1.5,  "01:30"},
        {1,    "01:00"},
        {4.5,  "04:30"},
        {3.5,  "03:30"},
        {2,    "02:00"},
        {4.5,  "04:30"},
        {0,    "??:??"},
        {0,    "??:??"},

        //
        // Episode Five Par Times
        //
        {2.5,  "02:30"},
        {1.5,  "01:30"},
        {2.5,  "02:30"},
        {2.5,  "02:30"},
        {4,    "04:00"},
        {3,    "03:00"},
        {4.5,  "04:30"},
        {3.5,  "03:30"},
        {0,    "??:??"},
        {0,    "??:??"},

        //
        // Episode Six Par Times
        //
        {6.5,  "06:30"},
        {4,    "04:00"},
        {4.5,  "04:30"},
        {6,    "06:00"},
        {5,    "05:00"},
        {5.5,  "05:30"},
        {5.5,  "05:30"},
        {8.5,  "08:30"},
        {0,    "??:??"},
        {0,    "??:??"}
    };



    CacheLump(LEVELEND_LUMP_START, LEVELEND_LUMP_END);
    ClearSplitVWB();
    VWB_Bar(0, 0, 320, 200 - STATUSLINES, 127);
    StartCPMusic(ENDLEVEL_MUS);

    //
    // do the intermission
    //
    IN_ClearKeysDown();
    IN_StartAck();

    VWB_DrawPic(0, 16, L_GUYPIC);

    if (mapon < 8)
    {
        Write(14, 2, "floor\ncompleted");

        Write(14, 7, STR_BONUS "     0");
        Write(16, 10, STR_TIME);
        Write(16, 12, STR_PAR);

        Write(9, 14,    STR_RAT2KILL);
        Write(5, 16,    STR_RAT2SECRET);
        Write(1, 18,    STR_RAT2TREASURE);

        Write(26, 2, itoa(gamestate.mapon + 1, tempstr, 10));

        Write(26, 12, parTimes[gamestate.episode * 10 + mapon].timestr);

        //
        // PRINT TIME
        //
        sec = gamestate.TimeCount / 70;

        if (sec > 99 * 60)
            sec = 99 * 60;

        if (gamestate.TimeCount < parTimes[gamestate.episode * 10 + mapon].time * 4200)
            timeleft = (parTimes[gamestate.episode * 10 + mapon].time * 4200) / 70 - sec;

        min = sec / 60;
        sec %= 60;

        i = 26 * 8;
        VWB_DrawPic(i, 10 * 8, L_NUM0PIC + (min / 10));
        i += 2 * 8;
        VWB_DrawPic(i, 10 * 8, L_NUM0PIC + (min % 10));
        i += 2 * 8;
        Write(i / 8, 10, ":");
        i += 1 * 8;
        VWB_DrawPic(i, 10 * 8, L_NUM0PIC + (sec / 10));
        i += 2 * 8;
        VWB_DrawPic(i, 10 * 8, L_NUM0PIC + (sec % 10));

        VW_UpdateScreen();
        VW_FadeIn();


        //
        // FIGURE RATIOS OUT BEFOREHAND
        //
        kr = sr = tr = 0;
        if (gamestate.killtotal)
            kr = (gamestate.killcount * 100) / gamestate.killtotal;
        if (gamestate.secrettotal)
            sr = (gamestate.secretcount * 100) / gamestate.secrettotal;
        if (gamestate.treasuretotal)
            tr = (gamestate.treasurecount * 100) / gamestate.treasuretotal;


        //
        // PRINT TIME BONUS
        //
        bonus = timeleft * PAR_AMOUNT;
        if (bonus)
        {
            for (i = 0; i <= timeleft; i++)
            {
                ltoa((long)i * PAR_AMOUNT, tempstr, 10);
                x = 36 - strlen(tempstr) * 2;
                Write(x, 7, tempstr);
                if (!(i % (PAR_AMOUNT / 10)))
                    SD_PlaySound(ENDBONUS1SND);
                VW_UpdateScreen();
                while (SD_SoundPlaying())
                    BJ_Breathe();
                if (IN_CheckAck())
                    goto done;
            }

            VW_UpdateScreen();
            SD_PlaySound(ENDBONUS2SND);
            while (SD_SoundPlaying())
                BJ_Breathe();
        }


        #define RATIOXX  37
        //
        // KILL RATIO
        //
        ratio = kr;
        for (i = 0; i <= ratio; i++)
        {
            itoa(i, tempstr, 10);
            x = RATIOXX - strlen(tempstr) * 2;
            Write(x, 14, tempstr);
            if (!(i % 10))
                SD_PlaySound(ENDBONUS1SND);
            VW_UpdateScreen();
            while (SD_SoundPlaying())
                BJ_Breathe();

            if (IN_CheckAck())
                goto done;
        }
        if (ratio == 100)
        {
            VW_WaitVBL(VBLWAIT);
            SD_StopSound();
            bonus += PERCENT100AMT;
            ltoa(bonus, tempstr, 10);
            x = (RATIOXX - 1) - strlen(tempstr) * 2;
            Write(x, 7, tempstr);
            VW_UpdateScreen();
            SD_PlaySound(PERCENT100SND);
        }
        else if (!ratio)
        {
            VW_WaitVBL(VBLWAIT);
            SD_StopSound();
            SD_PlaySound(NOBONUSSND);
        }
        else
            SD_PlaySound(ENDBONUS2SND);

        VW_UpdateScreen();
        while (SD_SoundPlaying())
            BJ_Breathe();


        //
        // SECRET RATIO
        //
        ratio = sr;
        for (i = 0; i <= ratio; i++)
        {
            itoa(i, tempstr, 10);
            x = RATIOXX - strlen(tempstr) * 2;
            Write(x, 16, tempstr);
            if (!(i % 10))
                SD_PlaySound(ENDBONUS1SND);
            VW_UpdateScreen();
            while (SD_SoundPlaying())
                BJ_Breathe();
            BJ_Breathe();

            if (IN_CheckAck())
                goto done;
        }
        if (ratio == 100)
        {
            VW_WaitVBL(VBLWAIT);
            SD_StopSound();
            bonus += PERCENT100AMT;
            ltoa(bonus, tempstr, 10);
            x = (RATIOXX - 1) - strlen(tempstr) * 2;
            Write(x, 7, tempstr);
            VW_UpdateScreen();
            SD_PlaySound(PERCENT100SND);
        }
        else if (!ratio)
        {
            VW_WaitVBL(VBLWAIT);
            SD_StopSound();
            SD_PlaySound(NOBONUSSND);
        }
        else
            SD_PlaySound(ENDBONUS2SND);
        VW_UpdateScreen();
        while (SD_SoundPlaying())
            BJ_Breathe();


        //
        // TREASURE RATIO
        //
        ratio = tr;
        for (i = 0; i <= ratio; i++)
        {
            itoa(i, tempstr, 10);
            x = RATIOXX - strlen(tempstr) * 2;
            Write(x, 18, tempstr);
            if (!(i % 10))
                SD_PlaySound(ENDBONUS1SND);
            VW_UpdateScreen();
            while (SD_SoundPlaying())
                BJ_Breathe();
            if (IN_CheckAck())
                goto done;
        }
        if (ratio == 100)
        {
            VW_WaitVBL(VBLWAIT);
            SD_StopSound();
            bonus += PERCENT100AMT;
            ltoa(bonus, tempstr, 10);
            x = (RATIOXX - 1) - strlen(tempstr) * 2;
            Write(x, 7, tempstr);
            VW_UpdateScreen();
            SD_PlaySound(PERCENT100SND);
        }
        else if (!ratio)
        {
            VW_WaitVBL(VBLWAIT);
            SD_StopSound();
            SD_PlaySound(NOBONUSSND);
        }
        else
            SD_PlaySound(ENDBONUS2SND);
        VW_UpdateScreen();
        while (SD_SoundPlaying())
            BJ_Breathe();


        //
        // JUMP STRAIGHT HERE IF KEY PRESSED
        //
        done:

        itoa(kr, tempstr, 10);
        x = RATIOXX - strlen(tempstr) * 2;
        Write(x, 14, tempstr);

        itoa(sr, tempstr, 10);
        x = RATIOXX - strlen(tempstr) * 2;
        Write(x, 16, tempstr);

        itoa(tr, tempstr, 10);
        x = RATIOXX - strlen(tempstr) * 2;
        Write(x, 18, tempstr);

        bonus = (long)timeleft * PAR_AMOUNT +
              (PERCENT100AMT * (kr == 100)) +
              (PERCENT100AMT * (sr == 100)) +
              (PERCENT100AMT * (tr == 100));

        GivePoints(bonus);
        ltoa(bonus, tempstr, 10);
        x = 36 - strlen(tempstr) * 2;
        Write(x, 7, tempstr);

        //
        // SAVE RATIO INFORMATION FOR ENDGAME
        //
        LevelRatios[mapon].kill = kr;
        LevelRatios[mapon].secret = sr;
        LevelRatios[mapon].treasure = tr;
        LevelRatios[mapon].time = min * 60 + sec;
    }
    else
    {
        Write(14, 4, "secret floor\n completed!");

        Write(10, 16, "15000 bonus!");

        VW_UpdateScreen();
        VW_FadeIn();

        GivePoints(15000);
    }


    DrawScore();
    VW_UpdateScreen();

    TimeCount = 0;
    IN_StartAck();
    while (!IN_CheckAck())
        BJ_Breathe();

    VW_FadeOut();
    temp = bufferofs;
    for (i = 0; i < 3; i++)
    {
        bufferofs = screenloc[i];
        DrawPlayBorder();
    }
    bufferofs = temp;

    UnCacheLump(LEVELEND_LUMP_START, LEVELEND_LUMP_END);
}



//==========================================================================


/*
=================
=
= PreloadGraphics
=
= Fill the cache up
=
=================
*/

boolean PreloadUpdate(word current, word total)
{
    unsigned w = WindowW - 10;

    VWB_Bar(WindowX + 5, WindowY + WindowH - 3, w, 2, BLACK);
    w = ((long)w * current) / total;
    if (w)
    {
        VWB_Bar(WindowX + 5, WindowY + WindowH - 3, w, 2, 0x37);
        VWB_Bar(WindowX + 5, WindowY + WindowH - 3, w - 1, 1, 0x32);
    }
    VW_UpdateScreen();
    return false;
}

void PreloadGraphics(void)
{
    DrawLevel();
    ClearSplitVWB();

    VWB_Bar(0, 0, 320, 200 - STATUSLINES, 127);

    LatchDrawPic(20 - 14, 80 - 3 * 8, GETPSYCHEDPIC);

    WindowX = 160 - 14 * 8;
    WindowY = 80 - 3 * 8;
    WindowW = 28 * 8;
    WindowH = 48;
    VW_UpdateScreen();
    VW_FadeIn();

    PM_Preload(PreloadUpdate);
    IN_UserInput(70);
    VW_FadeOut();

    DrawPlayBorder();
    VW_UpdateScreen();
}


//==========================================================================

/*
==================
=
= DrawHighScores
=
==================
*/

void DrawHighScores(void)
{
    char buffer[16], *str, buffer1[5];
    byte temp, temp1, temp2, temp3;
    word i, j,
         w, h,
         x, y;
    HighScore *s;

    MM_SortMem();

    CA_CacheGrChunk(HIGHSCORESPIC);
    CA_CacheGrChunk(STARTFONT);
    CA_CacheGrChunk(C_LEVELPIC);
    CA_CacheGrChunk(C_SCOREPIC);
    CA_CacheGrChunk(C_NAMEPIC);

    ClearMScreen();
    DrawStripes(10);

    VWB_DrawPic(48, 0, HIGHSCORESPIC);
    UNCACHEGRCHUNK(HIGHSCORESPIC);

    VWB_DrawPic(4 * 8, 68, C_NAMEPIC);
    VWB_DrawPic(20 * 8, 68, C_LEVELPIC);
    VWB_DrawPic(28 * 8, 68, C_SCOREPIC);
    fontnumber = 0;

    SETFONTCOLOR(15, 0x29);

    for (i = 0, s = Scores; i < MaxScores; i++, s++)
    {
        PrintY = 76 + (16 * i);

        //
        // name
        //
        PrintX = 4 * 8;
        US_Print(s->name);

        //
        // level
        //
        ultoa(s->completed, buffer, 10);

        //
        // level
        //
        ultoa(s->completed, buffer, 10);
        for (str = buffer; *str; str++)
            *str = *str + (129 - '0');
        USL_MeasureString(buffer, &w, &h);
        PrintX = (22 * 8) - w;

        PrintX -= 6;
        itoa(s->episode + 1, buffer1, 10);
        US_Print("E");
        US_Print(buffer1);
        US_Print("/L");

        US_Print(buffer);

        //
        // score
        //
        ultoa(s->score, buffer, 10);
        for (str = buffer; *str; str++)
            *str = *str + (129 - '0');
        USL_MeasureString(buffer, &w, &h);
        PrintX = (34 * 8) - 8 - w;
        US_Print(buffer);
    }

    VW_UpdateScreen();
}


//===========================================================================


/*
=======================
=
= CheckHighScore
=
=======================
*/

void CheckHighScore(long score, word other)
{
    word i, j;
    int n;
    HighScore myscore;

    strcpy(myscore.name, "");
    myscore.score = score;
    myscore.episode = gamestate.episode;
    myscore.completed = other;

    for (i = 0, n = -1; i < MaxScores; i++)
    {
        if
        (
            (myscore.score > Scores[i].score)
        ||  (
                (myscore.score == Scores[i].score)
            &&  (myscore.completed > Scores[i].completed)
            )
        )
        {
            for (j = MaxScores; --j > i;)
                Scores[j] = Scores[j - 1];
            Scores[i] = myscore;
            n = i;
            break;
        }
    }

    StartCPMusic(ROSTER_MUS);
    DrawHighScores();

    VW_FadeIn();

    if (n != -1)
    {
        //
        // got a high score
        //
        PrintY = 76 + (16 * n);
        PrintX = 4 * 8;
        backcolor = BORDCOLOR;
        fontcolor = 15;
        US_LineInput(PrintX, PrintY, Scores[n].name, nil, true, MaxHighName, 100);
    }
    else
    {
        IN_ClearKeysDown();
        IN_UserInput(500);
    }
}
