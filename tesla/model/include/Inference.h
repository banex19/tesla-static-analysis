#ifndef INFERENCE_H
#define INFERENCE_H

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace llvm;

struct Branch;

/**
 * Abstract class that represents an inferred condition.
 *
 * These conditions are inferred from the results of branches we have taken to
 * get to a particular basic block within a function. In particular, the
 * condition we have represents the strongest condition we are able to infer
 * about a particular basic block.
 */
struct Condition {
  enum ConditionKind {
    CK_ConstFalse,
    CK_ConstTrue,
    CK_Branch,
    CK_And,
    CK_Or
  };

private:
  const ConditionKind Kind;

public:
  ConditionKind getKind() const { return Kind; }

  Condition(ConditionKind K) : Kind(K) {}

  virtual Condition *Simplified() const = 0;
  virtual std::set<Branch> Branches() const = 0;
  virtual bool IsConstant() const = 0;
  virtual bool Eval() const = 0;
  virtual Condition *Restricted(Branch b, Condition *replace) const = 0;
  virtual Condition *Restricted(Branch b, Condition *tr, Condition *fr) const = 0;
  Condition *SplitOn(Branch b) const;
  Condition *Decomposed();

  virtual std::string str() const = 0;
  virtual bool Equal(Condition *other) const = 0;

  static std::map<BasicBlock *, Condition *> StrongestInferences(Function *f);
  static Condition *BranchCondition(BasicBlock *pred, BasicBlock *succ);
};

/**
 * A condition that is always false.
 */
struct ConstFalse : public Condition {
  ConstFalse() : Condition(CK_ConstFalse) {}

  virtual Condition *Simplified() const override;
  std::set<Branch> Branches() const override { return {}; }
  virtual bool IsConstant() const override { return true; }
  bool Eval() const override { return false; }
  virtual Condition *Restricted(Branch b, Condition *replace) const override;
  virtual Condition *Restricted(Branch b, Condition *tr, Condition *fr) const override;

  std::string str() const override;
  bool Equal(Condition *other) const override;

  static bool classof(const Condition *C) {
    return C->getKind() == CK_ConstFalse;
  }
};

/**
 * A condition representing constant `true`.
 */
struct ConstTrue : public Condition {
  ConstTrue() : Condition(CK_ConstTrue) {}

  virtual Condition *Simplified() const override;
  std::set<Branch> Branches() const override { return {}; }
  virtual bool IsConstant() const override { return true; }
  bool Eval() const override { return true; }
  virtual Condition *Restricted(Branch b, Condition *replace) const override;
  virtual Condition *Restricted(Branch b, Condition *tr, Condition *fr) const override;
  
  std::string str() const override;
  bool Equal(Condition *other) const override;

  static bool classof(const Condition *C) {
    return C->getKind() == CK_ConstTrue;
  }
};

/**
 * A condition that constrains an LLVM value to a constant true / false.
 */
struct Branch : public Condition {
  Branch(Value *v, bool c) : 
    Condition(CK_Branch), value(v), constraint(c) {}

  Branch *Negated() const { return new Branch{value, !constraint}; }

  virtual Condition *Simplified() const override;
  std::set<Branch> Branches() const override { return {*this}; }
  virtual bool IsConstant() const override { return false; }
  bool Eval() const override;
  virtual Condition *Restricted(Branch b, Condition *replace) const override;
  virtual Condition *Restricted(Branch b, Condition *tr, Condition *fr) const override;

  std::string str() const override;
  bool Equal(Condition *other) const override;

  static bool classof(const Condition *C) {
    return C->getKind() == CK_Branch;
  }

  bool Opposite(Branch *other) const;

  // Define this so that we can put Branches into a std::set
  bool operator <(const Branch& other) const {
    return (value < other.value) && (constraint < other.constraint);
  }

  bool operator ==(const Branch& other) const {
    return (value == other.value) && (constraint == other.constraint);
  }
private:
  Value *value;
  bool constraint;
};

/**
 * Logical operation over several conditions.
 */
struct LogicalOp : public Condition {
  template<class InputIt>
  LogicalOp(InputIt first, InputIt last, ConditionKind K) :
    Condition(K), operands(first, last) {}

  LogicalOp(std::initializer_list<Condition *> il, ConditionKind K) :
    LogicalOp(il.begin(), il.end(), K) {}

  LogicalOp(ConditionKind K) :
    LogicalOp({}, K) {}

  template<class C, class Zero, class Elim, class Match>
  Condition *SimplifyLogic() const;

  template<class C>
  Condition *RestrictedLogic(Branch b, Condition *rep) const;

  template<class C>
  Condition *RestrictedLogic(Branch b, Condition *tr, Condition *fr) const;

  virtual std::set<Branch> Branches() const override;
  virtual bool IsConstant() const override {
    return std::all_of(operands.begin(), operands.end(),
      [](Condition *c) { return c->IsConstant(); }
    );
  }

  static bool classof(const Condition *C) {
    return C->getKind() >= CK_And && C->getKind() <= CK_Or;
  }

protected:
  std::vector<Condition *> operands;
};

/**
 * Logical and of several conditions.
 */
struct And : public LogicalOp {
  template<class InputIt>
  And(InputIt first, InputIt last) :
    LogicalOp(first, last, CK_And) {}

  And(std::initializer_list<Condition *> il) : 
    And(il.begin(), il.end()) {}

  And() : And({}) {}

  virtual Condition *Simplified() const override;
  bool Eval() const override;
  virtual Condition *Restricted(Branch b, Condition *replace) const override;
  virtual Condition *Restricted(Branch b, Condition *tr, Condition *fr) const override;

  std::string str() const override;
  bool Equal(Condition *other) const override;

  static bool classof(const Condition *C) {
    return C->getKind() == CK_And;
  };
};

/**
 * Logical or of several conditions.
 */
struct Or : public LogicalOp {
  friend struct And;

  template<class InputIt>
  Or(InputIt first, InputIt last) :
    LogicalOp(first, last, CK_Or) {}

  Or(std::initializer_list<Condition *> il) : 
    Or(il.begin(), il.end()) {}

  Or() : Or({}) {}

  virtual Condition *Simplified() const override;
  bool Eval() const override;
  virtual Condition *Restricted(Branch b, Condition *replace) const override;
  virtual Condition *Restricted(Branch b, Condition *tr, Condition *fr) const override;

  std::string str() const override;
  bool Equal(Condition *other) const override;

  static bool classof(const Condition *C) {
    return C->getKind() == CK_Or;
  }
};

#endif
