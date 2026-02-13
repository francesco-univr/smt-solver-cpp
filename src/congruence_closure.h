#pragma once

#include "ast.h"
#include "union_find.h"
#include <vector>
#include <unordered_map>
#include <list>
#include <iostream>

// This is the core decision procedure for the Theory of Equality (TE)
// Key idea: if a = b, then f(a) = f(b) for any function f (congruence axiom)
class CongruenceClosure {
public:
    CongruenceClosure(const std::vector<TermPtr>& terms) : terms_(terms), uf_(terms.size()) {
        // For each term f(t1,...,tn), I register f as a "parent" of each argument ti.
        //  This way, when ti changes class, I know which terms might become congruent and need checking.
        for (int i = 0; i < (int)terms_.size(); ++i) {
            auto term = terms_[i];
            for (auto& arg : term->args) {
                uf_.add_ccpar(uf_.find(arg->id), i);
            }
        }
    }

    // Merge two terms into the same equivalence class
    // Returns true if successful, false if it would violate a disequality constraint
    bool merge(int u, int v) {
        if (uf_.find(u) != uf_.find(v)) {
            pending_.push_back({u, v});
            return propagate();
        }
        return true;
    }

    // Add a disequality constraint (u != v)
    // Returns false if u and v are already in the same class (contradiction!)
    bool add_inequality(int u, int v) {
        return uf_.add_forbidden(u, v);
    }

    // Check if two terms are CONGRUENT (not just equivalent!)
    // Two terms are congruent if they have the same function symbol and their corresponding arguments are equivalent
    bool are_congruent(int u, int v) {
        TermPtr t1 = terms_[u];
        TermPtr t2 = terms_[v];

        
        if (t1->symbol.name != t2->symbol.name || t1->args.size() != t2->args.size()) {
            return false;
        }

        // Check if all corresponding arguments are in the same equivalence class
        for (size_t i = 0; i < t1->args.size(); ++i) {
            if (uf_.find(t1->args[i]->id) != uf_.find(t2->args[i]->id)) {
                return false;
            }
        }
        return true;
    }
    
    // Check if two terms are in the same equivalence class
    bool is_equivalent(int u, int v) {
        return uf_.find(u) == uf_.find(v);
    }

    // Check if all disequality constraints are satisfied
    bool is_consistent(const std::vector<Literal>& literals) {
        for (const auto& lit : literals) {
            if (!lit.is_equality) {
                // If a != b is asserted but find(a) == find(b), that's a contradiction!
                if (uf_.find(lit.lhs->id) == uf_.find(lit.rhs->id)) {
                    return false;
                }
            }
        }
        return true;
    }

    int find(int i) {
        return uf_.find(i);
    }

private:
    std::vector<TermPtr> terms_;               // The term DAG
    UnionFind uf_;                              // Union-Find for equivalence classes
    std::list<std::pair<int, int>> pending_;   // Queue of merges to process

    // Propagate congruences until no more merges are possible
    // Returns false if a forbidden merge is attempted (contradiction!)
    bool propagate() {
        while (!pending_.empty()) {
            auto [u, v] = pending_.front();
            pending_.pop_front();

            int root_u = uf_.find(u);
            int root_v = uf_.find(v);

            if (root_u != root_v) {
                int result = uf_.union_sets(u, v);
                if (result == -1) {
                    return false;  // Forbidden merge = UNSAT!
                }
                if (result == 0) continue;  // Already merged, skip

                // Check all pairs of parent terms for new congruences
                // If f(a) and f(b) are both in ccpar and a is now equivalent to b, then f(a) must be merged with f(b)
                
                int new_root = uf_.find(u);
                const auto& combined_ccpar = uf_.get_ccpar(new_root);
                
                std::vector<int> parents(combined_ccpar.begin(), combined_ccpar.end());
                for (size_t i = 0; i < parents.size(); ++i) {
                    for (size_t j = i + 1; j < parents.size(); ++j) {
                        int p1 = parents[i];
                        int p2 = parents[j];
                        // If p1 and p2 are not yet equivalent but are congruent, Merge them
                        if (uf_.find(p1) != uf_.find(p2) && are_congruent(p1, p2)) {
                            pending_.push_back({p1, p2});
                        }
                    }
                }
            }
        }
        return true;
    }
};
