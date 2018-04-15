#include "ThinTeslaTypes.h"

bool TeslaTypes::populated = false;

StructType* TeslaTypes::GetStructType(StringRef name, ArrayRef<Type*> fields, Module& M)
{
    StructType* Ty = M.getTypeByName(name);

    if (Ty == nullptr)
        Ty = StructType::create(fields, name);

    assert(Ty->getName() == name);
    return Ty;
}

void TeslaTypes::Populate(Module& M)
{
    PopulateEventTy(M);
    PopulateAutomatonTy(M);
    populated = true;
}

void TeslaTypes::PopulateEventTy(Module& M)
{
    LLVMContext& C = M.getContext();

    Type* VoidTy = Type::getVoidTy(C);
    PointerType* VoidPtrTy = PointerType::getUnqual(VoidTy);
    PointerType* VoidPtrPtrTy = PointerType::getUnqual(VoidPtrTy);
    IntegerType* CharTy = IntegerType::getInt8Ty(C);
    PointerType* CharPtrTy = PointerType::getUnqual(CharTy);
    PointerType* CharPtrPtrTy = PointerType::getUnqual(CharPtrTy);
    IntegerType* Int32Ty = IntegerType::getInt32Ty(C);
    IntegerType* SizeTTy = IntegerType::get(C, sizeof(size_t) * 8);
    IntegerType* Int8Ty = IntegerType::getInt8Ty(C);
    PointerType* Int8PtrTy = PointerType::getUnqual(Int8Ty);
    //  IntegerType* IntPtrTy = DataLayout(&M).getIntPtrType(C);

    EventFlagsTy = GetStructType("TeslaEventFlags", {Int8Ty}, M);
    EventStateTy = GetStructType("TeslaEventState", {VoidPtrTy, Int8PtrTy}, M);
    EventTy = GetStructType("TeslaEvent", {VoidPtrPtrTy, EventFlagsTy, SizeTTy, SizeTTy, EventStateTy}, M);
}

void TeslaTypes::PopulateAutomatonTy(Module& M)
{
    LLVMContext& C = M.getContext();

    Type* VoidTy = Type::getVoidTy(C);
    PointerType* VoidPtrTy = PointerType::getUnqual(VoidTy);
    PointerType* VoidPtrPtrTy = PointerType::getUnqual(VoidPtrTy);
    IntegerType* CharTy = IntegerType::getInt8Ty(C);
    PointerType* CharPtrTy = PointerType::getUnqual(CharTy);
    PointerType* CharPtrPtrTy = PointerType::getUnqual(CharPtrTy);
    IntegerType* Int32Ty = IntegerType::getInt32Ty(C);
    IntegerType* SizeTTy = IntegerType::get(C, sizeof(size_t) * 8);
    IntegerType* Int8Ty = IntegerType::getInt8Ty(C);
    IntegerType* BoolTy = Int8Ty;
    PointerType* Int8PtrTy = PointerType::getUnqual(Int8Ty);

    PointerType* EventPtrTy = PointerType::getUnqual(EventTy);
    PointerType* EventPtrPtrTy = PointerType::getUnqual(EventPtrTy);

    AutomatonFlagsTy = GetStructType("TeslaAutomatonFlags", {Int8Ty}, M);
    AutomatonStateTy = GetStructType("TeslaAutomatonState", {SizeTTy, EventPtrTy, EventPtrTy, BoolTy}, M);
    AutomatonTy = GetStructType("TeslaAutomaton", {EventPtrPtrTy, AutomatonFlagsTy, SizeTTy, CharPtrTy, AutomatonStateTy}, M);
}
