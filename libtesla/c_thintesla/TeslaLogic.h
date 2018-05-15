#pragma once

#include "TeslaState.h"
#include "ThinTesla.h"

// Guideline mode is default in the kernel
#ifdef _KERNEL
#define GUIDELINE_MODE
#else
#define GUIDELINE_MODE
#endif

#define LATE_INIT

#define LINEAR_HISTORY


/* Generic */
void StartAutomaton(TeslaAutomaton* automaton);
TeslaAutomaton* GenerateAutomaton(TeslaAutomaton* base);
TeslaAutomaton* InitAutomaton(TeslaAutomaton* automaton);
TeslaAutomaton* LateInitAutomaton(TeslaAutomaton* base, TeslaAutomaton* automaton, TeslaEvent* event);
TeslaAutomaton* GenerateAndInitAutomaton(TeslaAutomaton* base);
void UpdateAutomaton(TeslaAutomaton* automaton, TeslaEvent* event, void* data);
void UpdateAutomatonDeterministic(TeslaAutomaton* automaton, TeslaEvent* event);
void UpdateAutomatonDeterministicGeneric(TeslaAutomaton* automaton, TeslaEvent* event, bool updateTag);
void VerifyAutomaton(TeslaAutomaton* automaton);
bool VerifyORBlock(TeslaAutomaton* automaton, size_t* i, TeslaTemporalTag* lowerBound, TeslaTemporalTag* upperBound);
void VerifyAfterAssertion(TeslaAutomaton* automaton, size_t i, TeslaTemporalTag lowerBound, TeslaTemporalTag upperBound);
void EndAutomaton(TeslaAutomaton* automaton, TeslaEvent* event);
void EndLinkedAutomata(TeslaAutomaton** automata, size_t numAutomata);

/* Linear history */
size_t GetFirstOREventFromLastInBlock(TeslaAutomaton* automaton, size_t lastOREvent);
bool MatchEvent(TeslaAutomaton* automaton, Observation* observation);
bool VerifyORBlockLinearHistory(TeslaAutomaton* automaton, Observation* invalid, Observation** currentObservation, size_t lastOREvent, size_t* out_i);
bool UpdateAutomatonLinearHistory(TeslaAutomaton* automaton, TeslaEvent* event, void* data);
void VerifyAutomatonLinearHistory(TeslaAutomaton* automaton, size_t assertionEventId);

/* Per-thread specific */
bool AreThreadKeysEqual(TeslaThreadKey first, TeslaThreadKey second);
TeslaThreadKey GetThreadKey(void);
TeslaAutomaton* GetThreadAutomaton(TeslaAutomaton* automaton);
TeslaAutomaton* GetThreadAutomatonKey(TeslaThreadKey key, TeslaAutomaton* automaton);
TeslaAutomaton* GetThreadAutomatonAndLast(TeslaThreadKey key, TeslaAutomaton* automaton, TeslaAutomaton** lastInChain);
TeslaAutomaton* GetUnusedAutomaton(TeslaAutomaton* automaton);
TeslaAutomaton* CreateAndCloneAutomaton(TeslaAutomaton* base);
TeslaAutomaton* CloneAutomaton(TeslaAutomaton* automaton, TeslaAutomaton* base);
void UpdateEventWithData(TeslaAutomaton* automaton, size_t eventId, void* data);

void FreeAutomaton(TeslaAutomaton* automaton);
TeslaAutomaton* ForkAutomaton(TeslaAutomaton* base, bool* leftover);

/* Helpers */
size_t GetSuccessor(TeslaEvent* event, TeslaEvent* successor);

/* Debug */
void DebugEvent(TeslaEvent* event);
void DebugAutomaton(TeslaAutomaton* automaton);
void DebugMatchArray(TeslaAutomaton* automaton, TeslaEvent* event);


#define INCLUDING_TESLA_MACROS
#include "TeslaMacros.h"
#undef INCLUDING_TESLA_MACROS