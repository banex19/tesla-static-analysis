#pragma once

#include "TeslaState.h"
#include "ThinTesla.h"

void StartAutomaton(TeslaAutomaton* automaton);
void UpdateAutomaton(TeslaAutomaton* automaton, TeslaEvent* event, void* data, size_t dataSize);
void UpdateAutomatonDeterministic(TeslaAutomaton* automaton, TeslaEvent* event);
void EndAutomaton(TeslaAutomaton* automaton, TeslaEvent* event);


void DebugEvent(TeslaEvent* event);
void DebugAutomaton(TeslaAutomaton* automaton);