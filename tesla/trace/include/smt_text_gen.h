#ifndef SMT_GEN_H
#define SMT_GEN_H

#include <map>
#include <sstream>
#include <string>

#include <llvm/InstVisitor.h>
#include <llvm/ADT/ValueMap.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>

using namespace llvm;

class SMTTextVisitor : public InstVisitor<SMTTextVisitor> {
public:
  SMTTextVisitor(Function &f, ValueMap<Value *, std::string>& ns);

  void run();

  void visitBinaryOperator(BinaryOperator &BO);
  void visitCallInst(CallInst &CI);
  void visitLoadInst(LoadInst &LI);
  void visitCmpInst(CmpInst &CI);

  std::string str() const { return ss.str(); }
private:
  std::string operand_str(Value *V) const;
  bool called_returns_bool(CallInst &ci) const;
  
  Function& function_;
  ValueMap<Value *, std::string>& names_;
  std::stringstream ss;
};

#endif
