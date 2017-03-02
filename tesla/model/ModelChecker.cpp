#include <numeric>
#include <map>
#include <vector>

#include "Debug.h"
#include "ModelChecker.h"
#include "SequenceExpressions.h"

using std::map;
using std::vector;

set<const tesla::Usage *> ModelChecker::SafeUsages() {
  set<const tesla::Usage *> safeUses;

  for(auto use : Manifest->RootAutomata()) {
    auto automaton = Manifest->FindAutomaton(use->identifier());
    auto expr = automaton->getAssertion().expression();
    
    auto safe = true; 

    auto allTraces = FiniteTraces{Graph}.OfLengthUpTo(Depth);
    auto boundedTraces = FiniteTraces::BoundedBy(allTraces, Bound);
    auto cyclicTraces = FiniteTraces::Cyclic(allTraces);

    for(auto trace : boundedTraces) {
      auto ttr = tagged(trace);
      MarkIgnoredEvents(expr, ttr);
      auto ch = CheckState(expr, ttr, 0);

      for(auto ev : ttr) {
        errs() << (ev.second ? "✓" : "✗") << " " <<  ev.first->GraphViz() << '\n';
      }

      errs() << "------------------\n";
      safe = safe && ch.Successful() && std::all_of(ttr.begin(), ttr.end(), 
        [](pair<Event *, bool> p) { return p.second; }
      );
    }

    for(auto trace : cyclicTraces) {
      auto ttr = tagged(trace);
      MarkIgnoredEvents(expr, ttr);
      CheckState(expr, ttr, 0);
      
      for(auto ev : ttr) {
        errs() << (ev.second ? "✓" : "✗") << " " <<  ev.first->GraphViz() << '\n';
      }

      errs() << "------------------\n";
      safe = safe && std::all_of(ttr.begin(), ttr.end(), 
        [](pair<Event *, bool> p) { return p.second; }
      );
    }
    
    if(safe) {
      safeUses.insert(use);
    }
  }

  return safeUses;
}

void ModelChecker::MarkIgnoredEvents(const tesla::Expression &ex, TaggedTrace &tr) {
  auto subs = SubExpressions(*Manifest).Get(ex);
  auto tr_copy = tr;

  for(int i = 0; i < tr.size(); i++) {
    auto ignore = true;

    for(auto subExpr : subs) {
      auto res = CheckState(*subExpr, tr_copy, i);

      if(res.Successful()) {
        ignore = false;
        break;
      }
    }

    if(ignore) {
      tr[i].second = true;
    }
  }
}

CheckResult ModelChecker::CheckState(const tesla::Expression &ex, ModelChecker::TaggedTrace &tr, int ind) {
  switch(ex.type()) {
    case tesla::Expression_Type_BOOLEAN_EXPR:
      return CheckBoolean(ex.booleanexpr(), tr, ind);

    case tesla::Expression_Type_SEQUENCE:
      return CheckSequence(ex.sequence(), tr, ind);

    case tesla::Expression_Type_NULL_EXPR:
      return CheckNull(tr, ind);

    case tesla::Expression_Type_ASSERTION_SITE:
      return CheckAssertionSite(ex.assertsite(), tr, ind);

    case tesla::Expression_Type_FUNCTION:
      return CheckFunction(ex.function(), tr, ind);

    case tesla::Expression_Type_FIELD_ASSIGN:
      return CheckFieldAssign(ex.fieldassign(), tr, ind);

    case tesla::Expression_Type_SUB_AUTOMATON:
      auto sub = Manifest->FindAutomaton(ex.subautomaton());
      return CheckSubAutomaton(*sub, tr, ind);
  }
}

/**
 * Collects all the checked expressions from the expression then reduces them
 * according to the operation in the expression (and / or / xor).
 */
CheckResult ModelChecker::CheckBoolean(const tesla::BooleanExpr &ex, ModelChecker::TaggedTrace &tr, int ind) {
  //errs() << "bool\n";

  vector<bool> results;
  for(int i = 0; i < ex.expression_size(); i++) {
    results.push_back(CheckState(ex.expression(i), tr, ind).Successful());
  }

  std::function<bool(bool, bool)> reducer;
  switch(ex.operation()) {
    case tesla::BooleanExpr_Operation_BE_Or:
      reducer = [](bool x, bool y) { return x || y; };
      break;
    case tesla::BooleanExpr_Operation_BE_Xor:
      reducer = [](bool x, bool y) { return x ^ y; };
      break;
    case tesla::BooleanExpr_Operation_BE_And:
      reducer = [](bool x, bool y) { return x && y; };
      break;
  }

  if(results.empty()) {
    return CheckResult::Success(0);
  }

  return std::accumulate(results.begin(), results.end(), results[0], reducer) ?
    CheckResult::Success(0) : CheckResult::Failed();
}

CheckResult ModelChecker::CheckSequence(const tesla::Sequence &ex, 
                                        ModelChecker::TaggedTrace &tr, 
                                        int ind)
{
  int min = ex.minreps();
  int max = ex.maxreps();
  int count = 0;
  int len = 0;
  int index = ind;

  for(; count < max; count++) {
    auto res = CheckSequenceOnce(ex, tr, index);

    if(res.Successful()) {
      len += res.Length();
      index += res.Length();
    } else {
      break;
    }
  }

  if(count < min) {
    return CheckResult::Failed();
  }

  return CheckResult::Success(len);
}

/**
 * Because sequences can have an arbi, int indary number of events in them, we need to
 * separate it out into a head / tail (if possible), then check the splits
 * appropriately. Also need to work out a way of extending to allow for reps to
 * be included - currently ignored (!)
 *
 * I *think* that sequences should be the sole way to handle recursing through
 * the graph - a one event sequence is equivalent to something like X[P] in LTL,
 * whereas every other type should just be checked at the current state.
 */
CheckResult ModelChecker::CheckSequenceOnce(const tesla::Sequence &ex, 
                                            ModelChecker::TaggedTrace &tr, 
                                            int ind,
                                            set<const tesla::Expression *> exprs)
{
  //errs() << "seq\n";

  // Don't go off the end of the trace
  if(ind >= tr.size()) {
    return CheckResult::Failed();
  }

  // A sequence with no expressions can be successfully matched if none of the
  // expressions we care about match in the future
  if(ex.expression_size() == 0) {
    return CheckResult::Success(0);
  }

  auto head = ex.expression(0);
  for(int i = ind; i < tr.size(); i++) {
    auto res = CheckState(head, tr, i);

    if(res.Successful()) {
      auto tail = tesla::Sequence{};
      for(int i = 1; i < ex.expression_size(); i++) {
        *tail.add_expression() = ex.expression(i);
      }

      auto ret = CheckSequenceOnce(tail, tr, i + res.Length());
      if(ret.Successful()) {
        return CheckResult::Success(ret.Length() + res.Length());
      } else {
        return ret;
      }
    }
  }

  return CheckResult::Failed();
}

/**
 * Any state satisfies a null expression.
 */
CheckResult ModelChecker::CheckNull(ModelChecker::TaggedTrace &tr, int ind) {
  //errs() << "null\n";
  return CheckResult::Success(0);
}

/**
 * Check passes in the state if we have the same location attached to an
 * assert event.
 */
CheckResult ModelChecker::CheckAssertionSite(const tesla::AssertionSite &ex, ModelChecker::TaggedTrace &tr, int ind) {
  //errs() << "assert: " << tr[ind]->GraphViz() << '\n';

  if(auto ae = dyn_cast<AssertEvent>(tr[ind].first)) {
    if(ex.location() == ae->Location()) {
      tr[ind].second = true;
      return CheckResult::Success(1);
    }
  }

  return CheckResult::Failed();
}

/**
 * When checking a function event, we look up the function named by the event in
 * our module, then see if it's the same as the one on the event (but only if
 * the event is an en, int indy / exit event with the correct direction).
 */
CheckResult ModelChecker::CheckFunction(const tesla::FunctionEvent &ex, 
                                        ModelChecker::TaggedTrace &tr, 
                                        int ind) 
{
  //errs() << "func\n";
  auto modFn = Mod->getFunction(ex.function().name());

  if(auto ent = dyn_cast<EntryEvent>(tr[ind].first)) {
    if(ex.direction() == tesla::FunctionEvent_Direction_Entry) {
      if(modFn && ent->Func && modFn == ent->Func) {
        tr[ind].second = true;
        return CheckResult::Success(1);
      }
    }
  }

  if(auto exit = dyn_cast<ExitEvent>(tr[ind].first)) {
    if(ex.direction() == tesla::FunctionEvent_Direction_Exit) {
      if(modFn && exit->Func && modFn == exit->Func) {
        tr[ind].second = true;
        return CheckResult::Success(1);
      }
    }
  }

  return CheckResult::Failed();
}

/**
 * Currently no state can satisfy a field assignment, but this might be changed
 * in the future when the event graph mechanism is upgraded.
 */
CheckResult ModelChecker::CheckFieldAssign(const tesla::FieldAssignment &ex, ModelChecker::TaggedTrace &tr, int ind) {
  //errs() << "assign\n";
  return CheckResult::Failed();
}

/**
 * Subautomaton checking just recurses into the named automaton's logical
 * expression at the current state.
 */
CheckResult ModelChecker::CheckSubAutomaton(const tesla::Automaton &ex, ModelChecker::TaggedTrace &tr, int ind) {
  auto ret = CheckState(ex.getAssertion().expression(), tr, ind);
  return ret;
}

ModelChecker::TaggedTrace ModelChecker::tagged(const FiniteTraces::Trace tr) {
  TaggedTrace ret;

  std::transform(tr.begin(), tr.end(), std::back_inserter(ret),
    [=](Event *e) {
      return pair<Event *, bool>{e, false};
    }
  );

  return ret;
}
