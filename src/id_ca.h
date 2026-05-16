// id_ca.h - Cache Manager header (SDL3 port)
#ifndef ID_CA_H
#define ID_CA_H

#include "wl_types.h"
#include "gfxv_wl6.h"
#include "audiowl6.h"
#include "mapswl6.h"

#pragma pack(push, 1)
typedef struct {
    longword planestart[3];
    word     planelength[3];
    word     width, height;
    char     name[16];
} maptype;
#pragma pack(pop)

extern char      audioname[13];
extern byte      *tinf;
extern int       mapon;
extern word      *mapsegs[MAPPLANES];
extern maptype   *mapheaderseg[NUMMAPS];
extern byte      *audiosegs[NUMSNDCHUNKS];
extern void      *grsegs[NUMCHUNKS];
extern size_t     grsegs_size[NUMCHUNKS];
extern byte      *grneeded;
extern byte      ca_levelbit, ca_levelnum;
extern char      *titleptr[8];
extern longword  *grstarts;
extern longword  *audiostarts;
extern void      (*drawcachebox)(void);
extern void      (*updatecachebox)(word, word);
extern void      (*finishcachebox)(void);

void CA_Startup(void);
void CA_Shutdown(void);
void CA_FarRead(byte *dest, longword length);
void CA_FarWrite(byte *source, longword length);
boolean CA_ReadFile(char *filename, memptr *ptr, longword *length);
boolean CA_LoadFile(char *filename, memptr *ptr, longword *length);
boolean CA_WriteFile(char *filename, byte *ptr, longword length);
void CA_RLEWCompress(word *source, longword length, word *dest, word rlewtag);
void CA_RLEWexpand(word *source, word *dest, longword length, word rlewtag);
void CA_SetGrPurge(void);
void CA_CacheGrChunk(int chunk);
void CA_CacheScreen(int chunk);
void CA_CacheAudioChunk(int chunk);
void CA_LoadAllSounds(void);
void CA_UpLevel(void);
void CA_DownLevel(void);
void CA_SetAllPurge(void);
void CA_ClearMarks(void);
void CA_ClearAllMarks(void);
void CA_CacheMarks(void);
void CA_CacheMap(int mapnum);
void CA_OpenDebug(void);
void CA_CloseDebug(void);

#define UNCACHEGRCHUNK(chunk) { MM_FreePtr(&grsegs[chunk]); grneeded[chunk] &= ~ca_levelbit; }
#define CA_MarkGrChunk(chunk) { grneeded[chunk] |= ca_levelbit; }

#endif
