#pragma once

#include "ThinTesla.h"
#include "TeslaMalloc.h"

typedef struct TeslaVector
{
    size_t size;
    size_t capacity;
    size_t elemSize;

    uint8_t* data;
} TeslaVector;

bool TeslaVector_Create(size_t elemSize, TeslaVector* out);
void TeslaVector_Destroy(TeslaVector* vector);
void TeslaVector_Clear(TeslaVector* vector);

bool TeslaVector_Add(TeslaVector* vector, void* elem);
uint8_t* TeslaVector_AddAndReturn(TeslaVector* vector, void* elem);
void TeslaVector_Get(TeslaVector* vector, size_t i, void* out);
void TeslaVector_PopBack(TeslaVector* vector);

bool TeslaVector_Resize(TeslaVector* vector, size_t newCapacity);