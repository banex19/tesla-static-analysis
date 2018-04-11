#pragma once

#include "ThinTesla.h"

typedef struct TeslaEventFlags {
    uint8_t isDeterministic : 1;
    uint8_t isAssertion : 1;
    uint8_t isBeforeAssertion : 1;
    uint8_t isOptional : 1;
    uint8_t isOR : 1;
} TeslaEventFlags;

typedef struct TeslaEvent {
    TeslaEvent** successors;
    TeslaEventFlags flags;
    size_t numSuccessors;

    void* store;
    size_t id;
    size_t paramValue;
} TeslaEvent;


typedef struct TeslaAutomatonFlags {
    uint8_t isDeterministic : 1;
} TeslaAutomatonFlags;

typedef struct TeslaAutomaton {
    TeslaEvent** events;
    TeslaAutomatonFlags flags;
    size_t numEvents;

    size_t currentTemporalTag;
    TeslaEvent* currentEvent;
    TeslaEvent* lastEvent;

    const char* name;
} TeslaAutomaton;