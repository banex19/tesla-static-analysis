#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Pass.h>

#include "Manifest.h"
#include "ThinTeslaAssertion.h"
#include "ThinTeslaTypes.h"
#include <map>

class ThinTeslaInstrumenter : public ThinTeslaEventVisitor, public llvm::ModulePass
{
  public:
    ThinTeslaInstrumenter(tesla::Manifest& manifest) : llvm::ModulePass(ID), manifest(manifest)
    {
        for (auto& automaton : manifest.RootAutomata())
        {
            ThinTeslaAssertionBuilder builder{manifest, automaton};
            for (auto& assertion : builder.GetAssertions())
            {
                assertions.push_back(*assertion);
            }
        }
    }

    virtual ~ThinTeslaInstrumenter() {}

    virtual bool runOnModule(llvm::Module& M) override;

  private:
    void Instrument(llvm::Module& M, ThinTeslaAssertion& assertion);

    void InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaEvent& event);
    void InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaFunction& event);
    void InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaParametricFunction& event);
    void InstrumentEvent(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaAssertionSite& event);
    void InstrumentEveryExit(llvm::Module& M, llvm::Function* function, ThinTeslaAssertion& assertion, ThinTeslaFunction& event);
    void InstrumentEndAutomaton(llvm::Module& M, llvm::IRBuilder<>& builder, ThinTeslaAssertion& assertion, ThinTeslaFunction& event);
    void InstrumentInstruction(llvm::Module& M, llvm::Instruction* instr, ThinTeslaAssertion& assertion, ThinTeslaFunction& event);
    void UpdateEventsWithParametersGlobal(llvm::Module& M, ThinTeslaAssertion& assertion, llvm::Instruction* insertPoint);
    void UpdateEventsWithParametersThread(llvm::Module& M, ThinTeslaAssertion& assertion, llvm::Instruction* insertPoint);
    Function* BuildInstrumentationCheck(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaParametricFunction& event);

    llvm::CallInst* GetTeslaAssertionInstr(llvm::Function* function, ThinTeslaAssertionSite& event);
    llvm::Instruction* GetFirstInstruction(llvm::Function* function);
    std::vector<llvm::Argument*> GetFunctionArguments(llvm::Function* function);
    std::vector<BasicBlock*> GetEveryExit(llvm::Function* function);
    llvm::Value* GetVariable(llvm::Function* function, ThinTeslaParameter& param, IRBuilder<>& builder);

    GlobalVariable* GetEventGlobal(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaEvent& event);
    Constant* GetEventMatchArray(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaEvent& event);
    GlobalVariable* GetEventsArray(llvm::Module& M, ThinTeslaAssertion& assertion);
    GlobalVariable* GetEventsStateArray(llvm::Module& M, ThinTeslaAssertion& assertion);
    GlobalVariable* GetAutomatonGlobal(llvm::Module& M, ThinTeslaAssertion& assertion);
    GlobalVariable* GetLinkedAutomataArray(llvm::Module& M, ThinTeslaAssertion& linkMaster);
    GlobalVariable* GetStringGlobal(llvm::Module& M, const std::string& str, const std::string& globalID);
    GlobalVariable* CreateGlobalVariable(llvm::Module& M, llvm::Type* type, llvm::Constant* initializer, const std::string& name, bool threadLocal = false);

    std::string GetEventID(ThinTeslaAssertion& assertion, ThinTeslaEvent& event);
    std::string GetAutomatonID(ThinTeslaAssertion& assertion);

    std::string GetFilenameFromPath(const std::string& path);
    std::set<std::string> GetFunctionsInstrumentedMoreThanOnce();
    std::set<std::string> CollectModuleFunctions(llvm::Module& M);
    std::vector<llvm::CallInst*> GetAllCallsToFunction(llvm::Module& M, const std::string& functionName);
    bool IsFunctionInstrumentedMultipleTimes(const std::string& functionName)
    {
        return multipleInstrumentedFunctions.find(functionName) != multipleInstrumentedFunctions.end();
    }

    const tesla::Manifest& manifest;

    std::vector<ThinTeslaAssertion> assertions;
    std::set<std::string> multipleInstrumentedFunctions;

    static char ID;
};