#include "TeslaAssertionBuilder.h"
#include <cstring>

TeslaAssertionBuilder::TeslaAssertionBuilder(event_id numEvents)
{
    events.reserve(numEvents);
}

TeslaAssertion TeslaAssertionBuilder::GetAssertion()
{
    return TeslaAssertion(events, flags);
}

void TeslaAssertionBuilder::AddStartEvent(TeslaEvent &e)
{
    DEBUG_ASSERT(events.size() == 0);

    SAFE_ASSERT(state == BuilderState::WAITING);

    state = BuilderState::BUILDING;

    AddTemporalEvent(e);
}

void TeslaAssertionBuilder::LinkSuccessors(const std::vector<event_id> &successors)
{
    DEBUG_ASSERT(successors.size() <= MAX_SUCCESSORS);

    for (event_id i = 0; i < numCurrentEvents; ++i)
    {
        for (auto &succ : successors)
            events[currentEvents[i]].AddSuccessor(succ);
    }

    memcpy(currentEvents, successors.data(), sizeof(event_id) * successors.size());
    numCurrentEvents = successors.size();
}

void TeslaAssertionBuilder::AddFinalEvent(TeslaEvent &e)
{
    AddTemporalEvent(e);
    state = BuilderState::FINISHED;
}

void TeslaAssertionBuilder::AddTemporalEvent(TeslaEvent &e)
{
    AssertStartedNotFinished();

    events.push_back(e);

    if (!e.IsDeterministic())
        flags |= TeslaAssertionFlags::ASSERTION_NONDETERMINISTIC;

    LinkSuccessors({events.size() - 1});
}

void TeslaAssertionBuilder::AddDisjunctedEvents(std::vector<TeslaEvent> &optEvents)
{
    AssertStartedNotFinished();

    SAFE_ASSERT(optEvents.size() <= MAX_SUCCESSORS);

    std::vector<event_id> successors;

    for (auto &e : optEvents)
    {
        if (!e.IsDeterministic())
            flags |= TeslaAssertionFlags::ASSERTION_NONDETERMINISTIC;

        events.push_back(e);
        successors.push_back(events.size() - 1);
    }

    LinkSuccessors(successors);
}

void TeslaAssertionBuilder::AddOptionalEvents(std::vector<TeslaEvent> &optEvents)
{
    AssertStartedNotFinished();

    SAFE_ASSERT(optEvents.size() <= MAX_SUCCESSORS);

    std::vector<event_id> successors;

    for (auto &e : optEvents)
    {
        if (!e.IsDeterministic())
            flags |= TeslaAssertionFlags::ASSERTION_NONDETERMINISTIC;

        events.push_back(e);
        successors.push_back(events.size() - 1);
    }

    LinkSuccessors(successors);

    // Save number of events and their ids.
    size_t numSavedEvents = successors.size();
    event_id savedEvents[MAX_SUCCESSORS];
    memcpy(savedEvents, currentEvents, sizeof(savedEvents));

    // Link all optionals between each other (respecting order).
    while (successors.size() > 1)
    {
        numCurrentEvents = 1;
        successors.erase(successors.begin());

        LinkSuccessors(successors);
    }

    numCurrentEvents = numSavedEvents;
    memcpy(currentEvents, savedEvents, sizeof(savedEvents));
}