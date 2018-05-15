#pragma once

#ifdef _KERNEL
#include "TeslaTypes.h"
#include "ThinTesla.h"
#include "TeslaState.h"

bool InitializeKernelThreadAutomata(KernelThreadAutomata* automata, size_t numAutomata);
bool CloneAutomatonToKernel(struct TeslaAutomaton* base, struct TeslaAutomaton* kernel);

bool DoesKernelAutomatonExist(struct TeslaAutomaton* base);
struct TeslaAutomaton* GetThreadAutomatonKernel(struct TeslaAutomaton* base);

#endif