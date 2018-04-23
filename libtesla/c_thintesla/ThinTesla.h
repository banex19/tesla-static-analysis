#pragma once


#include <stddef.h>
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifndef __cplusplus
#pragma pack(1)
typedef size_t uint64_t;
typedef enum   { false, true }  bool ;
_Static_assert(sizeof(bool) == 4, "Bool enum not int");
#pragma options align = reset
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


