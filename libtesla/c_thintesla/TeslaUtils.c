#include "TeslaTypes.h"
#include "TeslaUtils.h"

/* The caller must check for zero first */
uint64_t FirstZero(uint64_t x)
{
    return RightmostOne(~(x));
}

uint64_t LeftmostOne(uint64_t x)
{
    return 63 - __builtin_clzll(x);
}

uint64_t RightmostOne(uint64_t x)
{
    return __builtin_ctzll(x);
}

bool IsPowerOfTwo(size_t x)
{
    return (x & (x - 1)) == 0;
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

uint64_t MaskBetweenExclUpper(uint64_t x, uint64_t y)
{
    return x - y;
}

_Static_assert(sizeof(uint64_t) >= sizeof(TeslaTemporalTag), "Temporal tag type too large");
