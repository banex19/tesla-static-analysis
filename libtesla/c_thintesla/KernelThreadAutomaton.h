#pragma once

#ifdef _KERNEL
#include "TeslaTypes.h"
#include "ThinTesla.h"
#include "TeslaState.h"

bool InitializeKernelThreadAutomata(KernelThreadAutomata* automata, size_t numAutomata);
bool CloneAutomatonToKernel(struct TeslaAutomaton* base, struct TeslaAutomaton* kernel);

bool DoesKernelAutomatonExist(struct TeslaAutomaton* base);
struct TeslaAutomaton* GetThreadAutomatonKernel(struct TeslaAutomaton* base);

void DecreaseKernelActiveCount(void);
void IncreaseKernelActiveCount(void);
size_t GetKernelActiveCount(void);
void ResetAllAutomata(void);

void IncrementInitTag(void);

#endif