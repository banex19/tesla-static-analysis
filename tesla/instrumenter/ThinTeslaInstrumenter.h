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
            assertions.push_back(ThinTeslaAssertion{manifest, automaton});
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

    llvm::CallInst* GetTeslaAssertionInstr(llvm::Function* function, ThinTeslaAssertionSite& event);

    GlobalVariable* GetEventGlobal(llvm::Module& M, ThinTeslaAssertion& assertion, ThinTeslaEvent& event);
    GlobalVariable* GetEventsArray(llvm::Module& M, ThinTeslaAssertion& assertion);
    GlobalVariable* GetAutomatonGlobal(llvm::Module& M, ThinTeslaAssertion& assertion);
    GlobalVariable* GetStringGlobal(llvm::Module& M, const std::string& str, const std::string& globalID);

    std::string GetEventID(ThinTeslaAssertion& assertion, ThinTeslaEvent& event);
    std::string GetAutomatonID(ThinTeslaAssertion& assertion);

    std::string GetFilenameFromPath(const std::string& path);

    const tesla::Manifest& manifest;

    std::vector<ThinTeslaAssertion> assertions;

    static char ID;
};