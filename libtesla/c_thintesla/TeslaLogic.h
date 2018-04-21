#pragma once

#include "TeslaState.h"
#include "ThinTesla.h"

void StartAutomaton(TeslaAutomaton* automaton);
void UpdateAutomaton(TeslaAutomaton* automaton, TeslaEvent* event, void* data);
void UpdateAutomatonDeterministic(TeslaAutomaton* automaton, TeslaEvent* event);
void VerifyAutomaton(TeslaAutomaton* automaton);
void EndAutomaton(TeslaAutomaton* automaton, TeslaEvent* event);

size_t GetSuccessor(TeslaEvent* event, TeslaEvent* successor);

void DebugEvent(TeslaEvent* event);
void DebugAutomaton(TeslaAutomaton* automaton);
void DebugMatchArray(TeslaEvent* event);