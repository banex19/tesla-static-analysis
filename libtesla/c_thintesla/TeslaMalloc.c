#include "TeslaMalloc.h"

void* TeslaMalloc(size_t size)
{
#ifndef _KERNEL
    return malloc(size);
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