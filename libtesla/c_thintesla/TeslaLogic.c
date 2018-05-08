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

void printBits(size_t const size, void* ptr);

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
    printf("\n");
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

#ifdef LATE_INIT
#define GET_THREAD_AUTOMATON(automaton, event)          \
    do                                                  \
    {                                                   \
        TeslaAutomaton* base = automaton;               \
        automaton = GetThreadAutomaton(automaton);      \
        if (automaton == NULL)                          \
        {                                               \
            automaton = LateInitAutomaton(base, event); \
        }                                               \
    } while (0)
#else
#define GET_THREAD_AUTOMATON(automaton, event)     \
    do                                             \
    {                                              \
        automaton = GetThreadAutomaton(automaton); \
    } while (0)
#endif

#define RETURN_IF_DISABLED(automaton)                                                      \
    do                                                                                     \
    {                                                                                      \
        if (automaton == NULL || !automaton->state.isActive || automaton->state.hasFailed) \
        {                                                                                  \
            return;                                                                        \
        }                                                                                  \
    } while (0)

#define GET_THREAD_AUTOMATON_AND_NULL(automaton)   \
    do                                             \
    {                                              \
        automaton = GetThreadAutomaton(automaton); \
    } while (0)

#define GET_THREAD_AUTOMATON_IF_ENABLED(automaton, event) \
    do                                                    \
    {                                                     \
        GET_THREAD_AUTOMATON(automaton, event);           \
        RETURN_IF_DISABLED(automaton);                    \
    } while (0)

#ifdef LATE_INIT // If we perform late initialization, we cannot fail until we reach the exit bound.
#define AUTOMATON_FAIL_MESSAGE_RETURN(automaton, message, retval) \
    do                                                            \
    {                                                             \
        automaton->state.hasFailed = true;                        \
        automaton->state.isActive = false;                        \
        if (automaton->flags.isLinked)                            \
        {                                                         \
            TA_Reset(automaton);                                  \
            return retval;                                        \
        }                                                         \
        else                                                      \
            return retval;                                        \
    } while (0)
#else // If we don't perform late initialization, a failing assertion can immediately report that it failed.
#define AUTOMATON_FAIL_MESSAGE_RETURN(automaton, message, retval) \
    do                                                            \
    {                                                             \
        automaton->state.hasFailed = true;                        \
        automaton->state.isActive = false;                        \
        if (automaton->flags.isLinked)                            \
        {                                                         \
            TA_Reset(automaton);                                  \
            return retval;                                        \
        }                                                         \
        else                                                      \
            TeslaAssertionFailMessage(automaton, message);        \
    } while (0)
#endif

#define AUTOMATON_FAIL(automaton) AUTOMATON_FAIL_MESSAGE_RETURN(automaton, "", )
#define AUTOMATON_FAIL_MESSAGE(automaton, message) AUTOMATON_FAIL_MESSAGE_RETURN(automaton, message, )
#define AUTOMATON_FAIL_MESSAGE_FALSE(automaton, message) AUTOMATON_FAIL_MESSAGE_RETURN(automaton, message, false)

//#define PRINT_TRANSITIONS
//#define PRINT_VERIFICATION

void StartAutomaton(TeslaAutomaton* automaton)
{
#ifndef _KERNEL
    WASTE_TIME(1);
#endif

#ifndef LATE_INIT
    GenerateAndInitAutomaton(automaton);
#endif
}

TeslaAutomaton* GenerateAndInitAutomaton(TeslaAutomaton* base)
{
    TeslaAutomaton* automaton = GenerateAutomaton(base);

    if (automaton != NULL)
        TA_Init(automaton);

    return automaton;
}

TeslaAutomaton* GenerateAutomaton(TeslaAutomaton* base)
{
    DEBUG_ASSERT(base != NULL);

    TeslaAutomaton* automaton = base;

    if (base->flags.isThreadLocal)
    {
        bool leftover = false;
        automaton = ForkAutomaton(base, &leftover);
        if (automaton == NULL)
            return NULL;

        if (leftover) // Leftover automaton.
        {
            TA_Reset(automaton);
            automaton = ForkAutomaton(base, &leftover);
            if (automaton = NULL)
                return NULL;
        }
    }

    DEBUG_ASSERT(!automaton->state.isActive);
    DEBUG_ASSERT(automaton->numEvents > 0);

    return automaton;
}

TeslaAutomaton* LateInitAutomaton(TeslaAutomaton* base, TeslaEvent* event)
{
#ifdef LATE_INIT
    if (!event->flags.isInitial && !event->flags.isAssertion)
        return NULL;

    return GenerateAndInitAutomaton(base);
#endif
}

void UpdateAutomaton(TeslaAutomaton* automaton, TeslaEvent* event, void* data)
{
    GET_THREAD_AUTOMATON_IF_ENABLED(automaton, event);

#ifdef PRINT_TRANSITIONS
    DebugAutomaton(automaton);
    printf("Transitioning - from:\t");
    DebugEvent(automaton->state.currentEvent);

    printf("Encountered event:\t");
    DebugEvent(event);
    DebugMatchArray(automaton, event);
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
        /* bool insert = */ TeslaStore_Insert(state->store, automaton->state.currentTemporalTag, data);
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
    GET_THREAD_AUTOMATON_IF_ENABLED(automaton, event);
    UpdateAutomatonDeterministicGeneric(automaton, event, true);
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

    assert(automaton->state.currentEvent != NULL);

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
        //  printf("[%lu] Automaton %s reached assertion\n", automaton->threadKey, automaton->name);
        if (automaton->state.reachedAssertion)
            AUTOMATON_FAIL_MESSAGE(automaton, "Assertion site reached multiple times");

        if (!foundSuccessor)
        {
            AUTOMATON_FAIL_MESSAGE(automaton, "Assertion site didn't cause a transition");
        }

        if (!automaton->flags.isDeterministic)
            VerifyAutomaton(automaton);

        automaton->state.reachedAssertion = true;
    }

#ifdef GUIDELINE_MODE
    if (foundSuccessor && event->flags.isFinal) // In guideline mode, this automaton has just succesfully completed. Disable it.
    {
        automaton->state.isActive = false;
    }
#endif
}

bool VerifyORBlock(TeslaAutomaton* automaton, size_t* i, TeslaTemporalTag* lowerBound, TeslaTemporalTag* upperBound)
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
                AUTOMATON_FAIL_MESSAGE_FALSE(automaton, "No event in OR block has occurred");

            if (*lowerBound == INVALID_TAG)
                *lowerBound = max;

            *upperBound = max;
            *i = localIndex - 1;
            return true;
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
            AUTOMATON_FAIL_MESSAGE_FALSE(automaton, "OR event occurred multiple times");

        TeslaTemporalTag bound = ONE << LeftmostOne(tag);

        if (bound > max)
            max = bound;

        if (bound < min)
            min = bound;
    }

    assert(false);
    return false;
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
            if (!VerifyORBlock(automaton, &i, &lowerBound, &upperBound))
                return;

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
            AUTOMATON_FAIL_MESSAGE(automaton, "Required event didn't occur");
        }

        if (tag < upperBound)
        {
            AUTOMATON_FAIL_MESSAGE(automaton, "Event occurred in the past");
        }

        upperBound = ONE << LeftmostOne(tag);

        if (lowerBound == INVALID_TAG)
            lowerBound = upperBound;

        if ((MaskBetweenExclUpper(upperBound, lowerBound) & tag) != 0)
        {
            AUTOMATON_FAIL_MESSAGE(automaton, "Multiple events of the same type occurred");
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
            AUTOMATON_FAIL_MESSAGE(automaton, "Event after assertion happened before assertion");
        }
    }
}

void EndAutomaton(TeslaAutomaton* automaton, TeslaEvent* event)
{
    GET_THREAD_AUTOMATON(automaton, event);

    if (automaton == NULL) // No automaton, this is fine.
        return;

#ifdef LATE_INIT
    if (automaton->state.hasFailed)
        TeslaAssertionFailMessage(automaton, "Late-initialized automaton failed and was in temporal bounds");
#endif

    if (automaton->state.isActive && automaton->state.reachedAssertion) // Only check automata that were in the assertion path.
    {
        DEBUG_ASSERT(automaton->flags.isLinked || automaton->state.isActive);

        UpdateAutomatonDeterministic(automaton, event);

        // If this automaton is not in a final event, the assertion has failed.
        if (!automaton->state.currentEvent->flags.isEnd)
        {
            TeslaAssertionFailMessage(automaton, "Automaton has reached the final temporal bound but is not in a final state");
        }
        else
        {
#ifdef GUIDELINE_MODE // This should never happen in guideline mode. All succesfull automata should have been catched already.
            assert(false && "Assertion passed at EndAutomaton() in guideline mode");
#endif
        }
    }

    if (!automaton->flags.isLinked) // Linked automata will be reset later, not now.
        TA_Reset(automaton);
}

const bool xorMode = false;

void EndLinkedAutomata(TeslaAutomaton** automata, size_t numAutomata)
{
    // Only one automaton should succeed in XOR mode.
    bool oneSucceeded = false;

    for (size_t i = 0; i < numAutomata; ++i)
    {
        TeslaAutomaton* automaton = automata[i];

        GET_THREAD_AUTOMATON_AND_NULL(automaton);

        if (automaton == NULL || !automaton->state.isActive) // This automaton failed.
        {
            continue;
        }
        else if (automaton->state.hasFailed)
        {
#ifdef LATE_INIT
            continue;
#endif
        }

        if (automaton->state.currentEvent->flags.isEnd) // This automaton succeeded.
        {
            if (xorMode && oneSucceeded) // More than one succeeded in XOR mode.
                TeslaAssertionFail(automaton);

            oneSucceeded = true;
            TA_Reset(automaton);
        }
    }

    if (!oneSucceeded)
        TeslaAssertionFail(automata[0]);
}

void UpdateEventWithData(TeslaAutomaton* automaton, size_t eventId, void* data)
{
    TeslaEvent* event = automaton->events[eventId];
    GET_THREAD_AUTOMATON_IF_ENABLED(automaton, event);

    memcpy(automaton->eventStates[eventId].matchData, data, automaton->events[eventId]->matchDataSize * sizeof(size_t));
}

void DebugEvent(TeslaEvent* event)
{
#ifndef _KERNEL
    printf("Event %lu (%p) - " BYTE_TO_BINARY_PATTERN " flags - %lu successors\n", event->id, event,
           BYTE_TO_BINARY(*((uint8_t*)&event->flags)), event->numSuccessors);
#endif
}

void DebugAutomaton(TeslaAutomaton* automaton)
{
#ifndef _KERNEL
    printf("Automaton %s (thread local: %s) - Address %p (events %p) - Temporal tag: %lu\n", automaton->name, automaton->flags.isThreadLocal ? "true" : "false",
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

        size_t* data = (size_t*)state->matchData;
        for (size_t i = 0; i < event->matchDataSize; ++i)
        {
            printf("Event %lu (%p - match array %p) - match[%lu] = %lu\n", event->id, state->matchData, event, i, data[i]);
        }
    }
}