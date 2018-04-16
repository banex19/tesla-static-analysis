#include "ThinTeslaInstrumenter.h"

using namespace llvm;

char ThinTeslaInstrumenter::ID = 0;

std::set<std::string> CollectModuleFunctions(Module& M)
{
    std::set<std::string> funs;

    for (auto& F : M)
    {
        funs.insert(F.getName());
    }

    return funs;
}

bool ThinTeslaInstrumenter::runOnModule(llvm::Module& M)
{
    auto moduleFunctions = CollectModuleFunctions(M);

    bool instrumented = false;

    for (auto& assertion : assertions)
    {
        bool needsInstrumentation = (M.getName() == assertion.assertionFilename);

        if (!needsInstrumentation)
        {
            for (auto& affectedFunction : assertion.affectedFunctions)
            {
                if (moduleFunctions.find(affectedFunction) != moduleFunctions.end())
                {
                    needsInstrumentation = true;
                    break;
                }
            }
        }

        if (needsInstrumentation)
        {
            GlobalVariable* automaton = GetAutomatonGlobal(M, assertion);

            Instrument(M, assertion);
            instrumented = true;
        }
    }

    return instrumented;
}

void ThinTeslaInstrumenter::Instrument(llvm::Module& M, ThinTeslaAssertion& assertion)
{
    for (auto event : assertion.events)
    {
        event->Accept(M, assertion, *this);
    }
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaEvent& event)
{
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaParametricFunction& event)
{
    GlobalVariable* global = GetEventGlobal(M, assertion, event);
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaAssertionSite& event)
{
    GlobalVariable* global = GetEventGlobal(M, assertion, event);
}

Instruction* GetFirstInstruction(llvm::Function* function)
{
    BasicBlock& block = function->getEntryBlock();

    for (auto& I : block)
    {
        return &I;
    }

    return nullptr;
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaFunction& event)
{
    Function* function = M.getFunction(event.functionName);
    GlobalVariable* global = GetEventGlobal(M, assertion, event);

    Function* debugEvent = (Function*)M.getOrInsertFunction("DebugEvent",
                                                            FunctionType::get(Type::getVoidTy(M.getContext()), TeslaTypes::EventTy->getPointerTo()));

    Function* debugAutomaton = (Function*)M.getOrInsertFunction("DebugAutomaton",
                                                                FunctionType::get(Type::getVoidTy(M.getContext()), TeslaTypes::AutomatonTy->getPointerTo()));

    if (!function->isDeclaration())
    {
        if (event.isDeterministic)
        {
            IRBuilder<> builder(GetFirstInstruction(function));
            builder.CreateCall(debugEvent, global);
            builder.CreateCall(debugAutomaton, GetAutomatonGlobal(M, assertion));

            // llvm::errs() << *function << "\n";
        }
    }
}

GlobalVariable* ThinTeslaInstrumenter::GetEventsArray(llvm::Module& M, ThinTeslaAssertion& assertion)
{
    std::string autID = GetAutomatonID(assertion) + "_all";
    GlobalVariable* old = M.getGlobalVariable(autID);
    if (old != nullptr)
        return old;

    auto& C = M.getContext();
    PointerType* Int8PtrTy = PointerType::getUnqual(IntegerType::getInt8Ty(C));
    PointerType* VoidPtrPtrTy = PointerType::getUnqual(Int8PtrTy);

    std::vector<Constant*> events;
    for (auto event : assertion.events)
    {
        events.push_back(GetEventGlobal(M, assertion, *event));
    }

    ArrayType* eventsArrayTy = ArrayType::get(TeslaTypes::EventTy->getPointerTo(), assertion.events.size());
    Constant* eventsArray = ConstantArray::get(eventsArrayTy, events);

    GlobalVariable* var = new GlobalVariable(M, eventsArrayTy, true, GlobalValue::ExternalLinkage, eventsArray, autID);

    return var;
}

GlobalVariable* ThinTeslaInstrumenter::GetEventGlobal(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaEvent& event)
{
    std::string eventID = GetEventID(assertion, event);
    GlobalVariable* old = M.getGlobalVariable(eventID);
    if (old != nullptr)
        return old;

    auto& C = M.getContext();
    PointerType* Int8PtrTy = PointerType::getUnqual(IntegerType::getInt8Ty(C));
    PointerType* VoidPtrPtrTy = PointerType::getUnqual(Int8PtrTy);

    Constant* eventsArrayPtr = ConstantPointerNull::get(VoidPtrPtrTy);

    if (event.successors.size() > 0)
    {
        std::vector<Constant*> successors;
        for (auto succ : event.successors)
        {
            successors.push_back(GetEventGlobal(M, assertion, *succ));
        }

        ArrayType* eventsArrayTy = ArrayType::get(TeslaTypes::EventTy->getPointerTo(), successors.size());
        Constant* eventsArray = ConstantArray::get(eventsArrayTy, successors);
        GlobalVariable* eventsArrayGlobal = new GlobalVariable(M, eventsArrayTy, true, GlobalValue::ExternalLinkage, eventsArray, eventID + "_succ");
        eventsArrayPtr = ConstantExpr::getBitCast(eventsArrayGlobal, VoidPtrPtrTy);
    }

    TeslaEventFlags flags;
    flags.isOR = event.isOR;
    flags.isOptional = event.isOptional;
    flags.isDeterministic = event.isDeterministic;
    flags.isEnd = event.successors.size() == 0;
    Constant* cFlags = ConstantStruct::get(TeslaTypes::EventFlagsTy, TeslaTypes::GetInt(C, 8, *(uint8_t*)(&flags)));

    Constant* state = ConstantStruct::get(TeslaTypes::EventStateTy, ConstantPointerNull::get(Int8PtrTy), ConstantPointerNull::get(Int8PtrTy));

    Constant* init = ConstantStruct::get(TeslaTypes::EventTy, eventsArrayPtr, cFlags,
                                         TeslaTypes::GetSizeT(C, event.successors.size()), TeslaTypes::GetSizeT(C, event.id), state);

    GlobalVariable* var = new GlobalVariable(M, TeslaTypes::EventTy, true,
                                             GlobalValue::ExternalLinkage, init, eventID);

    var->setConstant(false); // This variable will be manipulated by libtesla.

    //  llvm::errs() << "Event " << event.id << " has " << event.successors.size() << " successors\n";

    return var;
}

GlobalVariable* ThinTeslaInstrumenter::GetAutomatonGlobal(llvm::Module& M, ThinTeslaAssertion& assertion)
{
    std::string autID = GetAutomatonID(assertion);
    GlobalVariable* old = M.getGlobalVariable(autID);
    if (old != nullptr)
        return old;

    auto& C = M.getContext();
    PointerType* Int8PtrTy = PointerType::getUnqual(IntegerType::getInt8Ty(C));
    PointerType* VoidPtrPtrTy = PointerType::getUnqual(Int8PtrTy);

    Constant* eventsArrayPtr = GetEventsArray(M, assertion);
    eventsArrayPtr = ConstantExpr::getBitCast(eventsArrayPtr, VoidPtrPtrTy);

    TeslaAutomatonFlags flags;
    flags.isDeterministic = assertion.isDeterministic;
    Constant* cFlags = ConstantStruct::get(TeslaTypes::AutomatonFlagsTy, TeslaTypes::GetInt(C, 8, *(uint8_t*)(&flags)));

    Constant* state = ConstantStruct::get(TeslaTypes::AutomatonStateTy,
                                          TeslaTypes::GetSizeT(C, 0),
                                          ConstantPointerNull::get(TeslaTypes::EventTy->getPointerTo()),
                                          ConstantPointerNull::get(TeslaTypes::EventTy->getPointerTo()),
                                          TeslaTypes::GetInt(C, 8, 0));

    Constant* init = ConstantStruct::get(TeslaTypes::AutomatonTy, eventsArrayPtr, cFlags,
                                         TeslaTypes::GetSizeT(C, assertion.events.size()),
                                         ConstantExpr::getBitCast(GetStringGlobal(M, autID, autID + "_name"), Int8PtrTy),
                                         state);

    GlobalVariable* var = new GlobalVariable(M, TeslaTypes::AutomatonTy, true,
                                             GlobalValue::ExternalLinkage, init, autID);

    return var;
}

GlobalVariable* ThinTeslaInstrumenter::GetStringGlobal(llvm::Module& M, const std::string& str, const std::string& globalID)
{
    GlobalVariable* old = M.getGlobalVariable(globalID);
    if (old != nullptr)
        return old;

    IntegerType* CharTy = IntegerType::getInt8Ty(M.getContext());
    PointerType* CharPtrTy = PointerType::getUnqual(CharTy);

    Constant* strConst = ConstantDataArray::getString(M.getContext(), str);
    GlobalVariable* var = new GlobalVariable(M, strConst->getType(), true,
                                             GlobalValue::ExternalLinkage, strConst, globalID);

    return var;
}

std::string ThinTeslaInstrumenter::GetAutomatonID(ThinTeslaAssertion& assertion)
{
    return assertion.assertionFilename + "_" + std::to_string(assertion.assertionLine) + "_" +
           std::to_string(assertion.assertionCounter);
}

std::string ThinTeslaInstrumenter::GetEventID(ThinTeslaAssertion& assertion, ThinTeslaEvent& event)
{
    return assertion.assertionFilename + "_" + std::to_string(assertion.assertionLine) + "_" +
           std::to_string(assertion.assertionCounter) + "_" + std::to_string(event.id);
}