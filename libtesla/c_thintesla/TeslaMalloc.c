#include "TeslaMalloc.h"

#define TRACK_TOTAL_ALLOC

#ifdef TRACK_TOTAL_ALLOC
size_t totalAllocated = 0;
bool cleanupRegistered = false;

void reportMemoryStats()
{
    printf("Total memory allocated (bytes): %lu\n", totalAllocated);
}
#endif

void* TeslaMalloc(size_t size)
{
#ifdef TESLA_USE_STATIC_STORAGE
    void* data = TeslaMallocStatic(size);
#else
    void* data = NULL;
#endif

#ifndef _KERNEL
#ifdef TRACK_TOTAL_ALLOC
    if (!cleanupRegistered)
    {
        atexit(reportMemoryStats);
        cleanupRegistered = true;
    }
    totalAllocated += size;
#endif
    if (data == NULL)
        data = malloc(size);
#endif

    return data;
}

void* TeslaMallocZero(size_t size)
{
#ifdef TESLA_USE_STATIC_STORAGE
    void* data = TeslaMallocStatic(size);
#else
    void* data = NULL;
#endif

#ifndef _KERNEL
    if (data == NULL)
    {
        data = TeslaMalloc(size);
        memset(data, 0, size);
    }
#endif

    return data;
}

void TeslaFree(void* ptr)
{
#ifndef _KERNEL
    free(ptr);
#else
    return;
#endif
}