// wl_main.c - Main entry point (SDL3 port)
// Ported from original WOLFSRC/WL_MAIN.C

#include "wl_def.h"

#include <SDL3/SDL.h>
#include <sys/stat.h>

// Forward declarations for functions defined in other game modules
// that are not yet declared in wl_def.h
extern void LoadLatchMem(void);

static int WL_GetVerifyDemoIndex(void);

/*
=============================================================================

                           WOLFENSTEIN 3-D

                      An Id Software production

                           by John Carmack

=============================================================================
*/

/*
=============================================================================

                         LOCAL CONSTANTS

=============================================================================
*/

#define FOCALLENGTH     (0x5700l)               // in global coordinates
#define VIEWGLOBAL      0x10000                 // globals visible flush to wall

#define VIEWWIDTH       256                     // size of view window
#define VIEWHEIGHT      144

/*
=============================================================================

                         GLOBAL VARIABLES

=============================================================================
*/

char            str[80], str2[20];
int             tedlevelnum;
boolean         tedlevel;
boolean         nospr;
boolean         IsA386;
int             dirangle[9] = {
    0, ANGLES/8, 2*ANGLES/8, 3*ANGLES/8, 4*ANGLES/8,
    5*ANGLES/8, 6*ANGLES/8, 7*ANGLES/8, ANGLES
};

//
// projection variables
//
fixed           focallength;
unsigned        screenofs;
int             viewwidth;
int             viewheight;
int             centerx;
int             shootdelta;
fixed           scale, maxslope;
long            heightnumerator;
int             minheightdiv;

boolean         startgame, virtualreality;
int             mouseadjustment;
int             LastDemo;          // attract-mode demo rotation index

char            configname[13] = "CONFIG.";

// stored command line
static int      g_argc = 0;
static char     **g_argv = NULL;

/*
=============================================================================

                         LOCAL VARIABLES

=============================================================================
*/

/*
====================
=
= ReadConfig
=
====================
*/

void ReadConfig(void)
{
    FILE *file;
    SDMode sd;
    SMMode sm;
    SDSMode sds;

    file = fopen(configname, "rb");
    if (file)
    {
        //
        // valid config file
        //
        fread(Scores, sizeof(HighScore), MaxScores, file);

        fread(&sd, sizeof(sd), 1, file);
        fread(&sm, sizeof(sm), 1, file);
        fread(&sds, sizeof(sds), 1, file);

        fread(&mouseenabled, sizeof(mouseenabled), 1, file);
        fread(&joystickenabled, sizeof(joystickenabled), 1, file);
        fread(&joypadenabled, sizeof(joypadenabled), 1, file);
        fread(&joystickprogressive, sizeof(joystickprogressive), 1, file);
        fread(&joystickport, sizeof(joystickport), 1, file);

        fread(&dirscan, sizeof(dirscan), 1, file);
        fread(&buttonscan, sizeof(buttonscan), 1, file);
        fread(&buttonmouse, sizeof(buttonmouse), 1, file);
        fread(&buttonjoy, sizeof(buttonjoy), 1, file);

        fread(&viewsize, sizeof(viewsize), 1, file);
        fread(&mouseadjustment, sizeof(mouseadjustment), 1, file);

        fclose(file);

        if (sd == sdm_AdLib && !AdLibPresent && !SoundBlasterPresent)
        {
            sd = sdm_PC;
            sm = smm_Off;
        }

        if ((sds == sds_SoundBlaster && !SoundBlasterPresent) ||
            (sds == sds_SoundSource && !SoundSourcePresent))
            sds = sds_Off;

        if (!MousePresent)
            mouseenabled = false;
        if (!JoysPresent[joystickport])
            joystickenabled = false;
    }
    else
    {
        //
        // no config file, so select by hardware
        //
        if (SoundBlasterPresent || AdLibPresent)
        {
            sd = sdm_AdLib;
            sm = smm_AdLib;
        }
        else
        {
            sd = sdm_PC;
            sm = smm_Off;
        }

        if (SoundBlasterPresent)
            sds = sds_SoundBlaster;
        else if (SoundSourcePresent)
            sds = sds_SoundSource;
        else
            sds = sds_Off;

        if (MousePresent)
            mouseenabled = true;

        joystickenabled = false;
        joypadenabled = false;
        joystickport = 0;
        joystickprogressive = false;

        viewsize = 15;
        mouseadjustment = 5;
    }

    SD_SetMusicMode(sm);
    SD_SetSoundMode(sd);
    SD_SetDigiDevice(sds);
}


/*
====================
=
= WriteConfig
=
====================
*/

void WriteConfig(void)
{
    FILE *file;

    file = fopen(configname, "wb");
    if (file)
    {
        fwrite(Scores, sizeof(HighScore), MaxScores, file);

        fwrite(&SoundMode, sizeof(SoundMode), 1, file);
        fwrite(&MusicMode, sizeof(MusicMode), 1, file);
        fwrite(&DigiMode, sizeof(DigiMode), 1, file);

        fwrite(&mouseenabled, sizeof(mouseenabled), 1, file);
        fwrite(&joystickenabled, sizeof(joystickenabled), 1, file);
        fwrite(&joypadenabled, sizeof(joypadenabled), 1, file);
        fwrite(&joystickprogressive, sizeof(joystickprogressive), 1, file);
        fwrite(&joystickport, sizeof(joystickport), 1, file);

        fwrite(&dirscan, sizeof(dirscan), 1, file);
        fwrite(&buttonscan, sizeof(buttonscan), 1, file);
        fwrite(&buttonmouse, sizeof(buttonmouse), 1, file);
        fwrite(&buttonjoy, sizeof(buttonjoy), 1, file);

        fwrite(&viewsize, sizeof(viewsize), 1, file);
        fwrite(&mouseadjustment, sizeof(mouseadjustment), 1, file);

        fclose(file);
    }
}


//===========================================================================

/*
========================
=
= Patch386
=
= In the SDL3 port, 386 detection is irrelevant. We skip the assembly
= patching entirely. The flag is kept for compatibility.
=
========================
*/

static char *JHParmStrings[] = {"no386", nil};

void Patch386(void)
{
    int i;

    for (i = 1; i < g_argc; i++)
        if (US_CheckParm(g_argv[i], JHParmStrings) == 0)
        {
            IsA386 = false;
            return;
        }

    // Assume modern CPU
    IsA386 = true;
}

//===========================================================================

/*
=====================
=
= NewGame
=
= Set up new game to start from the beginning
=
=====================
*/

void NewGame(int difficulty, int episode)
{
    memset(&gamestate, 0, sizeof(gamestate));
    gamestate.difficulty = difficulty;
    gamestate.weapon = gamestate.bestweapon
        = gamestate.chosenweapon = wp_pistol;
    gamestate.health = 100;
    gamestate.ammo = STARTAMMO;
    gamestate.lives = 3;
    gamestate.nextextra = EXTRAPOINTS;
    gamestate.episode = episode;

    startgame = true;
}

//===========================================================================

void DiskFlopAnim(int x, int y)
{
    static char which = 0;
    if (!x && !y)
        return;
    VWB_DrawPic(x, y, C_DISKLOADING1PIC + which);
    VW_UpdateScreen();
    which ^= 1;
}


long DoChecksum(byte *source, unsigned size, long checksum)
{
    unsigned i;

    for (i = 0; i < size - 1; i++)
        checksum += source[i] ^ source[i + 1];

    return checksum;
}


/*
==================
=
= SaveTheGame
=
==================
*/

boolean SaveTheGame(int file, int x, int y)
{
    long avail, size, checksum;
    objtype *ob, nullobj;
    FILE *fp = (FILE *)(intptr_t)file;

    if (!fp) return false;

    // Estimate available disk space - just assume plenty in SDL3 port
    avail = 1024 * 1024 * 100;  // assume 100MB free

    size = 0;
    for (ob = player; ob; ob = ob->next)
        size += sizeof(*ob);
    size += sizeof(nullobj);

    size += (long)sizeof(gamestate) +
            (long)sizeof(tilemap) +
            (long)sizeof(actorat) +
            (long)sizeof(laststatobj) +
            (long)sizeof(statobjlist) +
            (long)sizeof(doorposition) +
            (long)sizeof(pwallstate) +
            (long)sizeof(pwallx) +
            (long)sizeof(pwally) +
            (long)sizeof(pwalldir) +
            (long)sizeof(pwallpos);

    if (avail < size)
    {
        return false;
    }

    checksum = 0;

    DiskFlopAnim(x, y);
    fwrite(&gamestate, sizeof(gamestate), 1, fp);
    checksum = DoChecksum((byte *)&gamestate, sizeof(gamestate), checksum);

    DiskFlopAnim(x, y);
    fwrite(tilemap, sizeof(tilemap), 1, fp);
    checksum = DoChecksum((byte *)tilemap, sizeof(tilemap), checksum);
    DiskFlopAnim(x, y);
    fwrite(actorat, sizeof(actorat), 1, fp);
    checksum = DoChecksum((byte *)actorat, sizeof(actorat), checksum);

    fwrite(areaconnect, sizeof(areaconnect), 1, fp);
    fwrite(areabyplayer, sizeof(areabyplayer), 1, fp);

    for (ob = player; ob; ob = ob->next)
    {
        DiskFlopAnim(x, y);
        fwrite(ob, sizeof(*ob), 1, fp);
    }
    nullobj.active = ac_badobject;      // end of file marker
    DiskFlopAnim(x, y);
    fwrite(&nullobj, sizeof(nullobj), 1, fp);

    DiskFlopAnim(x, y);
    fwrite(&laststatobj, sizeof(laststatobj), 1, fp);
    checksum = DoChecksum((byte *)&laststatobj, sizeof(laststatobj), checksum);
    DiskFlopAnim(x, y);
    fwrite(statobjlist, sizeof(statobjlist), 1, fp);
    checksum = DoChecksum((byte *)statobjlist, sizeof(statobjlist), checksum);

    DiskFlopAnim(x, y);
    fwrite(doorposition, sizeof(doorposition), 1, fp);
    checksum = DoChecksum((byte *)doorposition, sizeof(doorposition), checksum);
    DiskFlopAnim(x, y);
    fwrite(doorobjlist, sizeof(doorobjlist), 1, fp);
    checksum = DoChecksum((byte *)doorobjlist, sizeof(doorobjlist), checksum);

    DiskFlopAnim(x, y);
    fwrite(&pwallstate, sizeof(pwallstate), 1, fp);
    checksum = DoChecksum((byte *)&pwallstate, sizeof(pwallstate), checksum);
    fwrite(&pwallx, sizeof(pwallx), 1, fp);
    checksum = DoChecksum((byte *)&pwallx, sizeof(pwallx), checksum);
    fwrite(&pwally, sizeof(pwally), 1, fp);
    checksum = DoChecksum((byte *)&pwally, sizeof(pwally), checksum);
    fwrite(&pwalldir, sizeof(pwalldir), 1, fp);
    checksum = DoChecksum((byte *)&pwalldir, sizeof(pwalldir), checksum);
    fwrite(&pwallpos, sizeof(pwallpos), 1, fp);
    checksum = DoChecksum((byte *)&pwallpos, sizeof(pwallpos), checksum);

    //
    // WRITE OUT CHECKSUM
    //
    fwrite(&checksum, sizeof(checksum), 1, fp);

    return true;
}

//===========================================================================

/*
==================
=
= LoadTheGame
=
==================
*/

boolean LoadTheGame(int file, int x, int y)
{
    long checksum, oldchecksum;
    objtype nullobj;
    FILE *fp = (FILE *)(intptr_t)file;

    if (!fp) return false;

    checksum = 0;

    DiskFlopAnim(x, y);
    fread(&gamestate, sizeof(gamestate), 1, fp);
    checksum = DoChecksum((byte *)&gamestate, sizeof(gamestate), checksum);

    DiskFlopAnim(x, y);
    SetupGameLevel();

    DiskFlopAnim(x, y);
    fread(tilemap, sizeof(tilemap), 1, fp);
    checksum = DoChecksum((byte *)tilemap, sizeof(tilemap), checksum);
    DiskFlopAnim(x, y);
    fread(actorat, sizeof(actorat), 1, fp);
    checksum = DoChecksum((byte *)actorat, sizeof(actorat), checksum);

    fread(areaconnect, sizeof(areaconnect), 1, fp);
    fread(areabyplayer, sizeof(areabyplayer), 1, fp);

    InitActorList();
    DiskFlopAnim(x, y);
    {
        struct objstruct *saved_next = player->next;
        struct objstruct *saved_prev = player->prev;
        fread(player, sizeof(*player), 1, fp);
        player->next = saved_next;
        player->prev = saved_prev;
    }

    while (1)
    {
        DiskFlopAnim(x, y);
        fread(&nullobj, sizeof(nullobj), 1, fp);
        if (nullobj.active == ac_badobject)
            break;
        GetNewActor();
        // don't copy over the links
        memcpy(new, &nullobj, (char *)&new->next - (char *)new);
    }

    DiskFlopAnim(x, y);
    fread(&laststatobj, sizeof(laststatobj), 1, fp);
    checksum = DoChecksum((byte *)&laststatobj, sizeof(laststatobj), checksum);
    DiskFlopAnim(x, y);
    fread(statobjlist, sizeof(statobjlist), 1, fp);
    checksum = DoChecksum((byte *)statobjlist, sizeof(statobjlist), checksum);

    DiskFlopAnim(x, y);
    fread(doorposition, sizeof(doorposition), 1, fp);
    checksum = DoChecksum((byte *)doorposition, sizeof(doorposition), checksum);
    DiskFlopAnim(x, y);
    fread(doorobjlist, sizeof(doorobjlist), 1, fp);
    checksum = DoChecksum((byte *)doorobjlist, sizeof(doorobjlist), checksum);

    DiskFlopAnim(x, y);
    fread(&pwallstate, sizeof(pwallstate), 1, fp);
    checksum = DoChecksum((byte *)&pwallstate, sizeof(pwallstate), checksum);
    fread(&pwallx, sizeof(pwallx), 1, fp);
    checksum = DoChecksum((byte *)&pwallx, sizeof(pwallx), checksum);
    fread(&pwally, sizeof(pwally), 1, fp);
    checksum = DoChecksum((byte *)&pwally, sizeof(pwally), checksum);
    fread(&pwalldir, sizeof(pwalldir), 1, fp);
    checksum = DoChecksum((byte *)&pwalldir, sizeof(pwalldir), checksum);
    fread(&pwallpos, sizeof(pwallpos), 1, fp);
    checksum = DoChecksum((byte *)&pwallpos, sizeof(pwallpos), checksum);

    fread(&oldchecksum, sizeof(oldchecksum), 1, fp);

    if (oldchecksum != checksum)
    {
        IN_ClearKeysDown();
        IN_Ack();

        gamestate.score = 0;
        gamestate.lives = 1;
        gamestate.weapon =
            gamestate.chosenweapon =
            gamestate.bestweapon = wp_pistol;
        gamestate.ammo = 8;
    }

    return true;
}

//===========================================================================

/*
==========================
=
= ShutdownId
=
= Shuts down all ID_?? managers
=
==========================
*/

void ShutdownId(void)
{
    US_Shutdown();
    SD_Shutdown();
    PM_Shutdown();
    IN_Shutdown();
    VW_Shutdown();
    CA_Shutdown();
    MM_Shutdown();
}


//===========================================================================

/*
==================
=
= BuildTables
=
= Calculates:
=
= scale                 projection constant
= sintable/costable     overlapping fractional tables
=
==================
*/

const float radtoint = (float)FINEANGLES / 2 / PI;

void BuildTables(void)
{
    int     i;
    float   angle, anglestep;
    double  tang;
    fixed   value;

    //
    // calculate fine tangents
    //
    for (i = 0; i < FINEANGLES / 8; i++)
    {
        tang = tan((i + 0.5) / radtoint);
        finetangent[i] = tang * TILEGLOBAL;
        finetangent[FINEANGLES / 4 - 1 - i] = 1 / tang * TILEGLOBAL;
    }

    //
    // costable overlays sintable with a quarter phase shift
    // ANGLES is assumed to be divisible by four
    //
    // The low word of the value is the fraction, the high bit is the sign bit,
    // bits 16-30 should be 0
    //

    angle = 0;
    anglestep = PI / 2 / ANGLEQUAD;
    for (i = 0; i <= ANGLEQUAD; i++)
    {
        value = GLOBAL1 * sin(angle);
        sintable[i] =
            sintable[i + ANGLES] =
            sintable[ANGLES / 2 - i] = value;
        sintable[ANGLES - i] =
            sintable[ANGLES / 2 + i] = value | 0x80000000l;
        angle += anglestep;
    }
}

//===========================================================================


/*
====================
=
= CalcProjection
=
= Uses focallength
=
====================
*/

void CalcProjection(long focal)
{
    int     i;
    long    intang;
    float   angle;
    double  tang;
    double  planedist;
    double  globinhalf;
    int     halfview;
    double  halfangle, facedist;

    focallength = focal;
    facedist = focal + MINDIST;
    halfview = viewwidth / 2;      // half view in pixels

    //
    // calculate scale value for vertical height calculations
    // and sprite x calculations
    //
    scale = halfview * facedist / (VIEWGLOBAL / 2);

    //
    // divide heightnumerator by a posts distance to get the posts height for
    // the heightbuffer.  The pixel height is height>>2
    //
    heightnumerator = (TILEGLOBAL * scale) >> 6;
    minheightdiv = heightnumerator / 0x7fff + 1;

    //
    // calculate the angle offset from view angle of each pixel's ray
    //
    for (i = 0; i < halfview; i++)
    {
        // start 1/2 pixel over, so viewangle bisects two middle pixels
        tang = (long)i * VIEWGLOBAL / viewwidth / facedist;
        angle = atan(tang);
        intang = angle * radtoint;
        pixelangle[halfview - 1 - i] = intang;
        pixelangle[halfview + i] = -intang;
    }

    //
    // if a point's abs(y/x) is greater than maxslope, the point is outside
    // the view area
    //
    maxslope = finetangent[pixelangle[0]];
    maxslope >>= 8;
}



//===========================================================================

/*
===================
=
= SetupWalls
=
= Map tile values to scaled pics
=
===================
*/

void SetupWalls(void)
{
    int i;

    for (i = 1; i < MAXWALLTILES; i++)
    {
        horizwall[i] = (i - 1) * 2;
        vertwall[i] = (i - 1) * 2 + 1;
    }
}

//===========================================================================

/*
==========================
=
= SignonScreen
=
==========================
*/

void SignonScreen(void)
{
    VL_SetVGAPlaneMode();
    VL_TestPaletteSet();
    VL_SetPalette((byte *)gamepal);

    // SDL port has no built-in linked signon bitmap (`introscn`/`signon`),
    // so draw the title art where available for an equivalent full-screen start.
    if (!virtualreality)
    {
        CA_CacheGrChunk(TITLEPIC);
        if (grsegs[TITLEPIC])
            VW_DrawPic(0, 0, TITLEPIC);
        else
            VL_ClearVideo(14);

        VL_Present();
    }
}

static int WL_GetVerifyDemoIndex(void)
{
    const char *env = SDL_getenv("WOLF3D_VERIFY_DEMO");
    const char *p;
    int value;

    if (!env || !*env)
        return -1;

    p = env;
    if (*p == '-' || *p == '+')
        p++;

    if (*p < '0' || *p > '9')
        return -1;

    value = SDL_atoi(env);
    if (value < 0)
        return -1;
    fprintf(stderr, "[verify-demo-env] WOLF3D_VERIFY_DEMO=%s parsed=%d masked=%d\n",
            env, value, value & 3);
    fflush(stderr);
    return value & 3;
}

/*
==========================
=
= FinishSignon
=
==========================
*/

void FinishSignon(void)
{
    if (!NoWait)
        IN_Ack();
}


//===========================================================================

/*
=================
=
= MS_CheckParm
=
====================
*/

boolean MS_CheckParm(char *check)
{
    int  i;
    char *parm;

    for (i = 1; i < g_argc; i++)
    {
        parm = g_argv[i];

        while (!isalpha(*parm))         // skip - / \ etc.. in front of parm
            if (!*parm++)
                break;                  // hit end of string without an alphanum

        if (wl_stricmp(check, parm) == 0)
            return true;
    }

    return false;
}

//===========================================================================

/*
=====================
=
= InitDigiMap
=
=====================
*/

static int wolfdigimap[] =
{
    // These first sounds are in the upload version
    HALTSND,                0,
    DOGBARKSND,             1,
    CLOSEDOORSND,           2,
    OPENDOORSND,            3,
    ATKMACHINEGUNSND,       4,
    ATKPISTOLSND,           5,
    ATKGATLINGSND,          6,
    SCHUTZADSND,            7,
    GUTENTAGSND,            8,
    MUTTISND,               9,
    BOSSFIRESND,            10,
    SSFIRESND,              11,
    DEATHSCREAM1SND,        12,
    DEATHSCREAM2SND,        13,
    DEATHSCREAM3SND,        13,
    TAKEDAMAGESND,          14,
    PUSHWALLSND,            15,

    LEBENSND,               20,
    NAZIFIRESND,            21,
    SLURPIESND,             22,

    YEAHSND,                32,

    // These are in all other episodes
    DOGDEATHSND,            16,
    AHHHGSND,               17,
    DIESND,                 18,
    EVASND,                 19,

    TOT_HUNDSND,            23,
    MEINGOTTSND,            24,
    SCHABBSHASND,           25,
    HITLERHASND,            26,
    SPIONSND,               27,
    NEINSOVASSND,           28,
    DOGATTACKSND,           29,
    LEVELDONESND,           30,
    MECHSTEPSND,            31,

    SCHEISTSND,             33,
    DEATHSCREAM4SND,        34,
    DEATHSCREAM5SND,        35,
    DONNERSND,              36,
    EINESND,                37,
    ERLAUBENSND,            38,
    DEATHSCREAM6SND,        39,
    DEATHSCREAM7SND,        40,
    DEATHSCREAM8SND,        41,
    DEATHSCREAM9SND,        42,
    KEINSND,                43,
    MEINSND,                44,
    ROSESND,                45,

    LASTSOUND
};


void InitDigiMap(void)
{
    int *map;

    for (map = wolfdigimap; *map != LASTSOUND; map += 2)
        DigiMap[map[0]] = map[1];
}

/*
==========================
=
= InitGame
=
= Load a few things right away
=
==========================
*/

void InitGame(void)
{
    int       i, x, y;
    unsigned  *blockstart;


    if (MS_CheckParm("virtual"))
        virtualreality = true;
    else
        virtualreality = false;

    MM_Startup();                   // so the signon screen can be freed

    VW_Startup();
    IN_Startup();
    PM_Startup();
    PM_UnlockMainMem();
    SD_Startup();
    CA_Startup();
    US_Startup();

    // The original demo flow loads resources (palette + title picture chunk)
    // before entering sign-on and title rendering.
    SignonScreen();

    //
    // build some tables
    //
    InitDigiMap();

    for (i = 0; i < MAPSIZE; i++)
    {
        nearmapylookup[i] = &tilemap[0][0] + MAPSIZE * i;
    }

    for (i = 0; i < PORTTILESHIGH; i++)
        uwidthtable[i] = UPDATEWIDE * i;

    blockstart = &blockstarts[0];
    for (y = 0; y < UPDATEHIGH; y++)
        for (x = 0; x < UPDATEWIDE; x++)
            *blockstart++ = SCREENWIDTH * 16 * y + x * TILEWIDTH;

    updateptr = &update[0];

    bufferofs = 0;
    displayofs = 0;
    ReadConfig();

    //
    // load in and lock down some basic chunks
    //
    CA_CacheGrChunk(STARTFONT);
    MM_SetLock(&grsegs[STARTFONT], true);

    LoadLatchMem();
    BuildTables();                   // trig tables
    SetupWalls();

    NewViewSize(viewsize);


    //
    // initialize variables
    //
    InitRedShifts();
    // Skip signon wait (no signon bitmap in SDL3 port), but reset
    // NoWait so the DemoLoop title/credits sequence runs normally.
    if (!virtualreality)
        NoWait = true;
    // FinishSignon();
    NoWait = false;


    displayofs = PAGE1START;
    bufferofs = PAGE2START;
}

//===========================================================================

/*
==========================
=
= SetViewSize
=
==========================
*/

boolean SetViewSize(unsigned width, unsigned height)
{
    viewwidth = width & ~15;             // must be divisible by 16
    viewheight = height & ~1;            // must be even
    centerx = viewwidth / 2 - 1;
    shootdelta = viewwidth / 10;
    screenofs = ((200 - STATUSLINES - viewheight) / 2 * SCREENWIDTH + (320 - viewwidth) / 2);

    //
    // calculate trace angles and projection constants
    //
    CalcProjection(FOCALLENGTH);

    //
    // build all needed compiled scalers
    //
    SetupScaling(viewwidth * 1.5);

    return true;
}


void ShowViewSize(int width)
{
    int oldwidth, oldheight;

    oldwidth = viewwidth;
    oldheight = viewheight;

    viewwidth = width * 16;
    viewheight = width * 16 * HEIGHTRATIO;
    DrawPlayBorder();

    viewheight = oldheight;
    viewwidth = oldwidth;
}


void NewViewSize(int width)
{
    CA_UpLevel();
    MM_SortMem();
    viewsize = width;
    SetViewSize(width * 16, width * 16 * HEIGHTRATIO);
    CA_DownLevel();
}



//===========================================================================

/*
==========================
=
= Quit
=
==========================
*/

void Quit(char *error)
{
    ClearMemory();
    if (!error || !*error)
    {
        WriteConfig();
    }

    ShutdownId();

    if (error && *error)
    {
        fprintf(stderr, "Error: %s\n", error);
        exit(1);
    }

    exit(0);
}

//===========================================================================



/*
=====================
=
= DemoLoop
=
=====================
*/

static char *ParmStrings[] = {"baby", "easy", "normal", "hard", ""};

void DemoLoop(void)
{
    int i, level;
    int verifyDemo;

    verifyDemo = WL_GetVerifyDemoIndex();
    fprintf(stderr, "[demoloop] WL_GetVerifyDemoIndex=%d\n", verifyDemo);
    fflush(stderr);
    if (verifyDemo >= 0)
    {
        fprintf(stderr, "[demoloop] dispatch PlayDemo(%d)\n", verifyDemo);
        fflush(stderr);
        StartCPMusic(INTROSONG);
        PlayDemo(verifyDemo);
        fprintf(stderr, "[demoloop] PlayDemo(%d) returned\n", verifyDemo);
        fflush(stderr);
        return;
    }


    //
    // check for launch from ted
    //
    if (tedlevel)
    {
        NoWait = true;
        NewGame(1, 0);

        for (i = 1; i < g_argc; i++)
        {
            if ((level = US_CheckParm(g_argv[i], ParmStrings)) != -1)
            {
                gamestate.difficulty = level;
                break;
            }
        }

        gamestate.episode = tedlevelnum / 10;
        gamestate.mapon = tedlevelnum % 10;
        GameLoop();
        Quit(NULL);
    }

//
// main game cycle
//
    StartCPMusic(INTROSONG);

    while (1)
    {
        while (!NoWait)
        {
//
// title page
//
            MM_SortMem();
            CA_CacheScreen(TITLEPIC);
            VW_UpdateScreen();
            VW_FadeIn();
            if (IN_UserInput(TickBase * 15))
                break;
            VW_FadeOut();
//
// credits page
//
            CA_CacheScreen(CREDITSPIC);
            VW_UpdateScreen();
            VW_FadeIn();
            if (IN_UserInput(TickBase * 10))
                break;
            VW_FadeOut();
//
// high scores
//
            DrawHighScores();
            VW_UpdateScreen();
            VW_FadeIn();
            if (IN_UserInput(TickBase * 10))
                break;
//
// demo
//
            PlayDemo(LastDemo++ % 4);
            if (LastDemo >= 4)
                LastDemo = 0;
            if (playstate == ex_abort)
                break;
        }

        VW_FadeOut();

        US_ControlPanel(0);

        if (startgame || loadedgame)
        {
            GameLoop();
            VW_FadeOut();
            StartCPMusic(INTROSONG);
        }
    }
}


//===========================================================================


/*
==========================
=
= main
=
==========================
*/

int main(int argc, char *argv[])
{
    // Store command line arguments globally for US_CheckParm / MS_CheckParm
    g_argc = argc;
    g_argv = argv;

    printf("Wolfenstein 3D SDL3 - starting...\n");

    // Initialize SDL early
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    printf("SDL initialized. Creating window...\n");

    Patch386();

    InitGame();

    printf("Game initialized. Running demo loop.\n");

    DemoLoop();

    // Should never reach here
    Quit("Demo loop exited???");
    return 0;
}
