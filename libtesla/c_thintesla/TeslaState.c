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
            {
                event->state.store = NULL;
                continue;
            }

            if (event->state.store != NULL)
            {
              //   printf("[Clear] Store for event %d: %p\n", event->id, event->state.store);
                TeslaStore_Clear(event->state.store);
                //   TeslaFree(event->state.store);
                //   event->state.store = NULL;
            }
        }
    }
}