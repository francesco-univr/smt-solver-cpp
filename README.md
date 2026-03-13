# smt-solver-cpp

> A from-scratch SMT solver in C++17 for the quantifier-free combination of equality with uninterpreted functions, list theory, and array theory — implementing Nelson-Oppen-style theory combination via congruence closure.

![C++17](https://img.shields.io/badge/C++-17-blue) ![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey) ![Tests](https://img.shields.io/badge/tests-27%20passing-brightgreen) ![License](https://img.shields.io/badge/license-MIT-green)

---

## Overview

This solver decides satisfiability of conjunctions of equalities and disequalities over the combined theory **TE ∪ Tcons ∪ TA**:

| Theory | Symbols | Axioms handled |
|---|---|---|
| **TE** — Equality + uninterpreted functions | `f(x)`, `g(a,b)` | Congruence closure |
| **Tcons** — Non-empty lists | `cons`, `car`, `cdr` | car/cdr/cons axioms |
| **TA** — Arrays (no extensionality) | `select`, `store` | Read-over-write axioms |

Given a formula in SMT-LIB 2.0 syntax, the solver outputs **SAT** or **UNSAT**.

---

## Architecture

The solver is implemented as a header-only C++17 library with a thin entry point in `main.cpp`. No external dependencies.
```
src/
├── parser.h              # SMT-LIB 2.0 parser with hash-consing for term sharing
├── ast.h                 # Term, Symbol, Literal — core data structures
├── union_find.h          # Union-Find with path compression + union by rank
├── congruence_closure.h  # Congruence Closure (Downey-Sethi-Tarjan style)
├── theory_solver.h       # Theory-specific axiom instantiation (TA, Tcons)
└── main.cpp              # Entry point (~50 lines)
```

**Key algorithmic choices:**
- **Hash-consing** in the parser ensures structural sharing — identical subterms are represented by a single pointer, making equality checks O(1)
- **Congruence Closure** propagates equalities across function applications without exhaustive enumeration
- **Lazy axiom instantiation** for TA and Tcons — axioms are generated on demand based on the current equivalence classes, avoiding exponential blowup

---

## Build

**Requirements:** C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
```bash
# Linux / macOS
cd src
g++ -std=c++17 -O2 -o smt_solver main.cpp
./smt_solver ../test/test_simple_unsat.smt
```
```cmd
# Windows (MinGW)
cd src
g++ -std=c++17 -O2 -o smt_solver.exe main.cpp

# Windows (MSVC)
cl /EHsc /O2 /std:c++17 main.cpp /Fe:smt_solver.exe
```

A pre-compiled Windows executable (`smt_solver.exe`) with required DLLs is included in the release.

---

## Usage
```bash
# File mode
./smt_solver formula.smt

# Interactive mode (stdin)
./smt_solver
(assert (= a b))
(assert (not (= a b)))
END
# → UNSAT
```

### Input format (SMT-LIB 2.0 subset)
```smt
(assert (= a b))                            ; equality
(assert (not (= x y)))                      ; disequality
(assert (= (f a) (f b)))                    ; uninterpreted function
(assert (= (car (cons x y)) x))             ; list axiom
(assert (= (select (store arr i v) i) v))   ; array read-over-write
```

Supported boolean connectives: `and`, `or`, `not`, `=>`, `xor`, `ite`, `distinct`, `let`.

---

## Test Suite

27 test cases covering single-theory and combined-theory scenarios:
```bash
# Run full test suite (Linux/macOS)
for f in test/*.smt; do
    result=$(./smt_solver "$f")
    expected=$(cat "${f%.smt}.out")
    [ "$result" = "$expected" ] && echo "PASS: $f" || echo "FAIL: $f"
done
```
```powershell
# Windows (PowerShell)
Get-ChildItem test\*.smt | ForEach-Object {
    Write-Host "$($_.Name): " -NoNewline
    .\smt_solver.exe $_.FullName | Select-String "^(SAT|UNSAT)$"
}
```

| Category | Count | What's tested |
|---|---|---|
| Base | 4 | Basic SAT/UNSAT, equality chains |
| Tcons | 6 | car/cdr/cons axioms, nested lists |
| TA | 6 | select/store, write-read interference |
| Mixed | 5 | Combined TE + TA + Tcons reasoning |
| Stress | 4 | Transitivity chains, deep congruences |
| Axiom verification | 2 | Individual axiom soundness checks |

---

## Key Learnings

1. **Hash-consing is non-negotiable for term-heavy solvers**: Without structural sharing, congruence closure degrades to O(n²) comparisons. Implementing hash-consing in the parser cut memory usage significantly on the stress tests.

2. **The order of axiom instantiation matters for TA**: The read-over-write axiom `select(store(a,i,v), i) = v` must be instantiated before propagating equalities — getting this order wrong produces incorrect UNSAT on satisfiable array formulas.

3. **Union-Find rank heuristics have outsized impact on congruence closure**: Path compression alone is insufficient — union by rank keeps the amortized cost near O(α(n)) even on the transitivity stress tests with 100+ equality chains.
