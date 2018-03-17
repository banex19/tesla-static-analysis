#pragma once

#include "TeslaHash.h"
#include "ThinTesla.h"

class TeslaHashTable
{
    struct BucketHeader
    {
        uint64_t full : 1;
        uint64_t timestamp : 63;
    } __attribute__((packed));

  public:
    TeslaHashTable(size_t initialCapacity, size_t dataSize)
        : dataSize(dataSize), bucketSize(sizeof(BucketHeader) + dataSize)
    {
        ResizeTable(initialCapacity);
    }

    void Insert(uint64_t timestamp, void* data, bool allowResizing = true);

    uint64_t LookupTimestamp(void* data);

    ~TeslaHashTable();

    size_t GetSize() { return size; }

  private:
    size_t GetHeaderSize() { return sizeof(BucketHeader); };
    size_t GetTableSize() { return bucketSize * capacity; }

    uint8_t* GetBucket(size_t index) { return table + bucketSize * index; }

    void ResizeTable(size_t newSize);
    void HashToNewTable(size_t oldCapacity, uint8_t* oldTable);

    const size_t dataSize = 0;
    const size_t bucketSize = 0;

    size_t capacity = 0;
    size_t size = 0;
    uint8_t* table = nullptr;
};