#pragma once

#include "TeslaStore.h"
#include "ThinTesla.h"
#include "TeslaTypes.h"

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
    uint8_t matchDataSize;
} TeslaEventState;

_Static_assert(sizeof(TeslaStore*) >= sizeof(TeslaTemporalTag), "Tesla temporal tag too large to fit in pointer");

typedef struct TeslaEvent
{
    struct TeslaEvent** successors;
    TeslaEventFlags flags;
    size_t numSuccessors;
    size_t id;

    TeslaEventState state;
} TeslaEvent;

typedef struct TeslaAutomatonFlags
{
    uint8_t isDeterministic : 1;
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

typedef struct TeslaAutomaton
{
    TeslaEvent** events;
    TeslaAutomatonFlags flags;
    size_t numEvents;
    char* name;

    TeslaAutomatonState state;
} TeslaAutomaton;

void TA_Reset(TeslaAutomaton* automaton);

#ifdef TESLA_PACK_STRUCTS
#pragma options align = reset
#endif
