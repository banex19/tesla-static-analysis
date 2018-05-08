#pragma once

#include "ThinTesla.h"

void* TeslaMalloc(size_t size);
void* TeslaMallocZero(size_t size);
void TeslaFree(void* ptr);

#define TESLA_STATIC_STORAGE_SIZE (5*1024*1024) 
#ifdef _KERNEL
#define TESLA_USE_STATIC_STORAGE
#endif

void* TeslaMallocStatic(size_t size);


void reportMemoryStats(void);