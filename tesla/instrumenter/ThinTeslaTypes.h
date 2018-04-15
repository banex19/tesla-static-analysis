#include "../../libtesla/c_thintesla/TeslaState.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#include <iostream>

using namespace llvm;

class TeslaTypes
{
  public:
    static void Populate(Module& M);

    static StructType* GetStructType(StringRef name, ArrayRef<Type*> fields, Module& M);

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