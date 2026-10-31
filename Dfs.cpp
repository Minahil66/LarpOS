// Dfs.cpp - Phase 2 (RED): Depth-First Search & Reverse Postorder Implementation

#include "Dfs.h"
#include <algorithm>
#include <cstddef>

namespace red {

namespace {

void initResult(DfsResult& res, std::size_t n, int entry) {
    res.entry = entry;
    res.preorder.clear();
    res.postorder.clear();
    res.rpo.clear();
    res.pre_num.assign(n, -1);
    res.post_num.assign(n, -1);
    res.rpo_index.assign(n, -1);
    res.parent.assign(n, -1);
    res.reachable.assign(n, false);
}

void finalizeRpo(DfsResult& res) {
    res.rpo = res.postorder;
    std::reverse(res.rpo.begin(), res.rpo.end());
    for (std::size_t i = 0; i < res.rpo.size(); ++i) {
        int u = res.rpo[i];
        res.rpo_index[u] = static_cast<int>(i);
    }
}

void dfsRecHelper(
    const std::vector<std::vector<int>>& succ,
    int u,
    DfsResult& res,
    int& pre_clock,
    int& post_clock
) {
    const int n = static_cast<int>(succ.size());
    res.reachable[u] = true;
    res.pre_num[u] = pre_clock++;
    res.preorder.push_back(u);

    for (int v : succ[u]) {
        if (v >= 0 && v < n && !res.reachable[v]) {
            res.parent[v] = u;
            dfsRecHelper(succ, v, res, pre_clock, post_clock);
        }
    }

    res.post_num[u] = post_clock++;
    res.postorder.push_back(u);
}

} // namespace

DfsResult runDfsIterative(const std::vector<std::vector<int>>& succ, int entry) {
    const int n = static_cast<int>(succ.size());
    DfsResult res;
    initResult(res, succ.size(), entry);

    if (n == 0 || entry < 0 || entry >= n) {
        return res;
    }

    struct Frame {
        int u;
        std::size_t edge_idx;
    };

    std::vector<Frame> stack;
    stack.reserve(succ.size());

    int pre_clock = 0;
    int post_clock = 0;

    res.reachable[entry] = true;
    res.pre_num[entry] = pre_clock++;
    res.preorder.push_back(entry);
    stack.push_back({entry, 0});

    while (!stack.empty()) {
        Frame& top = stack.back();
        int u = top.u;

        if (top.edge_idx < succ[u].size()) {
            int v = succ[u][top.edge_idx++];
            if (v >= 0 && v < n && !res.reachable[v]) {
                res.reachable[v] = true;
                res.parent[v] = u;
                res.pre_num[v] = pre_clock++;
                res.preorder.push_back(v);
                stack.push_back({v, 0});
            }
        } else {
            res.post_num[u] = post_clock++;
            res.postorder.push_back(u);
            stack.pop_back();
        }
    }

    finalizeRpo(res);
    return res;
}

DfsResult runDfsRecursive(const std::vector<std::vector<int>>& succ, int entry) {
    const int n = static_cast<int>(succ.size());
    DfsResult res;
    initResult(res, succ.size(), entry);

    if (n == 0 || entry < 0 || entry >= n) {
        return res;
    }

    int pre_clock = 0;
    int post_clock = 0;
    dfsRecHelper(succ, entry, res, pre_clock, post_clock);
    finalizeRpo(res);
    return res;
}

DfsResult runDfs(const std::vector<std::vector<int>>& succ, int entry) {
    return runDfsIterative(succ, entry);
}

} // namespace red
