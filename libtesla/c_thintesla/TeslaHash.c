#include "TeslaHash.h"

uint64_t FakeHash64(void* data, size_t len)
{
    return ((size_t*)data)[0];
}

uint64_t BadHash64(void* data, size_t len)
{
    DEBUG_ASSERT(len % sizeof(size_t) == 0);

    size_t* ptr = data;
    uint64_t hash = ptr[0];
    for (size_t i = 1; i < len / sizeof(size_t); ++i)
    {
        hash ^= ptr[i];
    }

    return hash;
}

uint64_t Hash64(void* data, size_t len)
{
    /* uint64_t out[2] = {0};

    MurmurHash3_x64_128(data, len, 19, out);

    return out[0]; */

    return FakeHash64(data, len);
}