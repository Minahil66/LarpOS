// Heap.h - Phase 4 (RED): Generic Array-Backed Binary Min-Heap & LoopPriorityQueue
// Implements BinaryHeap<T, Compare> in an array with O(log n) siftUp and siftDown.
// LoopPriorityQueue is a true min-heap ordered by "higher loop score counts as smaller"
// so popping the top yields the highest-ranked loop first.

#ifndef LARPOS_HEAP_H
#define LARPOS_HEAP_H

#include "Dominator.h"
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace red {

template <typename T, typename Compare = std::less<T>>
class BinaryHeap {
public:
    BinaryHeap() = default;
    explicit BinaryHeap(Compare comp) : comp_(std::move(comp)) {}

    // Bottom-up O(n) heapify constructor
    explicit BinaryHeap(std::vector<T> items, Compare comp = Compare())
        : data_(std::move(items)), comp_(std::move(comp)) {
        if (data_.size() > 1) {
            for (std::size_t i = data_.size() / 2; i > 0; --i) {
                siftDown(i - 1);
            }
        }
    }

    // Push an item in O(log n) time via siftUp
    void push(const T& item) {
        data_.push_back(item);
        siftUp(data_.size() - 1);
    }

    void push(T&& item) {
        data_.push_back(std::move(item));
        siftUp(data_.size() - 1);
    }

    // Peek at the root item in O(1) time
    const T& top() const {
        if (data_.empty()) {
            throw std::underflow_error("BinaryHeap::top() called on empty heap");
        }
        return data_.front();
    }

    // Pop and return the root item in O(log n) time via siftDown
    T pop() {
        if (data_.empty()) {
            throw std::underflow_error("BinaryHeap::pop() called on empty heap");
        }
        T root = std::move(data_.front());
        data_.front() = std::move(data_.back());
        data_.pop_back();
        if (!data_.empty()) {
            siftDown(0);
        }
        return root;
    }

    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }
    void clear() { data_.clear(); }

    // Check that the array satisfies the heap invariant for every parent/child pair
    bool isValidHeap() const {
        const std::size_t n = data_.size();
        for (std::size_t i = 0; i < n; ++i) {
            std::size_t l = leftChild(i);
            std::size_t r = rightChild(i);
            if (l < n && comp_(data_[l], data_[i])) return false;
            if (r < n && comp_(data_[r], data_[i])) return false;
        }
        return true;
    }

    const std::vector<T>& data() const { return data_; }

private:
    std::vector<T> data_;
    Compare comp_{};

    static std::size_t parent(std::size_t i) { return (i - 1) / 2; }
    static std::size_t leftChild(std::size_t i) { return (2 * i) + 1; }
    static std::size_t rightChild(std::size_t i) { return (2 * i) + 2; }

    void siftUp(std::size_t idx) {
        while (idx > 0) {
            std::size_t p = parent(idx);
            if (comp_(data_[idx], data_[p])) {
                std::swap(data_[idx], data_[p]);
                idx = p;
            } else {
                break;
            }
        }
    }

    void siftDown(std::size_t idx) {
        const std::size_t n = data_.size();
        while (true) {
            std::size_t best = idx;
            std::size_t l = leftChild(idx);
            std::size_t r = rightChild(idx);

            if (l < n && comp_(data_[l], data_[best])) {
                best = l;
            }
            if (r < n && comp_(data_[r], data_[best])) {
                best = r;
            }
            if (best != idx) {
                std::swap(data_[idx], data_[best]);
                idx = best;
            } else {
                break;
            }
        }
    }
};

// Comparator for LoopPriorityQueue:
// Orders so that a loop with a HIGHER score is considered "smaller" in the min-heap,
// placing the most important/complex loop at the root (index 0) of the BinaryHeap.
struct HigherScoreFirst {
    bool operator()(const LoopInfo& a, const LoopInfo& b) const {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        if (a.nesting_depth != b.nesting_depth) {
            return a.nesting_depth > b.nesting_depth;
        }
        if (a.instruction_count != b.instruction_count) {
            return a.instruction_count > b.instruction_count;
        }
        return a.header < b.header;
    }
};

class LoopPriorityQueue {
public:
    LoopPriorityQueue() = default;
    explicit LoopPriorityQueue(const std::vector<LoopInfo>& loops);

    void push(const LoopInfo& loop);
    const LoopInfo& top() const;
    LoopInfo pop();

    bool empty() const { return heap_.empty(); }
    std::size_t size() const { return heap_.size(); }

    // Rank all loops from highest priority (best first) to lowest priority using BinaryHeap
    static std::vector<LoopInfo> rank(const std::vector<LoopInfo>& loops);

private:
    BinaryHeap<LoopInfo, HigherScoreFirst> heap_;
};

} // namespace red

#endif // LARPOS_HEAP_H
