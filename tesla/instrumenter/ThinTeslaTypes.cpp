#include "ThinTeslaTypes.h"

#ifdef TESLA_PACK_STRUCTS
const bool TESLA_STRUCTS_PACKED = true;
#else
const bool TESLA_STRUCTS_PACKED = false;
#endif

bool TeslaTypes::populated = false;
StructType* TeslaTypes::AutomatonFlagsTy = nullptr;
StructType* TeslaTypes::AutomatonStateTy = nullptr;
StructType* TeslaTypes::AutomatonTy = nullptr;

StructType* TeslaTypes::EventFlagsTy = nullptr;
StructType* TeslaTypes::EventStateTy = nullptr;
StructType* TeslaTypes::EventTy = nullptr;

StructType* TeslaTypes::GetStructType(StringRef name, ArrayRef<Type*> fields, Module& M, bool packed)
{
    StructType* Ty = M.getTypeByName(name);

    if (Ty == nullptr)
        Ty = StructType::create(fields, name, packed);

    assert(Ty->getName() == name);
    return Ty;
}

void TeslaTypes::Populate(Module& M)
{
    if (!populated)
    {
        PopulateEventTy(M);
        PopulateAutomatonTy(M);
        populated = true;
    }
}

void TeslaTypes::PopulateEventTy(Module& M)
{
    LLVMContext& C = M.getContext();

    Type* VoidTy = Type::getVoidTy(C);
    IntegerType* CharTy = IntegerType::getInt8Ty(C);
    PointerType* CharPtrTy = PointerType::getUnqual(CharTy);
    PointerType* CharPtrPtrTy = PointerType::getUnqual(CharPtrTy);
    IntegerType* Int32Ty = IntegerType::getInt32Ty(C);
    IntegerType* SizeTTy = IntegerType::get(C, sizeof(size_t) * 8);
    IntegerType* Int8Ty = IntegerType::getInt8Ty(C);
    PointerType* Int8PtrTy = PointerType::getUnqual(Int8Ty);
    PointerType* VoidPtrTy = Int8PtrTy;
    PointerType* VoidPtrPtrTy = PointerType::getUnqual(VoidPtrTy);
    //IntegerType* IntPtrTy = DataLayout(&M).getIntPtrType(C);

    EventFlagsTy = GetStructType("TeslaEventFlags", {Int8Ty}, M, TESLA_STRUCTS_PACKED);
    EventStateTy = GetStructType("TeslaEventState", {VoidPtrTy, Int8PtrTy, Int8Ty}, M, TESLA_STRUCTS_PACKED);
    EventTy = GetStructType("TeslaEvent", {VoidPtrPtrTy, EventFlagsTy, SizeTTy, SizeTTy, EventStateTy}, M, TESLA_STRUCTS_PACKED);
}

void TeslaTypes::PopulateAutomatonTy(Module& M)
{
    LLVMContext& C = M.getContext();

    Type* VoidTy = Type::getVoidTy(C);
    IntegerType* CharTy = IntegerType::getInt8Ty(C);
    PointerType* CharPtrTy = PointerType::getUnqual(CharTy);
    PointerType* CharPtrPtrTy = PointerType::getUnqual(CharPtrTy);
    IntegerType* Int32Ty = IntegerType::getInt32Ty(C);
    IntegerType* SizeTTy = IntegerType::get(C, sizeof(size_t) * 8);
    IntegerType* Int8Ty = IntegerType::getInt8Ty(C);
    IntegerType* BoolTy = Int8Ty;
    PointerType* Int8PtrTy = PointerType::getUnqual(Int8Ty);
    PointerType* VoidPtrTy = Int8PtrTy;
    PointerType* VoidPtrPtrTy = PointerType::getUnqual(Int8PtrTy);

    PointerType* EventPtrTy = PointerType::getUnqual(EventTy);

    AutomatonFlagsTy = GetStructType("TeslaAutomatonFlags", {Int8Ty}, M, TESLA_STRUCTS_PACKED);
    AutomatonStateTy = GetStructType("TeslaAutomatonState", {SizeTTy, EventPtrTy, EventPtrTy, BoolTy, BoolTy}, M, TESLA_STRUCTS_PACKED);
    AutomatonTy = GetStructType("TeslaAutomaton", {VoidPtrPtrTy, AutomatonFlagsTy, SizeTTy, VoidPtrTy, AutomatonStateTy}, M, TESLA_STRUCTS_PACKED);
}

Function* TeslaTypes::GetUpdateAutomatonDeterministic(Module& M)
{
    auto& C = M.getContext();
    return (Function*)M.getOrInsertFunction("UpdateAutomatonDeterministic", FunctionType::get(Type::getVoidTy(C),
                                                                                              AutomatonTy->getPointerTo(), EventTy->getPointerTo()));
}

Function* TeslaTypes::GetUpdateAutomaton(Module& M)
{
    auto& C = M.getContext();
    return (Function*)M.getOrInsertFunction("UpdateAutomaton", FunctionType::get(Type::getVoidTy(C),
                                                                                 {AutomatonTy->getPointerTo(), EventTy->getPointerTo(),
                                                                                 Type::getInt8PtrTy(C)}, false));
}

Function* TeslaTypes::GetStartAutomaton(Module& M)
{
    auto& C = M.getContext();
    return (Function*)M.getOrInsertFunction("StartAutomaton", FunctionType::get(Type::getVoidTy(C),
                                                                                AutomatonTy->getPointerTo()));
}

Function* TeslaTypes::GetEndAutomaton(Module& M)
{
    auto& C = M.getContext();
    return (Function*)M.getOrInsertFunction("EndAutomaton", FunctionType::get(Type::getVoidTy(C),
                                                                              AutomatonTy->getPointerTo(), EventTy->getPointerTo()));
}