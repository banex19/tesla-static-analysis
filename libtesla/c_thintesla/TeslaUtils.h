#pragma once
#include "ThinTesla.h"

EXTERN_C

uint64_t FirstZero(uint64_t x);
uint64_t LeftmostOne(uint64_t x);
uint64_t RightmostOne(uint64_t x);
bool IsPowerOfTwo(uint64_t x);
uint64_t SetBit(uint64_t x, uint64_t bit);
uint64_t ClearBit(uint64_t x, uint64_t bit);
uint64_t MaskBetweenExclUpper(uint64_t x, uint64_t y);

EXTERN_C_END