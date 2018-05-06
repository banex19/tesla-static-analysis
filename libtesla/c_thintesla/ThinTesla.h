#pragma once

#ifndef _KERNEL
#include <stddef.h>
#else
#include <sys/stddef.h>
#endif

#ifndef _KERNEL
#include <assert.h>
#include <inttypes.h>
#endif

#ifndef _KERNEL
#include <stdint.h>
#else
#include <sys/stdint.h>
#endif

#ifndef _KERNEL
#include <stdio.h>
#else
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>

#define assert(...) KASSERT(__VA_ARGS__, "ThinTESLA assert")
#endif

#ifndef __cplusplus
#ifndef _KERNEL
#pragma pack(1)
typedef size_t uint64_t;
typedef enum
{
    false,
    true
} bool;
_Static_assert(sizeof(bool) == 4, "Bool enum not int");
#pragma options align = reset
#endif
#endif

#ifdef __cplusplus
#define EXTERN_C \
    extern "C"   \
    {
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
