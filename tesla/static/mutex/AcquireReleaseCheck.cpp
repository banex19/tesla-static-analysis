#include "AcquireReleaseCheck.h"
#include "ControlPath.h"
#include "AcquireSequenceAnalysis.h"
#include "AddressTakenAnalysis.h"
#include "CallOrderAnalysis.h"
#include "MissingCallAnalysis.h"
#include "NoBranchAnalysis.h"
#include "OtherLockAnalysis.h"
#include "ReleaseBeforeAcquireAnalysis.h"
#include "ReleaseDominanceAnalysis.h"

#include <llvm/Analysis/CallGraph.h>

AcquireReleaseCheck::AcquireReleaseCheck(const tesla::Automaton &A, 
                                         std::vector<tesla::Argument> args_) : 
  ModulePass(ID), 
  correctUsage(true),
  boundName(A.Use()->beginning().function().function().name()),
  args(args_)
{
}

void AcquireReleaseCheck::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<CallGraphWrapperPass>();
}

bool AcquireReleaseCheck::runOnModule(Module &M) {
  auto BoundFn = M.getFunction(boundName);
  if(!BoundFn) {
    errs() << "Bounding function " << boundName
           << " does not exist in this module.\n";
    return false;
  }

  BasicBlock &entry = BoundFn->getEntryBlock();
  Instruction *first = entry.getFirstNonPHIOrDbgOrLifetime();
  IRBuilder<> B(first);

  std::vector<Value *> Args(tesla::CollectArgs(first, args, M, B));
  
  /**
   * For now, the automata being analysed should only have a single parameter
   * (the lock structure in question).
   */
  if(Args.size() != 1) {
    errs() << "Automata has more than one parameter\n";
    return true;
  }

  std::vector<Analysis *> Analyses{
    new AddressTakenAnalysis(M),
    new MissingCallAnalysis(M, *BoundFn),
    new ReleaseDominanceAnalysis(M, *BoundFn),
    new OtherLockAnalysis(M, *BoundFn, *Args[0]),
    new NoBranchAnalysis(M),
    new ReleaseBeforeAcquireAnalysis(M, *BoundFn, *Args[0]),
    new CallOrderAnalysis(M, *BoundFn),
    new AcquireSequenceAnalysis(M, *BoundFn)
  };

  std::for_each(Analyses.begin(), Analyses.end(),
    [=](Analysis *a) {
      if(a->run()) {
        correctUsage = false;
        errs() << a->Message();
      }
    }
  );

  return true;
}
  
void AcquireReleaseCheck::print(raw_ostream &OS, const Module *m) const {
  OS << "[AcqRel] correct usage: " << correctUsage << '\n';
}

const char *AcquireReleaseCheck::getPassName() const {
  return "AcquireReleaseCheck";
}

char AcquireReleaseCheck::ID = 0;
