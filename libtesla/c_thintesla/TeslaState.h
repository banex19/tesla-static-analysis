#pragma once

#include "TeslaStore.h"
#include "TeslaTypes.h"
#include "ThinTesla.h"

#ifndef _KERNEL
#include <pthread.h>
#endif

//#define TESLA_PACK_STRUCTS
#ifdef TESLA_PACK_STRUCTS
#pragma pack(1)
#else
#endif

typedef struct TeslaEventFlags
{
    uint8_t isDeterministic : 1;
    uint8_t isAssertion : 1;
    uint8_t isBeforeAssertion : 1;
    uint8_t isOptional : 1;
    uint8_t isOR : 1;
    uint8_t isEnd : 1;
} TeslaEventFlags;

typedef struct TeslaEventState
{
    TeslaStore* store; // This pointer will be cannibalized to store the temporal tag for deterministic events.
    uint8_t* matchData;
} TeslaEventState;

_Static_assert(sizeof(TeslaStore*) >= sizeof(TeslaTemporalTag), "Tesla temporal tag too large to fit in pointer");

typedef struct TeslaEvent
{
    struct TeslaEvent** successors;
    TeslaEventFlags flags;
    size_t numSuccessors;
    size_t id;
    uint8_t matchDataSize;
} TeslaEvent;

typedef struct TeslaAutomatonFlags
{
    uint8_t isDeterministic : 1;
    uint8_t isThreadLocal : 1;
    uint8_t isLinked : 1;
} TeslaAutomatonFlags;

typedef struct TeslaAutomatonState
{
    TeslaTemporalTag currentTemporalTag;
    TeslaEvent* currentEvent;
    TeslaEvent* lastEvent;
    bool isCorrect;
    bool isActive;
    bool reachedAssertion;
} TeslaAutomatonState;

typedef size_t TeslaThreadKey;
#define INVALID_THREAD_KEY ((TeslaThreadKey)-1)

typedef struct TeslaAutomaton
{
    TeslaEvent** events;
    TeslaAutomatonFlags flags;
    size_t numEvents;
    char* name;

    TeslaAutomatonState state;
    TeslaEventState* eventStates;

    TeslaThreadKey threadKey;
    struct TeslaAutomaton* next;
} TeslaAutomaton;

void TA_Reset(TeslaAutomaton* automaton);

#ifdef TESLA_PACK_STRUCTS
#pragma options align = reset
#endif
