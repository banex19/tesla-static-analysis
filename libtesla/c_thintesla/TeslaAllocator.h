#pragma once

#include "ThinTesla.h"
#include "TeslaMalloc.h"
#include "TeslaVector.h"

/* A block allocator */
typedef struct TeslaAllocator
{
    size_t elemSize;
    size_t numElementsPerBlock;
    size_t numShards;

    uint8_t* nextFreeShard;
    uint8_t* lastUsedBlock;

    TeslaVector blocks;
} TeslaAllocator;

EXTERN_C

size_t TeslaAllocator_GetHeaderSize();
uint64_t* TeslaAllocator_GetShardHeader(uint8_t* shard);

size_t TeslaAllocator_GetBlockSize(TeslaAllocator* allocator);
size_t TeslaAllocator_GetShardSize(TeslaAllocator* allocator);
bool TeslaAllocator_IsElemInBlock(TeslaAllocator* allocator, void* elem, uint8_t* block);
size_t TeslaAllocator_GetElemIndexInShard(TeslaAllocator* allocator, void* elem, uint8_t* shard);

uint8_t* TeslaAllocator_GetBlockForElem(TeslaAllocator* allocator, void* elem);
uint64_t* TeslaAllocator_GetHeader(TeslaAllocator* allocator, void* elem);

bool TeslaAllocator_Create(size_t elemSize, size_t numElementsPerBlock, TeslaAllocator* out);
void TeslaAllocator_Destroy(TeslaAllocator* allocator);

void* TeslaAllocator_Allocate(TeslaAllocator* allocator);

void TeslaAllocator_Free(TeslaAllocator* allocator, void* elem);

bool TeslaAllocator_AllocateVector(TeslaAllocator* allocator);
bool TeslaAllocator_AllocateBlock(TeslaAllocator* allocator);
void TeslaAllocator_LinkBlock(TeslaAllocator* allocator, uint8_t* block);

EXTERN_C_END