#include <numeric>
#include <map>
#include <vector>

#include "Debug.h"
#include "ModelChecker.h"

using std::map;
using std::vector;

set<const tesla::Usage *> ModelChecker::SafeUsages() {
  set<const tesla::Usage *> safeUses;

  for(auto use : Manifest->RootAutomata()) {
    auto automaton = Manifest->FindAutomaton(use->identifier());
    auto expr = automaton->getAssertion().expression();
    
    auto safe = true; 

    auto allTraces = FiniteTraces{Graph}.OfLengthUpTo(25);
    auto boundedTraces = FiniteTraces::BoundedBy(allTraces, Mod->getFunction("main"));

    for(auto trace : boundedTraces) {
      for(auto ev : trace) {
        errs() << ev->GraphViz() << '\n';
      }
      errs() << "------------------\n";
      safe = safe && CheckState(expr, trace, 0);
    }
    
    if(safe) {
      safeUses.insert(use);
    }
  }

  return safeUses;
}

bool ModelChecker::CheckState(const tesla::Expression &ex, const FiniteTraces::Trace &tr, int ind) {
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
bool ModelChecker::CheckBoolean(const tesla::BooleanExpr &ex, const FiniteTraces::Trace &tr, int ind) {
  //errs() << "bool\n";

  vector<bool> results;
  for(int i = 0; i < ex.expression_size(); i++) {
    results.push_back(CheckState(ex.expression(i), tr, ind));
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
    return true;
  }

  return std::accumulate(results.begin(), results.end(), results[0], reducer);
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
bool ModelChecker::CheckSequence(const tesla::Sequence &ex, const FiniteTraces::Trace &tr, int ind) {
  //errs() << "seq\n";

  int size = ex.expression_size();

  // Degenerate sequence is always satisfied
  if(size == 0) {
    return true;
  }

  auto head = ex.expression(0);

  for(int i = ind; i < tr.size() - size; i++) {
    auto head_sat = CheckState(head, tr, i);
    auto tails_sat = false;

    for(int j = 1; j < ex.expression_size(); j++) {
      tails_sat = tails_sat || CheckState(ex.expression(j), tr, i);
    }

    if(head_sat) {
      tesla::Sequence tail;
      for(int k = 1; k < ex.expression_size(); k++) {
        *tail.add_expression() = ex.expression(k);
      }
      return CheckSequence(tail, tr, i + 1);
    }

    if(tails_sat) {
      return false;
    }
  }

  return false;
}

/**
 * Any state satisfies a null expression.
 */
bool ModelChecker::CheckNull(const FiniteTraces::Trace &tr, int ind) {
  //errs() << "null\n";
  return true;
}

/**
 * Check passes in the state if we have the same location attached to an
 * assert event.
 */
bool ModelChecker::CheckAssertionSite(const tesla::AssertionSite &ex, const FiniteTraces::Trace &tr, int ind) {
  //errs() << "assert\n";

  if(auto ae = dyn_cast<AssertEvent>(tr[ind])) {
    return ex.location() == ae->Location();
  }

  return false;
}

/**
 * When checking a function event, we look up the function named by the event in
 * our module, then see if it's the same as the one on the event (but only if
 * the event is an en, int indy / exit event with the correct direction).
 */
bool ModelChecker::CheckFunction(const tesla::FunctionEvent &ex, const FiniteTraces::Trace &tr, int ind) {
  //errs() << "func\n";
  auto modFn = Mod->getFunction(ex.function().name());

  if(auto ent = dyn_cast<EntryEvent>(tr[ind])) {
    if(ex.direction() == tesla::FunctionEvent_Direction_Entry) {
      return modFn && ent->Func && modFn == ent->Func;
    }
  }

  if(auto exit = dyn_cast<ExitEvent>(tr[ind])) {
    if(ex.direction() == tesla::FunctionEvent_Direction_Exit) {
      return modFn && exit->Func && modFn == exit->Func;
    }
  }

  return false;
}

/**
 * Currently no state can satisfy a field assignment, but this might be changed
 * in the future when the event graph mechanism is upgraded.
 */
bool ModelChecker::CheckFieldAssign(const tesla::FieldAssignment &ex, const FiniteTraces::Trace &tr, int ind) {
  //errs() << "assign\n";
  return false;
}

/**
 * Subautomaton checking just recurses into the named automaton's logical
 * expression at the current state.
 */
bool ModelChecker::CheckSubAutomaton(const tesla::Automaton &ex, const FiniteTraces::Trace &tr, int ind) {
  //errs() << "subauto " << tesla::ShortName(ex.getAssertion().identifier()) << '\n';
  return CheckState(ex.getAssertion().expression(), tr, ind);
}
