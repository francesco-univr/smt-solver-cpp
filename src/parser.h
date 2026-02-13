#pragma once

#include "ast.h"
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <cctype>
#include <map>

class Parser {
public:
    Parser(const std::string& input) : input_(input), pos_(0) {
        Symbol s{"true", SymbolType::Constant, 0};
        true_term_ = std::make_shared<Term>(s, std::vector<TermPtr>{});
        true_term_->id = 0;
        terms_.push_back(true_term_);
    }

    // Parses a literal/formula and returns its DNF (disjunction of conjunctions)
    // Inner vector is a conjunction (AND), outer vector is disjunction (OR).
    std::vector<std::vector<Literal>> parseFormula() {
        skipWhitespace();
        if (peek() != '(') {
            // Boolean variable/constant or Let-bound variable
            std::string token = parseToken();
            
            // Check substitution scopes (from top to bottom)
            for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
                if (it->count(token)) {
                    TermPtr t = it->at(token);
                    // Treat boolean term t as t = true
                    return {{{t, true_term_, true}}};
                }
            }

             // Cache constants/variables
             if (term_cache_.find(token) != term_cache_.end()) {
                 TermPtr t = term_cache_[token];
                 return {{{t, true_term_, true}}};
             }

             Symbol s{token, SymbolType::Constant, 0};
             auto t = std::make_shared<Term>(s, std::vector<TermPtr>{});
             t->id = (int)terms_.size();
             terms_.push_back(t);
             term_cache_[token] = t;
             // DNF: {{t=true}} (single conjunction with single literal)
             return {{{t, true_term_, true}}};
        }

        expect('(');
        std::string op = parseToken();
        
        if (op == "assert") {
            auto result = parseFormula(); 
            expect(')');
            return result;
        }

        // Handle annotations (! term :named ...)
        if (op == "!") {
            auto result = parseFormula();
            while (peek() != ')') {
                parseToken(); 
            }
            expect(')');
            return result;
        }

        // Handle Let bindings
        if (op == "let") {
            expect('(');
            std::map<std::string, TermPtr> current_scope;
            while (true) {
                skipWhitespace();
                if (peek() != '(') break;
                consume(); // (
                std::string var = parseToken();
                TermPtr val = parseTerm();
                expect(')');
                current_scope[var] = val;
            }
            expect(')'); // End of bindings list
            
            scopes_.push_back(current_scope);
            auto result = parseFormula();
            scopes_.pop_back();
            
            expect(')');
            return result;
        }

        // Handle distinct: (distinct t1 t2 t3) -> (and (not (= t1 t2)) (not (= t1 t3)) (not (= t2 t3)))
        if (op == "distinct") {
            std::vector<TermPtr> args;
            while (peek() != ')') {
                args.push_back(parseTerm());
            }
            expect(')');

            std::vector<std::vector<Literal>> result = {{}}; // Start with True
            
            for (size_t i = 0; i < args.size(); ++i) {
                for (size_t j = i + 1; j < args.size(); ++j) {
                    // not (= t1 t2)
                    Literal l{args[i], args[j], false};
                    // AND this literal to the result
                    // Since result is DNF, ANDing a single literal means adding it to every conjunction
                    for (auto& conjunction : result) {
                        conjunction.push_back(l);
                    }
                }
            }
            return result;
        }

        // Handle Quantifiers: (forall (vars) body) or (exists (vars) body)
        if (op == "forall" || op == "exists") {
            // Skip variable list: (var type) ...
            skipBalanced(); // Skips (vars)
            
            // Parse body
            auto result = parseFormula();
            expect(')');
            return result;
        }

        if (op == "and") {
            // (and A B C ...) -> DNF(A) AND DNF(B) AND ...
            // Start with {{}} (one empty conjunction, i.e., True)
            std::vector<std::vector<Literal>> result = {{}};
            
            while (peek() != ')') {
                auto sub_dnf = parseFormula();
                // Cartesian product: result = result X sub_dnf
                std::vector<std::vector<Literal>> new_result;
                for (const auto& c1 : result) {
                    for (const auto& c2 : sub_dnf) {
                        std::vector<Literal> combined = c1;
                        combined.insert(combined.end(), c2.begin(), c2.end());
                        new_result.push_back(combined);
                    }
                }
                result = new_result;
            }
            expect(')');
            return result;
        }

        if (op == "or") {
            // (or A B C ...) -> DNF(A) OR DNF(B) OR ...
            // Union of DNFs
            std::vector<std::vector<Literal>> result;
            while (peek() != ')') {
                auto sub_dnf = parseFormula();
                result.insert(result.end(), sub_dnf.begin(), sub_dnf.end());
            }
            expect(')');
            return result;
        }

        bool is_eq = true;
        if (op == "not") {
            // Handle (not ...) by parsing inner and negating the resulting DNF
            auto inner_dnf = parseFormula();
            expect(')');
            return negateDNF(inner_dnf);
        }

        if (op == "=") {
            TermPtr lhs = parseTerm();
            TermPtr rhs = parseTerm();
            expect(')');
            return {{{lhs, rhs, is_eq}}};
        }
        
        if (op == "=>") {
             // (=> A B) is (or (not A) B)
             auto dnf_a = parseFormula();
             auto dnf_b = parseFormula();
             expect(')');
             
             auto not_a = negateDNF(dnf_a);
             // Result = not_a OR b
             not_a.insert(not_a.end(), dnf_b.begin(), dnf_b.end());
             return not_a;
        }
        
        if (op == "xor") {
             // (xor A B) is (or (and A (not B)) (and (not A) B))
             auto dnf_a = parseFormula();
             auto dnf_b = parseFormula();
             expect(')');
             
             auto not_a = negateDNF(dnf_a);
             auto not_b = negateDNF(dnf_b);
             
             // Term 1: A AND not B
             auto term1 = andDNF(dnf_a, not_b);
             // Term 2: not A AND B
             auto term2 = andDNF(not_a, dnf_b);
             
             // Result = Term 1 OR Term 2
             term1.insert(term1.end(), term2.begin(), term2.end());
             return term1;
        }

        if (op == "ite") {
             // (ite C T E) is (or (and C T) (and (not C) E))
             auto dnf_c = parseFormula();
             auto dnf_t = parseFormula();
             auto dnf_e = parseFormula();
             expect(')');
             
             auto not_c = negateDNF(dnf_c);
             
             auto term1 = andDNF(dnf_c, dnf_t);
             auto term2 = andDNF(not_c, dnf_e);
             
             term1.insert(term1.end(), term2.begin(), term2.end());
             return term1;
        }

        if (op == "distinct") {
            // (distinct t1 ... tn) -> AND_{i<j} (not (= ti tj))
            std::vector<TermPtr> args;
            while (peek() != ')') {
                args.push_back(parseTerm());
            }
            expect(')');

            std::vector<std::vector<Literal>> result = {{}}; // True

            for (size_t i = 0; i < args.size(); ++i) {
                for (size_t j = i + 1; j < args.size(); ++j) {
                    // Create literal: ti != tj
                    // In DNF, this is a single clause with one literal
                    Literal lit{args[i], args[j], false}; // false = inequality
                    std::vector<std::vector<Literal>> clause = {{lit}};
                    result = andDNF(result, clause);
                }
            }
            return result;
        }

        // Reconstruct term
        std::vector<TermPtr> args;
        while (peek() != ')') {
            args.push_back(parseTerm());
        }
        expect(')');
        
        // Create term signature for interning
        std::string signature = op;
        for (const auto& arg : args) signature += " " + std::to_string(arg->id);
        
        TermPtr t;
        if (term_cache_.find(signature) != term_cache_.end()) {
            t = term_cache_[signature];
        } else {
             Symbol s{op, SymbolType::Function, (int)args.size()};
             t = std::make_shared<Term>(s, args);
             t->id = (int)terms_.size();
             terms_.push_back(t);
             term_cache_[signature] = t;
        }
        
        // Return t=true
        return {{{t, true_term_, true}}};
    }

    // (A1 or A2) AND (B1 or B2) -> (A1 and B1) or (A1 and B2) ...
    std::vector<std::vector<Literal>> andDNF(const std::vector<std::vector<Literal>>& dnf1, const std::vector<std::vector<Literal>>& dnf2) {
        std::vector<std::vector<Literal>> result;
        for (const auto& c1 : dnf1) {
            for (const auto& c2 : dnf2) {
                std::vector<Literal> combined = c1;
                combined.insert(combined.end(), c2.begin(), c2.end());
                result.push_back(combined);
            }
        }
        return result;
    }

    // not ((A and B) or C) -> (not A or not B) and (not C)
    std::vector<std::vector<Literal>> negateDNF(const std::vector<std::vector<Literal>>& dnf) {
        // Start with True (one empty conjunction)
        std::vector<std::vector<Literal>> result = {{}};
        
        for (const auto& conjunction : dnf) {
            // Negate conjunction: not (L1 and L2 ...) -> (not L1) or (not L2) ...
            // This is a DNF where each clause has 1 literal
            std::vector<std::vector<Literal>> neg_conjunction;
            for (const auto& lit : conjunction) {
                Literal neg_lit = lit;
                neg_lit.is_equality = !neg_lit.is_equality;
                neg_conjunction.push_back({neg_lit});
            }
            
            // Result = Result AND neg_conjunction
            result = andDNF(result, neg_conjunction);
        }
        return result;
    }

    void skipBalanced() {
        skipWhitespace();
        if (peek() == '(') {
            consume();
            int depth = 1;
            while (depth > 0 && pos_ < input_.length()) {
                char c = consume();
                if (c == '(') depth++;
                else if (c == ')') depth--;
            }
        } else {
            parseToken();
        }
    }

    void skipCommand() {
        // Have already consumed '(' and the command name (e.g. "declare-fun")
        // So here at depth 1. Need to consume until depth 0.
        int depth = 1;
        while (depth > 0 && pos_ < input_.length()) {
            char c = consume();
            if (c == '(') depth++;
            else if (c == ')') depth--;
        }
    }

    // Returns a list of contexts (each context is a list of literals to be satisfied together)
    std::vector<std::vector<Literal>> parse() {
        // Start with one empty context (True)
        std::vector<std::vector<Literal>> contexts = {{}};
        
        while (pos_ < input_.length()) {
            skipWhitespace();
            if (pos_ >= input_.length()) break;
            
            if (peek() == '(') {
                size_t start = pos_;
                expect('(');
                std::string token = parseToken();
                
                if (token == "assert") {
                    pos_ = start; // Backtrack to parse (assert ...)
                    auto dnf = parseFormula(); // Returns DNF of the assertion
                    
                    // Update all current contexts with this new assertion
                    // New Contexts = Current Contexts X Assertion DNF
                    std::vector<std::vector<Literal>> new_contexts;
                    for (const auto& ctx : contexts) {
                        for (const auto& clause : dnf) {
                            std::vector<Literal> combined = ctx;
                            combined.insert(combined.end(), clause.begin(), clause.end());
                            new_contexts.push_back(combined);
                        }
                    }
                    contexts = new_contexts;
                    
                } else if (token == "declare-fun" || token == "declare-sort" || token == "set-logic" || token == "set-info" || token == "check-sat" || token == "exit" || token == "set-option" || token == "declare-const") {
                    // Already consumed '(' and the command name, so use skipCommand (starts at depth 1)
                    skipCommand();
                } else {
                    // Skip other commands (declare-fun, etc.) or unknown
                    skipCommand();
                }
            } else {
                // Unexpected token at top level? Consume it to avoid infinite loop.
                consume();
            }
        }
        return contexts;
    }

    TermPtr parseTerm() {
        skipWhitespace();
        
        if (peek() == '(') {
            consume(); // '('
            std::string func_name = parseToken();
            
            // Check if it's a let binding
             if (func_name == "let") {
                // (let ((var val) ...) body)
                // First consume the opening '(' of the binding list
                expect('(');
                std::map<std::string, TermPtr> current_scope;

                while (true) {
                    skipWhitespace();
                    if (peek() != '(') break;
                    consume(); // (
                    std::string var = parseToken();
                    TermPtr val = parseTerm();
                    expect(')');
                    current_scope[var] = val;
                }
                expect(')'); // End of bindings list
                
                scopes_.push_back(current_scope);
                TermPtr result = parseTerm();
                scopes_.pop_back();
                
                expect(')');
                return result;
            }

            std::vector<TermPtr> args;
            while (true) {
                skipWhitespace();
                if (peek() == ')') break;
                args.push_back(parseTerm());
            }
            consume(); // ')'
            
            // Create signature for interning "name id1 id2 ..."
            std::string signature = func_name;
            for (const auto& arg : args) signature += " " + std::to_string(arg->id);
            
            TermPtr t;
            if (term_cache_.find(signature) != term_cache_.end()) {
                t = term_cache_[signature];
            } else {
                 Symbol s{func_name, SymbolType::Function, (int)args.size()};
                 t = std::make_shared<Term>(s, args);
                 t->id = (int)terms_.size();
                 terms_.push_back(t);
                 term_cache_[signature] = t;
            }
            return t;
        } else {
            std::string name = parseToken();
            
            // Check substitution scopes
            for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
                if (it->count(name)) {
                    return it->at(name);
                }
            }

            // Cache constants/variables
            if (term_cache_.find(name) == term_cache_.end()) {
                Symbol s{name, SymbolType::Constant, 0};
                auto term = std::make_shared<Term>(s, std::vector<TermPtr>{});
                term->id = (int)terms_.size();
                terms_.push_back(term);
                term_cache_[name] = term;
            }
            return term_cache_[name];
        }
    }

    // Helper to get all unique terms in the parsed literals (for DAG construction)
    std::vector<TermPtr> getTerms() {
        return terms_;
    }

private:
    std::string input_;
    size_t pos_;
    std::vector<TermPtr> terms_;
    TermPtr true_term_;
    std::map<std::string, TermPtr> term_cache_; // Simple DAG sharing for constants/variables
    std::vector<std::map<std::string, TermPtr>> scopes_; // Stack of substitution scopes for let bindings

    void skipWhitespace() {
        while (pos_ < input_.length() && std::isspace(input_[pos_])) {
            pos_++;
        }
    }

    char peek() {
        if (pos_ < input_.length()) return input_[pos_];
        return 0;
    }

    char consume() {
        return input_[pos_++];
    }

    void expect(char c) {
        skipWhitespace();
        if (consume() != c) {
            throw std::runtime_error(std::string("Expected '") + c + "'");
        }
    }

    std::string parseToken() {
        skipWhitespace();
        std::string token;
        while (pos_ < input_.length() && !std::isspace(input_[pos_]) && input_[pos_] != '(' && input_[pos_] != ')') {
            token += input_[pos_++];
        }
        return token;
    }
};
