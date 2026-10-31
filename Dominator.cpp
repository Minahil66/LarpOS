// Dominator.cpp - Phase 3 (RED): Cooper-Harvey-Kennedy Dominator Tree & Natural Loop Detection

#include "Dominator.h"
#include <algorithm>
#include <cstddef>

namespace red {

namespace {

// Cooper-Harvey-Kennedy two-finger intersection on the partially constructed dominator tree.
// Walks up the idom chain using Reverse Postorder indices (rpo_index) until both fingers meet.
int intersect(
    int b1,
    int b2,
    const std::vector<int>& idom,
    const std::vector<int>& rpo_index
) {
    int finger1 = b1;
    int finger2 = b2;
    while (finger1 != finger2) {
        while (rpo_index[finger1] > rpo_index[finger2]) {
            finger1 = idom[finger1];
        }
        while (rpo_index[finger2] > rpo_index[finger1]) {
            finger2 = idom[finger2];
        }
    }
    return finger1;
}

} // namespace

bool DominatorTree::dominates(int a, int b) const {
    const int n = static_cast<int>(idom.size());
    if (a < 0 || a >= n || b < 0 || b >= n) {
        return false;
    }
    if (idom[a] == -1 || idom[b] == -1) {
        return false; // Unreachable blocks cannot dominate or be dominated
    }

    int curr = b;
    while (curr != a) {
        int parent = idom[curr];
        if (parent == curr || parent == -1) {
            return false; // Reached entry root without hitting `a`
        }
        curr = parent;
    }
    return true;
}

bool DominatorTree::strictlyDominates(int a, int b) const {
    return (a != b) && dominates(a, b);
}

DominatorTree computeDominators(const std::vector<std::vector<int>>& succ, int entry) {
    const int n = static_cast<int>(succ.size());
    DominatorTree dom;
    dom.entry = entry;
    dom.idom.assign(n, -1);
    dom.children.assign(n, {});
    dom.depth.assign(n, -1);
    dom.dfs = runDfs(succ, entry);

    if (n == 0 || entry < 0 || entry >= n || dom.dfs.rpo.empty()) {
        return dom;
    }

    // Build reachable predecessor lists (skipping unreachable dead blocks)
    std::vector<std::vector<int>> preds(n);
    for (int u : dom.dfs.rpo) {
        for (int v : succ[u]) {
            if (v >= 0 && v < n && dom.dfs.isReachable(v)) {
                preds[v].push_back(u);
            }
        }
    }

    // Initialize start node
    dom.idom[entry] = entry;

    // Iterative Cooper-Harvey-Kennedy fixed-point loop over Reverse Postorder
    bool changed = true;
    while (changed) {
        changed = false;

        // Process all reachable nodes in RPO except the entry node at index 0
        for (std::size_t i = 1; i < dom.dfs.rpo.size(); ++i) {
            int b = dom.dfs.rpo[i];
            int new_idom = -1;

            for (int p : preds[b]) {
                if (dom.idom[p] != -1) { // Predecessor p has already been processed
                    if (new_idom == -1) {
                        new_idom = p;
                    } else {
                        new_idom = intersect(p, new_idom, dom.idom, dom.dfs.rpo_index);
                    }
                }
            }

            if (new_idom != -1 && dom.idom[b] != new_idom) {
                dom.idom[b] = new_idom;
                changed = true;
            }
        }
    }

    // Build Dominator Tree children lists and depths in RPO order (parents always precede children)
    dom.depth[entry] = 0;
    for (std::size_t i = 1; i < dom.dfs.rpo.size(); ++i) {
        int u = dom.dfs.rpo[i];
        int p = dom.idom[u];
        if (p != -1 && p != u) {
            dom.children[p].push_back(u);
            dom.depth[u] = dom.depth[p] + 1;
        }
    }

    return dom;
}

std::vector<BackEdge> findBackEdges(
    const std::vector<std::vector<int>>& succ,
    const DominatorTree& dom
) {
    std::vector<BackEdge> back_edges;
    const int n = static_cast<int>(succ.size());

    for (int u = 0; u < n; ++u) {
        if (dom.idom[u] == -1) {
            continue; // Ignore unreachable dead blocks
        }
        for (int v : succ[u]) {
            if (v >= 0 && v < n && dom.dominates(v, u)) {
                back_edges.push_back(BackEdge{u, v});
            }
        }
    }

    return back_edges;
}

std::vector<LoopInfo> findLoops(
    const std::vector<std::vector<int>>& succ,
    const DominatorTree& dom,
    const std::vector<int>& instrCountPerBlock
) {
    const int n = static_cast<int>(succ.size());
    std::vector<BackEdge> back_edges = findBackEdges(succ, dom);
    if (back_edges.empty()) {
        return {};
    }

    // Build reverse adjacency list of reachable blocks for natural loop extraction
    std::vector<std::vector<int>> preds(n);
    for (int u = 0; u < n; ++u) {
        if (dom.idom[u] == -1) continue;
        for (int v : succ[u]) {
            if (v >= 0 && v < n && dom.idom[v] != -1) {
                preds[v].push_back(u);
            }
        }
    }

    // Group back-edges by header so multiple back-edges to the same header merge into one natural loop
    std::vector<int> header_to_loop_idx(n, -1);
    std::vector<LoopInfo> loops;
    std::vector<std::vector<bool>> in_loop_body;

    for (const BackEdge& edge : back_edges) {
        int idx = header_to_loop_idx[edge.header];
        if (idx == -1) {
            idx = static_cast<int>(loops.size());
            header_to_loop_idx[edge.header] = idx;

            LoopInfo info;
            info.header = edge.header;
            loops.push_back(info);

            std::vector<bool> body_mask(n, false);
            body_mask[edge.header] = true;
            in_loop_body.push_back(std::move(body_mask));
        }

        loops[idx].latches.push_back(edge.latch);
        std::vector<bool>& body_mask = in_loop_body[idx];

        // Reverse DFS from latch back to header (only visiting nodes dominated by header)
        if (!body_mask[edge.latch]) {
            body_mask[edge.latch] = true;
            std::vector<int> worklist = {edge.latch};

            while (!worklist.empty()) {
                int m = worklist.back();
                worklist.pop_back();

                for (int p : preds[m]) {
                    if (!body_mask[p] && dom.dominates(edge.header, p)) {
                        body_mask[p] = true;
                        worklist.push_back(p);
                    }
                }
            }
        }
    }

    // Populate body vectors, instruction counts, and block counts
    for (std::size_t i = 0; i < loops.size(); ++i) {
        LoopInfo& loop = loops[i];
        std::sort(loop.latches.begin(), loop.latches.end());
        loop.latches.erase(std::unique(loop.latches.begin(), loop.latches.end()), loop.latches.end());
        loop.back_edge_count = loop.latches.size();

        std::size_t total_instrs = 0;
        for (int b = 0; b < n; ++b) {
            if (in_loop_body[i][b]) {
                loop.body.push_back(b);
                int count = (b < static_cast<int>(instrCountPerBlock.size()) && instrCountPerBlock[b] > 0)
                                ? instrCountPerBlock[b]
                                : 1;
                total_instrs += static_cast<std::size_t>(count);
            }
        }
        loop.block_count = loop.body.size();
        loop.instruction_count = total_instrs;
    }

    // Compute nesting depth: Loop i is nested inside Loop j if j's body contains i's header (and j != i)
    for (std::size_t i = 0; i < loops.size(); ++i) {
        int depth = 1;
        for (std::size_t j = 0; j < loops.size(); ++j) {
            if (i == j) continue;
            if (in_loop_body[j][loops[i].header] &&
                in_loop_body[j] != in_loop_body[i]) {
                ++depth;
            }
        }
        loops[i].nesting_depth = depth;

        // Heuristic ranking score based on nesting depth, back-edge count, block count, and instruction count
        loops[i].score =
            (static_cast<double>(loops[i].nesting_depth) * 100.0) +
            (static_cast<double>(loops[i].back_edge_count) * 25.0) +
            (static_cast<double>(loops[i].block_count) * 10.0) +
            (static_cast<double>(loops[i].instruction_count) * 1.0);
    }

    return loops;
}

} // namespace red
