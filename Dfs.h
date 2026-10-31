// Dfs.h - Phase 2 (RED): Graph Traversal, Pre/Post-Order Numbering & Reverse Postorder (RPO)
// Takes a plain adjacency list (std::vector<std::vector<int>>) so RED compiles standalone
// without colliding with BLUE's Graph.h / Graph.cpp.

#ifndef LARPOS_DFS_H
#define LARPOS_DFS_H

#include <vector>

namespace red {

struct DfsResult {
    int entry = 0;

    // Order of block IDs visited upon entering a node (Pre-order)
    std::vector<int> preorder;

    // Order of block IDs upon finishing/leaving a node (Post-order)
    std::vector<int> postorder;

    // Reverse Postorder (RPO): exact reverse of postorder.
    // Mathematically required by the Cooper-Harvey-Kennedy Dominator algorithm so that
    // every acyclic predecessor of a node is processed before the node itself.
    std::vector<int> rpo;

    // Direct O(1) indexed lookup tables (size == number of nodes N, -1 if unreachable)
    std::vector<int> pre_num;     // Pre-order number assigned when entering a node
    std::vector<int> post_num;    // Post-order number assigned when leaving a node
    std::vector<int> rpo_index;   // Position in Reverse Postorder (0 = entry, -1 = unreachable)
    std::vector<int> parent;      // DFS spanning tree parent (-1 for root or unreachable)
    std::vector<bool> reachable;  // True if node is reachable from entry

    bool isReachable(int u) const {
        return u >= 0 && u < static_cast<int>(reachable.size()) && reachable[u];
    }
};

// Default iterative DFS (uses an explicit stack so large binaries never overflow the call stack)
DfsResult runDfs(const std::vector<std::vector<int>>& succ, int entry = 0);

// Explicit-stack iterative DFS
DfsResult runDfsIterative(const std::vector<std::vector<int>>& succ, int entry = 0);

// Recursive DFS kept for cross-checking and Phase 2 specification
DfsResult runDfsRecursive(const std::vector<std::vector<int>>& succ, int entry = 0);

} // namespace red

#endif // LARPOS_DFS_H
