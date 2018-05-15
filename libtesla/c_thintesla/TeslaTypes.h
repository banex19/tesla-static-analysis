#pragma once

_Static_assert(sizeof(unsigned long) == 8, "Invalid size_t");

typedef unsigned long TeslaTemporalTag;

typedef unsigned long TeslaThreadKey;

typedef struct KernelThreadAutomata
{
    struct TeslaAutomaton* automata;
    unsigned long numAutomata;
} KernelThreadAutomata;

#define INVALID_THREAD_KEY ((TeslaThreadKey)-1)
