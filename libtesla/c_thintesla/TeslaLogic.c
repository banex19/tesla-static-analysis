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
    assert(automaton != NULL);
    assert(!automaton->state.isActive);
    assert(automaton->numEvents > 0);

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

void UpdateAutomatonDeterministic(TeslaAutomaton* automaton, TeslaEvent* event)
{
    //   DebugAutomaton(automaton);
    /*  printf("Transitioning - from:\t");
    DebugEvent(automaton->state.currentEvent); */

    if (!automaton->state.isActive)
        return;

    if (automaton->state.currentEvent == NULL && automaton->state.isActive)
    {
        TeslaAssertionFail(automaton);
    }

    if (automaton->state.currentEvent == event) // Double event is an error. Reset the automaton to the first event.
    {
        automaton->state.currentEvent = automaton->events[0];
    }
    else
    {
        TeslaEvent* current = automaton->state.currentEvent;

        assert(current != NULL);

        bool foundSuccessor = false;

        for (size_t i = 0; i < current->numSuccessors; ++i)
        {
            if (current->successors[i] == event) // Found a successor, this is a correct path.
            {
                automaton->state.currentEvent = current->successors[i];
                foundSuccessor = true;
                break;
            }
        }

        // This was not a successor of the previous event. Reset the automaton to the first event.
        if (!foundSuccessor)
            automaton->state.currentEvent = automaton->events[0];
    }

    /* printf("Transitioning - to:\t");
    DebugEvent(automaton->state.currentEvent); */

    if (automaton->state.currentEvent->flags.isEnd)
    {
        TA_Reset(automaton);
        automaton->state.isActive = true;
    }
}

void EndAutomaton(TeslaAutomaton* automaton, TeslaEvent* event)
{
    assert(automaton->state.isActive);

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