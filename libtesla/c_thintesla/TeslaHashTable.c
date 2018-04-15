#include "TeslaHashTable.h"
#include "TeslaMalloc.h"
#include "ThinTesla.h"

bool TeslaHT_Create(size_t initialCapacity, size_t dataSize, TeslaHT* hashtable)
{
    memset(hashtable, 0, sizeof(TeslaHT));

    hashtable->dataSize = dataSize;
    hashtable->bucketSize = TeslaHT_GetHeaderSize() + dataSize;

    return TeslaHT_ResizeTable(hashtable, initialCapacity);
}

void TeslaHT_Destroy(TeslaHT* hashtable)
{
    TeslaFree(hashtable->table);
}

bool TeslaHT_ResizeTable(TeslaHT* hashtable, size_t newCapacity)
{
    DEBUG_ASSERT(newCapacity > hashtable->capacity);

    uint8_t* newTable = TeslaMalloc(hashtable->bucketSize * newCapacity);

    if (newTable == NULL)
        return false;

    uint8_t* oldTable = hashtable->table;
    size_t oldCapacity = hashtable->capacity;

    hashtable->table = newTable;
    hashtable->capacity = newCapacity;

    memset(newTable, 0, TeslaHT_GetTableSize(hashtable));

    if (hashtable->size > 0)
        TeslaHT_HashToNewTable(hashtable, oldCapacity, oldTable);

    TeslaFree(oldTable);

    return true;
}

void TeslaHT_HashToNewTable(TeslaHT* hashtable, size_t oldCapacity, uint8_t* oldTable)
{
    size_t oldSize = hashtable->size;
    hashtable->size = 0;

    for (size_t i = 0; i < oldCapacity && hashtable->size < oldSize; ++i)
    {
        uint8_t* bucket = oldTable + hashtable->bucketSize * i;
        BucketHeader* header = (BucketHeader*)bucket;

        if (header->full)
        {
            assert(TeslaHT_InsertInternal(hashtable, header->tag, bucket + TeslaHT_GetHeaderSize(), false));
        }
    }
}

bool TeslaHT_Insert(TeslaHT* hashtable, uint64_t tag, void* data)
{
    return TeslaHT_InsertInternal(hashtable, tag, data, true);
}

bool TeslaHT_InsertInternal(TeslaHT* hashtable, uint64_t tag, void* data, bool allowResizing)
{
    if (hashtable->size == hashtable->capacity)
    {
        if (!TeslaHT_ResizeTable(hashtable, hashtable->capacity * 2))
            return false;
    }

    auto hash = Hash64(data, hashtable->dataSize);

    size_t bucketIndex = hash % hashtable->capacity;

    uint8_t* bucket = TeslaHT_GetBucket(hashtable, bucketIndex);

    BucketHeader* header = (BucketHeader*)bucket;

    while (header->full)
    {
        bucketIndex = (bucketIndex + 1) % hashtable->capacity;
        bucket = TeslaHT_GetBucket(hashtable, bucketIndex);
        header = (BucketHeader*)bucket;
    }

    header->full = 1;
    header->tag = tag;
    memcpy(bucket + TeslaHT_GetHeaderSize(), data, hashtable->dataSize);
    hashtable->size++;

    if (allowResizing && (hashtable->size > 0.75 * hashtable->capacity))
    {
        return TeslaHT_ResizeTable(hashtable, hashtable->capacity * 2);
    }

    return true;
}

uint64_t TeslaHT_LookupTag(TeslaHT* hashtable, void* data)
{
    uint64_t outTag = 0;

    auto hash = Hash64(data, hashtable->dataSize);

    size_t bucketIndex = hash % hashtable->capacity;

    uint8_t* bucket = TeslaHT_GetBucket(hashtable, bucketIndex);

    BucketHeader* header = (BucketHeader*)bucket;

    while (header->full)
    {
        if (memcmp(data, bucket + TeslaHT_GetHeaderSize(), hashtable->dataSize) == 0)
        {
            outTag = header->tag;
            break;
        }
        else
        {
            bucketIndex = (bucketIndex + 1) % hashtable->capacity;
            bucket = TeslaHT_GetBucket(hashtable, bucketIndex);
            header = (BucketHeader*)bucket;
        }
    }

    return outTag;
}

uint8_t* TeslaHT_GetBucket(TeslaHT* hashtable, size_t index)
{
    return hashtable->table + hashtable->bucketSize * index;
}

size_t TeslaHT_GetHeaderSize()
{
    return sizeof(BucketHeader);
}
size_t TeslaHT_GetTableSize(TeslaHT* hashtable)
{
    return hashtable->bucketSize * hashtable->capacity;
}