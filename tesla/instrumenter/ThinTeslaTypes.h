#pragma once

#include "../../libtesla/c_thintesla/TeslaState.h"
#include "llvm/Support/raw_ostream.h"

#include "ThinTeslaAssertion.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

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
    static Type* GetMatchType(LLVMContext& C)
    {
        return Type::getInt64Ty(C);
    }
    static Constant* GetMatchValue(LLVMContext& C, size_t val)
    {
        return ConstantInt::get(GetMatchType(C), val);
    }

    static llvm::Instruction::CastOps GetCastToInteger(Type* type)
    {
        if (type->isIntegerTy())
            return llvm::Instruction::CastOps::SExt;
        else if (type->isPointerTy())
            return llvm::Instruction::CastOps::PtrToInt;
        else if (type->isFloatingPointTy())
            return llvm::Instruction::CastOps::FPToSI;
        else
        {
            llvm::errs() << *type << "\n";
            assert(false && "Invalid type to cast to integer");
        }
        return llvm::Instruction::CastOps::BitCast;
    }

    static Function* GetUpdateAutomatonDeterministic(Module& M);
    static Function* GetUpdateAutomaton(Module& M);
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