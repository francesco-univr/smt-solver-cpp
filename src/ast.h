// ast.h - Abstract Syntax Tree structures for SMT terms and literals
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <unordered_map>
#include <algorithm>

// Use SymbolType to classify what kind of symbol I'm dealing with
enum class SymbolType {
    Function,   // A function symbol like f, g, cons, store, select
    Constant,   // A constant like a, b, c
    Variable    // A variable (I treat it as a constant in quantifier-free logic)
};

// A Symbol represents the "label" at the head of a term (e.g., "f" in f(a,b))
struct Symbol {
    std::string name;   // The name of the symbol
    SymbolType type;    // What kind of symbol this is
    int arity;          // Number of arguments (0 for constants)

    // Compare symbols by name AND arity to avoid confusing f/1 with f/2
    bool operator==(const Symbol& other) const {
        return name == other.name && arity == other.arity;
    }
};

// Forward declaration so I can use TermPtr before defining Term
struct Term;
using TermPtr = std::shared_ptr<Term>;

// A Term is a node in term DAG
struct Term {
    Symbol symbol;              // The head symbol of this term
    std::vector<TermPtr> args;  // The arguments (empty for constants)
    int id = -1;                // Unique ID assigned during parsing (for DAG identification)

    Term(Symbol s, std::vector<TermPtr> a) : symbol(s), args(a) {}

    // For debugging and output
    std::string toString() const {
        if (args.empty()) return symbol.name;
        std::string s = "(" + symbol.name;
        for (const auto& arg : args) {
            s += " " + arg->toString();
        }
        s += ")";
        return s;
    }
};

// A Literal is an atomic constraint: either lhs = rhs or lhs != rhs
struct Literal {
    TermPtr lhs;        // Left-hand side term
    TermPtr rhs;        // Right-hand side term
    bool is_equality;   // true means "=", false means "!="

    std::string toString() const {
        return (is_equality ? "" : "not (") + std::string("=") + " " + lhs->toString() + " " + rhs->toString() + (is_equality ? "" : ")");
    }
};
