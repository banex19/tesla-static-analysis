#include "TeslaVector.h"

bool TeslaVector_Create(size_t elemSize, TeslaVector* out)
{
    memset(out, 0, sizeof(TeslaVector));

    out->elemSize = elemSize;

    if (!TeslaVector_Resize(out, 10))
        return false;

    return true;
}

bool TeslaVector_Resize(TeslaVector* vector, size_t newCapacity)
{
    void* newData = TeslaMalloc(vector->elemSize * newCapacity);

    if (newData != NULL && vector->size > 0)
    {
        memcpy(newData, vector->data, vector->elemSize * vector->size);
        free(vector->data);
    }
    else if (newData == NULL)
    {
        return false;
    }

    vector->data = newData;
    vector->capacity = newCapacity;

    return true;
}

void TeslaVector_Destroy(TeslaVector* vector)
{
    TeslaFree(vector->data);
    vector->data = NULL;
}

bool TeslaVector_Add(TeslaVector* vector, void* elem)
{
    if (vector->size + 1 > vector->capacity)
    {
        if (!TeslaVector_Resize(vector, vector->capacity * 2))
            return false;
    }

    memcpy(vector->data + vector->elemSize * vector->size, elem, vector->elemSize);

    vector->size = vector->size + 1;

    return true;
}

void TeslaVector_Get(TeslaVector* vector, size_t i, void* out)
{
    SAFE_ASSERT(i < vector->size);
    DEBUG_ASSERT(vector->data != NULL);

    memcpy(out, vector->data + vector->elemSize * i, vector->elemSize);
}

void TeslaVector_PopBack(TeslaVector* vector)
{
    SAFE_ASSERT(vector->size > 0);
    vector->size = vector->size - 1;
}