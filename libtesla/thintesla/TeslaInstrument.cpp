#include "TeslaInstrument.h"

uint64_t __thintesla_instrument_call(size_t numArgs, ...)
{
    va_list args;
    va_start(args, numArgs);

    uint64_t sum = 0;

    for (size_t i = 0; i < numArgs; ++i)
    {
        sum += va_arg(args, uint64_t);
    }

    va_end(args);

    return sum;
}

uint64_t __thintesla_instrument_return(uint64_t retVal)
{
    return retVal;
}