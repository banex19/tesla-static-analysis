#include "ThinTeslaInstrumenter.h"
#include "Names.h"

#include "../../libtesla/c_thintesla/TeslaLogic.h"

using namespace llvm;

const bool THREAD_LOCAL = false;

const GlobalValue::LinkageTypes DEFAULT_LINKAGE = GlobalValue::LinkOnceODRLinkage;

char ThinTeslaInstrumenter::ID = 0;

void OutFunction(llvm::Function* f)
{
    llvm::errs() << "Function " << f->getName() << "\n"
                 << *f << "\n";
}

std::set<std::string> ThinTeslaInstrumenter::CollectModuleFunctions(llvm::Module& M)
{
    std::set<std::string> funs;

    for (auto& F : M)
    {
        funs.insert(F.getName());
    }

    return funs;
}

std::set<std::string> ThinTeslaInstrumenter::GetFunctionsInstrumentedMoreThanOnce()
{
    std::map<std::string, size_t> histogram;

    for (auto& assertion : assertions)
    {
        for (auto event : assertion.events)
        {
            if (event->NeedsParametricInstrumentation())
            {
                histogram[event->GetInstrumentationTarget()] += 1;
            }
        }
    }

    std::set<std::string> funs;

    for (auto& fun : histogram)
    {
        if (fun.second > 1)
        {
            funs.insert(fun.first);
        }
    }

    return funs;
}

bool ThinTeslaInstrumenter::runOnModule(llvm::Module& M)
{
    auto moduleFunctions = CollectModuleFunctions(M);

    bool instrumented = false;

    std::vector<ThinTeslaAssertion*> toBeInstrumented;

    multipleInstrumentedFunctions = GetFunctionsInstrumentedMoreThanOnce();

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
            toBeInstrumented.push_back(&assertion);
        }
    }

    for (auto& assertion : toBeInstrumented)
    {
        GlobalVariable* automaton = GetAutomatonGlobal(M, *assertion);

      //  llvm::errs() << "Instrumenting assertion " << GetAutomatonID(*assertion) << "\n";
        Instrument(M, *assertion);
        instrumented = true;
    }

    multipleInstrumentedFunctions.clear();

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

Function* ThinTeslaInstrumenter::BuildInstrumentationCheck(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaParametricFunction& event)
{
    LLVMContext& C = M.getContext();

    Function* baseFunc = M.getFunction(event.functionName);
    auto args = GetFunctionArguments(baseFunc);

    // Mapping from old arg index to new arg index.
    std::map<size_t, size_t> newIndices;

    std::vector<ThinTeslaParameter> allParams = event.params;

    std::vector<Type*> argTypes;
    size_t newIndex = 0;
    for (auto& param : allParams)
    {
        argTypes.push_back(args[param.index]->getType());
        newIndices[param.index] = newIndex;
        newIndex++;
    }

    if (event.returnValue.exists)
    {
        argTypes.push_back(baseFunc->getReturnType());
        newIndices[event.returnValue.index] = newIndex;
        allParams.push_back(event.returnValue);
    }

    Function* function = nullptr;
    function = (Function*)M.getOrInsertFunction(event.functionName + "_instrumented_" + GetEventID(assertion, event),
                                                FunctionType::get(Type::getVoidTy(C), argTypes, false));

    auto newArgs = GetFunctionArguments(function);

    BasicBlock* instrument = BasicBlock::Create(C, "instrumentation", function);
    BasicBlock* exit = BasicBlock::Create(C, "exit", function);
    IRBuilder<> builder{exit};
    builder.CreateRetVoid();

    BasicBlock* ifTrue = instrument;

    size_t constIndex = 0;

    // First match all constant parameters.
    for (auto& param : allParams)
    {
        if (!param.isConstant)
            continue;

        BasicBlock* ifBlock = BasicBlock::Create(C, "if_" + std::to_string(constIndex), function, ifTrue);

        auto arg = newArgs[newIndices[param.index]];

        builder.SetInsertPoint(ifBlock);
        auto cond = builder.CreateICmpEQ(arg, ConstantInt::get(arg->getType(), param.constantValue));
        builder.CreateCondBr(cond, ifTrue, exit);

        ifTrue = ifBlock;
        constIndex++;
    }

    // Now collect all runtime values if needed.
    builder.SetInsertPoint(instrument);

    if (event.GetMatchDataSize() > 0)
    {
        ConstantInt* totalArgSize = TeslaTypes::GetInt(C, 32, event.GetMatchDataSize());
        auto matchArray = builder.CreateAlloca(TeslaTypes::GetMatchType(C), totalArgSize, "args_array");

        size_t i = 0;
        for (auto& param : allParams)
        {
            if (param.isConstant)
                continue;

            Argument* arg = newArgs[newIndices[param.index]];

            builder.CreateStore(builder.CreateCast(TeslaTypes::GetCastToInteger(arg->getType()), arg, TeslaTypes::GetMatchType(C)),
                                builder.CreateGEP(matchArray, TeslaTypes::GetInt(C, 32, i)));

            ++i;
        }

        Function* updateAutomaton = TeslaTypes::GetUpdateAutomaton(M);
        builder.CreateCall(updateAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event),
                                             builder.CreateBitCast(matchArray, Type::getInt8PtrTy(C))});
    }
    else // No runtime values, all checks have been done statically.
    {
        Function* updateAutomaton = TeslaTypes::GetUpdateAutomatonDeterministic(M);
        builder.CreateCall(updateAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event)});
    }

    builder.CreateBr(exit);

    return function;
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaParametricFunction& event)
{
    Function* function = M.getFunction(event.functionName);

    if (event.calleeInstrumentation)
    {
        if (function == nullptr || function->isDeclaration())
            return;

        Function* instrCheck = BuildInstrumentationCheck(M, assertion, event);

        auto args = GetFunctionArguments(function);

        std::vector<Value*> callArgs;
        for (auto& param : event.params)
        {
            callArgs.push_back(args[param.index]);
        }

        if (!event.returnValue.exists) // Instrument at entry.
        {
            BasicBlock* entryBlock = &function->getEntryBlock();
            IRBuilder<> builder{entryBlock->getFirstNonPHI()};
            builder.CreateCall(instrCheck, callArgs);
        }
        else // Instrument every exit.
        {
            auto exits = GetEveryExit(function);
            for (auto& exit : exits)
            {
                auto retVal = dyn_cast<ReturnInst>(exit->getTerminator())->getReturnValue();
                assert(retVal != nullptr);
                callArgs.push_back(retVal);

                IRBuilder<> builder{exit->getTerminator()};
                builder.CreateCall(instrCheck, callArgs);
            }
        }
    }
    else // We must instrument every call to the event function.
    {
        Function* instrCheck = BuildInstrumentationCheck(M, assertion, event);

        auto callInsts = GetAllCallsToFunction(M, event.functionName);

        for (auto callInst : callInsts)
        {
            std::vector<Value*> callArgs;
            for (auto& param : event.params)
            {
                callArgs.push_back(callInst->getArgOperand(param.index));
            }

            if (!event.returnValue.exists) // Instrument before call.
            {
                IRBuilder<> builder(callInst);
                builder.CreateCall(instrCheck, callArgs);
            }
            else // Instrument after call.
            {
                callArgs.push_back(callInst);
                IRBuilder<> builder{callInst->getNextNode()};
                builder.CreateCall(instrCheck, callArgs);
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

llvm::Value* ThinTeslaInstrumenter::GetVariable(llvm::Function* function, ThinTeslaParameter& param, IRBuilder<>& builder)
{
    Value* value = nullptr;

    std::string& varName = param.varName;

    for (auto& instr : function->getEntryBlock())
    {
        if (instr.getName() == varName)
        {
            value = &instr;
            break;
        }
    }

    if (value == nullptr)
    {
        for (auto& arg : function->args())
        {
            if (arg.getName() == varName)
            {
                value = &arg;
                break;
            }
        }
    }

    if (value != nullptr && param.isIndirection)
    {
        StructType* structType = nullptr;

        if (value->getType()->isPointerTy())
        {
            if (((PointerType*)value->getType())->getElementType()->isPointerTy())
            {
                value = builder.CreateLoad(value);
            }

            structType = dyn_cast<StructType>(((PointerType*)value->getType())->getElementType());
        }

        assert(structType != nullptr && "Base variable of indirection is not a struct");
        assert(structType->getNumElements() > param.fieldIndex && "Type of base variable of indirection does not have enough fields");

        value = builder.CreateStructGEP(structType, value, param.fieldIndex);
        value = builder.CreateLoad(value);
    }
    else if (value != nullptr)
    {
        if (dyn_cast<AllocaInst>(value) != nullptr)
            value = builder.CreateLoad(value);
    }

    return value;
}

void ThinTeslaInstrumenter::UpdateEventsWithParametersGlobal(llvm::Module& M, ThinTeslaAssertion& assertion, llvm::Instruction* insertPoint)
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

            Value* var = GetVariable(function, param, builder);
            if (var == nullptr)
            {
                llvm::errs() << "Variable " << param.varName << " cannot be found (assertion " << GetAutomatonID(assertion) << ")\n";
                assert(var != nullptr && "Variable to match assertion has been optimized away by the compiler!");
            }

            builder.CreateStore(builder.CreateCast(TeslaTypes::GetCastToInteger(var->getType()), var, TeslaTypes::GetMatchType(C)),
                                builder.CreateGEP(builder.CreateBitCast(matchArray, TeslaTypes::GetMatchType(C)->getPointerTo()),
                                                  TeslaTypes::GetInt(C, 32, i)));

            ++i;
        }
    }
}

void ThinTeslaInstrumenter::UpdateEventsWithParametersThread(llvm::Module& M, ThinTeslaAssertion& assertion, llvm::Instruction* insertPoint)
{
    Function* function = insertPoint->getParent()->getParent();

    IRBuilder<> builder(insertPoint);

    LLVMContext& C = M.getContext();

    Function* updateFunction = TeslaTypes::GetUpdateEventWithData(M);
    auto automatonGlobal = GetAutomatonGlobal(M, assertion);

    for (auto event : assertion.events)
    {
        if (event->isDeterministic || event->GetMatchDataSize() == 0)
            continue;

        auto params = event->GetParameters();
        auto retVal = event->GetReturnValue();
        if (retVal.exists && !retVal.isConstant)
            params.push_back(retVal);

        size_t matchDataSize = event->GetMatchDataSize();
        auto dataArray = builder.CreateAlloca(TeslaTypes::GetMatchType(C), TeslaTypes::GetInt(C, 32, matchDataSize));

        size_t i = 0;

        for (auto& param : params)
        {
            if (param.isConstant)
                continue;

            Value* var = GetVariable(function, param, builder);
            if (var == nullptr)
            {
                llvm::errs() << "Variable " << param.varName << " cannot be found (assertion " << GetAutomatonID(assertion) << ")\n";
                assert(var != nullptr && "Variable to match assertion has been optimized away by the compiler!");
            }

            builder.CreateStore(builder.CreateCast(TeslaTypes::GetCastToInteger(var->getType()), var, TeslaTypes::GetMatchType(C)),
                                builder.CreateGEP(dataArray, TeslaTypes::GetInt(C, 32, i)));

            ++i;
        }

        builder.CreateCall(updateFunction, {automatonGlobal, TeslaTypes::GetSizeT(C, event->id), builder.CreateBitCast(dataArray, Type::getInt8PtrTy(C))});
    }

    // OutFunction(function);
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaAssertionSite& event)
{
    Function* function = M.getFunction(event.functionName);

    Function* updateAutomaton = TeslaTypes::GetUpdateAutomatonDeterministic(M);

    if (function != nullptr && !function->isDeclaration())
    {
        assert(event.isDeterministic);
        assert(!event.IsStart() && !event.IsEnd());

        CallInst* callInst = GetTeslaAssertionInstr(function, event);
        if (callInst == nullptr)
            llvm::errs() << *function << "\n";
        assert(callInst != nullptr);

        if (!assertion.isDeterministic)
        {
            if (!assertion.isThreadLocal)
            {
                UpdateEventsWithParametersGlobal(M, assertion, callInst);
            }
            else
            {
                UpdateEventsWithParametersThread(M, assertion, callInst);
            }
        }

        IRBuilder<> builder(callInst);
        builder.CreateCall(updateAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event)});

        if (!assertion.IsLinked() || (assertion.IsLinked() && assertion.IsLinkMaster()))
            callInst->eraseFromParent();
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
    Function* incrementInitTag = TeslaTypes::GetIncrementInitTag(M);

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
        // If we're doing late initialization, skip this.
#ifndef LATE_INIT
        builder.CreateCall(startAutomaton, {GetAutomatonGlobal(M, assertion)});
#else
        if (!instrumentedStart && (assertionsShareTemporalBounds && temporalBound == "amd64_syscall")) // For the kernel.
        {
            builder.CreateCall(incrementInitTag);
            instrumentedStart = true;
        }
#endif
    }
    else if (event.IsEnd())
    {
        InstrumentEndAutomaton(M, builder, assertion, event);
    }
    else
    {
        builder.CreateCall(updateAutomaton, {GetAutomatonGlobal(M, assertion), global});
    }
}

std::vector<llvm::CallInst*> ThinTeslaInstrumenter::GetAllCallsToFunction(llvm::Module& M, const std::string& functionName)
{
    std::vector<llvm::CallInst*> calls;

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
                    if (callInst != nullptr && callInst->getCalledFunction()->getName() == functionName)
                    {
                        calls.push_back(callInst);
                    }
                }
            }
        }
    }

    return calls;
}

void ThinTeslaInstrumenter::InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaFunction& event)
{
    Function* function = M.getFunction(event.functionName);

    if (function != nullptr && !function->isDeclaration() && event.calleeInstrumentation)
    {
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
    }

    if (!event.calleeInstrumentation) // We must instrument every call to the event function.
    {
        auto callInsts = GetAllCallsToFunction(M, event.functionName);

        for (auto callInst : callInsts)
        {
            IRBuilder<> builder(callInst);
            InstrumentInstruction(M, callInst, assertion, event);
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
        InstrumentEndAutomaton(M, builder, assertion, event);
    }
}

void ThinTeslaInstrumenter::InstrumentEndAutomaton(llvm::Module& M, llvm::IRBuilder<>& builder, ThinTeslaAssertion& assertion, ThinTeslaFunction& event)
{
    Function* endAutomaton = TeslaTypes::GetEndAutomaton(M);
    Function* endAllAutomataKernel = TeslaTypes::GetEndAllAutomataKernel(M);
    Function* endLinkedAutomata = TeslaTypes::GetEndLinkedAutomata(M);

    if (assertionsShareTemporalBounds && temporalBound == "amd64_syscall")
    {
        if (!instrumentedEnd)
        {
            builder.CreateCall(endAllAutomataKernel, {});
            instrumentedEnd = true;
        }
    }
    else
        builder.CreateCall(endAutomaton, {GetAutomatonGlobal(M, assertion), GetEventGlobal(M, assertion, event)});

    if (assertion.IsLinked() && assertion.IsLinkMaster())
    {
        builder.CreateCall(endLinkedAutomata,
                           {ConstantExpr::getBitCast(GetLinkedAutomataArray(M, assertion), TeslaTypes::GetVoidPtrPtrTy(M.getContext())),
                            TeslaTypes::GetSizeT(M.getContext(), assertion.linkedAssertions.size() + 1)});
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

    GlobalVariable* var = CreateGlobalVariable(M, eventsArrayTy, eventsArray, autID, THREAD_LOCAL);

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
        GlobalVariable* eventsArrayGlobal = CreateGlobalVariable(M, eventsArrayTy, eventsArray, eventID + "_succ", THREAD_LOCAL);
        eventsArrayPtr = ConstantExpr::getBitCast(eventsArrayGlobal, VoidPtrPtrTy);
    }

    TeslaEventFlags flags;
    flags.isOR = event.isOR;
    flags.isOptional = event.isOptional;
    flags.isDeterministic = event.isDeterministic;
    flags.isAssertion = event.IsAssertion();
    flags.isBeforeAssertion = event.isBeforeAssertion;
    flags.isEnd = event.IsEnd();
    flags.isFinal = event.IsFinal();
    flags.isInitial = event.IsInitial();
    Constant* cFlags = ConstantStruct::get(TeslaTypes::EventFlagsTy, TeslaTypes::GetInt(C, 8, *(uint8_t*)(&flags)));

    Constant* init = ConstantStruct::get(TeslaTypes::EventTy, eventsArrayPtr, cFlags,
                                         TeslaTypes::GetSizeT(C, event.successors.size()), TeslaTypes::GetSizeT(C, event.id),
                                         TeslaTypes::GetInt(C, 8, event.GetMatchDataSize()));

    GlobalVariable* var = CreateGlobalVariable(M, TeslaTypes::EventTy, init, eventID, THREAD_LOCAL);

    var->setConstant(false); // This variable will be manipulated by libtesla.

    return var;
}

Constant* ThinTeslaInstrumenter::GetEventMatchArray(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaEvent& event)
{
    if (event.GetMatchDataSize() == 0)
        return ConstantPointerNull::get(Type::getInt8PtrTy(M.getContext()));

    std::string id = GetEventID(assertion, event) + "_match";
    GlobalVariable* old = M.getGlobalVariable(id);
    if (old != nullptr)
        return old;

    LLVMContext& C = M.getContext();

    ArrayType* matchArrayTy = ArrayType::get(TeslaTypes::GetMatchType(C), event.GetMatchDataSize());
    Constant* matchArray = ConstantArray::get(matchArrayTy, TeslaTypes::GetMatchValue(C, 0));
    GlobalVariable* matchArrayGlobal = CreateGlobalVariable(M, matchArrayTy, matchArray, id, THREAD_LOCAL);

    matchArrayGlobal->setConstant(false); // This array will be populated at runtime.

    return matchArrayGlobal;
}

GlobalVariable* ThinTeslaInstrumenter::GetEventsStateArray(llvm::Module& M, ThinTeslaAssertion& assertion)
{
    std::string id = GetAutomatonID(assertion) + "_events_state";
    GlobalVariable* old = M.getGlobalVariable(id);
    if (old != nullptr)
        return old;

    LLVMContext& C = M.getContext();

    ArrayType* arrayTy = ArrayType::get(TeslaTypes::EventStateTy, assertion.events.size());

    std::vector<Constant*> states;
    for (size_t i = 0; i < assertion.events.size(); ++i)
    {
        Constant* state = ConstantStruct::get(TeslaTypes::EventStateTy, ConstantPointerNull::get(Type::getInt8PtrTy(C)),
                                              ConstantExpr::getBitCast(GetEventMatchArray(M, assertion, *assertion.events[i]), Type::getInt8PtrTy(C)));
        states.push_back(state);
    }

    Constant* array = ConstantArray::get(arrayTy, states);
    GlobalVariable* global = CreateGlobalVariable(M, arrayTy, array, id, THREAD_LOCAL);

    global->setConstant(false); // This array may be populated at runtime.

    return global;
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
    flags.isThreadLocal = assertion.isThreadLocal;
    flags.isLinked = assertion.IsLinked();
    Constant* cFlags = ConstantStruct::get(TeslaTypes::AutomatonFlagsTy, TeslaTypes::GetInt(C, 8, *(uint8_t*)(&flags)));

    Constant* state = ConstantStruct::get(TeslaTypes::AutomatonStateTy,
                                          TeslaTypes::GetSizeT(C, 0),
                                          ConstantPointerNull::get(TeslaTypes::EventTy->getPointerTo()),
                                          ConstantPointerNull::get(TeslaTypes::EventTy->getPointerTo()),
                                          TeslaTypes::GetBoolValue(C, 0),
                                          TeslaTypes::GetBoolValue(C, 0),
                                          TeslaTypes::GetBoolValue(C, 0),
                                          TeslaTypes::GetBoolValue(C, 0),
                                          TeslaTypes::GetBoolValue(C, 0),
                                          ConstantPointerNull::get(Int8PtrTy),
                                          TeslaTypes::GetSizeT(C, 0));

    Constant* init = ConstantStruct::get(TeslaTypes::AutomatonTy, eventsArrayPtr, cFlags,
                                         TeslaTypes::GetSizeT(C, assertion.events.size()),
                                         ConstantExpr::getBitCast(GetStringGlobal(M, autID, autID + "_name"), Int8PtrTy),
                                         state, ConstantExpr::getBitCast(GetEventsStateArray(M, assertion), TeslaTypes::EventStateTy->getPointerTo()),
                                         ConstantPointerNull::get(Int8PtrTy),
                                         TeslaTypes::GetSizeT(C, INVALID_THREAD_KEY), ConstantPointerNull::get(Int8PtrTy),
                                         TeslaTypes::GetSizeT(C, assertions.size()), TeslaTypes::GetSizeT(C, assertion.globalId));

    GlobalVariable* var = CreateGlobalVariable(M, TeslaTypes::AutomatonTy, init, autID, THREAD_LOCAL);

    var->setConstant(false);

    return var;
}

GlobalVariable* ThinTeslaInstrumenter::GetLinkedAutomataArray(llvm::Module& M, ThinTeslaAssertion& linkMaster)
{
    std::string autID = GetAutomatonID(linkMaster) + "_links";
    GlobalVariable* old = M.getGlobalVariable(autID);
    if (old != nullptr)
        return old;

    LLVMContext& C = M.getContext();

    ArrayType* arrayTy = ArrayType::get(TeslaTypes::AutomatonTy->getPointerTo(), linkMaster.linkedAssertions.size() + 1);

    std::vector<Constant*> automata;
    automata.push_back(GetAutomatonGlobal(M, linkMaster));

    for (size_t i = 0; i < linkMaster.linkedAssertions.size(); ++i)
    {
        Constant* automaton = GetAutomatonGlobal(M, *linkMaster.linkedAssertions[i]);
        automata.push_back(automaton);
    }

    Constant* array = ConstantArray::get(arrayTy, automata);
    GlobalVariable* global = CreateGlobalVariable(M, arrayTy, array, autID, THREAD_LOCAL);

    return global;
}

GlobalVariable* ThinTeslaInstrumenter::GetStringGlobal(llvm::Module& M, const std::string& str, const std::string& globalID)
{
    GlobalVariable* old = M.getGlobalVariable(globalID);
    if (old != nullptr)
        return old;

    IntegerType* CharTy = IntegerType::getInt8Ty(M.getContext());
    PointerType* CharPtrTy = PointerType::getUnqual(CharTy);

    Constant* strConst = ConstantDataArray::getString(M.getContext(), str);
    GlobalVariable* var = CreateGlobalVariable(M, strConst->getType(), strConst, globalID);

    return var;
}

GlobalVariable* ThinTeslaInstrumenter::CreateGlobalVariable(llvm::Module& M, llvm::Type* type, llvm::Constant* initializer,
                                                            const std::string& name, bool threadLocal)
{
    if (threadLocal)
    {
        return new GlobalVariable(M, type, true,
                                  DEFAULT_LINKAGE, initializer, name,
                                  nullptr, GlobalValue::ThreadLocalMode::GeneralDynamicTLSModel);
    }
    else
    {
        return new GlobalVariable(M, type, true,
                                  DEFAULT_LINKAGE, initializer, name);
    }
}

std::string ThinTeslaInstrumenter::GetAutomatonID(ThinTeslaAssertion& assertion)
{
    return assertion.assertionFilename + "_" + std::to_string(assertion.assertionLine) + "_" +
           std::to_string(assertion.assertionCounter) + "_" + std::to_string(assertion.id);
}

std::string ThinTeslaInstrumenter::GetEventID(ThinTeslaAssertion& assertion, ThinTeslaEvent& event)
{
    return assertion.assertionFilename + "_" + std::to_string(assertion.assertionLine) + "_" +
           std::to_string(assertion.assertionCounter) + "_" + std::to_string(assertion.id) + "E" + std::to_string(event.id);
}

std::string ThinTeslaInstrumenter::GetFilenameFromPath(const std::string& path)
{
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos || (pos == (path.size() - 1)))
        return path;

    return path.substr(pos + 1);
}