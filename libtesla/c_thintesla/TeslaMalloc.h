#pragma once

#include "ThinTesla.h"

void* TeslaMalloc(size_t size);
void* TeslaMallocZero(size_t size);
void TeslaFree(void* ptr);