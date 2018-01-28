/**
 * \file
 */

#ifndef FINITE_STATE_MACHINE_H
#define FINITE_STATE_MACHINE_H

#include <fsm/edge.h>
#include <fsm/state.h>

#include <cassert>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>

/**
 * Represents a finite state machine.
 *
 * A machine has a set of states with directed edges between them. These edges
 * can be labelled with values, and sequences can be checked for acceptance
 * against the machine (possibly using customised acceptance criteria).
 */
template<class T>
class FiniteStateMachine {
public:
  /**
   * Construct an empty FSM.
   *
   * The resulting machine has no states and no edges.
   */
  FiniteStateMachine() {}

  /**
   * Copy-construct an FSM.
   *
   * This will copy all of the \ref State "States" in \p other, then reconstruct
   * the edges in the new FSM to be identical to those in \p other.
   */
  FiniteStateMachine(const FiniteStateMachine<T>& other);

  /**
   * Copy-assignment using copy-and-swap.
   */
  FiniteStateMachine<T>& operator=(FiniteStateMachine<T> other);

  /**
   * Add a new state to the machine.
   *
   * This method takes a \ref State by value, and returns a `std::shared_ptr` to
   * a \ref State. The reason for this is that we should be able to have many
   * states with the same name and properties that are not conceptually the same
   * state.
   *
   * \p s is copied into a dynamically allocated \ref State, and the returned
   * pointer is the "canonical" way of addressing the new state.
   */
  std::shared_ptr<State> AddState(State s);

  /**
   * Add a new state to the machine.
   *
   * This is a convenience method that forwards its arguments to construct a
   * \ref State.
   */
  template<class... Args>
  std::shared_ptr<State> AddState(Args&&...);

  /**
   * Add a number of default-constructed states to the machine.
   *
   * This is a convenience method that allows for a number of states to be added
   * to the machine easily, while retaining a reference to the added states (so
   * that they can be modified or edges can be added).
   */
  std::vector<std::shared_ptr<State>> AddStates(size_t n);

  /**
   * Add an epsilon edge between two states.
   */
  Edge<T> AddEdge(std::shared_ptr<State> begin, std::shared_ptr<State> end);

  /**
   * Add a labelled edge between two states.
   */
  Edge<T> AddEdge(std::shared_ptr<State> begin, std::shared_ptr<State> end, T val);
  
  /**
   * Add another finite state machine into this machine.
   *
   * The way this works is a little subtle - this machine will share the same
   * states as \p other (but with separate adjacency lists). The edges from \p
   * other are recreated in this machine. Because states are shared, \p other
   * can be destroyed and the states will remain valid as long as this machine
   * is alive.
   *
   * Note that this does not perform any "sanitisation" - accepting and initial
   * states in \p other will need to be modified before this machine can be
   * used. This is a domain-specific problem, and so this method does not
   * prescribe how that should be performed.
   */
  void AddSubMachine(const FiniteStateMachine<T>& other);

  /**
   * Get the initial state of this machine.
   *
   * If there are multiple initial states (i.e. if the machine is in an
   * intermediate form), then the result of this call should not be relied on.
   * Returns `nullptr` if there is no initial state.
   */
  std::shared_ptr<State> InitialState() const;
  
  /**
   * Get a set of all the edges leaving a state.
   */
  std::set<Edge<T>> Edges(std::shared_ptr<State> state) const;

  /**
   * Get a set of all the edge labels used in this machine.
   */
  std::set<T> AllLabels() const;

  /**
   * Returns true if this machine is deterministic.
   *
   * A machine is deterministic if there are no epsilon edges, and each state
   * has only one edge with any given label.
   */
  bool IsDeterministic() const;

  /**
   * Returns true if this machine has a single accepting state only.
   */
  bool HasSingleAccept() const;

  /**
   * Get the epsilon closure of a state.
   *
   * The epsilon closure is the set of states that are reachable using only
   * epsilon transitions.
   */
  std::set<std::shared_ptr<State>> EpsilonClosure(std::shared_ptr<State> state);

  /**
   * Get the set of accepting states for this machine.
   */
  std::set<std::shared_ptr<State>> AcceptingStates();

  /**
   * Get all the states in this machine.
   */
  std::set<std::shared_ptr<State>> States();

  /**
   * Create a new machine equivalent to this one with no epsilon edges.
   *
   * The resulting machine is behaviourally equivalent to this one, but uses the
   * epsilon closure algorithm to remove epsilon edges. The resulting machine
   * may still be nondeterministic.
   */
  FiniteStateMachine<T> EpsilonFree();

  /**
   * Create a new machine equivalent to this one, but deterministic.
   *
   * This uses the powerset construction, and the number of states in the new
   * machine may be exponentially more than the number in this machine.
   */
  FiniteStateMachine<T> Deterministic();

  /**
   * Create a new machine equivalent to this one with states relabelled.
   *
   * If a machine is created mechanically, its state names can be unwieldy (i.e.
   * too long or not existing). This relabels the states sequentially to make
   * debug output easier to understand.
   */
  FiniteStateMachine<T> Relabeled();

  /**
   * Create a new machine that is the cross product of this one with \p other.
   *
   * The cross product of machines `A` and `B` draws its states from the set `A
   * x B`, and has edges `(a_i, b_i) -> (a_j, b_i)` if `A` has an edge `a_i ->
   * a_j` (similarly for edges in `B`).
   *
   * This construction implements a form of inclusive-or on finite state
   * machines where the transitions from either machine are permitted, and
   * accepting states are `(a_i, b_i)` where either `a_i` or `b_i` are
   * accepting.
   */
  FiniteStateMachine<T> CrossProduct(FiniteStateMachine<T> other) const;

  /**
   * Returns true if this machine accepts the sequence in \p input.
   */
  template<class Container>
  bool AcceptsSequence(Container input);

  /**
   * Returns true if this machine accepts the sequence [begin, end).
   */
  template<class Iterator>
  bool AcceptsSequence(Iterator begin, Iterator end);

  /**
   * Returns true if this machine accepts the sequence in \p input.
   *
   * Uses \p acc as a custom acceptance function (rather than equality).
   */
  template<class Container, class E>
  bool AcceptsSequence(Container input, std::function<bool (E,T)> acc);

  /**
   * Returns true if this machine accepts the sequence [begin, end).
   *
   * Uses \p acc as a custom acceptance function (rather than equality).
   */
  template<class Iterator, class E>
  bool AcceptsSequence(Iterator begin, Iterator end, std::function<bool (E,T)> acc);

  /**
   * Get a DOT formatted debug output representing this machine.
   *
   * The output is a valid DOT digraph.
   */
  std::string Dot() const;

  /**
   * Get a DOT formatted debug output representing this machine.
   *
   * The output is a valid DOT digraph, and edge values are printed using the
   * function \p printer.
   */
  std::string Dot(std::function<std::string (T)> printer) const;
private:
  std::map<std::shared_ptr<State>, std::set<Edge<T>>> adjacency_; 
};

// "Law of the big two" rather than the rule of three applies here because the
// only resources the class manages are smart pointers. The reason we want to
// implement the copy constructor and copy assignment operator is that the
// shared_ptrs that an instance manages are *internal* only (to copy a machine,
// we want to copy the pointed-to states).
template<class T>
FiniteStateMachine<T>::FiniteStateMachine(const FiniteStateMachine<T>& other)
{
  auto cache = std::map<std::shared_ptr<State>, std::shared_ptr<State>>{};

  for(const auto& adj_list : other.adjacency_) {
    if(cache.find(adj_list.first) == cache.end()) {
      cache[adj_list.first] = AddState(*adj_list.first);
    }
  }

  for(const auto& adj_list : other.adjacency_) {
    for(const auto& edge : adj_list.second) {
      if(edge.IsEpsilon()) {
        AddEdge(cache[adj_list.first], cache[edge.End()]);
      } else {
        AddEdge(cache[adj_list.first], cache[edge.End()], edge.Value());
      }
    }
  }
}

template<class T>
FiniteStateMachine<T>& 
  FiniteStateMachine<T>::operator=(FiniteStateMachine<T> other)
{
  std::swap(adjacency_, other.adjacency_);
  return *this;
}

template<class T>
std::shared_ptr<State> FiniteStateMachine<T>::AddState(State s)
{
  auto pointer = std::shared_ptr<State>{new State{s}};
  adjacency_[pointer] = {};
  return pointer;
}

template<class T>
template<class... Args>
std::shared_ptr<State> FiniteStateMachine<T>::AddState(Args&&... args)
{
  return AddState(State(std::forward<Args>(args)...));
}

template<class T>
std::vector<std::shared_ptr<State>> FiniteStateMachine<T>::AddStates(size_t n)
{
  std::vector<std::shared_ptr<State>> states;
  states.reserve(n);

  for(auto i = 0; i < n; ++i) {
    states.push_back(AddState());
  }

  return states;
}

template<class T>
Edge<T> FiniteStateMachine<T>::AddEdge(std::shared_ptr<State> begin, 
                                       std::shared_ptr<State> end)
{
  auto edge = Edge<T>{end};
  adjacency_[begin].insert(edge);
  return edge;
}

template<class T>
Edge<T> FiniteStateMachine<T>::AddEdge(std::shared_ptr<State> begin, 
                                       std::shared_ptr<State> end, T val)
{
  auto edge = Edge<T>{end, val};
  adjacency_[begin].insert(edge);
  return edge;
}

template<class T>
void FiniteStateMachine<T>::AddSubMachine(const FiniteStateMachine<T>& other)
{
  for(const auto& adj_list : other.adjacency_) {
    adjacency_[adj_list.first] = adj_list.second;
  }
}

template<class T>
std::shared_ptr<State> FiniteStateMachine<T>::InitialState() const
{
  auto found = std::find_if(adjacency_.begin(), adjacency_.end(),
    [=](auto p) { 
      return p.first->initial; 
    }
  );

  if(found != adjacency_.end()) {
    return found->first;
  } else {
    return nullptr;
  }
}

template<class T>
std::set<Edge<T>> FiniteStateMachine<T>::Edges(std::shared_ptr<State> state) const
{
  if(adjacency_.find(state) == adjacency_.end()) {
    return {};
  }

  return adjacency_.find(state)->second;
}

template<class T>
std::set<T> FiniteStateMachine<T>::AllLabels() const
{
  auto labels = std::set<T>{};
  for(const auto& pair : adjacency_) {
    for(const auto& edge : pair.second) {
      if(!edge.IsEpsilon()) { labels.insert(edge.Value()); }
    }
  }
  return labels;
}

template<class T>
bool FiniteStateMachine<T>::IsDeterministic() const
{
  return std::all_of(adjacency_.begin(), adjacency_.end(),
    [=](auto adj_list) {
      auto epsilon_free = std::all_of(adj_list.second.begin(), adj_list.second.end(),
        [=](auto edge) {
          return !edge.IsEpsilon();
        }
      );

      auto edges = std::set<T>{};
      auto unique = true;
      for(const auto& edge : adj_list.second) {
        if(edge.IsEpsilon()) {
          continue;
        }

        auto found = edges.find(edge.Value());
        if(found == edges.end()) {
          edges.insert(edge.Value());
        } else {
          unique = false;
          break;
        }
      }

      return epsilon_free && unique;
    }
  );
}

template<class T>
bool FiniteStateMachine<T>::HasSingleAccept() const
{
  return std::count_if(adjacency_.begin(), adjacency_.end(),
    [=](auto adj_list) {
      return adj_list.first->accepting;
    }
  ) == 1;
}

template<class T>
std::set<std::shared_ptr<State>>
  FiniteStateMachine<T>::EpsilonClosure(std::shared_ptr<State> state)
{
  auto ret = std::set<std::shared_ptr<State>>{ state };
  auto visited = decltype(ret){};

  auto work_queue = std::queue<std::shared_ptr<State>>{};
  work_queue.push(state);

  while(!work_queue.empty()) {
    auto next = work_queue.front();
    work_queue.pop();

    if(visited.find(next) != visited.end()) {
      continue;
    } else {
      visited.insert(next);
    }

    for(const auto& edge : adjacency_[next]) {
      if(edge.IsEpsilon()) {
        ret.insert(edge.End());
        work_queue.push(edge.End());
      }
    }
  }

  return ret;
}

template<class T>
std::set<std::shared_ptr<State>> FiniteStateMachine<T>::AcceptingStates()
{
  auto ret = std::set<std::shared_ptr<State>>{};

  for(const auto& adj_list : adjacency_) {
    if(adj_list.first->accepting) {
      ret.insert(adj_list.first);
    }
  }

  return ret;
}

template<class T>
std::set<std::shared_ptr<State>> FiniteStateMachine<T>::States()
{
  auto ret = std::set<std::shared_ptr<State>>{};

  for(auto&& adj_list : adjacency_) {
    ret.insert(adj_list.first);
  }

  return ret;
}

template<class T>
FiniteStateMachine<T> FiniteStateMachine<T>::EpsilonFree()
{
  auto closure_map = std::map<std::shared_ptr<State>, std::shared_ptr<State>>{};
  auto eps_free = FiniteStateMachine<T>{};

  for(const auto& adj_list : adjacency_) {
    const auto& state = adj_list.first;
    auto combined = State::Combined(EpsilonClosure(state));
    combined.initial = state->initial;

    auto added = eps_free.AddState(combined);
    closure_map[state] = added;
  }

  for(const auto& adj_list : adjacency_) {
    const auto& state = adj_list.first;

    for(const auto& member : EpsilonClosure(state)) {
      for(const auto& edge : adjacency_[member]) {
        if(!edge.IsEpsilon()) {
          eps_free.AddEdge(closure_map[state], closure_map[edge.End()], edge.Value());
        }
      }
    }
  }

  return eps_free;
}

template<class T>
FiniteStateMachine<T> FiniteStateMachine<T>::Deterministic()
{
  auto eps_free = EpsilonFree();
  auto dfa = FiniteStateMachine<T>{};

  auto initial_state = eps_free.InitialState();
  auto work_queue = std::queue<std::set<std::shared_ptr<State>>>{};
  work_queue.push({initial_state});

  auto combined = std::map<std::set<std::shared_ptr<State>>, std::shared_ptr<State>>{};
  auto visited = std::set<std::set<std::shared_ptr<State>>>{};

  while(!work_queue.empty()) {
    auto next = work_queue.front();
    work_queue.pop();

    if(visited.find(next) == visited.end()) {
      visited.insert(next);
    } else {
      continue;
    }

    if(combined.find(next) == combined.end()) {
      auto cs = State::Combined(next);
      auto csa = dfa.AddState(cs);
      combined[next] = csa;
    }

    auto reachable = std::map<T, std::set<std::shared_ptr<State>>>{};
    for(const auto& state : next) {
      for(const auto& edge : eps_free.adjacency_[state]) {
        reachable[edge.Value()].insert(edge.End());
      }
    }

    for(const auto& reach_pair : reachable) {
      work_queue.push(reach_pair.second);

      if(combined.find(reach_pair.second) == combined.end()) {
        auto cs = State::Combined(reach_pair.second);
        auto csa = dfa.AddState(cs);
        combined[reach_pair.second] = csa;
      }

      dfa.AddEdge(combined[next], combined[reach_pair.second], reach_pair.first);
    }
  }

  return dfa;
}

template<class T>
FiniteStateMachine<T> FiniteStateMachine<T>::Relabeled()
{
  auto new_fsm = *this;
  int label = 0;

  for(const auto& adj_list : new_fsm.adjacency_) {
     adj_list.first->name = "s" + std::to_string(label++);
  }

  return new_fsm;
}

template<class T>
FiniteStateMachine<T> FiniteStateMachine<T>::CrossProduct(FiniteStateMachine<T> other) const
{
  FiniteStateMachine<T> fsm;

  using StatePair = std::pair<std::shared_ptr<State>, std::shared_ptr<State>>;
  std::map<StatePair, std::shared_ptr<State>> mapping;

  for(auto&& this_list : adjacency_) {
    for(auto&& other_list: other.adjacency_) {
      std::set<std::shared_ptr<State>> p{this_list.first, other_list.first};
      mapping[std::make_pair(this_list.first, other_list.first)] =
        fsm.AddState(State::Combined(p));
    }
  }

  mapping[std::make_pair(InitialState(), other.InitialState())]->initial = true;
  for(auto&& pair : mapping) {
    if(pair.first.first->accepting || pair.first.second->accepting) {
      pair.second->accepting = true;
    }
  }

  for(auto&& this_list : adjacency_) {
    for(auto&& other_list : other.adjacency_) {
      auto p = mapping[std::make_pair(this_list.first, other_list.first)];
      for(auto&& edge : this_list.second) {
        auto q = mapping[std::make_pair(edge.End(), other_list.first)];
        if(edge.IsEpsilon()) {
          fsm.AddEdge(p, q);
        } else {
          fsm.AddEdge(p, q, edge.Value());
        }
      }

      for(auto&& edge : other_list.second) {
        auto q = mapping[std::make_pair(this_list.first, edge.End())];
        if(edge.IsEpsilon()) {
          fsm.AddEdge(p, q);
        } else {
          fsm.AddEdge(p, q, edge.Value());
        }
      }
    }
  }

  return fsm;
}

template<class T>
template<class Container>
bool FiniteStateMachine<T>::AcceptsSequence(Container input)
{
  return AcceptsSequence(std::begin(input), std::end(input));
}

template<class T>
template<class Iterator>
bool FiniteStateMachine<T>::AcceptsSequence(Iterator begin, Iterator end)
{
  using value_type = typename std::iterator_traits<Iterator>::value_type;
  return AcceptsSequence<Iterator, T>(begin, end, std::equal_to<value_type>{});
}

template<class T>
template<class Container, class E>
bool FiniteStateMachine<T>::AcceptsSequence(Container input, std::function<bool (E,T)> acc)
{
  return AcceptsSequence(std::begin(input), std::end(input), acc);
}

template<class T>
template<class Iterator, class E>
bool FiniteStateMachine<T>::AcceptsSequence(Iterator begin, Iterator end, 
                                            std::function<bool (E,T)> acc)
{
  static_assert(std::is_same<typename std::iterator_traits<Iterator>::value_type, E>::value,
                "Wrong iterator type used for acceptance check");

  auto fsm = (IsDeterministic() ? *this : Deterministic());

  auto state = fsm.InitialState();
  for(auto it = begin; it != end; it++) {
    auto accepting_edge = std::find_if(fsm.adjacency_[state].begin(), fsm.adjacency_[state].end(),
      [=](Edge<T> edge) {
        return edge.Accepts(*it, acc);
      }
    );

    if(accepting_edge == fsm.adjacency_[state].end()) {
      return false;
    } else {
      state = accepting_edge->End();
    }
  }

  return state->accepting;
}

template<class T>
std::string FiniteStateMachine<T>::Dot() const
{
  return Dot([](auto t) {
    std::stringstream ss;
    ss << t;
    return ss.str();
  });
}

template<class T>
std::string FiniteStateMachine<T>::Dot(std::function<std::string (T)> printer) const
{
  std::stringstream out;

  out << "digraph {\n";
  out << "  node[shape=circle]\n";
  out << "  graph[charset=utf8]\n";
  for(const auto& adj_list : adjacency_) {
    out << "  " << adj_list.first->Dot() << '\n';
    for(const auto& edge : adj_list.second) {
      out << "  \"" << adj_list.first->name << "\" -> " << edge.Dot(printer) << "\n";
    }
  }
  out << "}";

  return out.str();
}

#endif
