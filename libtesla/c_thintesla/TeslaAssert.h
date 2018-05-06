#pragma once
#include "TeslaState.h"

void TeslaAssertionFail(TeslaAutomaton* automaton);
void TeslaAssertionFailMessage(TeslaAutomaton* automaton, const char* message);

void TeslaPanic(void);

void TeslaWarning(const char* warning);