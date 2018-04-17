#include "TeslaAssert.h"

void TeslaAssertionFail(TeslaAutomaton* automaton)
{
#ifndef _KERNEL
    fprintf(stderr, "TESLA ASSERTION FAILED - Automaton %s\n", automaton->name);
#endif

    TeslaPanic();
}

void TeslaPanic()
{
#ifndef _KERNEL
    fflush(stderr);

    assert(false && "TESLA panic");
#endif
}