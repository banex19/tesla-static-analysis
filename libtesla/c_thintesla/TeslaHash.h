#pragma once

#include "MurmurHash3.h"
#include "ThinTesla.h"

typedef uint64_t HashSize;

HashSize Hash64(void* data, size_t len);

HashSize FakeHash64(void* data, size_t len);

HashSize BadHash64(void* data, size_t len);

