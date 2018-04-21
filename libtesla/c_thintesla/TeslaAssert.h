#pragma once
#include "TeslaState.h"

void TeslaAssertionFail(TeslaAutomaton* automaton);

void TeslaPanic();

void TeslaWarning(const char* warning);