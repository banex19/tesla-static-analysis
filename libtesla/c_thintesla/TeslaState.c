#include "TeslaState.h"

void TA_Reset(TeslaAutomaton* automaton)
{
    memset(&automaton->state, 0, sizeof(automaton->state));

    for (size_t i = 0; i < automaton->numEvents; ++i)
    {
        TeslaEvent* event = automaton->events[i];

        TeslaStore_Destroy(event->state.store);
        TeslaFree(event->state.matchData);
        TeslaFree(event->state.store);

        memset(&event->state, 0, sizeof(event->state));
    }
}