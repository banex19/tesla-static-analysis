#pragma once

#include "TeslaStore.h"
#include "ThinTesla.h"

#pragma pack(1)

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
    TeslaStore* store;
    uint8_t* matchData;
} TeslaEventState;

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
    size_t currentTemporalTag;
    TeslaEvent* currentEvent;
    TeslaEvent* lastEvent;
    bool isCorrect;
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

#pragma options align = reset