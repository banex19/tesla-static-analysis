#pragma once

#include "TeslaState.h"
#include "ThinTesla.h"

/* Generic */
void StartAutomaton(TeslaAutomaton* automaton);
void UpdateAutomaton(TeslaAutomaton* automaton, TeslaEvent* event, void* data);
void UpdateAutomatonDeterministic(TeslaAutomaton* automaton, TeslaEvent* event);
void UpdateAutomatonDeterministicGeneric(TeslaAutomaton* automaton, TeslaEvent* event, bool updateTag);
void VerifyAutomaton(TeslaAutomaton* automaton);
void VerifyAfterAssertion(TeslaAutomaton* automaton, size_t i, TeslaTemporalTag lowerBound, TeslaTemporalTag upperBound);
void EndAutomaton(TeslaAutomaton* automaton, TeslaEvent* event);
void EndLinkedAutomata(TeslaAutomaton** automata, size_t numAutomata);

/* Per-thread specific */
bool AreThreadKeysEqual(TeslaThreadKey first, TeslaThreadKey second);
TeslaThreadKey GetThreadKey();
TeslaAutomaton* GetThreadAutomaton(TeslaAutomaton* automaton);
TeslaAutomaton* GetThreadAutomatonKey(TeslaThreadKey key, TeslaAutomaton* automaton);
TeslaAutomaton* GetThreadAutomatonAndLast(TeslaThreadKey key, TeslaAutomaton* automaton, TeslaAutomaton** lastInChain);
TeslaAutomaton* GetUnusedAutomaton(TeslaAutomaton* automaton);
void UpdateEventWithData(TeslaAutomaton* automaton, size_t eventId, void* data);

void FreeAutomaton(TeslaAutomaton* automaton);
TeslaAutomaton* ForkAutomaton(TeslaAutomaton* base);

/* Helpers */
size_t GetSuccessor(TeslaEvent* event, TeslaEvent* successor);

/* Debug */
void DebugEvent(TeslaEvent* event);
void DebugAutomaton(TeslaAutomaton* automaton);
void DebugMatchArray(TeslaAutomaton* automaton, TeslaEvent* event);