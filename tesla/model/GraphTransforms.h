#ifndef GRAPH_TRANSFORMS_H
#define GRAPH_TRANSFORMS_H

#include "EventGraph.h"

namespace GraphTransforms {

Event *CallsOnly(Event *e);

struct Tag{
  Tag(Event *e, string s) :
    ev(e), str_(s) {}

  Event *operator()(Event *e);

private:
  Event *ev;
  string str_;
};

}

#endif
