// id_mm.h - Memory Manager (malloc/free port)
#ifndef ID_MM_H
#define ID_MM_H

#include "wl_types.h"

#define MAXBLOCKS       700
#define SAVENEARHEAP    0x400
#define SAVEFARHEAP     0
#define BUFFERSIZE      0x1000

typedef struct {
    long nearheap, farheap, EMSmem, XMSmem, mainmem;
} mminfotype;

extern mminfotype mminfo;
extern memptr     bufferseg;
extern boolean    mmerror;
extern void       (*beforesort)(void);
extern void       (*aftersort)(void);

void MM_Startup(void);
void MM_Shutdown(void);
void MM_MapEMS(void);
void MM_GetPtr(memptr *ptr, unsigned long size);
void MM_FreePtr(memptr *ptr);
void MM_SetPurge(memptr *ptr, int purge);
void MM_SetLock(memptr *ptr, boolean lock);
void MM_SortMem(void);
void MM_ShowMemory(void);
long MM_UnusedMemory(void);
long MM_TotalFree(void);
void MM_BombOnError(boolean bomb);

#endif
