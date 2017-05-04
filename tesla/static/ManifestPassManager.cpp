#include "ManifestPass.h"

#include <llvm/Support/raw_ostream.h>

using std::unique_ptr;

namespace tesla {

ManifestPassManager::ManifestPassManager(unique_ptr<tesla::Manifest> Ma, 
                                         unique_ptr<llvm::Module> Mo) :
  Manifest(std::move(Ma)), Mod(std::move(Mo)) {}

void ManifestPassManager::addPass(ManifestPass *pass) {
  passes.push_back(pass);
}

void ManifestPassManager::runPasses() {
  auto result = std::move(Manifest);

  for(auto pass : passes) {
    result = pass->run(*result, *Mod);
    if(!result) {
      llvm::errs() << "Pass failed: "
                   << pass->PassName() << '\n';
      break;
    }
  }

  Manifest = std::move(result);
}

const tesla::Manifest& ManifestPassManager::getResult() const {
  return *Manifest;
}

bool ManifestPassManager::successful() const {
  return static_cast<bool>(Manifest);
}

}
