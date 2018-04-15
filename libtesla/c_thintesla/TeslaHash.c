#include "TeslaHash.h"

uint64_t Hash64(void* data, size_t len)
{
    uint64_t out[2] = {0};

    MurmurHash3_x64_128(data, len, 19, out);

    return out[0];
}