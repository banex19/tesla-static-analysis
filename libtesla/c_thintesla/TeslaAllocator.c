#include "TeslaAllocator.h"
#include <string.h>

static const size_t SHARD_NUM_ELEMS = 16;
static const uint64_t BITMAP_MASK = 0xFFFF000000000000;
static const uint64_t PTR_MASK = 0x0000FFFFFFFFFFFF;

size_t TeslaAllocator_GetHeaderSize() { return sizeof(uint64_t); }

size_t TeslaAllocator_GetBlockSize(TeslaAllocator* allocator)
{
    return allocator->numShards * TeslaAllocator_GetHeaderSize() + allocator->elemSize * allocator->numElementsPerBlock;
}

size_t TeslaAllocator_GetShardSize(TeslaAllocator* allocator)
{
    return TeslaAllocator_GetHeaderSize() + allocator->elemSize * SHARD_NUM_ELEMS;
}

bool TeslaAllocator_IsElemInBlock(TeslaAllocator* allocator, void* elem, uint8_t* block)
{
    return elem > block && ((uint8_t*)elem - block <= TeslaAllocator_GetBlockSize(allocator));
}

size_t TeslaAllocator_GetElemIndexInShard(TeslaAllocator* allocator, void* elem, uint8_t* shard)
{
    return ((uint8_t*)elem - (shard + TeslaAllocator_GetHeaderSize())) / allocator->elemSize;
}

uint64_t* TeslaAllocator_GetShardHeader(uint8_t* shard) { return (uint64_t*)shard; }

bool TeslaAllocator_Create(size_t elemSize, size_t numElementsPerBlock, TeslaAllocator* out)
{
    memset(out, 0, sizeof(TeslaAllocator));

    DEBUG_ASSERT(elemSize > 0 && numElementsPerBlock > 0);

    // Round up to SHARD_NUM_ELEMS.
    if (numElementsPerBlock % SHARD_NUM_ELEMS != 0)
        numElementsPerBlock += SHARD_NUM_ELEMS - (numElementsPerBlock % SHARD_NUM_ELEMS);

    // Check for overflow.
    DEBUG_ASSERT(!(elemSize != 0 && ((elemSize * numElementsPerBlock) / elemSize != numElementsPerBlock)));

    out->elemSize = elemSize;
    out->numElementsPerBlock = numElementsPerBlock;
    out->numShards = numElementsPerBlock / SHARD_NUM_ELEMS;

    if (!TeslaAllocator_AllocateVector(out))
        return false;

    if (!TeslaAllocator_AllocateBlock(out))
    {
        TeslaVector_Destroy(&out->blocks);
        return false;
    }

    return true;
}

bool TeslaAllocator_AllocateVector(TeslaAllocator* allocator)
{
    return TeslaVector_Create(sizeof(void*), &allocator->blocks);
}

void TeslaAllocator_Destroy(TeslaAllocator* allocator)
{
    for (size_t i = 0; i < allocator->blocks.size; ++i)
    {
        void* block = NULL;
        TeslaVector_Get(&allocator->blocks, i, &block);
        TeslaFree(block);      
    }

    TeslaVector_Destroy(&allocator->blocks);
}

bool TeslaAllocator_AllocateBlock(TeslaAllocator* allocator)
{
    uint8_t* block = TeslaMalloc(sizeof(uint8_t) * TeslaAllocator_GetBlockSize(allocator));
    if (block != NULL)
    {
        TeslaAllocator_LinkBlock(allocator, block);
    }
    else
    {
        return false;
    }

    if (!TeslaVector_Add(&allocator->blocks, &block))
    {
        TeslaFree(block);
        return false;
    }

    allocator->nextFreeShard = block;
    allocator->lastUsedBlock = block;

    return true;
}

void TeslaAllocator_LinkBlock(TeslaAllocator* allocator, uint8_t* block)
{
    for (size_t i = 0; i < allocator->numShards - 1; ++i)
    {
        uintptr_t* linkPtr = (uintptr_t*)(block + TeslaAllocator_GetShardSize(allocator) * i);
        *linkPtr = (uintptr_t)((uint8_t*)linkPtr + TeslaAllocator_GetShardSize(allocator));
    }

    memset(block + TeslaAllocator_GetShardSize(allocator) * (allocator->numShards - 1), 0, sizeof(uintptr_t));
}

void* TeslaAllocator_Allocate(TeslaAllocator* allocator)
{
    uint64_t* header = TeslaAllocator_GetShardHeader(allocator->nextFreeShard);
    uint64_t bitmap = (*header & BITMAP_MASK) >> 48;
    uint64_t ptr = (*header & PTR_MASK);

    uint64_t freeBlock = bitmap == 0 ? 0 : FirstZero(bitmap) - 1;

    // std::cout << "Free block index: " <<  freeBlock << "\n";

    // std::cout << "Bitmap before: " <<  bitmap << "\n";
    bitmap = SetBit(bitmap, freeBlock);
    // std::cout << "Bitmap after: " <<  bitmap << "\n";

    *header = ((bitmap << 48) | ptr);

    if (bitmap == 0xFFFF) // Shard fully allocated, use next one.
    {
        allocator->nextFreeShard = (uint8_t*)ptr;

        if (allocator->nextFreeShard == NULL)
            TeslaAllocator_AllocateBlock(allocator);
    }

    //  std::cout << "(Alloc) Next shard: " << (uintptr_t)nextFreeShard << "\n";

    return (uint8_t*)header + TeslaAllocator_GetHeaderSize() + allocator->elemSize * freeBlock;
}

void TeslaAllocator_Free(TeslaAllocator* allocator, void* elem)
{
    uint64_t* header = TeslaAllocator_GetHeader(allocator, elem);
    uint64_t bitmap = (*header & BITMAP_MASK) >> 48;
    DEBUG_ASSERT(bitmap != 0);

    uint64_t index = TeslaAllocator_GetElemIndexInShard(allocator, elem, (uint8_t*)header);

    bitmap = ClearBit(bitmap, index);

    // Link to what was the most recent free shard.
    uint64_t ptr = (uint64_t)allocator->nextFreeShard;

    allocator->nextFreeShard = (uint8_t*)header;

    // std::cout << "(Free) Next shard: " << (uintptr_t)nextFreeShard << "\n";

    *header = ((bitmap << 48) | ptr);
}

uint8_t* TeslaAllocator_GetBlockForElem(TeslaAllocator* allocator, void* elem)
{
    if (TeslaAllocator_IsElemInBlock(allocator, elem, allocator->lastUsedBlock))
        return allocator->lastUsedBlock;

    for (size_t i = 0; i < allocator->blocks.size; ++i)
    {
        void* block = NULL;
        TeslaVector_Get(&allocator->blocks, i, &block);

        if (TeslaAllocator_IsElemInBlock(allocator, elem, block))
        {
            allocator->lastUsedBlock = block;
            return block;
        }
    }

    return NULL;
}

uint64_t* TeslaAllocator_GetHeader(TeslaAllocator* allocator, void* elem)
{
    uint8_t* block = TeslaAllocator_GetBlockForElem(allocator, elem);
    DEBUG_ASSERT(block != NULL);

    size_t offset = (uint8_t*)elem - block;
    size_t shardNum = offset / TeslaAllocator_GetShardSize(allocator);

    return (uint64_t*)(block + TeslaAllocator_GetShardSize(allocator) * shardNum);
}