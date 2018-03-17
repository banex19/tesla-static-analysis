#pragma once

#include "ThinTesla.h"
#include <vector>

/* A block allocator */
class TeslaAllocator
{
    static const size_t SHARD_NUM_ELEMS = 16;
    static const uint64_t BITMAP_MASK = 0xFFFF000000000000;
    static const uint64_t PTR_MASK = 0x0000FFFFFFFFFFFF;

  public:
    TeslaAllocator(size_t elemSize, size_t numElementsPerBlock);
    ~TeslaAllocator();

    void *Allocate();

    void Free(void *elem);

  private:
    void AllocateBlock();
    void LinkBlock(uint8_t *block);

    size_t GetBlockSize() { return numShards * GetHeaderSize() + elemSize * numElementsPerBlock; }
    size_t GetShardSize() { return GetHeaderSize() + elemSize * SHARD_NUM_ELEMS; }
    size_t GetHeaderSize() { return sizeof(uint64_t); }
    bool IsElemInBlock(void *elem, uint8_t *block) { return elem > block && ((uint8_t *)elem - block <= GetBlockSize()); }
    size_t GetElemIndexInShard(void *elem, uint8_t *shard) { return ((uint8_t *)elem - (shard + GetHeaderSize())) / elemSize; }
    uint64_t *GetShardHeader(uint8_t *shard) { return (uint64_t *)shard; }

    uint8_t *GetBlockForElem(void *elem);
    uint64_t *GetHeader(void *elem);

    size_t elemSize;
    size_t numElementsPerBlock;
    size_t numShards;

    std::vector<uint8_t *> blocks;

    uint8_t *nextFreeShard = nullptr;
    uint8_t *lastUsedBlock = nullptr;
};