#pragma once

#include "TeslaHashTable.h"
#include "TeslaTypes.h"
#include "TeslaMalloc.h"

typedef enum
{
    TESLA_STORE_INVALID,
    TESLA_STORE_HT,
    TESLA_STORE_SINGLE
} StoreType;

typedef struct TeslaStore
{
    StoreType type;
    size_t dataSize;

    union store {
        struct TeslaHashTable hashtable;
        uint8_t* data;
    } store;
} TeslaStore;

bool TeslaStore_Create(StoreType type, size_t initialCapacity, size_t dataSize, TeslaStore* store);
void TeslaStore_Destroy(TeslaStore* store);
void TeslaStore_Clear(TeslaStore* store);

bool TeslaStore_Insert(TeslaStore* store, TeslaTemporalTag tag, void* data);
TeslaTemporalTag TeslaStore_Get(TeslaStore* store, void* data);
