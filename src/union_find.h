// Disjoint Set Union with ccpar heuristic for Congruence Closure
#pragma once

#include <vector>
#include <numeric>
#include <unordered_set>
#include <iostream>
#include <cassert>

// Implement Union-Find with several optimizations for the CC algorithm
class UnionFind {
public:
    UnionFind(int size) : parent(size), ccpar(size), members(size), forbidden(size) {
        // Each element is its own representative
        std::iota(parent.begin(), parent.end(), 0);
        // Track which elements belong to each equivalence class for eager updates
        for (int i = 0; i < size; ++i) {
            members[i] = {i};
        }
    }

    // FIND operation - Get the representative of element i
    // This is O(1) because I update parent pointers eagerly during UNION
    int find(int i) {
        assert(i >= 0 && i < (int)parent.size() && "find() called with out-of-bounds ID");
        return parent[i];
    }

    // UNION operation - Merge the equivalence classes of i and j
    // Returns 1 if merged successfully, 0 if already same class, -1 if forbidden
    int union_sets(int i, int j) {
        int root_i = find(i);
        int root_j = find(j);

        if (root_i != root_j) {
            // First Check if this merge is forbidden
            if (forbidden[root_i].count(root_j)) {
                return -1;
            }

            // The class with more parent terms becomes winner
            int winner = root_j;
            int loser = root_i;
            if (ccpar[root_i].size() > ccpar[root_j].size()) {
                winner = root_i;
                loser = root_j;
            }

            // Update all parent pointers in the loser class to point to winner
            // This makes future find() calls O(1)
            for (int member : members[loser]) {
                parent[member] = winner;
            }

            // Merge the member lists
            members[winner].insert(members[winner].end(), members[loser].begin(), members[loser].end());
            members[loser].clear();

            // Merge the ccpar sets (parent terms that need congruence checking)
            ccpar[winner].insert(ccpar[loser].begin(), ccpar[loser].end());
            ccpar[loser].clear();

            // Anything forbidden to loser is now forbidden to winner
            for (int k : forbidden[loser]) {
                forbidden[k].erase(loser);
                forbidden[k].insert(winner);
                forbidden[winner].insert(k);
            }
            forbidden[loser].clear();

            return 1;
        }
        return 0;  // Already in the same class
    }

    // Add a forbidden pair (for disequality constraints like a != b)
    // Returns false if they're already in the same class (contradiction!)
    bool add_forbidden(int i, int j) {
        int root_i = find(i);
        int root_j = find(j);
        if (root_i == root_j) {
            return false;  // CONFLICT: a != b but they're already equal!
        }
        forbidden[root_i].insert(root_j);
        forbidden[root_j].insert(root_i);
        return true;
    }

    // Register that term parent_term_id has rep as one of its arguments
    void add_ccpar(int rep, int parent_term_id) {
        ccpar[rep].insert(parent_term_id);
    }

    // Get all parent term IDs for a representative
    const std::unordered_set<int>& get_ccpar(int rep) {
        return ccpar[rep];
    }

private:
    std::vector<int> parent;                        // parent[i] = representative of i
    std::vector<std::unordered_set<int>> ccpar;     // ccpar[rep] = terms that have rep as an argument
    std::vector<std::vector<int>> members;          // members[rep] = all elements in this class
    std::vector<std::unordered_set<int>> forbidden; // forbidden[rep] = classes that cannot merge with rep
};
