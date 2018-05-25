
#include "KernelThreadAutomaton.h"
#include "TeslaAssert.h"
#include "TeslaLogic.h"
#include "TeslaMalloc.h"
#include "TeslaUtils.h"
#ifdef _KERNEL
#include <sys/proc.h>
#endif

//#define ENABLE_THREAD_DEBUG
void DebugThread(const char* message);
void DebugThread(const char* message)
{
#ifdef ENABLE_THREAD_DEBUG
    printf("[%lu] %s\n", GetThreadKey(), message);
#endif
}

void* BAD_VALUE = (void*)0xFFFFFFFFFFFFFFFF;

bool AreThreadKeysEqual(TeslaThreadKey first, TeslaThreadKey second)
{
    return first == second;
}

TeslaThreadKey GetThreadKey()
{
#ifdef _KERNEL
    TeslaThreadKey key = (TeslaThreadKey)(curthread->td_tid);
#else
    TeslaThreadKey key = (TeslaThreadKey)pthread_self();
#endif

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

    threadAutomaton = automaton;

    assert(*lastInChain != NULL);

    return threadAutomaton;
}

TeslaAutomaton* GetThreadAutomatonKey(TeslaThreadKey key, TeslaAutomaton* automaton)
{
    assert(automaton->flags.isThreadLocal);

    TeslaAutomaton* last = NULL;
    return GetThreadAutomatonAndLast(key, automaton, &last);
}

TeslaAutomaton* GetThreadAutomaton(TeslaAutomaton* automaton)
{
    assert(automaton->flags.isThreadLocal);

#ifndef _KERNEL
    return GetThreadAutomatonKey(GetThreadKey(), automaton);
#else
    return GetThreadAutomatonKernel(automaton);
#endif
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

TeslaAutomaton* ForkAutomaton(TeslaAutomaton* base, bool* leftover)
{
    TeslaThreadKey key = GetThreadKey();

    TeslaAutomaton* automaton = NULL;

retrysearch:
    (void)key;
    TeslaAutomaton* lastInChain = NULL;
    TeslaAutomaton* existing = GetThreadAutomatonAndLast(key, base, &lastInChain);
    if (existing != NULL)
    {
        *leftover = true;
        return existing;
    }
    else
    {
        existing = GetUnusedAutomaton(base);
        if (existing != NULL)
        {
            if (existing->threadKey != INVALID_THREAD_KEY || !__sync_bool_compare_and_swap(&(existing->threadKey), INVALID_THREAD_KEY, key))
                goto retrysearch; // Somebody was faster, try again.

            DEBUG_ASSERT(!existing->state.isActive);
            return existing;
        }

        // DebugThread("No unused automaton");
    }

tryappendagain:
    (void)key;

    if (automaton == NULL)
    {
        automaton = CreateAndCloneAutomaton(base);
        if (automaton == NULL)
            return NULL;
    }

    // Atomically append to chain.
    if (!__sync_bool_compare_and_swap(&(lastInChain->next), NULL, automaton))
    {
        GetThreadAutomatonAndLast(key, base, &lastInChain);
        goto tryappendagain;
    }

    return automaton;
}

TeslaAutomaton* CreateAndCloneAutomaton(TeslaAutomaton* base)
{
    TeslaAutomaton* automaton = TeslaMallocZero(sizeof(TeslaAutomaton));

    return CloneAutomaton(automaton, base);
}

TeslaAutomaton* CloneAutomaton(TeslaAutomaton* automaton, TeslaAutomaton* base)
{
    if (automaton == NULL)
        return NULL;

    // Copy static information.
    automaton->numEvents = base->numEvents;
    automaton->flags = base->flags;
    automaton->events = base->events;
    automaton->name = base->name;
    automaton->threadKey = GetThreadKey();

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

    return automaton;
}