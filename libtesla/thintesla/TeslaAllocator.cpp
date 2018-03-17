#include "TeslaAllocator.h"
#include <cstring>
#include <iostream>

TeslaAllocator::TeslaAllocator(size_t elemSize, size_t numElementsPerBlock)
	: elemSize(elemSize), numElementsPerBlock(numElementsPerBlock)
{
	DEBUG_ASSERT(elemSize > 0 && numElementsPerBlock > 0);

	// Round up to SHARD_NUM_ELEMS.
	if (numElementsPerBlock % SHARD_NUM_ELEMS != 0)
		numElementsPerBlock += SHARD_NUM_ELEMS - (numElementsPerBlock % SHARD_NUM_ELEMS);

	// Check for overflow.
	DEBUG_ASSERT(!(elemSize != 0 && ((elemSize * numElementsPerBlock) / elemSize != numElementsPerBlock)));

	numShards = numElementsPerBlock / SHARD_NUM_ELEMS;

	AllocateBlock();
}

TeslaAllocator::~TeslaAllocator()
{
	for (auto& block : blocks)
	{
		delete[] block;
	}
}

void TeslaAllocator::AllocateBlock()
{
	uint8_t* block = new uint8_t[GetBlockSize()];

	LinkBlock(block);

	nextFreeShard = block;
	lastUsedBlock = block;
}

void TeslaAllocator::LinkBlock(uint8_t* block)
{
	for (size_t i = 0; i < numShards - 1; ++i)
	{
		uintptr_t* linkPtr = (uintptr_t*)(block + GetShardSize() * i);
		*linkPtr = (uintptr_t)((uint8_t*)linkPtr + GetShardSize());
	}

	memset(block + GetShardSize() * (numShards - 1), 0, sizeof(uintptr_t));
}

void* TeslaAllocator::Allocate()
{
	uint64_t* header = GetShardHeader(nextFreeShard);
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
		nextFreeShard = (uint8_t*)ptr;

		if (nextFreeShard == nullptr)
			AllocateBlock();
	}

	//  std::cout << "(Alloc) Next shard: " << (uintptr_t)nextFreeShard << "\n";

	return (uint8_t*)header + GetHeaderSize() + elemSize * freeBlock;
}

void TeslaAllocator::Free(void* elem)
{
	uint64_t* header = GetHeader(elem);
	uint64_t bitmap = (*header & BITMAP_MASK) >> 48;
	DEBUG_ASSERT(bitmap != 0);

	uint64_t index = GetElemIndexInShard(elem, (uint8_t*)header);

	bitmap = ClearBit(bitmap, index);

	// Link to what was the most recent free shard.
	uint64_t ptr = (uint64_t)nextFreeShard;

	nextFreeShard = (uint8_t*)header;

	// std::cout << "(Free) Next shard: " << (uintptr_t)nextFreeShard << "\n";

	*header = ((bitmap << 48) | ptr);
}

uint8_t* TeslaAllocator::GetBlockForElem(void* elem)
{
	if (IsElemInBlock(elem, lastUsedBlock))
		return lastUsedBlock;

	for (uint8_t* block : blocks)
	{
		if (IsElemInBlock(elem, block))
		{
			lastUsedBlock = block;
			return block;
		}
	}

	return nullptr;
}

uint64_t* TeslaAllocator::GetHeader(void* elem)
{
	uint8_t* block = GetBlockForElem(elem);
	DEBUG_ASSERT(block != nullptr);

	size_t offset = (uint8_t*)elem - block;
	size_t shardNum = offset / GetShardSize();

	return (uint64_t*)(block + GetShardSize() * shardNum);
}