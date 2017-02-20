/** @file */

#ifndef EVENT_GRAPH_H
#define EVENT_GRAPH_H

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <vector>

using std::set;
using std::string;
using namespace llvm;

struct Event;
struct EventRange;

struct EventGraph {
  friend struct Event;

  using EventTransformation = std::function<Event *(Event *)>;

  EventGraph(string n)
    : Name(n) {}

  EventGraph() 
    : EventGraph("") {}

  void replace(Event *from, Event *to);
  void replace(Event *from, EventRange *to);

  void transform(EventTransformation T);

  static EventGraph *BasicBlockGraph(Function *f);
  static EventGraph *InstructionGraph(Function *f);
  static EventGraph *ModuleGraph(Module *M, Function *root);

  set<Event *> entries();
  set<Event *> exits();

  EventRange *ReleasedRange();

  string GraphViz() const;
private:
  void releaseAllEvents();
  void assert_valid();
  void consolidate();
  set<Event *> Events;
  string Name;

  string GraphVizStyle() const {
    return "node [shape = none]";
  }
};

struct Event {
  friend struct EventGraph;
  friend struct EventRange;

  enum EventKind {
    EV_Instruction,
    EV_Empty,
    EV_BasicBlock,
    EV_Call,
    EV_Enter,
    EV_Exit
  };

  static Event *Create(Instruction *I);

  virtual string Name() const = 0;
  virtual string GraphViz() const;

  const EventGraph &ParentGraph() {
    assert(Graph && "Event not initialised!");
    return *Graph; 
  }

  void Register(EventGraph *g) {
    assert((Graph == nullptr || Graph == g) && "Can't re-register event!");
    Graph = g;
  }

  void addSuccessor(Event *e) { successors.insert(e); }

  EventKind getKind() const { return Kind; }
protected:
  Event(EventKind k, EventGraph *g);
  Event(EventKind k) : Event(k, nullptr) {}

private:
  set<Event *> successors;
  EventGraph *Graph;
  EventKind Kind;
};

struct InstructionEvent : public Event {
  InstructionEvent(EventGraph *g, Instruction *I)
    : Event(EV_Instruction, g), Instr_(I) {}

  InstructionEvent(Instruction *I)
    : InstructionEvent(nullptr, I) {}

  Instruction *Instr() const { return Instr_; }

  virtual string Name() const override;

  static bool classof(const Event *other) {
    return other->getKind() == EV_Instruction;
  }
private:
  Instruction *Instr_;
};

struct CallEvent : public Event {
  CallEvent(EventGraph *g, CallInst *CI)
    : Event(EV_Call, g), Call_(CI) {}

  CallEvent(CallInst *CI)
    : CallEvent(nullptr, CI) {}

  CallInst *Call() const { return Call_; }

  virtual string Name() const override;

  static bool classof(const Event *other) {
    return other->getKind() == EV_Call;
  }
private:
  CallInst *Call_;
};

struct EmptyEvent : public Event {
  EmptyEvent(EventGraph *g) 
    : Event(EV_Empty, g) {}

  EmptyEvent()
    : EmptyEvent(nullptr) {}

  virtual string Name() const override;

  static bool classof(const Event *other) {
    return other->getKind() == EV_Empty;
  }
};

struct BasicBlockEvent : public Event {
  BasicBlockEvent(EventGraph *g, BasicBlock *bb)
    : Event(EV_BasicBlock, g), Block(bb) {}

  BasicBlockEvent(BasicBlock *bb)
    : BasicBlockEvent(nullptr, bb) {}

  virtual string Name() const override {
    std::stringstream ss;
    ss << Block->getName().str() << ":" << Block;
    return ss.str();
  }

  static bool classof(const Event *other) {
    return other->getKind() == EV_BasicBlock;
  }

  BasicBlock *Block;
};

struct EntryEvent : public Event {
  EntryEvent(EventGraph *g, string n)
    : Event(EV_Enter, g), Description(n) {}

  EntryEvent(string n)
    : EntryEvent(nullptr, n) {}

  string Description;

  virtual string Name() const override {
    return "enter:" + Description;
  }

  static bool classof(const Event *other) {
    return other->getKind() == EV_Enter;
  }
};

struct ExitEvent : public Event {
  ExitEvent(EventGraph *g, string n)
    : Event(EV_Enter, g), Description(n) {}

  ExitEvent(string n)
    : ExitEvent(nullptr, n) {}

  string Description;

  virtual string Name() const override {
    return "exit:" + Description;
  }

  static bool classof(const Event *other) {
    return other->getKind() == EV_Exit;
  }
};

struct EventRange {
  EventRange(Event *b, Event *e);

  static EventRange *Create(EventGraph *g, BasicBlock *bb);

  Event *const begin;
  Event *const end;

  set<Event *> Events;
};

#endif
