#include "TeslaState.h"
#include "ThinTesla.h"

#include <bitset>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#include <x86intrin.h>

void TestPassed(const std::string& name)
{
    std::cout << "Test [" << name << "] passed\n";
}

using std::cout;
using namespace std::chrono_literals;

class NullOutput
{
};

template <typename T>
NullOutput& operator<<(NullOutput& null, const T& matrix)
{
    return null;
}

NullOutput nullOutput;
std::ostream& alwaysOut = std::cout;

#define DISABLE_ALL_OUTPUT
#ifdef DISABLE_ALL_OUTPUT
#define cout nullOutput
#endif

//#define DEBUG

//#define OLD_TECHNIQUE

uint64_t LastOne(uint64_t x)
{
    return 63 - __builtin_clzll(x);
}

uint64_t FirstOne(uint64_t x)
{
    return __builtin_ctzll(x);
}

bool IsPowerOfTwo(size_t x)
{
    return (x & (x - 1)) == 0;
}

const size_t ONE = (size_t)1;

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

    size_t Contains(int param)
    {
        for (auto& point : data)
        {
            if (point.second == param)
                return point.first;
        }

        return 0;
    }

    size_t temporalTags = 0;
    std::vector<std::pair<size_t, int>> data;
};

void Transition(TeslaAutomaton* automaton, TeslaEvent* event, int param)
{
    automaton->currentEvent = event;
    automaton->lastEvent = event;

    StoreMock* store = (StoreMock*)event->store;
    store->AddDataPoint(param, automaton->currentTemporalTag);
}

#define PRINT_VERIFICATION

bool HandleOptionalBlock(TeslaAutomaton* automaton, size_t* eventNum, size_t* validTags, size_t* invalidZone, bool* wentInFuture)
{
    size_t valid = *validTags;
    size_t frontierMask = valid - 1;
    for (size_t i = *eventNum; i < automaton->numEvents; ++i)
    {
        TeslaEvent* event = automaton->events[i];
        StoreMock* store = (StoreMock*)event->store;

        if (!event->flags.isOptional)
        {
            *eventNum = i;
            *validTags = valid;
            return true;
        }

        if (!event->flags.isAssertion && !event->flags.isDeterministic)
        {
            size_t tag = store->Contains(event->paramValue);
            if (tag == 0)
            {
                continue;
            }
            else
            {
#ifdef PRINT_VERIFICATION
                std::bitset<64> bits{tag};
                std::bitset<64> validO{valid};
                cout << "\n";
                cout << "Tag:\t" << bits << "\n";
                cout << "Valid:\t" << validO << "\n";
#endif

                bool happenedInPast = false;

                if ((tag & valid) && (LastOne(tag) > LastOne(tag & valid)))
                {
                    return false;
                }
                else if ((tag & valid) == 0 && LastOne(valid) > LastOne(tag))
                {
                    happenedInPast = true;
                }

                if (*wentInFuture)
                {
                    size_t mustBeZero = *invalidZone & tag;
                    if (mustBeZero != 0)
                        return false;
                }

                if (!happenedInPast)
                {
                    if ((ONE << LastOne(tag)) > valid)
                    {
                        *wentInFuture = true;
                        size_t frontierMask = valid - 1;
                        size_t fullMask = (ONE << LastOne(tag)) - 1;

                        *invalidZone |= (fullMask ^ frontierMask);
                        std::bitset<64> invalid{*invalidZone};
                        cout << "Inv:\t" << invalid << "\n";

                        size_t mustBeZero = *invalidZone & tag;
                        if (mustBeZero != 0)
                            return false;
                    }

                    valid = ONE << LastOne(tag);
                }
            }
        }
    }

    assert(false);
    return true;
}

bool VerifyAssertion(TeslaAutomaton* automaton)
{
    size_t validTags = 0;
    size_t lastTag = 0;
    size_t invalidZone = 0;

    bool wentInFuture = false;

    for (size_t i = 0; i < automaton->numEvents; ++i)
    {
        TeslaEvent* event = automaton->events[i];
        StoreMock* store = (StoreMock*)event->store;

#ifdef DEBUG
        cout << "[" << automaton << "] Verifying event " << event << " with store " << store << "\n";
#endif

        if (!event->flags.isAssertion && !event->flags.isDeterministic)
        {
            if (event->flags.isOptional)
            {
                if (!HandleOptionalBlock(automaton, &i, &validTags, &invalidZone, &wentInFuture))
                    return false;

                assert(i > 0);
                i--;
                continue;
            }

            size_t tag = store->Contains(event->paramValue);
            if (tag == 0)
            {
                return false;
            }
            else
            {
#ifdef PRINT_VERIFICATION
                std::bitset<64> bits{tag};
                std::bitset<64> valid{validTags};
                std::bitset<64> last{lastTag};
                cout << "\n";
                cout << "Tag:\t" << bits << "\n";
                cout << "Valid:\t" << valid << "\n";
                cout << "Last:\t" << last << "\n";
#endif
                // cout << "Last one tag: " << LastOne(tag) << "\n";
                // cout << "Last one valid: " << LastOne(validTags) << "\n";

                if (i != 0)
                {
                    // Check that there's nothing happening both now and in the future.
                    if ((tag & validTags) && (LastOne(tag) > LastOne(tag & validTags)))
                    {
                        return false;
                    }
                    // Check that this event didn't happen in the past.
                    else if ((tag & validTags) == 0 && LastOne(validTags) > LastOne(tag))
                    {
                        return false;
                    }

                    if (wentInFuture)
                    {
                        size_t mustBeZero = invalidZone & tag;
                        if (mustBeZero != 0)
                            return false;
                    }

                    if ((ONE << LastOne(tag)) > validTags)
                    {
                        wentInFuture = true;
                        size_t frontierMask = validTags - 1;
                        size_t fullMask = (ONE << LastOne(tag)) - 1;

                        invalidZone |= (fullMask ^ frontierMask);
                        std::bitset<64> invalid{invalidZone};
                        cout << "Inv:\t" << invalid << "\n";

                        size_t mustBeZero = invalidZone & tag;
                        if (mustBeZero != 0)
                            return false;
                    }
                }

                validTags = ONE << LastOne(tag);
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
        if (automaton->events[i] == first)
            return true;
        else if (automaton->events[i] == second)
            return false;
    }

    assert(false);
}

bool UpdateAutomaton(TeslaAutomaton* automaton, TeslaEvent* event, int param, bool isAssertion = false)
{
    bool transitioned = false;
    if (automaton->currentEvent == nullptr && automaton->events[0] == event)
    {
        Transition(automaton, event, param);
        transitioned = true;
    }
    else if (automaton->currentEvent == event)
    {
        if (event->flags.isDeterministic)
        {
            cout << "\"Assertion " << automaton->name << "\" failed during execution\n";
            return false;
        }
        else
        {
            if (automaton->currentEvent == automaton->lastEvent)
                automaton->currentTemporalTag <<= 1;

            Transition(automaton, event, param);
        }
    }
    else if (automaton->currentEvent != nullptr)
    {
        for (size_t i = 0; i < automaton->currentEvent->numSuccessors; ++i)
        {
            /*   if (automaton->currentEvent->numSuccessors > 0)
            {
                cout << "Current: " << automaton->currentEvent << "\n";
                cout << "Successor: " << automaton->currentEvent->successors[i] << "\n";
                cout << "Event: " << event << "\n";
            } */
            if (automaton->currentEvent->successors[i] == event)
            {
                Transition(automaton, event, param);
                transitioned = true;
                break;
            }
        }

        if (!transitioned)
        {
            if (DoesEventComeBefore(automaton, event, automaton->lastEvent))
            {
                automaton->currentTemporalTag <<= 1;
                StoreMock* store = (StoreMock*)event->store;
                store->AddDataPoint(param, automaton->currentTemporalTag);

                automaton->lastEvent = event;
            }
            else if (DoesEventComeBefore(automaton, event, automaton->currentEvent))
            {
                StoreMock* store = (StoreMock*)event->store;
                store->AddDataPoint(param, automaton->currentTemporalTag);

                automaton->lastEvent = event;
            }
            else
            {
                if (!event->flags.isDeterministic)
                {
                    StoreMock* store = (StoreMock*)event->store;
                    store->AddDataPoint(param, automaton->currentTemporalTag);
                    automaton->currentTemporalTag <<= 1;
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
            cout << "Assertion \"" << automaton->name << "\" " << (result ? "passed" : "failed") << "\n";
            return result;
        }
        else
        {
            assert(!isAssertion);
        }
    }

    //  assert(!isAssertion);
    return false;
}

bool a(int x, TeslaAutomaton* automaton, TeslaEvent* event)
{
    UpdateAutomaton(automaton, event, x);
    return true;
}

bool b(int x, TeslaAutomaton* automaton, TeslaEvent* event)
{
    UpdateAutomaton(automaton, event, x);
    return true;
}

bool c(int x, TeslaAutomaton* automaton, TeslaEvent* event)
{
    UpdateAutomaton(automaton, event, x);
    return false;
}

bool d(int x, TeslaAutomaton* automaton, TeslaEvent* event)
{
    UpdateAutomaton(automaton, event, x);
    return false;
}

bool e(int x, TeslaAutomaton* automaton, TeslaEvent* event)
{
    UpdateAutomaton(automaton, event, x);
    return false;
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

typedef bool (*fntype)(int, TeslaAutomaton*, TeslaEvent*);

fntype fns[10] = {a, b, c, d, e};

void Fuzz(TeslaAutomaton* automaton, TeslaEvent* events, size_t numEvents, TeslaAutomaton* initialAutomaton, TeslaEvent* initialState,
          size_t maxRandomEvents, size_t numTests, bool alwaysCorrect, size_t seed = 0)
{
    std::random_device rd;
    std::mt19937 mt(seed);
    std::uniform_int_distribution<size_t> dist(1, maxRandomEvents);
    std::uniform_int_distribution<size_t> eventDist(0, numEvents - 2);
    std::uniform_int_distribution<size_t> fakeDist(0, 1);

    size_t numChecked = 0;
    size_t numPassed = 0;

    for (size_t i = 0; i < numTests; ++i)
    {
        ResetAutomaton(automaton, initialAutomaton);
        ResetEvents(events, initialState, numEvents);

        size_t numRandomEvents = dist(mt);

        std::vector<size_t> stack;

        bool expectTrue = true;
        for (size_t k = 0; k < numRandomEvents; ++k)
        {
            size_t eventNumber = eventDist(mt);
            bool correctEvent = alwaysCorrect || fakeDist(mt);

            if (correctEvent)
                cout << "Called " << eventNumber << "\n";
            else
                cout << "(Incorrectly) called " << eventNumber << "\n";

            if (correctEvent)
                stack.push_back(eventNumber);

            fns[eventNumber](correctEvent ? eventNumber : (eventNumber + 100), automaton, events + eventNumber);
        }

        if (automaton->currentEvent == (events + (numEvents - 2)))
        {
            if (stack.size() >= numEvents - 1)
            {
                size_t optionalsSkipped = 0;
                for (size_t i = 0; i < numEvents - optionalsSkipped - 1; ++i)
                {
                    while (true)
                    {
                        size_t numExpected = numEvents - (i + 2 + optionalsSkipped);

                        if (stack[stack.size() - (i + 1)] != numExpected)
                        {
                            if (events[numExpected].flags.isOptional)
                                optionalsSkipped++;
                            else
                            {
                                numExpected = 0;
                            }
                        }
                        else
                        {
                            break;
                        }

                        if (numExpected == 0)
                        {
                            expectTrue = false;
                            break;
                        }
                    }
                }

                bool result = assertion(0, automaton, events + (numEvents - 1));
                if (result != expectTrue)
                {
                    //  cout << "ERROR\n";
                    // std::this_thread::sleep_for(5s);
                    assert(false && "Assertion incorrect");
                }

                numChecked++;
                numPassed += (expectTrue ? 1 : 0);
            }
        }
    }

    alwaysOut << "[Seed: " << seed << "] Checked assertions: " << numChecked << " (" << numPassed << " passed)\n";
}

void FuzzTest()
{
    std::vector<std::vector<TeslaEvent*>> successors;
    std::vector<TeslaEvent*> automatonEvents;

    constexpr size_t numEvents = 5;
    TeslaEvent events[numEvents];
    StoreMock stores[numEvents];
    memset(events, 0, sizeof(TeslaEvent) * numEvents);
    successors.resize(numEvents);

    for (size_t i = 0; i < numEvents - 1; ++i)
    {
        successors[i].push_back(events + (i + 1));
        events[i].successors = successors[i].data();
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

    for (size_t i = 0; i < numEvents; ++i)
    {
#ifdef DEBUG
        cout << "Event: " << (events + i) << "\n";
        cout << "Store: " << (stores + i) << "\n";
#endif
        automatonEvents.push_back(events + i);
    }

    TeslaEvent initialState[numEvents];
    memcpy(initialState, events, sizeof(events));

    TeslaAutomaton automaton;
    memset(&automaton, 0, sizeof(TeslaAutomaton));
    automaton.events = automatonEvents.data();
    automaton.numEvents = numEvents;
    automaton.currentTemporalTag = 1;
    automaton.name = "a -> b -> c -> d -> NOW";

#ifdef DEBUG
    cout << "Automaton: " << &automaton << "\n";
#endif

    TeslaAutomaton initialAutomaton;
    memcpy(&initialAutomaton, &automaton, sizeof(TeslaAutomaton));

    // Optional events.
    /* initialAutomaton.name = "optional";
    successors[0].clear();
    successors[0].push_back(events + 1);
    successors[0].push_back(events + 2);
    initialState[1].flags.isOptional = true;
    initialState[0].numSuccessors = 2;
    initialState[0].successors = successors[0].data(); 

    // Multiple optional events.
    initialAutomaton.name = "multiple optionals";
    initialState[1].flags.isOptional = true;
    initialState[2].flags.isOptional = true;
    successors[0].clear();
    successors[0].push_back(events + 1);
    successors[0].push_back(events + 2);
    successors[0].push_back(events + 3);
    successors[1].clear();
    successors[1].push_back(events + 2);
    successors[1].push_back(events + 3);
    initialState[0].numSuccessors = 3;
    initialState[0].successors = successors[0].data();
    initialState[1].numSuccessors = 2;
    initialState[1].successors = successors[1].data(); */

    // Disjoint optional events.
    assert(numEvents >= 5);
    initialAutomaton.name = "disjoint optionals";
    initialState[1].flags.isOptional = true;
    initialState[3].flags.isOptional = true;
    successors[0].clear();
    successors[0].push_back(events + 1);
    successors[0].push_back(events + 2);
    successors[2].clear();
    successors[2].push_back(events + 3);
    successors[2].push_back(events + 4);
    initialState[0].numSuccessors = 2;
    initialState[0].successors = successors[0].data();
    initialState[2].numSuccessors = 2;
    initialState[2].successors = successors[2].data();

    for (size_t i = 0; i < 10; ++i)
        Fuzz(&automaton, events, numEvents, &initialAutomaton, initialState,
             40, 1'000'000, false, i);
}

void TestSpecific()
{
    std::vector<std::vector<TeslaEvent*>> successors;
    std::vector<TeslaEvent*> automatonEvents;

    constexpr size_t numEvents = 5;
    TeslaEvent events[numEvents];
    StoreMock stores[numEvents];
    memset(events, 0, sizeof(TeslaEvent) * numEvents);
    successors.resize(numEvents);

    for (size_t i = 0; i < numEvents - 1; ++i)
    {
        successors[i].push_back(events + (i + 1));
        events[i].successors = successors[i].data();
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

    for (size_t i = 0; i < numEvents; ++i)
    {
#ifdef DEBUG
        cout << "Event: " << (events + i) << "\n";
        cout << "Store: " << (stores + i) << "\n";
#endif
        automatonEvents.push_back(events + i);
    }

    TeslaEvent initialState[numEvents];
    memcpy(initialState, events, sizeof(events));

    TeslaAutomaton automaton;
    memset(&automaton, 0, sizeof(TeslaAutomaton));
    automaton.events = automatonEvents.data();
    automaton.numEvents = numEvents;
    automaton.currentTemporalTag = 1;
    automaton.name = "a -> b -> c -> d -> NOW";

#ifdef DEBUG
    cout << "Automaton: " << &automaton << "\n";
#endif

    TeslaAutomaton initialAutomaton;
    memcpy(&initialAutomaton, &automaton, sizeof(TeslaAutomaton));

    // Disjoint optional events.
    assert(numEvents >= 5);
    initialAutomaton.name = "disjoint optionals";
    initialState[1].flags.isOptional = true;
    initialState[3].flags.isOptional = true;
    successors[0].clear();
    successors[0].push_back(events + 1);
    successors[0].push_back(events + 2);
    successors[2].clear();
    successors[2].push_back(events + 3);
    successors[2].push_back(events + 4);
    initialState[0].numSuccessors = 2;
    initialState[0].successors = successors[0].data();
    initialState[2].numSuccessors = 2;
    initialState[2].successors = successors[2].data();

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    c(20, &automaton, events + 2);
    a(10, &automaton, events + 0);
    b(1, &automaton, events + 1);
    d(3, &automaton, events + 3);
    d(30, &automaton, events + 3);
    a(0, &automaton, events + 0);
    d(30, &automaton, events + 3);
    a(0, &automaton, events + 0);
    a(0, &automaton, events + 0);
    d(30, &automaton, events + 3);
    d(30, &automaton, events + 3);
    a(0, &automaton, events + 0);
    d(3, &automaton, events + 3);

    assert(assertion(0, &automaton, events + 4) == false);
}

void TestLogic()
{
    std::vector<std::vector<TeslaEvent*>> successors;
    std::vector<TeslaEvent*> automatonEvents;

    constexpr size_t numEvents = 4;
    TeslaEvent events[numEvents];
    StoreMock stores[numEvents];
    memset(events, 0, sizeof(TeslaEvent) * numEvents);
    successors.resize(numEvents);

    for (size_t i = 0; i < numEvents - 1; ++i)
    {
        successors[i].push_back(events + (i + 1));
        events[i].successors = successors[i].data();
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

    for (size_t i = 0; i < numEvents; ++i)
    {
#ifdef DEBUG
        cout << "Event: " << (events + i) << "\n";
        cout << "Store: " << (stores + i) << "\n";
#endif
        automatonEvents.push_back(events + i);
    }

    TeslaEvent initialState[numEvents];
    memcpy(initialState, events, sizeof(events));

    TeslaAutomaton automaton;
    memset(&automaton, 0, sizeof(TeslaAutomaton));
    automaton.events = automatonEvents.data();
    automaton.numEvents = numEvents;
    automaton.currentTemporalTag = 1;
    automaton.name = "a -> b -> c -> NOW";

#ifdef DEBUG
    cout << "Automaton: " << &automaton << "\n";
#endif

    TeslaAutomaton initialAutomaton;
    memcpy(&initialAutomaton, &automaton, sizeof(TeslaAutomaton));

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);

    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

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

    // Optional events.
    initialAutomaton.name = "a -> optional(b) -> c -> NOW";
    successors[0].clear();
    successors[0].push_back(events + 1);
    successors[0].push_back(events + 2);
    initialState[1].flags.isOptional = true;
    initialState[0].numSuccessors = 2;
    initialState[0].successors = successors[0].data();

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    b(5, &automaton, events + 1);
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
    c(2, &automaton, events + 2);
    c(3, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    b(1, &automaton, events + 1);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    c(5, &automaton, events + 2);
    b(1, &automaton, events + 1);
    b(3, &automaton, events + 1);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    a(5, &automaton, events + 0);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);

    assert(assertion(0, &automaton, events + 3) == false);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);

    assert(assertion(0, &automaton, events + 3) == false);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);

    assert(assertion(0, &automaton, events + 3) == false);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);
    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);
    a(0, &automaton, events + 0);
    c(4, &automaton, events + 2);
    b(3, &automaton, events + 1);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(2, &automaton, events + 1);
    a(0, &automaton, events + 0);
    c(5, &automaton, events + 2);
    b(1, &automaton, events + 1);
    a(3, &automaton, events + 0);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    // Multiple optional events.
    initialAutomaton.name = "a -> optional(b) -> optional(c) -> NOW";
    initialState[1].flags.isOptional = true;
    initialState[2].flags.isOptional = true;
    successors[0].clear();
    successors[0].push_back(events + 1);
    successors[0].push_back(events + 2);
    successors[0].push_back(events + 3);
    successors[1].clear();
    successors[1].push_back(events + 2);
    successors[1].push_back(events + 3);
    initialState[0].numSuccessors = 3;
    initialState[0].successors = successors[0].data();
    initialState[1].numSuccessors = 2;
    initialState[1].successors = successors[1].data();

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);

    a(0, &automaton, events + 0);
    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);

    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
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
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);

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
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);

    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);
    a(0, &automaton, events + 0);
    a(1, &automaton, events + 0);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);

    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == false);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);

    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);
    a(0, &automaton, events + 0);
    c(2, &automaton, events + 2);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);
    c(2, &automaton, events + 2);
    a(0, &automaton, events + 0);

    assert(assertion(0, &automaton, events + 3) == true);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);

    a(0, &automaton, events + 0);
    a(0, &automaton, events + 0);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == false);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);

    a(0, &automaton, events + 0);
    a(0, &automaton, events + 0);
    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);
    a(1, &automaton, events + 0);
    b(2, &automaton, events + 1);
    c(2, &automaton, events + 2);

    assert(assertion(0, &automaton, events + 3) == false);

    ResetAutomaton(&automaton, &initialAutomaton);
    ResetEvents(events, initialState, numEvents);

    a(0, &automaton, events + 0);
    b(1, &automaton, events + 1);
    c(2, &automaton, events + 2);
    b(1, &automaton, events + 1);
    a(0, &automaton, events + 0);

    assert(assertion(0, &automaton, events + 3) == true);
}

int main()
{
    // TestSpecific();
    TestLogic();
    FuzzTest();

    TestPassed("Tesla logic");
    return 0;
}