#include "TeslaLogic.h"

bool UpdateAutomatonLinearHistory(TeslaAutomaton* automaton, TeslaEvent* event, void* data)
{
    if (automaton->history == NULL || !automaton->history->valid)
        return false;

    return TeslaHistory_Add(automaton->history, event->id, event->matchDataSize, data);
}

bool MatchEvent(TeslaAutomaton* automaton, Observation* observation)
{
    size_t numEvent = observation->header.numEvent;
    TeslaEvent* event = automaton->events[numEvent];
    TeslaEventState* state = &automaton->eventStates[numEvent];

    // printf("Matching event %lu\n", numEvent);

    if (event->flags.isDeterministic)
        return true;
    else
        return Hash64(state->matchData, GetEventMatchSize(event)) == observation->hash;
}

size_t GetFirstOREventFromLastInBlock(TeslaAutomaton* automaton, size_t lastOREvent)
{
    size_t current = lastOREvent - 1;

    while (automaton->events[current]->flags.isOR)
    {
        current--;
    }

    return current + 1;
}

bool VerifyORBlockLinearHistory(TeslaAutomaton* automaton, Observation* invalid, Observation** currentObservation, size_t lastOREvent, size_t* out_i)
{
    size_t firstOREvent = GetFirstOREventFromLastInBlock(automaton, lastOREvent);

    bool atLeastOne = false;

    Observation* current = *currentObservation;

    while (current != invalid)
    {
        if (!MatchEvent(automaton, current))
        {
            current--;
            continue;
        }

        if (current->header.numEvent >= firstOREvent && current->header.numEvent <= lastOREvent)
        {
            atLeastOne = true;
        }
        else // We encountered an event that is not in this OR block.
        {
            break;
        }

        current--;
    }

    if (!atLeastOne)
    {
        AUTOMATON_FAIL_MESSAGE_FALSE(automaton, "No event in OR block has occurred");
    }

    *currentObservation = current;
    *out_i = firstOREvent - 1;

    return true;
}

void VerifyAutomatonLinearHistory(TeslaAutomaton* automaton, size_t assertionEventId)
{
    assert(automaton->history != NULL);
    assert(automaton->history->valid);

    size_t numObservations = 0;
    Observation* observations = TeslaHistory_GetObservations(automaton->history, &numObservations);
    Observation* invalid = (observations == NULL) ? NULL : (observations - 1);

    Observation* current = (observations == NULL) ? NULL : (observations + (numObservations - 1));

    size_t i = assertionEventId - 1;

    while (i >= 1)
    {
        TeslaEvent* event = automaton->events[i];

        if (event->flags.isOR)
        {
            if (!VerifyORBlockLinearHistory(automaton, invalid, &current, i, &i))
                return;

            continue;
        }

        if (event->flags.isDeterministic)
        {
            i--;
            continue;
        }

        if (current == invalid) // We have no more observations.
        {
            if (!event->flags.isOptional)
            {
                AUTOMATON_FAIL_MESSAGE(automaton, "Required event didn't occur");
            }
            else
            {
                i--;
                continue;
            }
        }

        if (!MatchEvent(automaton, current)) // This is an event we don't care about.
        {
            current--;
        }

        if (current->header.numEvent != event->id) // This is another named event.
        {
            if (event->flags.isOptional) // We may have skipped this event if it's optional, just continue.
            {
                i--;
                continue;
            }
            else // We were expecting a specific event and we got another.
            {
                //     printf("Event %d\n", i);
                AUTOMATON_FAIL_MESSAGE(automaton, "Required event didn't occur");
            }
        }

        //  printf("Verified event %lu\n", i);

        // We observed the event we were looking for.
        i--;
        current--;
    }

    // Check that all other events are not named events that should only happen after the assertion.
    while (current != invalid)
    {
        if (current->header.numEvent > assertionEventId)
        {
            if (MatchEvent(automaton, current))
            {
                AUTOMATON_FAIL_MESSAGE(automaton, "Event after assertion happened before assertion");
            }
        }
        current--;
    }
}