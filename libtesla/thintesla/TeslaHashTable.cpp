#include "TeslaHashTable.h"
#include <cstring>

TeslaHashTable::~TeslaHashTable()
{
    delete[] table;
    table = nullptr;
}

void TeslaHashTable::ResizeTable(size_t newCapacity)
{
    DEBUG_ASSERT(newCapacity > capacity);

    uint8_t* newTable = new uint8_t[bucketSize * newCapacity];

    uint8_t* oldTable = table;
    size_t oldCapacity = capacity;

    table = newTable;
    capacity = newCapacity;

    memset(newTable, 0, GetTableSize());

    if (size > 0)
        HashToNewTable(oldCapacity, oldTable);

    delete[] oldTable;
}

void TeslaHashTable::HashToNewTable(size_t oldCapacity, uint8_t* oldTable)
{
    size_t oldSize = size;
    size = 0;

    for (size_t i = 0; i < oldCapacity && size < oldSize; ++i)
    {
        uint8_t* bucket = oldTable + bucketSize * i;
        BucketHeader* header = (BucketHeader*)bucket;

        if (header->full)
        {
            Insert(header->timestamp, bucket + GetHeaderSize(), false);
        }
    }
}

void TeslaHashTable::Insert(uint64_t timestamp, void* data, bool allowResizing)
{
    if (size == capacity)
        ResizeTable(capacity * 2);

    auto hash = TeslaHash::Hash64(data, dataSize);

    size_t bucketIndex = hash % capacity;

    uint8_t* bucket = GetBucket(bucketIndex);

    BucketHeader* header = (BucketHeader*)bucket;

    while (header->full)
    {
        bucketIndex = (bucketIndex + 1) % capacity;
        bucket = GetBucket(bucketIndex);
        header = (BucketHeader*)bucket;
    }

    header->full = 1;
    header->timestamp = timestamp;
    memcpy(bucket + GetHeaderSize(), data, dataSize);
    size++;

    if (allowResizing && size > 0.75 * capacity)
    {
        ResizeTable(capacity * 2);
    }
}

uint64_t TeslaHashTable::LookupTimestamp(void* data)
{
    uint64_t outTimestamp = 0;

    auto hash = TeslaHash::Hash64(data, dataSize);

    size_t bucketIndex = hash % capacity;

    uint8_t* bucket = GetBucket(bucketIndex);

    BucketHeader* header = (BucketHeader*)bucket;

    while (header->full)
    {
        if (memcmp(data, bucket + GetHeaderSize(), dataSize) == 0)
        {
            outTimestamp = header->timestamp;
            break;
        }
        else
        {
            bucketIndex = (bucketIndex + 1) % capacity;
            bucket = GetBucket(bucketIndex);
            header = (BucketHeader*)bucket;
        }
    }

    return outTimestamp;
}