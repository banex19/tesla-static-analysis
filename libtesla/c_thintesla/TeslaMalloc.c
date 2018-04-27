#include "TeslaMalloc.h"

#define TRACK_TOTAL_ALLOC

#ifdef TRACK_TOTAL_ALLOC
size_t totalAllocated = 0;
bool cleanupRegistered = false;

void reportMemoryStats()
{
    printf("Total memory allocated (bytes): %llu\n", totalAllocated);
}
#endif

void* TeslaMalloc(size_t size)
{
#ifndef _KERNEL
#ifdef TRACK_TOTAL_ALLOC
    if (!cleanupRegistered)
    {
        atexit(reportMemoryStats);
        cleanupRegistered = true;
    }
    totalAllocated += size;
#endif
    return malloc(size);
#else
    return NULL;
#endif
}

void* TeslaMallocZero(size_t size)
{
#ifndef _KERNEL
    void* data = TeslaMalloc(size);
    memset(data, 0, size);
    return data;
#else
    return NULL;
#endif
}

void TeslaFree(void* ptr)
{
#ifndef _KERNEL
    free(ptr);
#else
    return;
#endif
}