#ifndef FINITE_TRACES_H
#define FINITE_TRACES_H

#include <llvm/IR/Function.h>

#include <map>
#include <mutex>
#include <set>
#include <vector>

#include "EventGraph.h"

using std::map;
using std::set;
using std::vector;
using namespace llvm;

struct FiniteTraces {
  using Trace = vector<Event *>;
  using TraceSet = set<Trace>;

  FiniteTraces(EventGraph *eg) :
    Graph(eg), Root(nullptr) {}

  FiniteTraces(EventGraph *eg, Event *root) :
    Graph(eg), Root(root) {}

  TraceSet OfLength(size_t len);
  TraceSet OfLengthUpTo(size_t len);

  static Trace BoundedBy(Trace t, Function *f);
  static TraceSet BoundedBy(TraceSet traces, Function *f);

  static Trace Cycle(Trace t);
  static TraceSet Cyclic(TraceSet t);
private:
  map<size_t, TraceSet> cache;
  EventGraph *Graph;
  Event *Root;
  std::mutex cache_lock;
};

#endif
