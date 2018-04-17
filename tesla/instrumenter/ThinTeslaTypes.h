#pragma once

#include "../../libtesla/c_thintesla/TeslaState.h"
#include "llvm/Support/raw_ostream.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include "ThinTeslaAssertion.h"

#include <iostream>

using namespace llvm;

class TeslaTypes
{
  public:
    static void Populate(Module& M);

    static ConstantInt* GetInt(LLVMContext& C, size_t numBits, size_t val)
    {
        IntegerType* type = IntegerType::get(C, numBits);
        return ConstantInt::get(type, val);
    }
    static ConstantInt* GetSizeT(LLVMContext& C, size_t val)
    {
        return GetInt(C, sizeof(size_t) * 8, val);
    }

    static Function* GetUpdateAutomatonDeterministic(Module& M);
    static Function* GetStartAutomaton(Module& M);
    static Function* GetEndAutomaton(Module& M);

    static StructType* GetStructType(StringRef name, ArrayRef<Type*> fields, Module& M, bool packed = true);

    static StructType* AutomatonFlagsTy;
    static StructType* AutomatonStateTy;
    static StructType* AutomatonTy;

    static StructType* EventFlagsTy;
    static StructType* EventStateTy;
    static StructType* EventTy;
    static bool populated;

  private:
    static void PopulateAutomatonTy(Module& M);
    static void PopulateEventTy(Module& M);
};