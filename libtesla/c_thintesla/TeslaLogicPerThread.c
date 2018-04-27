#include "TeslaAssert.h"
#include "TeslaLogic.h"
#include "TeslaMalloc.h"
#include "TeslaUtils.h"

//#define ENABLE_THREAD_DEBUG

void DebugThread(const char* message)
{
#ifdef ENABLE_THREAD_DEBUG
    printf("[%llu] %s\n", GetThreadKey(), message);
#endif
}

bool AreThreadKeysEqual(TeslaThreadKey first, TeslaThreadKey second)
{
    return first == second;
}

TeslaThreadKey GetThreadKey()
{
    TeslaThreadKey key = (TeslaThreadKey)pthread_self();
    DEBUG_ASSERT(key != INVALID_THREAD_KEY);

    return key;
}

TeslaAutomaton* GetUnusedAutomaton(TeslaAutomaton* automaton)
{
    while (automaton != NULL && !AreThreadKeysEqual(automaton->threadKey, INVALID_THREAD_KEY))
    {
        automaton = automaton->next;
    }

    return automaton;
}

TeslaAutomaton* GetThreadAutomatonAndLast(TeslaThreadKey key, TeslaAutomaton* automaton, TeslaAutomaton** lastInChain)
{
    *lastInChain = automaton;
    TeslaAutomaton* threadAutomaton = NULL;

    while (automaton != NULL && !AreThreadKeysEqual(automaton->threadKey, key))
    {
        *lastInChain = automaton;
        automaton = automaton->next;
    }

    if (automaton != NULL)
        threadAutomaton = automaton;

    assert(*lastInChain != NULL);

    return threadAutomaton;
}

TeslaAutomaton* GetThreadAutomaton(TeslaAutomaton* automaton)
{
    if (!automaton->flags.isThreadLocal)
        return automaton;

    TeslaAutomaton* last = NULL;
    return GetThreadAutomatonAndLast(GetThreadKey(), automaton, &last);
}

TeslaAutomaton* GetThreadAutomatonKey(TeslaThreadKey key, TeslaAutomaton* automaton)
{
    if (!automaton->flags.isThreadLocal)
        return automaton;

    TeslaAutomaton* last = NULL;
    return GetThreadAutomatonAndLast(key, automaton, &last);
}

void FreeAutomaton(TeslaAutomaton* automaton)
{
    if (automaton != NULL)
    {
        if (automaton->eventStates != NULL)
        {
            for (size_t i = 0; i < automaton->numEvents; ++i)
            {
                if (!automaton->events[i]->flags.isDeterministic)
                    TeslaFree(automaton->eventStates[i].matchData);
            }

            TeslaFree(automaton->eventStates);
        }

        TeslaFree(automaton);
    }
}

TeslaAutomaton* ForkAutomaton(TeslaAutomaton* base)
{
    TeslaThreadKey key = GetThreadKey();

    TeslaAutomaton* automaton = NULL;

retrysearch:
    key;
    TeslaAutomaton* lastInChain = NULL;
    TeslaAutomaton* existing = GetThreadAutomatonAndLast(key, base, &lastInChain);
    if (existing != NULL)
    {
        FreeAutomaton(automaton);
        return existing;
    }
    else
    {
        existing = GetUnusedAutomaton(base);
        if (existing != NULL)
        {
            if (!__sync_bool_compare_and_swap(&existing->threadKey, INVALID_THREAD_KEY, key))
                goto retrysearch; // Somebody was faster, try again.

            FreeAutomaton(automaton);
            return existing;
        }

        DebugThread("No unused automaton");
    }

tryappendagain:
    key;

    if (automaton == NULL)
    {
        automaton = TeslaMallocZero(sizeof(TeslaAutomaton));

        if (automaton == NULL)
            return NULL;

        // Copy static information.
        automaton->numEvents = base->numEvents;
        automaton->flags = base->flags;
        automaton->events = base->events;
        automaton->name = base->name;
        automaton->threadKey = key;

        if (!base->flags.isDeterministic)
        {
            automaton->eventStates = TeslaMallocZero(sizeof(TeslaEventState) * base->numEvents);

            if (automaton->eventStates == NULL)
            {
                FreeAutomaton(automaton);
                return NULL;
            }

            for (size_t i = 0; i < automaton->numEvents; ++i)
            {
                if (!automaton->events[i]->flags.isDeterministic)
                {
                    automaton->eventStates[i].matchData = TeslaMalloc(automaton->events[i]->matchDataSize * sizeof(size_t));

                    if (automaton->eventStates[i].matchData == NULL)
                    {
                        FreeAutomaton(automaton);
                        return NULL;
                    }
                }
            }
        }
    }

    DebugThread("Trying to fork new automaton");

    // Atomically append to chain.
    TeslaAutomaton* nullPtr = NULL;
    if (!__sync_bool_compare_and_swap(&lastInChain->next, NULL, automaton))
        goto tryappendagain;

    DebugThread("Forked new automaton");

    return automaton;
}

void UpdateEventWithData(TeslaAutomaton* automaton, size_t eventId, void* data)
{
    automaton = GetThreadAutomaton(automaton);
    if (automaton == NULL)
        return;

    memcpy(automaton->eventStates[eventId].matchData, data, automaton->events[eventId]->matchDataSize * sizeof(size_t));
}