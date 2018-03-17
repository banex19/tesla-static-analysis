#pragma once

#include <stdint.h>
#include <cstddef>
#include <cassert>

#include "TeslaUtils.h"

#ifdef _DEBUG
#define DEBUG_ASSERT(x) \
    do                  \
    {                   \
        assert(x);      \
    } while (false)
#else
#define DEBUG_ASSERT(x)
#endif

#if true
#define SAFE_ASSERT(x) \
    do                 \
    {                  \
        assert(x);     \
    } while (false)
#endif

