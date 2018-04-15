#pragma once

#include "TeslaState.h"
#include "ThinTesla.h"

void StartAutomaton(TeslaAutomaton* automaton);
void UpdateAutomaton(TeslaAutomaton* automaton, TeslaEvent* event, void* data, size_t dataSize);
void UpdateAutomatonDeterministic(TeslaAutomaton* automaton, TeslaEvent* event, void* data, size_t dataSize);
void EndAutomaton(TeslaAutomaton* automaton);