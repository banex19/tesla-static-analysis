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

    if (function == nullptr || function->isDeclaration())
        return;

    auto& C = M.getContext();

    BasicBlock* originalEntry = &function->getEntryBlock();
    BasicBlock* instrumentBlock = nullptr;
    Value* matchArray = nullptr;

    auto args = GetFunctionArguments(function);
    ConstantInt* totalArgSize = TeslaTypes::GetInt(C, 32, event.GetMatchDataSize());

    if (event.GetMatchDataSize() > 0) // Store runtime parameters to an array on the stack.
    {

        instrumentBlock = BasicBlock::Create(C, "instrumentation", function, originalEntry);
        IRBuilder<> builder{instrumentBlock};

        matchArray = builder.CreateAlloca(TeslaTypes::GetMatchType(C), totalArgSize, "args_array");

        size_t i = 0;
        for (auto& param : event.params)
        {
            if (param.isConstant)
                continue;

            Argument* arg = args[param.index];

            builder.CreateStore(builder.CreateCast(TeslaTypes::GetCastToInteger(arg->getType()), arg, TeslaTypes::GetMatchType(C)),
                                builder.CreateGEP(matchArray, TeslaTypes::GetInt(C, 32, i)));

            ++i;
        }

        /* Function* test = (Function*)M.getOrInsertFunction("testArray", FunctionType::get(Type::getVoidTy(C),
                                                                                         TeslaTypes::GetMatchType(C)->getPointerTo(),
                                                                                         Type::getInt32Ty(C)));
        builder.CreateCall(test, {builder.CreateBitCast(array, TeslaTypes::GetMatchType(C)->getPointerTo()), totalArgSize}); */

        if (!event.returnValue.exists)
        {
            Function* updateAutomaton = TeslaTypes::GetUpdateAutomaton(M);
            builder.CreateCall(updateAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event),
                                                 builder.CreateBitCast(matchArray, Type::getInt8PtrTy(C))});
        }

        builder.CreateBr(originalEntry);
    }

    if (instrumentBlock == nullptr)
    {
        instrumentBlock = BasicBlock::Create(C, "instrumentation", function, originalEntry);
        IRBuilder<> builder{instrumentBlock};

        if (!event.returnValue.exists)
        {
            Function* updateAutomaton = TeslaTypes::GetUpdateAutomatonDeterministic(M);
            builder.CreateCall(updateAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event)});
        }

        builder.CreateBr(originalEntry);
    }

    // Statically match constant arguments.
    std::vector<ThinTeslaParameter> constParams;

    for (auto& param : event.params)
    {
        if (param.isConstant)
            constParams.push_back(param);
    }

    BasicBlock* disableInstrumentationBlock = BasicBlock::Create(C, "no_instrumentation", function);
    IRBuilder<> builder{disableInstrumentationBlock};
    builder.CreateBr(originalEntry);

    if (constParams.size() > 0)
    {
        BasicBlock* thenBlock = instrumentBlock;

        for (int i = (int)constParams.size() - 1; i >= 0; --i)
        {
            size_t index = constParams[i].index;
            size_t val = constParams[i].constantValue;
            auto arg = args[index];

            BasicBlock* ifBlock = BasicBlock::Create(C, "if_" + std::to_string(index), function, &function->getEntryBlock());

            IRBuilder<> builder{ifBlock};
            Value* cond = builder.CreateICmpEQ(builder.CreateCast(TeslaTypes::GetCastToInteger(arg->getType()), arg, TeslaTypes::GetMatchType(C)),
                                               ConstantInt::get(TeslaTypes::GetMatchType(C), val));

            builder.CreateCondBr(cond, thenBlock, disableInstrumentationBlock);

            thenBlock = ifBlock;
        }
    }

    if (event.returnValue.exists)
    {
        IRBuilder<> builder{originalEntry->getFirstNonPHI()};
        auto instrumentationActive = builder.CreatePHI(IntegerType::get(C, 1), 2, "instrumentation_active");
        instrumentationActive->addIncoming(ConstantInt::get(IntegerType::get(C, 1), 1), instrumentBlock);
        instrumentationActive->addIncoming(ConstantInt::get(IntegerType::get(C, 1), 0), disableInstrumentationBlock);

        auto phiMatchArray = builder.CreatePHI(TeslaTypes::GetMatchType(C)->getPointerTo(), 2, "argsArray_PHI");
        if (matchArray != nullptr)
            phiMatchArray->addIncoming(matchArray, instrumentBlock);
        else
            phiMatchArray->addIncoming(ConstantPointerNull::get(TeslaTypes::GetMatchType(C)->getPointerTo()), instrumentBlock);

        phiMatchArray->addIncoming(ConstantPointerNull::get(TeslaTypes::GetMatchType(C)->getPointerTo()), disableInstrumentationBlock);

        BasicBlock* checkInstrumentationBlock = BasicBlock::Create(C, "check_instrumentation_return", function);
        builder.SetInsertPoint(checkInstrumentationBlock);

        auto exits = GetEveryExit(function);

        std::vector<Value*> retVals;
        for (auto exit : exits)
        {
            IRBuilder<> exitBuilder{exit};
            ReturnInst* returnInst = dyn_cast<ReturnInst>(exit->getTerminator());
            assert(returnInst != nullptr && returnInst->getReturnValue() != nullptr);

            retVals.push_back(returnInst->getReturnValue());
            exitBuilder.CreateBr(checkInstrumentationBlock);
            returnInst->eraseFromParent();
        }

        auto returnValue = builder.CreatePHI(function->getReturnType(), exits.size(), "returnValue");
        for (size_t i = 0; i < exits.size(); ++i)
        {
            returnValue->addIncoming(retVals[i], exits[i]);
        }

        BasicBlock* exitNormal = BasicBlock::Create(C, "exit_normal", function);
        IRBuilder<> exitBuilder{exitNormal};
        exitBuilder.CreateRet(returnValue);

        BasicBlock* instrumentExitBlock = BasicBlock::Create(C, "instrumentation_return", function);

        auto cond = builder.CreateICmpEQ(instrumentationActive, ConstantInt::get(IntegerType::get(C, 1), 1));
        builder.CreateCondBr(cond, instrumentExitBlock, exitNormal);

        builder.SetInsertPoint(instrumentExitBlock);

        if (!event.returnValue.isConstant) // Runtime return value, constant/runtime parameters.
        {
            builder.CreateStore(builder.CreateCast(TeslaTypes::GetCastToInteger(returnValue->getType()), returnValue, TeslaTypes::GetMatchType(C)),
                                builder.CreateGEP(phiMatchArray, TeslaTypes::GetInt(C, 32, event.GetMatchDataSize() - 1)));

            Function* updateAutomaton = TeslaTypes::GetUpdateAutomaton(M);
            builder.CreateCall(updateAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event),
                                                 builder.CreateBitCast(phiMatchArray, Type::getInt8PtrTy(C))});

            builder.CreateRet(returnValue);
        }
        else
        {
            BasicBlock* exitInstrument = BasicBlock::Create(C, "instrumentation_return_match", function);

            Value* cond = builder.CreateICmpEQ(builder.CreateCast(TeslaTypes::GetCastToInteger(returnValue->getType()), returnValue, TeslaTypes::GetMatchType(C)),
                                               ConstantInt::get(TeslaTypes::GetMatchType(C), event.returnValue.constantValue));
            builder.CreateCondBr(cond, exitInstrument, exitNormal);
            builder.SetInsertPoint(exitInstrument);

            if (event.GetMatchDataSize() > 0) // Constant return value, runtime paramenters.
            {
                Function* updateAutomaton = TeslaTypes::GetUpdateAutomaton(M);
                builder.CreateCall(updateAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event),
                                                     builder.CreateBitCast(phiMatchArray, Type::getInt8PtrTy(C))});
            }
            else // Constant return value, constant parameters.
            {
                Function* updateAutomaton = TeslaTypes::GetUpdateAutomatonDeterministic(M);
                builder.CreateCall(updateAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event)});
            }

            builder.CreateRet(returnValue);
        }
    }

    //   llvm::errs() << *function << "\n";
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

llvm::Value* ThinTeslaInstrumenter::GetVariable(llvm::Function* function, const std::string& varName)
{
    for (auto& arg : function->args())
    {
        if (arg.getName() == varName)
            return &arg;
    }

    for (auto& instr : function->getEntryBlock())
    {
        if (instr.getName() == varName)
            return &instr;
    }

    return nullptr;
}

void ThinTeslaInstrumenter::UpdateEventsWithParameters(llvm::Module& M, ThinTeslaAssertion& assertion, llvm::Instruction* insertPoint)
{
    Function* function = insertPoint->getParent()->getParent();

    IRBuilder<> builder(insertPoint);

    LLVMContext& C = M.getContext();

    for (auto event : assertion.events)
    {
        if (event->isDeterministic || event->GetMatchDataSize() == 0)
            continue;

        auto eventGlobal = GetEventGlobal(M, assertion, *event);
        auto matchArray = GetEventMatchArray(M, assertion, *event);

        auto params = event->GetParameters();
        auto retVal = event->GetReturnValue();
        if (retVal.exists && !retVal.isConstant)
            params.push_back(retVal);

        size_t i = 0;

        for (auto& param : params)
        {
            if (param.isConstant)
                continue;

            Value* var = GetVariable(function, param.varName);
            assert(var != nullptr && "Variable to match assertion has been optimized away by the compiler!");

            if (dyn_cast<AllocaInst>(var) != nullptr)
            {
                var = builder.CreateLoad(var);
            }

            builder.CreateStore(builder.CreateCast(TeslaTypes::GetCastToInteger(var->getType()), var, TeslaTypes::GetMatchType(C)),
                                builder.CreateGEP(builder.CreateBitCast(matchArray, TeslaTypes::GetMatchType(C)->getPointerTo()),
                                                  TeslaTypes::GetInt(C, 32, i)));

            ++i;
        }
    }
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaAssertionSite& event)
{
    Function* function = M.getFunction(event.functionName);

    Function* updateAutomaton = TeslaTypes::GetUpdateAutomatonDeterministic(M);

    if (function != nullptr && !function->isDeclaration())
    {
        assert(!event.IsInstrumented());
        assert(event.isDeterministic);
        assert(!event.IsStart() && !event.IsEnd());

        CallInst* callInst = GetTeslaAssertionInstr(function, event);
        if (callInst == nullptr)
            llvm::errs() << *function << "\n";
        assert(callInst != nullptr);

        if (!assertion.isDeterministic)
        {
            UpdateEventsWithParameters(M, assertion, callInst);
        }

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

    if (function != nullptr && !function->isDeclaration() && event.calleeInstrumentation)
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
                        if (callInst != nullptr && callInst->getCalledFunction()->getName() == event.functionName)
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

std::vector<BasicBlock*> ThinTeslaInstrumenter::GetEveryExit(Function* function)
{
    std::vector<BasicBlock*> exits;
    for (auto& block : *function)
    {
        if (block.getTerminator() != nullptr && dyn_cast<ReturnInst>(block.getTerminator()) != nullptr)
        {
            exits.push_back(&block);
        }
    }

    return exits;
}

void ThinTeslaInstrumenter::InstrumentEveryExit(llvm::Module& M, Function* function, ThinTeslaAssertion& assertion, ThinTeslaFunction& event)
{
    GlobalVariable* global = GetEventGlobal(M, assertion, event);

    Function* endAutomaton = TeslaTypes::GetEndAutomaton(M);

    auto exits = GetEveryExit(function);
    for (auto exit : exits)
    {
        IRBuilder<> builder(exit->getTerminator());
        builder.CreateCall(endAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event)});
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

    GlobalVariable* var = new GlobalVariable(M, eventsArrayTy, true, GlobalValue::WeakAnyLinkage, eventsArray, autID);

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
        GlobalVariable* eventsArrayGlobal = new GlobalVariable(M, eventsArrayTy, true, GlobalValue::WeakAnyLinkage, eventsArray, eventID + "_succ");
        eventsArrayPtr = ConstantExpr::getBitCast(eventsArrayGlobal, VoidPtrPtrTy);
    }

    TeslaEventFlags flags;
    flags.isOR = event.isOR;
    flags.isOptional = event.isOptional;
    flags.isDeterministic = event.isDeterministic;
    flags.isAssertion = event.IsAssertion();
    flags.isBeforeAssertion = event.isBeforeAssertion;
    flags.isEnd = event.successors.size() == 0;
    Constant* cFlags = ConstantStruct::get(TeslaTypes::EventFlagsTy, TeslaTypes::GetInt(C, 8, *(uint8_t*)(&flags)));

    Constant* matchArray = ConstantPointerNull::get(Int8PtrTy);

    size_t matchDataSize = event.GetMatchDataSize();
    if (matchDataSize > 0)
    {
        GlobalVariable* matchArrayGlobal = GetEventMatchArray(M, assertion, event);

        matchArray = ConstantExpr::getBitCast(matchArrayGlobal, Int8PtrTy);
    }

    Constant* state = ConstantStruct::get(TeslaTypes::EventStateTy, ConstantPointerNull::get(Int8PtrTy), matchArray,
                                          TeslaTypes::GetInt(C, 8, matchDataSize));

    Constant* init = ConstantStruct::get(TeslaTypes::EventTy, eventsArrayPtr, cFlags,
                                         TeslaTypes::GetSizeT(C, event.successors.size()), TeslaTypes::GetSizeT(C, event.id), state);

    GlobalVariable* var = new GlobalVariable(M, TeslaTypes::EventTy, true,
                                             GlobalValue::WeakAnyLinkage, init, eventID);

    var->setConstant(false); // This variable will be manipulated by libtesla.

    return var;
}

GlobalVariable* ThinTeslaInstrumenter::GetEventMatchArray(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaEvent& event)
{
    std::string id = GetEventID(assertion, event) + "_match";
    GlobalVariable* old = M.getGlobalVariable(id);
    if (old != nullptr)
        return old;

    LLVMContext& C = M.getContext();

    ArrayType* matchArrayTy = ArrayType::get(TeslaTypes::GetMatchType(C), event.GetMatchDataSize());
    Constant* matchArray = ConstantArray::get(matchArrayTy, TeslaTypes::GetMatchValue(C, 0));
    GlobalVariable* matchArrayGlobal = new GlobalVariable(M, matchArrayTy, true, GlobalValue::WeakAnyLinkage, matchArray, id);

    matchArrayGlobal->setConstant(false); // This array will be populated at runtime.

    return matchArrayGlobal;
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
                                          TeslaTypes::GetBoolValue(C, 0),
                                          TeslaTypes::GetBoolValue(C, 0),
                                          TeslaTypes::GetBoolValue(C, 0));

    Constant* init = ConstantStruct::get(TeslaTypes::AutomatonTy, eventsArrayPtr, cFlags,
                                         TeslaTypes::GetSizeT(C, assertion.events.size()),
                                         ConstantExpr::getBitCast(GetStringGlobal(M, autID, autID + "_name"), Int8PtrTy),
                                         state);

    GlobalVariable* var = new GlobalVariable(M, TeslaTypes::AutomatonTy, true,
                                             GlobalValue::WeakAnyLinkage, init, autID);

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
                                             GlobalValue::WeakAnyLinkage, strConst, globalID);

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