#include "TeslaLogic.h"
#include "TeslaAssert.h"
#include "TeslaMalloc.h"

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)       \
    (byte & 0x80 ? '1' : '0'),     \
        (byte & 0x40 ? '1' : '0'), \
        (byte & 0x20 ? '1' : '0'), \
        (byte & 0x10 ? '1' : '0'), \
        (byte & 0x08 ? '1' : '0'), \
        (byte & 0x04 ? '1' : '0'), \
        (byte & 0x02 ? '1' : '0'), \
        (byte & 0x01 ? '1' : '0')

void StartAutomaton(TeslaAutomaton* automaton)
{
    DEBUG_ASSERT(automaton != NULL);
    DEBUG_ASSERT(!automaton->state.isActive);
    DEBUG_ASSERT(automaton->numEvents > 0);

    // Initial state.
    automaton->state.currentEvent = automaton->events[0];
    automaton->state.lastEvent = automaton->state.currentEvent;
    automaton->state.isActive = true;

    // Beginning of time.
    automaton->state.currentTemporalTag = 1;

    // We assume that this automaton will return a correct response for now.
    // This can change whenever, for example, an allocation fails.
    automaton->state.isCorrect = true;
}

void UpdateAutomaton(TeslaAutomaton* automaton, TeslaEvent* event, void* data, size_t dataSize)
{
    if (!event->flags.isDeterministic)
    {
        if (event->state.store == NULL || event->state.store->type == TESLA_STORE_INVALID)
        {
            event->state.store = TeslaMalloc(sizeof(TeslaStore));
            if (event->state.store == NULL)
            {
                automaton->state.isCorrect = false;
            }
            else
            {
                if (!TeslaStore_Create(TESLA_STORE_HT, 10, dataSize, event->state.store))
                {
                    event->state.store->type = TESLA_STORE_INVALID;
                    automaton->state.isCorrect = false;
                }
            }
        }

        if (event->state.store != NULL && event->state.store->type != TESLA_STORE_INVALID)
            TeslaStore_Insert(&event->state.store, automaton->state.currentTemporalTag, data);
    }
}

const size_t NO_SUCC = SIZE_MAX;

size_t GetSuccessor(TeslaEvent* event, TeslaEvent* successor)
{
    for (size_t i = 0; i < event->numSuccessors; ++i)
    {
        if (event->successors[i] == successor)
            return i;
    }

    return NO_SUCC;
}

//#define PRINT_TRANSITIONS

void UpdateAutomatonDeterministic(TeslaAutomaton* automaton, TeslaEvent* event)
{
    bool triedAgain = false;

//   DebugAutomaton(automaton);

#ifdef PRINT_TRANSITIONS
    printf("Transitioning - from:\t");
    DebugEvent(automaton->state.currentEvent);
#endif

    if (!automaton->state.isActive)
        return;

    if (automaton->state.currentEvent == NULL && automaton->state.isActive)
    {
        TeslaAssertionFail(automaton);
    }

tryagain:
    if (automaton->state.currentEvent == event) // Double event is an error. Reset the automaton to the first event and retry.
    {
        DEBUG_ASSERT(event != automaton->events[0]);
        automaton->state.currentEvent = automaton->events[0];
        if (!triedAgain)
        {
            triedAgain = true;
            goto tryagain;
        }
    }
    else
    {
        TeslaEvent* current = automaton->state.currentEvent;

        DEBUG_ASSERT(current != NULL);

        bool foundSuccessor = false;

        size_t succIndex = GetSuccessor(current, event);
        if (succIndex != NO_SUCC)
        {
            automaton->state.currentEvent = current->successors[succIndex];
            foundSuccessor = true;
        }

        if (!foundSuccessor)
        {
            // If both events are in the same OR block (whatever their relative order), this is fine.
            if (current->flags.isOR && event->flags.isOR && GetSuccessor(event, current) != NO_SUCC)
                foundSuccessor = true;
        }

        // This was not a successor of the previous event. Reset the automaton to the first event and retry.
        if (!foundSuccessor)
        {
            DEBUG_ASSERT(event != automaton->events[0]);
            automaton->state.currentEvent = automaton->events[0];
            if (!triedAgain)
            {
                triedAgain = true;
                goto tryagain;
            }
        }
    }

#ifdef PRINT_TRANSITIONS
    printf("Transitioning - to:\t");
    DebugEvent(automaton->state.currentEvent);
#endif

    if (automaton->state.currentEvent->flags.isEnd)
    {
        TA_Reset(automaton);
        automaton->state.isActive = true;
    }
}

void EndAutomaton(TeslaAutomaton* automaton, TeslaEvent* event)
{
    DEBUG_ASSERT(automaton->state.isActive);
    DEBUG_ASSERT(false);

    UpdateAutomatonDeterministic(automaton, event);

    // If this automaton is still active, the assertion failed.
    if (automaton->state.currentEvent != NULL)
    {
        TeslaAssertionFail(automaton);
    }

    automaton->state.isActive = false;
}

void DebugEvent(TeslaEvent* event)
{
#ifndef _KERNEL
    printf("Event %d - " BYTE_TO_BINARY_PATTERN " flags - %d successors\n", event->id,
           BYTE_TO_BINARY(*((uint8_t*)&event->flags)), event->numSuccessors);
#endif
}

void DebugAutomaton(TeslaAutomaton* automaton)
{
#ifndef _KERNEL
    printf("Automaton %s - %d events\n", automaton->name,
           automaton->numEvents);
#endif
}