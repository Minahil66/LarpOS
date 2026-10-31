// Heap.cpp - Phase 4 (RED): LoopPriorityQueue Implementation

#include "Heap.h"

namespace red {

LoopPriorityQueue::LoopPriorityQueue(const std::vector<LoopInfo> &loops)
    : heap_(loops) {}

void LoopPriorityQueue::push(const LoopInfo &loop) { heap_.push(loop); }

const LoopInfo &LoopPriorityQueue::top() const { return heap_.top(); }

LoopInfo LoopPriorityQueue::pop() { return heap_.pop(); }

std::vector<LoopInfo>
LoopPriorityQueue::rank(const std::vector<LoopInfo> &loops) {
  LoopPriorityQueue pq(loops);
  std::vector<LoopInfo> ranked;
  ranked.reserve(pq.size());
  while (!pq.empty()) {
    ranked.push_back(pq.pop());
  }
  return ranked;
}

} // namespace red
