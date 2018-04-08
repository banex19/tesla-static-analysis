#pragma once
#include "ThinTesla.h"

EXTERN_C

uint64_t FirstZero(uint64_t x);
uint64_t FirstOne(uint64_t x);
uint64_t LastOne(uint64_t x);
uint64_t SetBit(uint64_t x, uint64_t bit);
uint64_t ClearBit(uint64_t x, uint64_t bit);

EXTERN_C_END