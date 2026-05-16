// id_mm.c - Memory Manager (malloc/free port)
// Replaces the original DOS EMS/XMS/segment-based memory manager
// with simple malloc/free wrappers.

#include "id_mm.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <SDL3/SDL.h>

#ifdef __linux__
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#elif !defined(_WIN32)
#include <unistd.h>
#endif

extern void Quit(char *error);

mminfotype mminfo;
memptr     bufferseg;
boolean    mmerror;
void       (*beforesort)(void);
void       (*aftersort)(void);

static boolean bombonerror = false;
static const uint32_t MM_MAGIC = 0x4D4D4D4DUL;

typedef struct
{
    uint32_t magic;
    size_t size;
} mm_alloc_header_t;

static size_t get_system_memory_limit_bytes(void)
{
#ifdef __linux__
    struct sysinfo info;
    if (sysinfo(&info) == 0)
    {
        return (size_t)info.totalram * (size_t)info.mem_unit;
    }
#elif defined(_WIN32)
    int mb = SDL_GetSystemRAM();
    if (mb > 0)
    {
        return (size_t)mb * 1024ULL * 1024ULL;
    }
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_size > 0)
    {
        return (size_t)pages * (size_t)page_size;
    }
#endif
    return 0;
}

static long clamp_to_long(size_t value)
{
    if (value > (size_t)LONG_MAX)
        return LONG_MAX;
    return (long)value;
}

static long long_add_safe(long long a, long long b)
{
    long long r = a + b;
    if (r < 0)
        return 0;
    if (r > (long long)LONG_MAX)
        return LONG_MAX;
    return (long)r;
}

void MM_Startup(void)
{
    memset(&mminfo, 0, sizeof(mminfo));
    bufferseg = malloc(BUFFERSIZE);
    if (!bufferseg)
        Quit("MM_Startup: Couldn't allocate buffer segment");
    mmerror = false;
    beforesort = NULL;
    aftersort = NULL;
}

void MM_Shutdown(void)
{
    if (bufferseg)
    {
        mminfo.farheap = 0;
        mminfo.mainmem = 0;
        free(bufferseg);
        bufferseg = NULL;
    }
}

void MM_MapEMS(void)
{
    // No-op: EMS does not exist on modern systems
}

void MM_GetPtr(memptr *ptr, unsigned long size)
{
    mm_alloc_header_t *h;

    if (!ptr)
        Quit("MM_GetPtr: Null pointer");

    h = (mm_alloc_header_t *)malloc(sizeof(mm_alloc_header_t) + (size_t)size);
    if (!h)
    {
        mmerror = true;
        if (bombonerror)
            Quit("MM_GetPtr: Out of memory");
        *ptr = NULL;
        return;
    }

    h->magic = MM_MAGIC;
    h->size = (size_t)size;
    *ptr = (memptr)(h + 1);

    mminfo.farheap = long_add_safe((long long)mminfo.farheap, (long long)h->size);
    mminfo.mainmem = long_add_safe((long long)mminfo.mainmem, (long long)(h->size + sizeof(mm_alloc_header_t)));
}

void MM_FreePtr(memptr *ptr)
{
    if (!ptr || !*ptr)
        return;

    mm_alloc_header_t *h = (mm_alloc_header_t *)((uint8_t *)(*ptr) - sizeof(mm_alloc_header_t));
    if (h->magic != MM_MAGIC)
    {
        free(*ptr);
        *ptr = NULL;
        return;
    }
    mminfo.farheap = long_add_safe((long long)mminfo.farheap, -(long long)h->size);
    mminfo.mainmem = long_add_safe((long long)mminfo.mainmem, -((long long)h->size + (long long)sizeof(mm_alloc_header_t)));
    free(h);
    *ptr = NULL;
}

void MM_SetPurge(memptr *ptr, int purge)
{
    (void)ptr; (void)purge;
}

void MM_SetLock(memptr *ptr, boolean lock)
{
    (void)ptr; (void)lock;
}

void MM_SortMem(void)
{
    if (beforesort)
        beforesort();
    if (aftersort)
        aftersort();
}

void MM_ShowMemory(void)
{
    long total = MM_TotalFree();
    long used = mminfo.farheap;

    if (total > 0)
    {
        printf("MM: used=%ld bytes, approx free=%ld bytes\n", used, total);
    }
    else
    {
        printf("MM: used=%ld bytes, total unknown\n", used);
    }
}

long MM_UnusedMemory(void)
{
    size_t system_bytes = get_system_memory_limit_bytes();
    if (!system_bytes)
        return 0;
    if (system_bytes < mminfo.farheap)
        return 0;
    return clamp_to_long(system_bytes - mminfo.farheap);
}

long MM_TotalFree(void)
{
    return MM_UnusedMemory();
}

void MM_BombOnError(boolean bomb)
{
    bombonerror = bomb;
}
