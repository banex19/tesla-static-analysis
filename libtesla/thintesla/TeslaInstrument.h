#pragma once
#include "ThinTesla.h"

#include <stdarg.h>

uint64_t __thintesla_instrument_call(size_t numArgs, ...);
uint64_t __thintesla_instrument_return(uint64_t retVal);