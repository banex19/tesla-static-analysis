#pragma once

#ifdef _KERNEL
#include "TeslaTypes.h"
#include "ThinTesla.h"

bool InitializeKernelThreadAutomata(KernelThreadAutomata* automata, size_t numAutomata);
bool CloneAutomatonToKernel(struct TeslaAutomaton* base, struct TeslaAutomaton* kernel);

struct TeslaAutomaton* GetThreadAutomatonKernel(struct TeslaAutomaton* base);

#endif