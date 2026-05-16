// id_pm.h - Page Manager header (SDL3 port)
#ifndef ID_PM_H
#define ID_PM_H

#include "wl_types.h"

#define PMPageSize          4096
#define PMPageSizeKB        4
#define PMMinMainMem        10
#define PMMaxMainMem        100
#define PMThrashThreshold   1
#define PMUnThrashThreshold 5

typedef enum { pml_Unlocked, pml_Locked } PMLockType;
typedef enum { pmba_Unused = 0, pmba_Used = 1, pmba_Allocated = 2 } PMBlockAttr;

typedef struct {
    longword offset;
    word     length;
    int      xmsPage;
    PMLockType locked;
    int      emsPage;
    int      mainPage;
    longword lastHit;
} PageListStruct;

extern boolean       XMSPresent, EMSPresent;
extern word          XMSPagesAvail, EMSPagesAvail;
extern word          ChunksInFile, PMSpriteStart, PMSoundStart;
extern PageListStruct *PMPages;
extern char          PageFileName[13];

void PM_Startup(void);
void PM_Shutdown(void);
void PM_Reset(void);
memptr PM_GetPage(int page);
memptr PM_GetPageAddress(int page);
void PM_SetPageLock(int page, PMLockType lock);
void PM_SetMainPurge(int purge);
void PM_SetMainMemPurge(int purge);
void PM_CheckMainMem(void);
void PM_NextFrame(void);
void PM_Preload(boolean (*update)(word, word));

#define PM_GetSoundPage(v)   PM_GetPage(PMSoundStart + (v))
#define PM_GetSpritePage(v)  PM_GetPage(PMSpriteStart + (v))
#define PM_LockMainMem()     PM_SetMainMemPurge(0)
#define PM_UnlockMainMem()   PM_SetMainMemPurge(3)

#endif
