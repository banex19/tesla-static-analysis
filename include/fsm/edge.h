/**
 * \file
 */

#ifndef FSM_EDGE_H
#define FSM_EDGE_H

#include <fsm/state.h>

#include <functional>
#include <memory>
#include <sstream>
#include <string>

/**
 * Represents a single edge in a finite state machine.
 *
 * An edge knows its end state, but is independent of its start state (this
 * information is stored in the finite state machine itself).
 *
 * Edges can have a value associated with them (which is then used to implement
 * acceptance and transducing, or they can be epsilon-edges).
 */
template<class T>
class Edge {
public:
  /**
   * Construct an epsilon edge (one that has no associated value).
   *
   * \param end The end state of the edge
   */
  Edge(std::shared_ptr<State> end) :
    end_(end), edge_value_(nullptr) {}

  /**
   * Construct a labelled edge (one that has an associated value).
   *
   * \param end The end state of the edge
   * \param value The edge value
   */
  Edge(std::shared_ptr<State> end, T val) :
    end_(end), edge_value_(new T{val}) {}

  /**
   * Copy construct an edge.
   *
   * This copies the label value into a new `unique_ptr` (as we're really using
   * it to simulate an optional).
   */
  Edge(const Edge<T>& other) :
    end_(other.end_),
    edge_value_(other.edge_value_ ? new T{*other.edge_value_} : nullptr) {}

  /**
   * Copy-and-swap assignment.
   */
  Edge<T>& operator=(Edge<T> other) {
    std::swap(*this, other);
    return *this;
  }

  /**
   * Returns true if the edge accepts a value.
   *
   * The function \p comp is used to customise acceptance criteria for different
   * types. The edge label and checked value are passed as the parameters to \p
   * comp.
   */
  template<class E>
  bool Accepts(E val, std::function<bool (E,T)> comp) const;

  /**
   * Returns true if the edge is labelled with a value.
   *
   * \p val is checked against the edge label using `std::equal_to`. If they are
   * equal, the edge accepts \p val.
   */
  bool Accepts(T val) const;

  /**
   * Generate a new value from \p val and the edge label.
   *
   * This can be used to implement finite state transducers that transform input
   * sequences into an output sequence.
   */
  template<class E, class O>
  O Transduce(E val, std::function<O (E,T)> output) const;

  /**
   * Returns true if the edge is an epsilon edge (it has no label).
   */
  bool IsEpsilon() const { return !edge_value_; }

  /**
   * Access the value labelling this edge.
   *
   * This is not safe to call if the edge is an epsilon edge.
   */
  const T& Value() const { return *edge_value_; }

  /**
   * Get the state at the end of this edge.
   */
  std::shared_ptr<State> End() const { return end_; }

  /**
   * Lexicographical comparison on (end state, value).
   */
  bool operator<(const Edge& other) const;

  /**
   * Get a DOT formatted representation of this edge.
   *
   * The returned string can be used in a graphviz graph. This overload uses a
   * custom printing function to print edge labels.
   */
  std::string Dot(std::function<std::string (T)> printer) const;

  /**
   * Get a DOT formatted representation of this edge.
   *
   * The returned string can be used in a graphviz graph.
   */
  std::string Dot() const;
private:
  std::shared_ptr<State> end_;
  std::unique_ptr<T> edge_value_;
};

template<class T>
template<class E>
bool Edge<T>::Accepts(E val, std::function<bool (E,T)> acceptor) const
{
  return edge_value_ && acceptor(val, *edge_value_);
}

template<class T>
bool Edge<T>::Accepts(T val) const
{
  return Accepts<T>(val, std::equal_to<T>{});
}

template<class T>
std::string Edge<T>::Dot(std::function<std::string (T)> printer) const {
  std::stringstream out;

  out << "\"" << end_->name << "\"";
  out << " [label=\"  ";
  if(edge_value_) {
    out << printer(*edge_value_);
  } else {
    out << "&#949;";
  }
  out << "\"]";

  return out.str();
}

template<class T>
std::string Edge<T>::Dot() const {
  return Dot([](auto t) {
    std::stringstream ss;
    ss << t;
    return ss.str();
  });
}

// TODO: this dereferences the edge value, meaning that it will crash when
// called on an epsilon-edge. Best way to handle this is maybe to add a default
// O param?
template<class T>
template<class E, class O>
O Edge<T>::Transduce(E val, std::function<O (E,T)> output) const
{
  return output(val, *edge_value_);
}

template<class T>
bool Edge<T>::operator<(const Edge& other) const {
  return (end_ < other.end_) || 
         (end_ == other.end_ && edge_value_ < other.edge_value_);
}

#endif
