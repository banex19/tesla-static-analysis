#pragma once

#include "TeslaHash.h"
#include "ThinTesla.h"

struct BucketHeader
{
    uint64_t tag : 63;
    uint64_t full : 1;
} __attribute__ ((packed));

typedef struct BucketHeader BucketHeader;

typedef struct TeslaHashTable
{
    size_t dataSize;
    size_t bucketSize;

    size_t capacity;
    size_t size;
    uint8_t* table;
} TeslaHT;

bool TeslaHT_Create(size_t initialCapacity, size_t dataSize, TeslaHT* hashtable);
void TeslaHT_Destroy(TeslaHT* hashtable);
void TeslaHT_Clear(TeslaHT* hashtable);

bool TeslaHT_Insert(TeslaHT* hashtable, uint64_t tag, void* data);
bool TeslaHT_InsertInternal(TeslaHT* hashtable, uint64_t tag, void* data, bool allowResizing);
uint64_t TeslaHT_LookupTag(TeslaHT* hashtable, void* data);
BucketHeader* TeslaHT_LookupTagPtr(TeslaHT* hashtable, void* data);

size_t TeslaHT_GetHeaderSize(void);
size_t TeslaHT_GetTableSize(TeslaHT* hashtable);
uint8_t* TeslaHT_GetBucket(TeslaHT* hashtable, size_t index);

bool TeslaHT_ResizeTable(TeslaHT* hashtable, size_t newCapacity);
void TeslaHT_HashToNewTable(TeslaHT* hashtable, size_t oldCapacity, uint8_t* oldTable);
