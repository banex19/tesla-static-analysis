/**
 * @file
 */
#ifndef FSM_STATE_H
#define FSM_STATE_H

#include <memory>
#include <set>
#include <sstream>
#include <string>

/**
 * Represents a single state in a finite state machine.
 */
struct State {
  /**
   * Construct a state whose name is the empty string.
   *
   * The constructed state is neither accepting nor initial.
   */
  State() {}

  /**
   * Construct a state with the given name.
   *
   * The constructed state is neither accepting nor initial.
   *
   * \param s The name assigned to the state
   */
  State(std::string s) :
    name(s) {}

  /**
   * Names a state for debugging output.
   */
  std::string name = "";

  /**
   * `true` if an FSM accepts in this state.
   */
  bool accepting = false;

  /**
   * `true` if this is an initial state for its FSM.
   */
  bool initial = false;

  /**
   * Returns a DOT representation of this state.
   *
   * A string returned from this method can be placed in a GraphViz digraph in
   * order to print an entire FSM.
   *
   * \returns DOT formatted string representing the state
   */
  std::string Dot() const;

  /**
   * Construct a new state from a range of existing states.
   *
   * The resulting state has a name derived from the names of the passed states,
   * and derives its properties from these states as follows:
   * * If any of the passed states are accepting, the combined state is
   *   accepting.
   * * If there is exactly one state in the range (and it is initial), the new
   *   state is also initial.
   *
   * \param states A range of states to combine into a new state.
   * \returns A new state derived from the given range of states.
   */
  template<class Iter>
  static State Combined(Iter begin, Iter end);

  /**
   * Wraps \ref State::Combined(Iter, Iter).
   *
   * This method is a wrapper that allows containers to be passed directly by
   * calling `std::begin` and `std::end` on them.
   */
  template<class Container>
  static State Combined(Container in);
};

template<class Iter>
State State::Combined(Iter begin, Iter end) 
{
  auto size = std::distance(begin, end);

  auto state = State{};

  std::stringstream name;
  name << "{";

  auto i = 0;
  for(auto it = begin; it != end; i++, it++) {
    name << (*it)->name;
    if(i != size - 1) {
      name << ", ";
    }
  }

  name << "}";
  state.name = name.str();

  state.accepting = std::any_of(begin, end,
    [=](auto st) {
      return st->accepting;
    }
  );

  state.initial = (size == 1 && (*begin)->initial);

  return state;
}

template<class Container>
State State::Combined(Container in)
{
  return Combined(std::begin(in), std::end(in));
}

inline std::string State::Dot() const
{
  std::stringstream out;

  out << "\"" << name << "\"";

  if(accepting) {
    out << " [shape=doublecircle]";
  }

  if(initial) {
    out << ";\"secret\" [style=invis,shape=point];\"secret\"->\"" << name << "\"";
  }

  return out.str();
}

#endif
