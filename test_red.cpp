// test_red.cpp - Comprehensive Verification Suite for LarpOS RED Module
// Tests:
// 1. DFS and Reverse Postorder (iterative vs recursive cross-check, unreachable blocks).
// 2. Diamond-shaped graph dominators.
// 3. Nested loops with verified nesting depth and instruction counts.
// 4. Self-loops and multiple back-edges to a single loop header (merged natural loop).
// 5. BinaryHeap<int> on 1,000 random numbers + LoopPriorityQueue ranking.
// 6. 200 random graphs (with unreachable dead blocks, branches, and cycles), checking
//    Cooper-Harvey-Kennedy dominators against the textbook path-removal definition.

#include "Dfs.h"
#include "Dominator.h"
#include "Heap.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

namespace {

// Slow textbook definition of dominance for cross-checking:
// Node `d` dominates node `u` iff `u` is reachable from `entry` AND removing `d`
// makes `u` unreachable from `entry`.
std::vector<std::vector<bool>> computeTextbookDominance(
    const std::vector<std::vector<int>>& succ,
    int entry
) {
    const int n = static_cast<int>(succ.size());
    std::vector<std::vector<bool>> dom(n, std::vector<bool>(n, false));

    auto reachableWithout = [&](int banned) {
        std::vector<bool> vis(n, false);
        if (entry < 0 || entry >= n || entry == banned) {
            return vis;
        }
        std::vector<int> stack = {entry};
        vis[entry] = true;
        while (!stack.empty()) {
            int u = stack.back();
            stack.pop_back();
            for (int v : succ[u]) {
                if (v >= 0 && v < n && v != banned && !vis[v]) {
                    vis[v] = true;
                    stack.push_back(v);
                }
            }
        }
        return vis;
    };

    std::vector<bool> base_reach = reachableWithout(-1);
    for (int d = 0; d < n; ++d) {
        if (!base_reach[d]) continue;
        std::vector<bool> without_d = reachableWithout(d);
        for (int u = 0; u < n; ++u) {
            if (base_reach[u] && !without_d[u]) {
                dom[d][u] = true;
            }
        }
    }
    return dom;
}

void testDfsAndRpo() {
    // Graph with a DAG + back-edge + unreachable node 5
    std::vector<std::vector<int>> succ = {
        {1, 2}, // 0
        {3},    // 1
        {3},    // 2
        {1, 4}, // 3 (back-edge 3 -> 1)
        {},     // 4
        {4}     // 5 (unreachable dead block)
    };

    auto iter_res = red::runDfsIterative(succ, 0);
    auto rec_res  = red::runDfsRecursive(succ, 0);

    assert(iter_res.preorder == rec_res.preorder);
    assert(iter_res.postorder == rec_res.postorder);
    assert(iter_res.rpo == rec_res.rpo);
    assert(iter_res.pre_num == rec_res.pre_num);
    assert(iter_res.post_num == rec_res.post_num);
    assert(iter_res.rpo_index == rec_res.rpo_index);

    assert(iter_res.isReachable(0));
    assert(iter_res.isReachable(4));
    assert(!iter_res.isReachable(5));
    assert(iter_res.rpo.front() == 0);
    assert(iter_res.rpo_index[5] == -1);
}

void testDiamondGraph() {
    // Classic diamond: 0 -> {1, 2} -> 3, plus dead block 4
    std::vector<std::vector<int>> succ = {
        {1, 2}, // 0
        {3},    // 1
        {3},    // 2
        {},     // 3
        {3}     // 4 (unreachable)
    };

    auto dom = red::computeDominators(succ, 0);
    assert(dom.idom[0] == 0);
    assert(dom.idom[1] == 0);
    assert(dom.idom[2] == 0);
    assert(dom.idom[3] == 0); // Both 1 and 2 merge at 3, so 0 is idom of 3
    assert(dom.idom[4] == -1); // Unreachable block marked -1

    assert(dom.dominates(0, 3));
    assert(!dom.dominates(1, 3));
    assert(!dom.dominates(2, 3));
    assert(!dom.dominates(4, 3));
}

void testNestedLoopsAndInstructions() {
    // 0 -> 1 (outer header) -> 2 (inner header) -> 3 (inner latch -> 2, and -> 4)
    // 4 (outer latch -> 1, and -> 5 exit)
    std::vector<std::vector<int>> succ = {
        {1},    // 0: 5 instrs
        {2},    // 1: 4 instrs
        {3},    // 2: 10 instrs
        {2, 4}, // 3: 6 instrs (back-edge 3 -> 2)
        {1, 5}, // 4: 3 instrs (back-edge 4 -> 1)
        {}      // 5: 2 instrs
    };
    std::vector<int> instrs = {5, 4, 10, 6, 3, 2};

    auto dom   = red::computeDominators(succ, 0);
    auto loops = red::findLoops(succ, dom, instrs);
    assert(loops.size() == 2);

    const red::LoopInfo* outer = nullptr;
    const red::LoopInfo* inner = nullptr;
    for (const auto& l : loops) {
        if (l.header == 1) outer = &l;
        if (l.header == 2) inner = &l;
    }
    assert(outer != nullptr && inner != nullptr);

    // Outer loop contains {1, 2, 3, 4}: 4 + 10 + 6 + 3 = 23 instructions, depth = 1
    assert((outer->body == std::vector<int>{1, 2, 3, 4}));
    assert(outer->instruction_count == 23);
    assert(outer->nesting_depth == 1);

    // Inner loop contains {2, 3}: 10 + 6 = 16 instructions, depth = 2
    assert((inner->body == std::vector<int>{2, 3}));
    assert(inner->instruction_count == 16);
    assert(inner->nesting_depth == 2);

    // Ranked via LoopPriorityQueue: inner nested loop has higher score and comes first
    auto top = red::LoopPriorityQueue::rank(loops);
    assert(top.size() == 2);
    assert(top[0].header == 2);
    assert(top[1].header == 1);
}

void testSelfLoopsAndMergedHeaders() {
    // 0 -> 1
    // 1 -> {2, 3} with both 2 -> 1 and 3 -> 1 (two back-edges to header 1)
    // 1 -> 4, where 4 -> 4 (self-loop on block 4) and 4 -> 5
    std::vector<std::vector<int>> succ = {
        {1},       // 0
        {2, 3, 4}, // 1 (loop header with two latches: 2 and 3)
        {1},       // 2 (latch 1)
        {1},       // 3 (latch 2)
        {4, 5},    // 4 (self-loop 4 -> 4)
        {}         // 5
    };
    std::vector<int> instrs = {2, 5, 4, 6, 8, 1};

    auto dom   = red::computeDominators(succ, 0);
    auto loops = red::findLoops(succ, dom, instrs);

    // Should be 2 loops total (header 1 merged from 2 back-edges, and self-loop header 4)
    assert(loops.size() == 2);

    const red::LoopInfo* merged = nullptr;
    const red::LoopInfo* self_loop = nullptr;
    for (const auto& l : loops) {
        if (l.header == 1) merged = &l;
        if (l.header == 4) self_loop = &l;
    }
    assert(merged != nullptr && self_loop != nullptr);

    assert(merged->back_edge_count == 2);
    assert((merged->latches == std::vector<int>{2, 3}));
    assert((merged->body == std::vector<int>{1, 2, 3}));
    assert(merged->instruction_count == (5 + 4 + 6));

    assert(self_loop->back_edge_count == 1);
    assert((self_loop->latches == std::vector<int>{4}));
    assert((self_loop->body == std::vector<int>{4}));
    assert(self_loop->instruction_count == 8);
}

void testBinaryHeap1000RandomNumbers() {
    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> dist(-100000, 100000);

    red::BinaryHeap<int> min_heap;
    std::vector<int> expected;
    expected.reserve(1000);

    for (int i = 0; i < 1000; ++i) {
        int val = dist(rng);
        min_heap.push(val);
        expected.push_back(val);
    }

    assert(min_heap.size() == 1000);
    assert(min_heap.isValidHeap());

    std::sort(expected.begin(), expected.end());
    for (int i = 0; i < 1000; ++i) {
        assert(min_heap.top() == expected[i]);
        int popped = min_heap.pop();
        assert(popped == expected[i]);
    }
    assert(min_heap.empty());
}

void test200RandomGraphsAgainstTextbook() {
    std::mt19937 rng(42);

    for (int trial = 0; trial < 200; ++trial) {
        int n = 5 + (trial % 25); // 5 to 29 nodes
        std::vector<std::vector<int>> succ(n);

        // Random edge density between 15% and 45%
        std::uniform_real_distribution<double> prob(0.0, 1.0);
        double density = 0.15 + 0.30 * (static_cast<double>(trial % 10) / 10.0);

        for (int u = 0; u < n; ++u) {
            for (int v = 0; v < n; ++v) {
                if (prob(rng) < density) {
                    succ[u].push_back(v);
                }
            }
        }

        auto dom = red::computeDominators(succ, 0);
        auto textbook = computeTextbookDominance(succ, 0);

        // Verify reachability and every pair (a, b) dominance matches the textbook definition
        for (int a = 0; a < n; ++a) {
            bool reach_a = textbook[0][a];
            assert(dom.dfs.isReachable(a) == reach_a);
            if (!reach_a) {
                assert(dom.idom[a] == -1);
            } else {
                assert(dom.idom[a] != -1);
            }

            for (int b = 0; b < n; ++b) {
                assert(dom.dominates(a, b) == textbook[a][b]);
            }
        }
    }
}

} // namespace

int main() {
    testDfsAndRpo();
    testDiamondGraph();
    testNestedLoopsAndInstructions();
    testSelfLoopsAndMergedHeaders();
    testBinaryHeap1000RandomNumbers();
    test200RandomGraphsAgainstTextbook();

    std::cout << "ALL RED TESTS PASSED\n";
    return 0;
}
