#pragma once

#ifndef INCLUDING_TESLA_MACROS
#error "Do not include TeslaMacros.h from anywhere but TeslaLogic.h"
#endif

#ifdef _KERNEL
#include <sys/proc.h>
#endif

#ifdef LATE_INIT
#ifdef _KERNEL
#define GET_THREAD_AUTOMATON(automaton, event)                                                                               \
    do                                                                                                                       \
    {                                                                                                                        \
        TeslaAutomaton* baseAutomaton = automaton;                                                                           \
        if (!DoesKernelAutomatonExist(baseAutomaton) && (!event->flags.isInitial && !event->flags.isAssertion))              \
            automaton = NULL;                                                                                                \
        else                                                                                                                 \
        {                                                                                                                    \
            automaton = GetThreadAutomaton(baseAutomaton);                                                                   \
            if (automaton != NULL && curthread->automata != NULL && automaton->state.initTag < curthread->automata->initTag) \
                TA_Reset(automaton);                                                                                         \
            if (automaton == NULL || !automaton->state.isInit)                                                               \
            {                                                                                                                \
                automaton = LateInitAutomaton(baseAutomaton, automaton, event);                                              \
            }                                                                                                                \
        }                                                                                                                    \
    } while (0)
#else
#define GET_THREAD_AUTOMATON(automaton, event)                              \
    do                                                                      \
    {                                                                       \
        TeslaAutomaton* baseAutomaton = automaton;                          \
        if (automaton->flags.isThreadLocal)                                 \
        {                                                                   \
            automaton = GetThreadAutomaton(baseAutomaton);                  \
        }                                                                   \
        if (automaton == NULL || !automaton->state.isInit)                  \
        {                                                                   \
            automaton = LateInitAutomaton(baseAutomaton, automaton, event); \
        }                                                                   \
    } while (0)
#endif
#endif

#ifndef LATE_INIT
#define GET_THREAD_AUTOMATON(automaton, event)         \
    do                                                 \
    {                                                  \
        if (automaton->flags.isThreadLocal)            \
            automaton = GetThreadAutomaton(automaton); \
    } while (0)
#endif

#define RETURN_IF_DISABLED(automaton)                                                      \
    do                                                                                     \
    {                                                                                      \
        if (automaton == NULL || !automaton->state.isActive || automaton->state.hasFailed) \
        {                                                                                  \
            return;                                                                        \
        }                                                                                  \
    } while (0)

#define GET_THREAD_AUTOMATON_AND_NULL(automaton)       \
    do                                                 \
    {                                                  \
        if (automaton->flags.isThreadLocal)            \
            automaton = GetThreadAutomaton(automaton); \
    } while (0)

#define GET_THREAD_AUTOMATON_IF_ENABLED(automaton, event) \
    do                                                    \
    {                                                     \
        GET_THREAD_AUTOMATON(automaton, event);           \
        RETURN_IF_DISABLED(automaton);                    \
    } while (0)

#ifdef LATE_INIT // If we perform late initialization, we cannot fail until we reach the exit bound.
#define AUTOMATON_FAIL_MESSAGE_RETURN(automaton, message, retval) \
    do                                                            \
    {                                                             \
        automaton->state.hasFailed = true;                        \
        automaton->state.isActive = false;                        \
        automaton->state.failReason = message;                    \
        if (automaton->flags.isLinked)                            \
        {                                                         \
            TA_Reset(automaton);                                  \
            return retval;                                        \
        }                                                         \
        else                                                      \
            return retval;                                        \
    } while (0)
#else // If we don't perform late initialization, a failing assertion can immediately report that it failed.
#define AUTOMATON_FAIL_MESSAGE_RETURN(automaton, message, retval) \
    do                                                            \
    {                                                             \
        automaton->state.hasFailed = true;                        \
        automaton->state.isActive = false;                        \
        if (automaton->flags.isLinked)                            \
        {                                                         \
            TA_Reset(automaton);                                  \
            return retval;                                        \
        }                                                         \
        else                                                      \
            TeslaAssertionFailMessage(automaton, message);        \
    } while (0)
#endif

#define AUTOMATON_FAIL(automaton) AUTOMATON_FAIL_MESSAGE_RETURN(automaton, "", )
#define AUTOMATON_FAIL_MESSAGE(automaton, message) AUTOMATON_FAIL_MESSAGE_RETURN(automaton, message, )
#define AUTOMATON_FAIL_MESSAGE_FALSE(automaton, message) AUTOMATON_FAIL_MESSAGE_RETURN(automaton, message, false)