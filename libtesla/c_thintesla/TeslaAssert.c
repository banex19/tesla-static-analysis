#include "TeslaAssert.h"

void TeslaWarning(const char* warning)
{
#ifndef _KERNEL
    fprintf(stderr, "TESLA WARNING - %s\n", warning);
#else
    printf("TESLA WARNING - %s\n", warning);
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
#else
    if (strcmp(message, "") != 0)
    {
        printf("TESLA ASSERTION FAILED - Automaton %s\nReason: %s\n", automaton->name, message);
    }
    else
    {
        printf("TESLA ASSERTION FAILED - Automaton %s\n", automaton->name);
    }
#endif

    TeslaPanic();
}

void TeslaPanic()
{
#ifndef _KERNEL
    fflush(stderr);

    assert(false && "TESLA panic");
#else
    panic("TESLA assertion failed");
#endif
}