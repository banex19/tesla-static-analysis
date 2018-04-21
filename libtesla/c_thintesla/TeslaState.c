#include "TeslaState.h"

void TA_Reset(TeslaAutomaton* automaton)
{
    memset(&automaton->state, 0, sizeof(automaton->state));

    if (!automaton->flags.isDeterministic)
    {
        for (size_t i = 0; i < automaton->numEvents; ++i)
        {
            TeslaEvent* event = automaton->events[i];

            if (event->flags.isDeterministic)
                continue;

            if (event->state.store != NULL)
            {
                TeslaStore_Destroy(event->state.store);

                //   TeslaFree(event->state.store);
                //   event->state.store = NULL;
            }
        }
    }
}