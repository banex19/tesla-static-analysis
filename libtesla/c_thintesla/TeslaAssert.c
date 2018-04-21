#include "TeslaAssert.h"

void TeslaWarning(const char* warning)
{
#ifndef _KERNEL
    fprintf(stderr, "TESLA WARNING - %s\n", warning);
#endif
}

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