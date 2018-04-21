#include "TeslaLogic.h"
#include "TeslaAssert.h"
#include "TeslaMalloc.h"
#include "TeslaUtils.h"

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

const size_t NO_SUCC = SIZE_MAX;

size_t GetSuccessor(TeslaEvent* event, TeslaEvent* successor)
{
    if (successor->id < event->id)
        return NO_SUCC;

    for (size_t i = 0; i < event->numSuccessors; ++i)
    {
        if (event->successors[i] == successor)
            return i;
    }

    return NO_SUCC;
}

const TeslaTemporalTag INVALID_TAG = 0;
const TeslaTemporalTag ONE = 1;

//#define PRINT_TRANSITIONS

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

    if (!automaton->flags.isDeterministic)
    {
        bool allCorrect = true;

        for (size_t i = 0; i < automaton->numEvents; ++i)
        {
            TeslaEvent* event = automaton->events[i];

            if (event->flags.isDeterministic)
                continue;

            size_t dataSize = event->state.matchDataSize * sizeof(size_t);

            // Try to allocate space for the store data structure.
            if (event->state.store == NULL)
            {
                event->state.store = TeslaMalloc(sizeof(TeslaStore));
                if (event->state.store == NULL)
                {
                    allCorrect = false;
                }
                else
                    event->state.store->type = TESLA_STORE_INVALID;
            }

            // Try to construct the store.
            if (event->state.store != NULL && event->state.store->type == TESLA_STORE_INVALID)
            {
                if (!TeslaStore_Create(TESLA_STORE_HT, 10, dataSize, event->state.store))
                {
                    event->state.store->type = TESLA_STORE_INVALID;
                    allCorrect = false;
                }
            }
        }

        automaton->state.isCorrect = allCorrect;
    }

    if (!automaton->state.isCorrect)
    {
        TeslaWarning("Automaton may be incorrect");
    }
}

void UpdateAutomaton(TeslaAutomaton* automaton, TeslaEvent* event, void* data)
{
#ifdef PRINT_TRANSITIONS
    DebugAutomaton(automaton);
    printf("Transitioning - from:\t");
    DebugEvent(automaton->state.currentEvent);

    printf("Encountered event:\t");
    DebugEvent(event);
    DebugMatchArray(event);
#endif

    if (!automaton->state.isActive)
        return;

    size_t succ = GetSuccessor(automaton->state.currentEvent, event);

    TeslaEvent* current = automaton->state.currentEvent;
    TeslaEvent* last = automaton->state.lastEvent;

    if (succ != NO_SUCC)
    {
        automaton->state.currentEvent = event;
    }
    else if (event->id <= last->id)
    {
        automaton->state.currentTemporalTag <<= 1;
    }

    automaton->state.lastEvent = event;

    if (event->state.store != NULL && event->state.store->type != TESLA_STORE_INVALID)
    {
        bool insert = TeslaStore_Insert(event->state.store, automaton->state.currentTemporalTag, data);
    }

    if (event->id > current->id && succ == NO_SUCC)
    {
        automaton->state.currentTemporalTag <<= 1;
    }

#ifdef PRINT_TRANSITIONS
    printf("Transitioning - to:\t");
    DebugEvent(automaton->state.currentEvent);
#endif
}

void UpdateAutomatonDeterministic(TeslaAutomaton* automaton, TeslaEvent* event)
{
    bool triedAgain = false;
    bool foundSuccessor = false;

    TeslaEvent* originalCurrent = automaton->state.currentEvent;

#ifdef PRINT_TRANSITIONS
    DebugAutomaton(automaton);
    printf("Transitioning - from:\t");
    DebugEvent(automaton->state.currentEvent);

    printf("Encountered event:\t");
    DebugEvent(event);
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

        size_t succIndex = GetSuccessor(current, event);
        if (succIndex != NO_SUCC)
        {
            automaton->state.currentEvent = current->successors[succIndex];
            foundSuccessor = true;
        }

        if (!foundSuccessor)
        {
            // If both events are in the same OR block (regardless of their relative order), this is fine.
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

    if (!automaton->flags.isDeterministic)
    {
        if ((!foundSuccessor || triedAgain) && event->id <= originalCurrent->id) // We're backtracking.
            automaton->state.currentTemporalTag <<= 1;

        *((uintptr_t*)&(event->state.store)) |= automaton->state.currentTemporalTag;

        if ((!foundSuccessor || triedAgain) && event->id > originalCurrent->id) // We briefly went to the future, now we're going back to the past.
            automaton->state.currentTemporalTag <<= 1;

        automaton->state.lastEvent = automaton->state.currentEvent;
    }

    if (event->flags.isAssertion)
    {
        if (!foundSuccessor)
        {
            TeslaAssertionFail(automaton);
        }

        if (!automaton->flags.isDeterministic)
            VerifyAutomaton(automaton);

        automaton->state.reachedAssertion = true;
    }
}

//#define PRINT_VERIFICATION


void VerifyAutomaton(TeslaAutomaton* automaton)
{
    TeslaTemporalTag upperBound = INVALID_TAG;
    TeslaTemporalTag lowerBound = INVALID_TAG;

    for (size_t i = 1; i < automaton->numEvents; ++i)
    {
        TeslaEvent* event = automaton->events[i];

        if (event->flags.isAssertion)
            break;

        if (!event->flags.isDeterministic && (event->state.store == NULL || event->state.store->type == TESLA_STORE_INVALID))
            continue;

        TeslaTemporalTag tag = event->flags.isDeterministic ? (TeslaTemporalTag)((uintptr_t)event->state.store)
                                                            : TeslaStore_Get(event->state.store, event->state.matchData);
#ifdef PRINT_VERIFICATION
        printf("Current tag: %llu\n", upperBound);
        printf("Tag for event %llu: %llu\n", event->id, tag);
#endif

        if (tag == INVALID_TAG && !event->flags.isOptional)
        {
            TeslaAssertionFailMessage(automaton, "Required event didn't occur");
        }

        if (tag < upperBound && !event->flags.isOptional)
        {
            TeslaAssertionFailMessage(automaton, "Event occurred in the past");
        }

        upperBound = ONE << LeftmostOne(tag);

        if (lowerBound == INVALID_TAG)
            lowerBound = upperBound;

        if ((MaskBetweenExclUpper(upperBound, lowerBound) & tag) != 0)
        {
            TeslaAssertionFailMessage(automaton, "Multiple events of the same type occurred");
        }
    }
}

void EndAutomaton(TeslaAutomaton* automaton, TeslaEvent* event)
{
    if (automaton->state.reachedAssertion) // Only check automata that were in the assertion path.
    {
        DEBUG_ASSERT(automaton->state.isActive);

        UpdateAutomatonDeterministic(automaton, event);

        // If this automaton is not in a final event, the assertion has failed.
        if (!automaton->state.currentEvent->flags.isEnd)
        {
            TeslaAssertionFail(automaton);
        }
    }

    TA_Reset(automaton);
}

void DebugEvent(TeslaEvent* event)
{
#ifndef _KERNEL
    printf("Event %d (%p) - " BYTE_TO_BINARY_PATTERN " flags - %d successors\n", event->id, event,
           BYTE_TO_BINARY(*((uint8_t*)&event->flags)), event->numSuccessors);
#endif
}

void DebugAutomaton(TeslaAutomaton* automaton)
{
#ifndef _KERNEL
    printf("Automaton %s - Temporal tag: %llu\n", automaton->name, automaton->state.currentTemporalTag);
    for (size_t i = 0; i < automaton->numEvents; ++i)
    {
        DebugEvent(automaton->events[i]);
    }
#endif
}

void DebugMatchArray(TeslaEvent* event)
{
    size_t* data = event->state.matchData;
    for (size_t i = 0; i < event->state.matchDataSize; ++i)
    {
        printf("Event %d (%p) - match[%d] = %llu\n", event->id, event, i, data[i]);
    }
}