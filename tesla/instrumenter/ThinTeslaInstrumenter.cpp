#include "ThinTeslaInstrumenter.h"
#include "Names.h"

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
        bool needsInstrumentation = (GetFilenameFromPath(M.getName()) == GetFilenameFromPath(assertion.assertionFilename));

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
        event.get()->Accept(M, assertion, *this);
    }
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaEvent& event)
{
    assert(false);
}

std::vector<llvm::Argument*> ThinTeslaInstrumenter::GetFunctionArguments(llvm::Function* function)
{
    std::vector<llvm::Argument*> args;
    for (auto& arg : function->args())
    {
        args.push_back(&arg);
    }

    return args;
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaParametricFunction& event)
{
    Function* function = M.getFunction(event.functionName);

    auto& C = M.getContext();

    if (function->isDeclaration())
        return;

    BasicBlock* originalEntry = &function->getEntryBlock();
    BasicBlock* instrumentBlock = nullptr;

    auto args = GetFunctionArguments(function);

    if (event.GetMatchDataSize() > 0) // Store runtime parameters to an array on the stack.
    {
        ConstantInt* totalArgSize = TeslaTypes::GetInt(C, 32, event.GetMatchDataSize());

        instrumentBlock = BasicBlock::Create(C, "instrumentation", function, originalEntry);
        IRBuilder<> builder{instrumentBlock};

        auto array = builder.CreateAlloca(TeslaTypes::GetMatchType(C), totalArgSize, "args_array");

        size_t i = 0;
        for (auto& param : event.params)
        {
            if (param.isConstant)
                continue;

            Argument* arg = args[param.index];

            builder.CreateStore(builder.CreateCast(TeslaTypes::GetCastToInteger(arg->getType()), arg, TeslaTypes::GetMatchType(C)),
                                builder.CreateGEP(array, TeslaTypes::GetInt(C, 32, i)));

            ++i;
        }

        Function* test = (Function*)M.getOrInsertFunction("testArray", FunctionType::get(Type::getVoidTy(C),
                                                                                         TeslaTypes::GetMatchType(C)->getPointerTo(),
                                                                                         Type::getInt32Ty(C)));
        builder.CreateCall(test, {builder.CreateBitCast(array, TeslaTypes::GetMatchType(C)->getPointerTo()), totalArgSize});

        builder.CreateBr(originalEntry);
    }

    if (!function->isDeclaration()) // Statically match constant arguments.
    {
        std::vector<ThinTeslaParameter> constParams;

        for (auto& param : event.params)
        {
            if (param.isConstant)
                constParams.push_back(param);
        }

        if (constParams.size() > 0)
        {
            BasicBlock* thenBlock = instrumentBlock;

            for (int i = (int)constParams.size() - 1; i >= 0; --i)
            {
                size_t index = constParams[i].index;
                size_t val = constParams[i].constantValue;
                auto arg = args[index];

                BasicBlock* ifBlock = BasicBlock::Create(C, "if_" + std::to_string(index), function, thenBlock);

                IRBuilder<> builder{ifBlock};
                Value* cond = builder.CreateICmpEQ(builder.CreateCast(TeslaTypes::GetCastToInteger(arg->getType()), arg, TeslaTypes::GetMatchType(C)),
                                                   ConstantInt::get(TeslaTypes::GetMatchType(C), val));

                builder.CreateCondBr(cond, thenBlock, originalEntry);

                thenBlock = ifBlock;
            }
        }
    }
}

llvm::CallInst* ThinTeslaInstrumenter::GetTeslaAssertionInstr(llvm::Function* function, ThinTeslaAssertionSite& event)
{
    for (auto& block : *function)
    {
        for (auto& inst : block)
        {
            CallInst* callInst = dyn_cast<CallInst>(&inst);

            if (callInst != nullptr)
            {
                if (callInst->getCalledFunction()->getName() == tesla::INLINE_ASSERTION)
                {
                    ConstantInt* line = dyn_cast<ConstantInt>(callInst->getOperand(2));
                    ConstantInt* counter = dyn_cast<ConstantInt>(callInst->getOperand(3));

                    assert(line != nullptr && counter != nullptr);

                    if ((line->getLimitedValue(std::numeric_limits<int32_t>::max()) == event.line) &&
                        (counter->getLimitedValue(std::numeric_limits<int32_t>::max()) == event.counter))
                        return callInst;
                }
            }
        }
    }

    return nullptr;
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaAssertionSite& event)
{
    Function* function = M.getFunction(event.functionName);

    Function* updateAutomaton = TeslaTypes::GetUpdateAutomatonDeterministic(M);

    if (!function->isDeclaration())
    {
        assert(!event.IsInstrumented());
        assert(event.isDeterministic);
        assert(!event.IsStart() && !event.IsEnd());

        CallInst* callInst = GetTeslaAssertionInstr(function, event);
        if (callInst == nullptr)
            llvm::errs() << *function << "\n";
        assert(callInst != nullptr);

        IRBuilder<> builder(callInst);
        builder.CreateCall(updateAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event)});

        callInst->eraseFromParent();

        event.SetInstrumented();
    }
}

Instruction* ThinTeslaInstrumenter::GetFirstInstruction(llvm::Function* function)
{
    BasicBlock& block = function->getEntryBlock();

    for (auto& I : block)
    {
        return &I;
    }

    return nullptr;
}

void ThinTeslaInstrumenter::InstrumentInstruction(llvm::Module& M, llvm::Instruction* instr, ThinTeslaAssertion& assertion, ThinTeslaFunction& event)
{
    Function* updateAutomaton = TeslaTypes::GetUpdateAutomatonDeterministic(M);
    Function* startAutomaton = TeslaTypes::GetStartAutomaton(M);
    Function* endAutomaton = TeslaTypes::GetEndAutomaton(M);

    GlobalVariable* global = GetEventGlobal(M, assertion, event);

    IRBuilder<> builder(M.getContext());

    if (event.IsEnd())
    {
        assert(!event.calleeInstrumentation);
        builder.SetInsertPoint(instr->getNextNode());
    }
    else
    {
        builder.SetInsertPoint(instr);
    }

    if (event.IsStart())
    {
        builder.CreateCall(startAutomaton, {GetAutomatonGlobal(M, assertion)});
    }
    else if (event.IsEnd())
    {
        builder.CreateCall(endAutomaton, {GetAutomatonGlobal(M, assertion), global});
    }
    else
    {
        builder.CreateCall(updateAutomaton, {GetAutomatonGlobal(M, assertion), global});
    }
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaFunction& event)
{
    Function* function = M.getFunction(event.functionName);

    if (!function->isDeclaration() && event.calleeInstrumentation)
    {
        assert(!event.IsInstrumented());

        if (event.isDeterministic)
        {
            if (event.IsEnd())
            {
                InstrumentEveryExit(M, function, assertion, event);
            }
            else
            {
                InstrumentInstruction(M, GetFirstInstruction(function), assertion, event);
            }
        }

        event.SetInstrumented();
    }

    if (!event.calleeInstrumentation) // We must instrument every call to the event function.
    {
        auto functions = CollectModuleFunctions(M);

        for (auto& fName : functions)
        {
            Function* f = M.getFunction(fName);
            if (f != nullptr && !f->isDeclaration())
            {
                for (auto& block : *f)
                {
                    for (auto& instr : block)
                    {
                        CallInst* callInst = dyn_cast<CallInst>(&instr);
                        if (callInst != nullptr && callInst->getCalledFunction() == function)
                        {
                            IRBuilder<> builder(callInst);
                            InstrumentInstruction(M, callInst, assertion, event);
                        }
                    }
                }
            }
        }
    }
}

void ThinTeslaInstrumenter::InstrumentEveryExit(llvm::Module& M, Function* function, ThinTeslaAssertion& assertion, ThinTeslaFunction& event)
{
    GlobalVariable* global = GetEventGlobal(M, assertion, event);

    Function* endAutomaton = TeslaTypes::GetEndAutomaton(M);

    for (auto& block : *function)
    {
        //  llvm::errs() << "Terminator: " << *block.getTerminator() << "\n";
        if (dyn_cast<ReturnInst>(block.getTerminator()) != nullptr)
        {
            IRBuilder<> builder(block.getTerminator());
            builder.CreateCall(endAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event)});
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

    Constant* matchArray = ConstantPointerNull::get(Int8PtrTy);

    size_t matchDataSize = event.GetMatchDataSize();
    if (matchDataSize > 0)
    {
        ArrayType* matchArrayTy = ArrayType::get(TeslaTypes::GetMatchType(C), matchDataSize);
        Constant* matchArray = ConstantArray::get(matchArrayTy, TeslaTypes::GetMatchValue(C, 0));
        GlobalVariable* matchArrayGlobal = new GlobalVariable(M, matchArrayTy, true, GlobalValue::ExternalLinkage, matchArray, eventID + "_match");

        matchArrayGlobal->setConstant(false); // This array will be populated at runtime.

        matchArray = ConstantExpr::getBitCast(matchArrayGlobal, Int8PtrTy);
    }

    Constant* state = ConstantStruct::get(TeslaTypes::EventStateTy, ConstantPointerNull::get(Int8PtrTy), matchArray);

    Constant* init = ConstantStruct::get(TeslaTypes::EventTy, eventsArrayPtr, cFlags,
                                         TeslaTypes::GetSizeT(C, event.successors.size()), TeslaTypes::GetSizeT(C, event.id), state);

    GlobalVariable* var = new GlobalVariable(M, TeslaTypes::EventTy, true,
                                             GlobalValue::ExternalLinkage, init, eventID);

    var->setConstant(false); // This variable will be manipulated by libtesla.

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
                                          TeslaTypes::GetInt(C, 8, 0),
                                          TeslaTypes::GetInt(C, 8, 0));

    Constant* init = ConstantStruct::get(TeslaTypes::AutomatonTy, eventsArrayPtr, cFlags,
                                         TeslaTypes::GetSizeT(C, assertion.events.size()),
                                         ConstantExpr::getBitCast(GetStringGlobal(M, autID, autID + "_name"), Int8PtrTy),
                                         state);

    GlobalVariable* var = new GlobalVariable(M, TeslaTypes::AutomatonTy, true,
                                             GlobalValue::ExternalLinkage, init, autID);

    var->setConstant(false);

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

    return var; // This variable will be manipulated by libtesla.
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

std::string ThinTeslaInstrumenter::GetFilenameFromPath(const std::string& path)
{
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos || (pos == (path.size() - 1)))
        return path;

    return path.substr(pos + 1);
}