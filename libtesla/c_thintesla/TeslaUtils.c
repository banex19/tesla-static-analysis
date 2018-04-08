
#include "TeslaUtils.h"



/* The caller must check for zero first */
uint64_t FirstZero(uint64_t x)
{
    return FirstOne(~(x));
}

uint64_t FirstOne(uint64_t x)
{
    return ffs(x);
}

uint64_t LastOne(uint64_t x)
{
   return 31 - __builtin_clz(x);
}

uint64_t SetBit(uint64_t x, uint64_t bit)
{
    x |= (uint64_t)1 << bit;
    return x;
}

uint64_t ClearBit(uint64_t x, uint64_t bit)
{
    x &= ~((uint64_t)1 << bit);
    return x;
}

