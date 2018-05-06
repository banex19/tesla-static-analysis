#pragma once

#include "MurmurHash3.h"
#include "ThinTesla.h"

uint64_t Hash64(void* data, size_t len);

uint64_t FakeHash64(void* data, size_t len);

uint64_t BadHash64(void* data, size_t len);