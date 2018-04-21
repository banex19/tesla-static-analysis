#include "TeslaAssert.h"
#include <string.h>

void TeslaWarning(const char* warning)
{
#ifndef _KERNEL
    fprintf(stderr, "TESLA WARNING - %s\n", warning);
#endif
}

void TeslaAssertionFail(TeslaAutomaton* automaton)
{
    TeslaAssertionFailMessage(automaton, "");
}

void TeslaAssertionFailMessage(TeslaAutomaton* automaton, const char* message)
{
#ifndef _KERNEL
    if (strcmp(message, "") != 0)
    {
        fprintf(stderr, "TESLA ASSERTION FAILED - Automaton %s\nReason: %s\n", automaton->name, message);
    }
    else
    {
        fprintf(stderr, "TESLA ASSERTION FAILED - Automaton %s\n", automaton->name);
    }
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