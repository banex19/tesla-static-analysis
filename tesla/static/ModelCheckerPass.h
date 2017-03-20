#ifndef MODEL_CHECKER_PASS_H
#define MODEL_CHECKER_PASS_H

#include "Debug.h"
#include "ManifestPass.h"

#include <llvm/Support/raw_ostream.h>

using std::unique_ptr;
using std::set;

namespace tesla {

class ModelCheckerPass : public ManifestPass {
  public:
    ModelCheckerPass(int unroll, int trace) :
      UnrollDepth(unroll), TraceBound(trace) {}

    virtual unique_ptr<Manifest> run(Manifest &Ma, llvm::Module &Mo) override;
    virtual const std::string PassName() const override;
  private:
    int UnrollDepth;
    int TraceBound;
    bool CheckUsage(Manifest &Man, llvm::Module &Mo, const Usage *use);
};

}

#endif
