#pragma once

#ifndef INCLUDING_TESLA_MACROS
#error "Do not include TeslaMacros.h from anywhere but TeslaLogic.h"
#endif

#ifdef LATE_INIT
#define GET_THREAD_AUTOMATON(automaton, event)                   \
    do                                                           \
    {                                                            \
        TeslaAutomaton* baseAutomaton = automaton;               \
        automaton = GetThreadAutomaton(baseAutomaton);           \
        if (automaton == NULL || !automaton->state.isInit)                                   \
        {                                                        \
            automaton = LateInitAutomaton(baseAutomaton, event); \
        }                                                        \
    } while (0)
#else
#define GET_THREAD_AUTOMATON(automaton, event)     \
    do                                             \
    {                                              \
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

#define GET_THREAD_AUTOMATON_AND_NULL(automaton)   \
    do                                             \
    {                                              \
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