#include "TeslaState.h"

void TA_Reset(TeslaAutomaton* automaton)
{
    memset(&automaton->state, 0, sizeof(automaton->state));

    if (!automaton->flags.isDeterministic)
    {
        for (size_t i = 0; i < automaton->numEvents; ++i)
        {
            TeslaEvent* event = automaton->events[i];
            TeslaEventState* state = &automaton->eventStates[i];

            if (event->flags.isDeterministic)
            {
                state->store = NULL;
                continue;
            }

            if (state->store != NULL)
            {
                //   printf("[Clear] Store for event %d: %p\n", event->id, event->state.store);
                TeslaStore_Clear(state->store);
                //   TeslaFree(event->state.store);
                //   event->state.store = NULL;
            }
        }
    }

 //   printf("[%lu] Resetting automaton %p\n",  automaton->threadKey, automaton);

    // Signal this automaton can be reused.
    automaton->threadKey = INVALID_THREAD_KEY;
}

void TA_Init(TeslaAutomaton* automaton)
{
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