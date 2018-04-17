#include "TeslaState.h"

void TA_Reset(TeslaAutomaton* automaton)
{
    memset(&automaton->state, 0, sizeof(automaton->state));

    if (!automaton->flags.isDeterministic)
    {
        for (size_t i = 0; i < automaton->numEvents; ++i)
        {
            TeslaEvent* event = automaton->events[i];

            if (event->state.store != NULL)
                TeslaStore_Destroy(event->state.store);

            TeslaFree(event->state.matchData);
            TeslaFree(event->state.store);

            memset(&event->state, 0, sizeof(event->state));
        }
    }
}