#pragma once
#include "TeslaAssertion.h"

class TeslaAssertionBuilder
{
    enum BuilderState
    {
        WAITING = 0,
        BUILDING,
        FINISHED
    };

  public:
    TeslaAssertionBuilder(event_id numEvents = 0);

    TeslaAssertion GetAssertion();

    void AddStartEvent(TeslaEvent &e);
    void AddFinalEvent(TeslaEvent &e);

    void AddTemporalEvent(TeslaEvent &e);
    void AddOptionalEvents(std::vector<TeslaEvent> &optEvents);
    void AddDisjunctedEvents(std::vector<TeslaEvent> &optEvents);

  private:
    void LinkSuccessors(const std::vector<event_id> &successors);

    void AssertStartedNotFinished()
    {
        SAFE_ASSERT(state != BuilderState::WAITING && BuilderState::FINISHED != state);
    }

    BuilderState state = BuilderState::WAITING;

    std::vector<TeslaEvent> events;
    size_t flags = TeslaAssertionFlags::ASSERTION_DETERMINISTIC;

    size_t numCurrentEvents = 0;
    event_id currentEvents[MAX_SUCCESSORS];
};