#include "TeslaAssert.h"

void TeslaAssertionFail(TeslaAutomaton* automaton)
{
#ifndef _KERNEL
    printf("TESLA ASSERTION FAILED - Automaton %s", automaton->name);
#endif

    TeslaPanic();
}

void TeslaPanic()
{
#ifndef _KERNEL
    assert(false && "TESLA panic");
#endif
}