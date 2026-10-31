// Dominator.h - Phase 3 (RED): The Dominator Tree & Back-Edge Loop Detection
// Implements the Cooper-Harvey-Kennedy iterative dominance algorithm ("A Simple, Fast Dominance Algorithm")
// on plain adjacency lists (std::vector<std::vector<int>>), handling unreachable dead blocks cleanly,
// and detects natural loops (merging loops that share a header, handling self-loops, computing nesting depth).

#ifndef LARPOS_DOMINATOR_H
#define LARPOS_DOMINATOR_H

#include "Dfs.h"
#include <cstddef>
#include <utility>
#include <vector>

namespace red {

struct BackEdge {
    int latch = -1;   // Source block u jumping backward
    int header = -1;  // Target dominator/boss block v (where v dominates u)

    bool operator==(const BackEdge& other) const {
        return latch == other.latch && header == other.header;
    }
};

struct DominatorTree {
    int entry = 0;

    // Immediate dominator of each block (size == N):
    // - idom[entry] == entry
    // - idom[u] == -1 if block u is unreachable from entry
    std::vector<int> idom;

    // Dominator tree children adjacency list: children[u] holds blocks immediately dominated by u
    std::vector<std::vector<int>> children;

    // Depth in the dominator tree (0 for entry, -1 for unreachable blocks)
    std::vector<int> depth;

    // Reverse Postorder traversal result used to compute the dominators
    DfsResult dfs;

    // Returns true iff block `a` dominates block `b` (every path from entry to `b` goes through `a`).
    // Unreachable blocks neither dominate nor are dominated by anything.
    bool dominates(int a, int b) const;

    // Returns true iff `a` strictly dominates `b` (`a != b` and `dominates(a, b)`).
    bool strictlyDominates(int a, int b) const;
};

struct LoopInfo {
    int header = -1;                  // Loop boss / header block ID
    std::vector<int> latches;         // All back-edge source blocks u with back-edge u -> header
    std::vector<int> body;            // Sorted block IDs belonging to the natural loop
    std::size_t block_count = 0;      // Number of basic blocks in the loop body
    std::size_t instruction_count = 0;// Total instruction count across all blocks in the loop
    std::size_t back_edge_count = 0;  // Number of back-edges merged into this loop header
    int nesting_depth = 1;            // 1 = outer loop; >1 = nested inside outer loop(s)
    double score = 0.0;               // Heuristic complexity/size ranking score for LoopPriorityQueue
};

// Build the Dominator Tree using the Cooper-Harvey-Kennedy iterative algorithm.
// Unreachable blocks are skipped and have idom[u] == -1.
DominatorTree computeDominators(const std::vector<std::vector<int>>& succ, int entry = 0);

// Find all back-edges u -> v where both u and v are reachable and v dominates u (including self-loops u == v).
std::vector<BackEdge> findBackEdges(
    const std::vector<std::vector<int>>& succ,
    const DominatorTree& dom
);

// Extract natural loops from the CFG and DominatorTree:
// - Back-edges targeting the same header are merged into a single natural loop.
// - Handles single-block self-loops (u -> u).
// - Computes nesting depth, block count, instruction count (defaults to 1 per block if instrCountPerBlock is empty),
//   and heuristic priority score.
std::vector<LoopInfo> findLoops(
    const std::vector<std::vector<int>>& succ,
    const DominatorTree& dom,
    const std::vector<int>& instrCountPerBlock = {}
);

} // namespace red

#endif // LARPOS_DOMINATOR_H
