#pragma once


#include <stddef.h>
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifndef __cplusplus
typedef size_t uint64_t;
typedef enum { false, true } bool;
#endif

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C
#define EXTERN_C_END
#endif

#ifndef RELEASE 
#define DEBUG_ASSERT(x) \
    do                  \
    {                   \
        assert(x);      \
    } while (0)
#else
#define DEBUG_ASSERT(x)
#endif

#define SAFE_ASSERT(x) \
    do                 \
    {                  \
        assert(x);     \
    } while (0)


