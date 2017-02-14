#include <set>

#include "EventGraph.h"

using std::set;

EventGraph::EventGraph(Function *b) {
  RootNode = new FuncEntryNode(b);
  ExitNode = nullptr;

  map<BasicBlock *, EventGraph *> cache;

  auto entry = &b->getEntryBlock();
  queue<BasicBlock *> blocks;
  set<BasicBlock *> visits;

  blocks.push(entry);
  
  while(!blocks.empty()) {
    auto bb = blocks.front();
    blocks.pop();

    if(visits.find(bb) == visits.end()) {
      visits.insert(bb);
    } else {
      continue;
    }

    auto gr = BBCachedCreate(cache, bb);
    RootNode->addNeighbour(gr->RootNode);

    auto term = bb->getTerminator();
    for(auto i = 0; i < term->getNumSuccessors(); i++) {
      auto suc = term->getSuccessor(i);
      auto sucGr = BBCachedCreate(cache, suc);
      gr->ExitNode->addNeighbour(sucGr->RootNode);

      blocks.push(suc);
    }
  }
  /*for(auto &BB : F) {
    auto eg = new EventGraph{&BB};
    if(!eg->Empty()) {
      errs() << eg->GraphViz();
    }
  }*/
}

EventGraph::EventGraph(BasicBlock *bb) {
  RootNode = nullptr;

  auto tail = RootNode;
  for(auto &I : *bb) {
    if(auto call = dyn_cast<CallInst>(&I)) {
      if(call->getCalledFunction()->isDeclaration()) { continue; }

      auto node = new CallNode(call);

      if(!tail) {
        RootNode = node;
        tail = node;
      } else {
        tail->addNeighbour(node);
        tail = node;
      }
    }
  }

  ExitNode = tail;

  if(!RootNode) {
    RootNode = new EmptyNode;
    ExitNode = RootNode;
  }
}

EventGraph *EventGraph::BBCachedCreate(map<BasicBlock *, EventGraph *> &c, BasicBlock *bb) {
  if(c.find(bb) != c.end()) {
    return c[bb];
  }

  auto ret = new EventGraph{bb};
  c[bb] = ret;
  return ret;
}

EventNode *EventNode::splice(EventGraph *g) {
  addNeighbour(g->RootNode);
  return g->ExitNode;
}

void EventGraph::concat(EventGraph *other) {
  if(!ExitNode) { 
    return;
  }

  ExitNode = ExitNode->splice(other);
}

string EventNode::GraphViz() const {
  stringstream ss;
  ss << name() << ";\n";
  for(auto n : neighbours) {
    ss << name() << " -> " << n->name() << ";\n";
  }
  return ss.str();
}

string EventGraph::GraphViz() const {
  stringstream ss;
  ss << "subgraph {\n";

  queue<EventNode *> q;
  set<EventNode *> visits;

  q.push(RootNode);
  while(!q.empty()) {
    auto node = q.front();
    q.pop();

    if(visits.find(node) == visits.end()) {
      visits.insert(node);
    } else {
      continue;
    }
    
    if(node) {
      ss << node->GraphViz();
      for(auto n : node->neighbours) {
        q.push(n);
      }
    }
  }

  ss << "}\n";
  return ss.str();
}
