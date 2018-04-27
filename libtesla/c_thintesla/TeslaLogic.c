#include "TeslaLogic.h"
#include "TeslaAssert.h"
#include "TeslaMalloc.h"
#include "TeslaUtils.h"

volatile size_t useless_var = 0;
#define WASTE_TIME(val)                  \
    do                                   \
    {                                    \
        for (size_t i = 0; i < 100; ++i) \
            useless_var += i;            \
    } while (0)
//#define WASTE_TIME(val)

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

void printBits(size_t const size, void* ptr)
{
    unsigned char* b = (unsigned char*)ptr;
    unsigned char byte;
    int i, j;

    for (i = size - 1; i >= 0; i--)
    {
        for (j = 7; j >= 0; j--)
        {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
    puts("");
}

const size_t NO_SUCC = SIZE_MAX;

size_t GetSuccessor(TeslaEvent* event, TeslaEvent* successor)
{
    if (successor->id <= event->id)
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

#define GET_THREAD_AUTOMATON(automaton)            \
    do                                             \
    {                                              \
        automaton = GetThreadAutomaton(automaton); \
        if (automaton == NULL)                     \
            return;                                \
    } while (0)

//#define PRINT_START
//#define PRINT_TRANSITIONS
//#define PRINT_VERIFICATION

void StartAutomaton(TeslaAutomaton* automaton)
{
    WASTE_TIME(1);

    DEBUG_ASSERT(automaton != NULL);

    if (automaton->flags.isThreadLocal)
    {
        automaton = ForkAutomaton(automaton);
        if (automaton == NULL)
            return;
    }

    DEBUG_ASSERT(!automaton->state.isActive);
    DEBUG_ASSERT(automaton->numEvents > 0);

#ifdef PRINT_START
    DebugAutomaton(automaton);
#endif

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
            TeslaEventState* state = &automaton->eventStates[i];

            if (event->flags.isDeterministic)
                continue;

            size_t dataSize = event->matchDataSize * sizeof(size_t);

            // Try to allocate space for the store data structure.
            if (state->store == NULL)
            {
                state->store = TeslaMalloc(sizeof(TeslaStore));
                if (state->store == NULL)
                {
                    allCorrect = false;
                }
                else
                    state->store->type = TESLA_STORE_INVALID;
            }

            // Try to construct the store.
            if (state->store != NULL && state->store->type == TESLA_STORE_INVALID)
            {
                if (!TeslaStore_Create(TESLA_STORE_HT, 10, dataSize, state->store))
                {
                    state->store->type = TESLA_STORE_INVALID;
                    allCorrect = false;
                }
            }
        }

        automaton->state.isCorrect = allCorrect;
    }

    for (size_t i = 0; i < automaton->numEvents; ++i)
    {
        //    printf("[Init] Store for event %d: %p\n", automaton->events[i]->id, automaton->events[i]->state.store);
    }

    if (!automaton->state.isCorrect)
    {
        TeslaWarning("Automaton may be incorrect");
    }
}

void UpdateAutomaton(TeslaAutomaton* automaton, TeslaEvent* event, void* data)
{
    GET_THREAD_AUTOMATON(automaton);

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

    TeslaEventState* state = &automaton->eventStates[event->id];

    if (automaton->state.reachedAssertion) // We've already reached the assertion, we have more information now.
    {
        if (memcmp(data, state->matchData, event->matchDataSize * sizeof(size_t)) == 0)
            UpdateAutomatonDeterministicGeneric(automaton, event, false);
        return;
    }

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

    if (state->store != NULL && state->store->type != TESLA_STORE_INVALID)
    {
        bool insert = TeslaStore_Insert(state->store, automaton->state.currentTemporalTag, data);
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
    GET_THREAD_AUTOMATON(automaton);
    return UpdateAutomatonDeterministicGeneric(automaton, event, true);
}

void UpdateAutomatonDeterministicGeneric(TeslaAutomaton* automaton, TeslaEvent* event, bool updateTag)
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

        if (updateTag)
        {
            TeslaEventState* state = &automaton->eventStates[event->id];

            *((uintptr_t*)&(state->store)) |= automaton->state.currentTemporalTag;
        }

        if ((!foundSuccessor || triedAgain) && event->id > originalCurrent->id) // We briefly went to the future, now we're going back to the past.
            automaton->state.currentTemporalTag <<= 1;

        automaton->state.lastEvent = automaton->state.currentEvent;
    }

    if (event->flags.isAssertion)
    {
        if (automaton->state.reachedAssertion)
            TeslaAssertionFailMessage(automaton, "Assertion site reached multiple times");

        if (!foundSuccessor)
        {
            TeslaAssertionFail(automaton);
        }

        if (!automaton->flags.isDeterministic)
            VerifyAutomaton(automaton);

        automaton->state.reachedAssertion = true;
    }
}

void VerifyORBlock(TeslaAutomaton* automaton, size_t* i, TeslaTemporalTag* lowerBound, TeslaTemporalTag* upperBound)
{
    size_t localIndex = *i;

    TeslaTemporalTag max = *upperBound;
    TeslaTemporalTag min = (size_t)-1;

    TeslaTemporalTag validMask = (*lowerBound - ONE) ^ min;
#ifdef PRINT_VERIFICATION
    printf("[OR] Valid mask:\t");
    printBits(sizeof(validMask), &validMask);
#endif

    bool atLeastOnceinOR = false;
    for (; localIndex < automaton->numEvents; ++localIndex)
    {
        TeslaEvent* event = automaton->events[localIndex];
        TeslaEventState* state = &automaton->eventStates[localIndex];

        if (!event->flags.isDeterministic && (state->store == NULL || state->store->type == TESLA_STORE_INVALID))
            continue;

        if (!event->flags.isOR)
        {
            if (!atLeastOnceinOR)
                TeslaAssertionFailMessage(automaton, "No event in OR block has occurred");

            if (*lowerBound == INVALID_TAG)
                *lowerBound = max;

            *upperBound = max;
            *i = localIndex - 1;
            return;
        }

        TeslaTemporalTag tag = event->flags.isDeterministic ? (TeslaTemporalTag)((uintptr_t)state->store)
                                                            : TeslaStore_Get(state->store, state->matchData);

#ifdef PRINT_VERIFICATION
        printf("[OR] Current tag:\t");
        printBits(sizeof(*upperBound), upperBound);
        printf("[OR] Tag for event %llu:\t", event->id);
        printBits(sizeof(tag), &tag);
#endif

        if (tag != INVALID_TAG && tag >= *upperBound) // Real event.
        {
            atLeastOnceinOR = true;
        }
        else if (tag == INVALID_TAG || tag < *lowerBound) // Didn't happen or happened too far in the past.
            continue;

        if (validMask && (!IsPowerOfTwo(validMask & tag) || (validMask & tag) < *upperBound))
            TeslaAssertionFailMessage(automaton, "OR event occurred multiple times");

        TeslaTemporalTag bound = ONE << LeftmostOne(tag);

        if (bound > max)
            max = bound;

        if (bound < min)
            min = bound;
    }
}

void VerifyAutomaton(TeslaAutomaton* automaton)
{
    TeslaTemporalTag upperBound = INVALID_TAG;
    TeslaTemporalTag lowerBound = INVALID_TAG;

    for (size_t i = 1; i < automaton->numEvents - 1; ++i)
    {
        TeslaEvent* event = automaton->events[i];
        TeslaEventState* state = &automaton->eventStates[i];

        if (event->flags.isAssertion)
        {
            VerifyAfterAssertion(automaton, i + 1, lowerBound, upperBound);
            return;
        }

        if (!event->flags.isDeterministic && (state->store == NULL || state->store->type == TESLA_STORE_INVALID))
        {
            assert(false);
        }

        if (event->flags.isOR)
        {
            VerifyORBlock(automaton, &i, &lowerBound, &upperBound);
            continue;
        }

        TeslaTemporalTag tag = event->flags.isDeterministic ? (TeslaTemporalTag)((uintptr_t)state->store)
                                                            : TeslaStore_Get(state->store, state->matchData);

#ifdef PRINT_VERIFICATION
        printf("Current tag:\t\t");
        printBits(sizeof(upperBound), &upperBound);
        printf("Tag for event %llu:\t", event->id);
        printBits(sizeof(tag), &tag);
#endif

        if (event->flags.isOptional && (tag == INVALID_TAG || tag < upperBound))
            continue;

        if (tag == INVALID_TAG)
        {
            TeslaAssertionFailMessage(automaton, "Required event didn't occur");
        }

        if (tag < upperBound)
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

void VerifyAfterAssertion(TeslaAutomaton* automaton, size_t i, TeslaTemporalTag lowerBound, TeslaTemporalTag upperBound)
{
    for (; i < automaton->numEvents - 1; ++i)
    {
        TeslaEvent* event = automaton->events[i];
        TeslaEventState* state = &automaton->eventStates[i];

        if (!event->flags.isDeterministic && (state->store == NULL || state->store->type == TESLA_STORE_INVALID))
        {
            assert(false);
        }

        //  printf("Store for event %d: %p\n", event->id, state->store);

        TeslaTemporalTag tag = event->flags.isDeterministic ? (TeslaTemporalTag)((uintptr_t)state->store)
                                                            : TeslaStore_Get(state->store, state->matchData);

#ifdef PRINT_VERIFICATION
        printf("[AFT] Lower bound:\t");
        printBits(sizeof(lowerBound), &lowerBound);
        printf("[AFT] Tag for event %llu:\t", event->id);
        printBits(sizeof(tag), &tag);
#endif

        if (tag != INVALID_TAG && tag >= lowerBound)
        {
            TeslaAssertionFailMessage(automaton, "Event after assertion happened before assertion");
        }
    }
}

void EndAutomaton(TeslaAutomaton* automaton, TeslaEvent* event)
{
    GET_THREAD_AUTOMATON(automaton);

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
    printf("Automaton %s (thread local: %s) - Address %p (events %p) - Temporal tag: %llu\n", automaton->name, automaton->flags.isThreadLocal ? "true" : "false",
           automaton, automaton->events, automaton->state.currentTemporalTag);
    for (size_t i = 0; i < automaton->numEvents; ++i)
    {
        DebugMatchArray(automaton, automaton->events[i]);
        DebugEvent(automaton->events[i]);
    }
#endif
}

void DebugMatchArray(TeslaAutomaton* automaton, TeslaEvent* event)
{
    if (automaton->eventStates != NULL)
    {
        TeslaEventState* state = &automaton->eventStates[event->id];

        size_t* data = state->matchData;
        for (size_t i = 0; i < event->matchDataSize; ++i)
        {
            printf("Event %d (%p - match array %p) - match[%d] = %llu\n", event->id, state->matchData, event, i, data[i]);
        }
    }
}