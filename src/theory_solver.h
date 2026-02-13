//  Main SMT solver for TE + Tcons + TA theories
#pragma once

#include "ast.h"
#include "congruence_closure.h"
#include <vector>
#include <set>
#include <utility>

// Implement the theory solver that combines:
// - TE: Theory of Equality (handled by Congruence Closure)
// - Tcons: Theory of non-empty Lists (car, cdr, cons axioms)
// - TA: Theory of Arrays (select, store axioms)

class TheorySolver {
public:
    TheorySolver(std::vector<Literal> literals, std::vector<TermPtr> terms) 
        : literals_(literals), terms_(terms) {
            get_or_create_term("true", {});
            get_or_create_term("false", {});
        }

    bool solve() {
        return solve_recursive(literals_);
    }

private:
    std::vector<Literal> literals_;   // The literals need to satisfy
    std::vector<TermPtr> terms_;      // All terms in the problem (the DAG)
    
    
    // Track which (select, store) pairs I've already processed to avoid infinite loops
    std::set<std::pair<int, int>> visited_select_store_;

    // Look for a pattern: select(base) where base is equivalent to some store(...)
    
    std::pair<TermPtr, TermPtr> find_select_store(const std::vector<Literal>& current_lits) {
        // Build a temporary CC to find which terms are equivalent
        CongruenceClosure cc(terms_);
        for (const auto& lit : current_lits) {
            if (lit.is_equality) {
                cc.merge(lit.lhs->id, lit.rhs->id);
            }
        }

        std::vector<TermPtr> select_terms;
        std::vector<TermPtr> store_terms;

        // Identify select and store terms from all known terms
        for (const auto& term : terms_) {
             if (term->symbol.name == "select" && term->args.size() == 2) select_terms.push_back(term);
             if (term->symbol.name == "store" && term->args.size() == 3) store_terms.push_back(term);
        }

        for (const auto& sel : select_terms) {
            TermPtr base = sel->args[0];
            int base_id = cc.find(base->id);

            for (const auto& sto : store_terms) {
                if (cc.find(sto->id) == base_id) {
                    // Check if this specific pair (select, store) has been visited
                    if (visited_select_store_.count({sel->id, sto->id})) continue;
                    return {sel, sto};
                }
            }
        }
        return {nullptr, nullptr};
    }

    bool is_congruent_to_cons(int term_id, std::shared_ptr<CongruenceClosure> cc) {
        int root = cc->find(term_id);
        for (const auto& t : terms_) {
            if (t->symbol.name == "cons" && t->args.size() == 2) {
                if (cc->find(t->id) == root) return true;
            }
        }
        return false;
    }

    bool solve_recursive(std::vector<Literal> current_lits) {
        auto [select_term, store_term] = find_select_store(current_lits);
        
        if (select_term) {
            // Mark this specific pair as visited
            visited_select_store_.insert({select_term->id, store_term->id});
            
            TermPtr a = store_term->args[0];
            TermPtr i = store_term->args[1];
            TermPtr v = store_term->args[2];
            TermPtr j = select_term->args[1];

            // Branch 1: i = j
            std::vector<Literal> branch1 = current_lits;
            branch1.push_back({i, j, true}); 
            replace_term_in_literals(branch1, select_term, v);
            if (solve_recursive(branch1)) return true;

            // Branch 2: i != j
            std::vector<Literal> branch2 = current_lits;
            branch2.push_back({i, j, false}); 
            
            TermPtr select_a_j = get_or_create_term("select", {a, j});
            
            replace_term_in_literals(branch2, select_term, select_a_j);
            if (solve_recursive(branch2)) return true;

            return false;  // Both branches lead to UNSAT
        }

        
        // Instantiate axioms for Tcons and TA before creating CongruenceClosure
        // to ensure all new terms are included in the Union-Find sizing
        std::vector<std::pair<TermPtr, TermPtr>> axiom_merges;
        for (size_t i = 0; i < terms_.size(); ++i) {
            TermPtr t = terms_[i];
            
            // Tcons axiom: car(cons(x,y)) = x and cdr(cons(x,y)) = y
            if (t->symbol.name == "cons" && t->args.size() == 2) {
                TermPtr car_term = get_or_create_term("car", {t});
                TermPtr cdr_term = get_or_create_term("cdr", {t});
                axiom_merges.push_back({car_term, t->args[0]});  // car(cons(x,y)) = x
                axiom_merges.push_back({cdr_term, t->args[1]});  // cdr(cons(x,y)) = y
            }

            // TA axiom: select(store(a, i, v), i) = v (read-over-write at same index)
            if (t->symbol.name == "store" && t->args.size() == 3) {
                 TermPtr i_idx = t->args[1];
                 TermPtr v = t->args[2];
                 TermPtr sel = get_or_create_term("select", {t, i_idx});
                 axiom_merges.push_back({sel, v});  // select(store(a,i,v), i) = v
            }
        }

        // Create LOCAL CongruenceClosure
        auto cc = std::make_shared<CongruenceClosure>(terms_);

        // Now merge the instantiated terms using the pre-collected list
        // This avoids calling get_or_create_term (which could modify terms_) while cc is active
        for (const auto& pair : axiom_merges) {
            if (!cc->merge(pair.first->id, pair.second->id)) return false;
        }

        // Enforce boolean axioms and propagate theory
        if (!propagate_boolean_theory(cc)) return false;

        for (const auto& lit : current_lits) {
            if (lit.is_equality) {
                if (!cc->merge(lit.lhs->id, lit.rhs->id)) return false;
            } else {
                if (!cc->add_inequality(lit.lhs->id, lit.rhs->id)) return false;
            }
        }

        // Propagate again after literals
        if (!propagate_boolean_theory(cc)) return false;

        // Handle Atom Predicate
        if (!handle_atom_predicate(current_lits, cc)) return false;

        // Boolean Splitting:
        TermPtr true_term = nullptr;
        TermPtr false_term = nullptr;
        for (const auto& t : terms_) {
            if (t->symbol.name == "true" && t->args.empty()) true_term = t;
            if (t->symbol.name == "false" && t->args.empty()) false_term = t;
        }

        if (true_term && false_term) {
            // Collect potential boolean terms to split on
            std::vector<TermPtr> candidates;
            for (const auto& t : terms_) {
                if (t->symbol.name == "not" || t->symbol.name == "and" || 
                    t->symbol.name == "or" || t->symbol.name == "=>") {
                    candidates.push_back(t);
                    for (const auto& arg : t->args) candidates.push_back(arg);
                }
            }

            for (const auto& x : candidates) {
                bool is_true = cc->is_equivalent(x->id, true_term->id);
                bool is_false = cc->is_equivalent(x->id, false_term->id);

                if (!is_true && !is_false) {
                    // Split on x
                    // Branch 1: x = true
                    std::vector<Literal> branch1 = current_lits;
                    branch1.push_back({x, true_term, true});
                    if (solve_recursive(branch1)) return true;

                    // Branch 2: x = false
                    std::vector<Literal> branch2 = current_lits;
                    branch2.push_back({x, false_term, true});
                    if (solve_recursive(branch2)) return true;

                    return false; // Both branches UNSAT
                }
            }
        }

        return true;
    }



    TermPtr get_or_create_term(std::string name, std::vector<TermPtr> args) {
        // Simple linear search for now 
        for (const auto& t : terms_) {
            if (t->symbol.name == name && t->args.size() == args.size()) {
                bool match = true;
                for (size_t i = 0; i < args.size(); ++i) {
                    if (t->args[i] != args[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) return t;
            }
        }
        
        Symbol s{name, SymbolType::Function, (int)args.size()};
        auto t = std::make_shared<Term>(s, args);
        t->id = (int)terms_.size();
        terms_.push_back(t);
        return t;
    }

    void replace_term_in_literals(std::vector<Literal>& lits, TermPtr old_term, TermPtr new_term) {
        for (auto& lit : lits) {
            lit.lhs = replace_in_term(lit.lhs, old_term, new_term);
            lit.rhs = replace_in_term(lit.rhs, old_term, new_term);
        }
    }

    TermPtr replace_in_term(TermPtr t, TermPtr old_term, TermPtr new_term) {
        if (t == old_term) return new_term;
        bool changed = false;
        std::vector<TermPtr> new_args;
        for (const auto& arg : t->args) {
            TermPtr new_arg = replace_in_term(arg, old_term, new_term);
            new_args.push_back(new_arg);
            if (new_arg != arg) changed = true;
        }
        if (changed) {
            // Use get_or_create_term to ensure interning
            return get_or_create_term(t->symbol.name, new_args);
        }
        return t;
    }

    bool propagate_boolean_theory(std::shared_ptr<CongruenceClosure> cc) {
        TermPtr true_term = nullptr;
        TermPtr false_term = nullptr;

        // Find true/false terms
        for (const auto& t : terms_) {
            if (t->symbol.name == "true" && t->args.empty()) true_term = t;
            if (t->symbol.name == "false" && t->args.empty()) false_term = t;
        }

        if (!true_term || !false_term) return true; // Should not happen due to constructor

        if (!cc->add_inequality(true_term->id, false_term->id)) return false;

        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& t : terms_) {
                // Propagate NOT
                if (t->symbol.name == "not" && t->args.size() == 1) {
                    TermPtr x = t->args[0];
                    
                    // not(x) != x
                    if (!cc->add_inequality(t->id, x->id)) return false;

                    // not(not(x)) = x
                    if (x->symbol.name == "not" && x->args.size() == 1) {
                        if (cc->find(t->id) != cc->find(x->args[0]->id)) {
                             if (!cc->merge(t->id, x->args[0]->id)) return false;
                             changed = true;
                        }
                    }

                    // not(true) = false
                    if (cc->is_equivalent(x->id, true_term->id)) {
                        if (cc->find(t->id) != cc->find(false_term->id)) {
                            if (!cc->merge(t->id, false_term->id)) return false;
                            changed = true;
                        }
                    }
                    // not(false) = true
                    if (cc->is_equivalent(x->id, false_term->id)) {
                        if (cc->find(t->id) != cc->find(true_term->id)) {
                            if (!cc->merge(t->id, true_term->id)) return false;
                            changed = true;
                        }
                    }
                    // Backward: if not(x) = true -> x = false
                    if (cc->is_equivalent(t->id, true_term->id)) {
                        if (cc->find(x->id) != cc->find(false_term->id)) {
                            if (!cc->merge(x->id, false_term->id)) return false;
                            changed = true;
                        }
                    }
                    // Backward: if not(x) = false -> x = true
                    if (cc->is_equivalent(t->id, false_term->id)) {
                        if (cc->find(x->id) != cc->find(true_term->id)) {
                            if (!cc->merge(x->id, true_term->id)) return false;
                            changed = true;
                        }
                    }
                }

                // Propagate AND
                if (t->symbol.name == "and") {
                    bool all_true = true;
                    bool any_false = false;
                    for (const auto& arg : t->args) {
                        if (cc->is_equivalent(arg->id, false_term->id)) any_false = true;
                        if (!cc->is_equivalent(arg->id, true_term->id)) all_true = false;
                    }

                    // Forward
                    if (any_false) {
                        if (cc->find(t->id) != cc->find(false_term->id)) {
                            if (!cc->merge(t->id, false_term->id)) return false;
                            changed = true;
                        }
                    }
                    if (all_true) {
                        if (cc->find(t->id) != cc->find(true_term->id)) {
                            if (!cc->merge(t->id, true_term->id)) return false;
                            changed = true;
                        }
                    }

                    // Backward
                    if (cc->is_equivalent(t->id, true_term->id)) {
                        // All args must be true
                        for (const auto& arg : t->args) {
                            if (cc->find(arg->id) != cc->find(true_term->id)) {
                                if (!cc->merge(arg->id, true_term->id)) return false;
                                changed = true;
                            }
                        }
                    }
                }

                // Propagate OR
                if (t->symbol.name == "or") {
                    bool any_true = false;
                    bool all_false = true;
                    for (const auto& arg : t->args) {
                        if (cc->is_equivalent(arg->id, true_term->id)) any_true = true;
                        if (!cc->is_equivalent(arg->id, false_term->id)) all_false = false;
                    }

                    // Forward
                    if (any_true) {
                        if (cc->find(t->id) != cc->find(true_term->id)) {
                            if (!cc->merge(t->id, true_term->id)) return false;
                            changed = true;
                        }
                    }
                    if (all_false) {
                        if (cc->find(t->id) != cc->find(false_term->id)) {
                            if (!cc->merge(t->id, false_term->id)) return false;
                            changed = true;
                        }
                    }

                    // Backward
                    if (cc->is_equivalent(t->id, false_term->id)) {
                        // All args must be false
                        for (const auto& arg : t->args) {
                            if (cc->find(arg->id) != cc->find(false_term->id)) {
                                if (!cc->merge(arg->id, false_term->id)) return false;
                                changed = true;
                            }
                        }
                    }
                }

                // Propagate IMPLIES (=> a b)
                if (t->symbol.name == "=>" && t->args.size() == 2) {
                    TermPtr a = t->args[0];
                    TermPtr b = t->args[1];
                    
                    bool a_true = cc->is_equivalent(a->id, true_term->id);
                    bool a_false = cc->is_equivalent(a->id, false_term->id);
                    bool b_true = cc->is_equivalent(b->id, true_term->id);
                    bool b_false = cc->is_equivalent(b->id, false_term->id);

                    // Forward
                    if (a_false || b_true) {
                        if (cc->find(t->id) != cc->find(true_term->id)) {
                            if (!cc->merge(t->id, true_term->id)) return false;
                            changed = true;
                        }
                    }
                    if (a_true && b_false) {
                        if (cc->find(t->id) != cc->find(false_term->id)) {
                            if (!cc->merge(t->id, false_term->id)) return false;
                            changed = true;
                        }
                    }

                    // Backward
                    if (cc->is_equivalent(t->id, false_term->id)) {
                        // Must be a=true, b=false
                        if (!a_true) { if (!cc->merge(a->id, true_term->id)) return false; changed = true; }
                        if (!b_false) { if (!cc->merge(b->id, false_term->id)) return false; changed = true; }
                    }
                    if (cc->is_equivalent(t->id, true_term->id)) {
                        if (a_true) {
                             if (!b_true && cc->find(b->id) != cc->find(true_term->id)) {
                                 if (!cc->merge(b->id, true_term->id)) return false; changed = true;
                             }
                        }
                    }
                }

                // Propagate Equality (= a b)
                if (t->symbol.name == "=" && t->args.size() == 2) {
                    TermPtr a = t->args[0];
                    TermPtr b = t->args[1];

                    if (cc->is_equivalent(a->id, b->id)) {
                        if (cc->find(t->id) != cc->find(true_term->id)) {
                            if (!cc->merge(t->id, true_term->id)) return false;
                            changed = true;
                        }
                    }
                    
                    // Backward: if (= a b) is true -> merge a b
                    if (cc->is_equivalent(t->id, true_term->id)) {
                        if (cc->find(a->id) != cc->find(b->id)) {
                            if (!cc->merge(a->id, b->id)) return false;
                            changed = true;
                        }
                    }
                    
                    // Backward: if (= a b) is false -> distinct a b
                    if (cc->is_equivalent(t->id, false_term->id)) {
                        if (!cc->add_inequality(a->id, b->id)) return false;
                    }
                }
            }
        }
        return true;
    }

    bool handle_atom_predicate(std::vector<Literal> current_lits, std::shared_ptr<CongruenceClosure> cc);
};

inline bool TheorySolver::handle_atom_predicate(std::vector<Literal> current_lits, std::shared_ptr<CongruenceClosure> cc) {
    TermPtr true_term = nullptr;
    TermPtr false_term = nullptr;
    for (const auto& t : terms_) {
        if (t->symbol.name == "true" && t->args.empty()) true_term = t;
        if (t->symbol.name == "false" && t->args.empty()) false_term = t;
    }
    if (!true_term || !false_term) return true;


    // If we have atom(x) != true, it implies atom(x) = false
    // If we have atom(x) != false, it implies atom(x) = true
    for (const auto& lit : current_lits) {
        if (!lit.is_equality) {
            TermPtr atom_term = nullptr;
            TermPtr other = nullptr;
            
            if (lit.lhs->symbol.name == "atom") { atom_term = lit.lhs; other = lit.rhs; }
            else if (lit.rhs->symbol.name == "atom") { atom_term = lit.rhs; other = lit.lhs; }

            if (atom_term) {
                if (cc->is_equivalent(other->id, true_term->id)) {
                    if (!cc->merge(atom_term->id, false_term->id)) return false;
                }
                else if (cc->is_equivalent(other->id, false_term->id)) {
                    if (!cc->merge(atom_term->id, true_term->id)) return false;
                }
            }
        }
    }

    std::vector<TermPtr> atom_terms;
    for (const auto& t : terms_) {
        if (t->symbol.name == "atom" && t->args.size() == 1) {
            atom_terms.push_back(t);
        }
    }

    for (const auto& atom : atom_terms) {
        TermPtr x = atom->args[0];
        
        // Case 1: atom(x) = true
        if (cc->is_equivalent(atom->id, true_term->id)) {
            if (is_congruent_to_cons(x->id, cc)) {
                return false; // Conflict: atom(x) is true, but x is a cons
            }
        }

        // Case 2: atom(x) = false
        if (cc->is_equivalent(atom->id, false_term->id)) {
            if (!is_congruent_to_cons(x->id, cc)) {
                // Instantiate x = cons(u1, u2)
                std::string suffix = std::to_string(terms_.size());
                TermPtr u1 = get_or_create_term("gen_car_" + suffix, {});
                TermPtr u2 = get_or_create_term("gen_cdr_" + suffix, {});
                
        
                TermPtr cons_term = get_or_create_term("cons", {u1, u2});

                // Add equality x = cons(u1, u2)
                std::vector<Literal> next_lits = current_lits;
                next_lits.push_back({x, cons_term, true});
                
                // Recurse
                return solve_recursive(next_lits);
            }
        }
    }
    return true;
}
