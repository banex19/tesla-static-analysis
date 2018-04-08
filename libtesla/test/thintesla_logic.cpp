#include "TeslaState.h"
#include "ThinTesla.h"

#include <bitset>
#include <cstring>
#include <iostream>
#include <vector>

void TestPassed(const std::string& name)
{
    std::cout << "Test [" << name << "] passed\n";
}

uint64_t LastOne(uint64_t x)
{
    return 31 - __builtin_clz(x);
}

bool IsPowerOfTwo(size_t x)
{
    return (x & (x - 1)) == 0;
}

class StoreMock
{
  public:
    void Clear()
    {
        data.clear();
        temporalTags = 0;
    }

    void AddDataPoint(int param, size_t temporalTag)
    {
        assert(IsPowerOfTwo(temporalTag));
        temporalTags |= temporalTag;

        for (size_t i = 0; i < data.size(); ++i)
        {
            if (data[i].second == param)
            {
                data[i] = std::make_pair(data[i].first | temporalTag, param);
                return;
            }
        }

        data.push_back(std::make_pair(temporalTag, param));
    }

    std::pair<bool, size_t> Contains(int param)
    {
        for (auto& point : data)
        {
            if (point.second == param)
                return std::make_pair(true, point.first);
        }

        return std::make_pair(false, 0);
    }

    size_t temporalTags = 0;
    std::vector<std::pair<size_t, int>> data;
};

void Transition(TeslaAutomaton* automaton, TeslaEvent* event, int param)
{
    automaton->currentEvent = event;

    StoreMock* store = (StoreMock*)event->store;
    store->AddDataPoint(param, automaton->currentTemporalTag);
}

bool VerifyAssertion(TeslaAutomaton* automaton)
{
    size_t maxTemporalTag = 0;
    size_t validTags = 0;
    size_t lastTag = 0;
    for (size_t i = 0; i < automaton->numEvents; ++i)
    {
        TeslaEvent* event = automaton->events + i;
        StoreMock* store = (StoreMock*)event->store;

        if (!event->flags.isAssertion && !event->flags.isDeterministic)
        {
            size_t maxCurrentTag = LastOne(store->temporalTags);

            if (maxCurrentTag > maxTemporalTag)
            {
                maxTemporalTag = maxCurrentTag;
            }

            auto point = store->Contains(event->paramValue);
            if (point.first == false)
            {
                return false;
            }
            else
            {
                std::bitset<64> bits{point.second};
                std::bitset<64> valid{validTags};
                // std::cout << "Tag:\t" << bits << "\n";
                // std::cout << "Valid:\t" << valid << "\n";
                // std::cout << "Last one tag: " << LastOne(point.second) << "\n";
                // std::cout << "Last one valid: " << LastOne(validTags) << "\n";

                if ((point.second & validTags) && (LastOne(point.second) > LastOne(point.second & validTags)))
                {
                    return false;
                }
                else if ((point.second & validTags) == 0 && LastOne(validTags) > LastOne(point.second))
                {
                    return false;
                }
                else
                {
                    validTags = 1 << LastOne(point.second);
                    if (lastTag != 0 &&
                        (LastOne(lastTag) != LastOne(point.second)) &&
                        ((validTags >> 1 & lastTag) == 0))
                        return false;

                    lastTag = point.second;
                }
            }
        }
    }

    return true;
}

// True if first comes before second.
bool DoesEventComeBefore(TeslaAutomaton* automaton, TeslaEvent* first, TeslaEvent* second)
{
    for (size_t i = 0; i < automaton->numEvents; ++i)
    {
        if (automaton->events + i == first)
            return true;
        else if (automaton->events + i == second)
            return false;
    }

    assert(false);
}

bool UpdateAutomaton(TeslaAutomaton* automaton, TeslaEvent* event, int param, bool isAssertion = false)
{
    bool transitioned = false;
    if (automaton->currentEvent == nullptr && automaton->events + 0 == event)
    {
        Transition(automaton, event, param);
        transitioned = true;
    }
    else if (automaton->currentEvent == event)
    {
        if (event->flags.isDeterministic)
        {
            std::cout << "\"Assertion " << automaton->name << "\" failed during execution\n";
            return false;
        }
        else
        {
            automaton->currentTemporalTag <<= 1;
            Transition(automaton, event, param);
            transitioned = true;
        }
    }
    else if (automaton->currentEvent != nullptr)
    {
        for (size_t i = 0; i < automaton->currentEvent->numSuccessors; ++i)
        {
            if (automaton->currentEvent->successors + i == event)
            {
                Transition(automaton, event, param);
                transitioned = true;
                break;
            }
        }

        if (!transitioned)
        {
            if (DoesEventComeBefore(automaton, event, automaton->currentEvent))
            {
                automaton->currentTemporalTag <<= 1;
                StoreMock* store = (StoreMock*)event->store;
                store->AddDataPoint(param, automaton->currentTemporalTag);
            }
            else
            {
                if (!event->flags.isDeterministic)
                {
                    StoreMock* store = (StoreMock*)event->store;
                    store->AddDataPoint(param, automaton->currentTemporalTag << 1);
                }
            }
        }
    }

    if (transitioned)
    {
        if (automaton->currentEvent->flags.isAssertion)
        {
            assert(isAssertion);

            bool result = VerifyAssertion(automaton);
            std::cout << "Assertion \"" << automaton->name << "\" " << (result ? "passed" : "failed") << "\n";
            return result;
        }
        else
        {
            assert(!isAssertion);
        }
    }

    assert(!isAssertion);

    return false;
}

int a(int x, TeslaAutomaton* automaton, TeslaEvent* event)
{
    UpdateAutomaton(automaton, event, x);
    return x * 2;
}

int b(int x, TeslaAutomaton* automaton, TeslaEvent* event)
{
    UpdateAutomaton(automaton, event, x);
    return x / 2;
}

int c(int x, TeslaAutomaton* automaton, TeslaEvent* event)
{
    UpdateAutomaton(automaton, event, x);
    return x;
}

bool assertion(int x, TeslaAutomaton* automaton, TeslaEvent* event)
{
    return UpdateAutomaton(automaton, event, x, true);
}

void ResetAutomaton(TeslaAutomaton* toReset, TeslaAutomaton* initialState)
{
    memcpy(toReset, initialState, sizeof(TeslaAutomaton));
}

void ResetEvents(TeslaEvent* toReset, TeslaEvent* initial, size_t numEvents)
{
    memcpy(toReset, initial, sizeof(TeslaEvent) * numEvents);
    for (size_t i = 0; i < numEvents; ++i)
    {
        StoreMock* store = (StoreMock*)toReset[i].store;
        store->Clear();
    }
}

void SimpleLogic()
{
    constexpr size_t numEvents = 4;
    TeslaEvent events[numEvents];
    StoreMock stores[numEvents];
    memset(events, 0, sizeof(TeslaEvent) * numEvents);
    for (size_t i = 0; i < numEvents - 1; ++i)
    {
        events[i].successors = events + (i + 1);
        events[i].numSuccessors = 1;
        events[i].flags.isDeterministic = false;
        events[i].flags.isBeforeAssertion = true;
        events[i].id = i;
        events[i].store = (void*)(stores + i);
        events[i].paramValue = i;
    }

    events[numEvents - 1].flags.isAssertion = true;
    events[numEvents - 1].id = numEvents;
    events[numEvents - 1].store = (void*)(stores + numEvents - 1);

    TeslaEvent initialState[numEvents];
    memcpy(initialState, events, sizeof(events));

    TeslaAutomaton automaton;
    memset(&automaton, 0, sizeof(TeslaAutomaton));
    automaton.events = events;
    automaton.numEvents = numEvents;
    automaton.currentTemporalTag = 1;
    automaton.name = "a -> b -> c -> NOW";

    TeslaAutomaton initialAutomaton;
    memcpy(&initialAutomaton, &automaton, sizeof(TeslaAutomaton));

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);

    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    a(0, &automaton, events + 0);
    b(3, &automaton, events + 1);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);

    a(0, &automaton, events + 0);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);

    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == false);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == false);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);
    a(0, &automaton, events + 0);

    assert(assertion(0, &automaton, events + 3) == false);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);

    assert(automaton.currentEvent == events + 1);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    a(1, &automaton, events + 0);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    b(5, &automaton, events + 1);
    c(2, &automaton, events + 2);
    b(5, &automaton, events + 1);
    c(10, &automaton, events + 1);

    assert(assertion(0, &automaton, events + 3) == true);
    assert(automaton.currentEvent == events + 3);
}

int main()
{
    SimpleLogic();

    TestPassed("Tesla logic");
    return 0;
}