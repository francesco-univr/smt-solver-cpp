# SMT Solver - Francesco Simbola (VR545665)

## Description

An SMT solver for the **quantifier-free** combination of three theories:
- **TE**: Equality with uninterpreted functions
- **Tcons**: Non-empty lists (`cons`, `car`, `cdr`)
- **TA**: Arrays without extensionality (`store`, `select`)

The solver checks whether a conjunction of equalities and disequalities is **satisfiable (SAT)** or **unsatisfiable (UNSAT)**.

---

## How to Execute

### Windows (pre-compiled executable)

The archive includes `smt_solver.exe` and required DLL libraries.

**From command line:**
```cmd
smt_solver.exe test\test_simple_unsat.smt
```

**Expected output:**
```
Parsing...
Parsing done. Contexts: 1
Solving...
Solving done.
UNSAT
```

### Interactive mode (stdin)

Run without arguments to enter formulas manually:
```cmd
smt_solver.exe
```

Then type your assertions and:
- Type `END` on a new line to start solving
- Type `EXIT` to quit the program

**Example:**
```
smt_solver.exe
(assert (= a b))
(assert (not (= a b)))
END
```

### Run all tests (PowerShell)

```powershell
Get-ChildItem test\*.smt | ForEach-Object { 
    Write-Host "$($_.Name): " -NoNewline
    .\smt_solver.exe $_.FullName | Select-String "^(SAT|UNSAT)$" 
}
```

---

## How to Compile

**Requirements:** C++17 compatible compiler (GCC 7+, Clang 5+ or MSVC 2017+)

### Linux / macOS
```bash
cd src
g++ -std=c++17 -O2 -o smt_solver main.cpp
./smt_solver ../test/test_simple_unsat.smt
```

### Windows (MinGW)
```cmd
cd src
g++ -std=c++17 -O2 -o smt_solver.exe main.cpp
smt_solver.exe ..\test\test_simple_unsat.smt
```

### Windows (Visual Studio Developer Command Prompt)
```cmd
cd src
cl /EHsc /O2 /std:c++17 main.cpp /Fe:smt_solver.exe
smt_solver.exe ..\test\test_simple_unsat.smt
```

**Note:** The source is header-only. Only `main.cpp` needs to be compiled; all other files are included automatically.

---

## Input Format

The solver accepts a subset of **SMT-LIB 2.0**:

```smt
; Comment
(assert (= a b))                          ; equality
(assert (not (= x y)))                    ; disequality
(assert (= (f a) (f b)))                  ; uninterpreted functions
(assert (= (car (cons x y)) x))           ; lists
(assert (= (select (store arr i v) i) v)) ; arrays
```

**Supported constructs:**
- `(assert φ)` — assertions
- `(= t1 t2)`, `(not (= t1 t2))` — equalities/disequalities
- `(and ...)`, `(or ...)`, `(not ...)` — boolean connectives
- `(=> φ ψ)`, `(xor ...)`, `(ite c t e)` — syntactic sugar
- `(distinct t1 ... tn)` — expanded to n(n-1)/2 disequalities
- `(let ((x t)) body)` — local bindings
- `(forall ...)`, `(exists ...)` — quantifiers are dropped, body is parsed

**Skipped SMT-LIB commands (parsed but ignored):**
`declare-fun`, `declare-sort`, `declare-const`, `set-logic`, `set-info`, `set-option`, `check-sat`, `exit`

**Interactive mode commands:**
- `END` — triggers solving (when reading from stdin)
- `EXIT` — quits the program (when reading from stdin)

---

## Output Format

The solver prints one of two results:
- **SAT** — there exists an interpretation satisfying all constraints
- **UNSAT** — no interpretation can satisfy all constraints

---

## Project Structure

```
FrancescoSimbolaVR545665/
├── README.md                 # This file
├── smt_solver.exe            # Windows executable
├── libgcc_s_seh-1.dll        # GCC runtime library
├── libstdc++-6.dll           # C++ standard library
├── libwinpthread-1.dll       # POSIX thread library
├── src/
│   ├── ast.h                 # Term, Symbol, Literal definitions
│   ├── union_find.h          # Equivalence classes (Union-Find)
│   ├── congruence_closure.h  # Congruence Closure algorithm
│   ├── theory_solver.h       # TA/Tcons theory handling
│   ├── parser.h              # SMT-LIB parser with hash-consing
│   └── main.cpp              # Entry point
└── test/
    ├── test_*.smt            # 27 input files
    └── test_*.out            # 27 expected output files
```

---

## Test Suite

The `test/` directory contains 27 test cases:

| Category | Files | Description |
|----------|-------|-------------|
| Base | `test_simple_*.smt` | Basic SAT/UNSAT tests |
| Tcons | `test_tcons_*.smt` | car/cdr/cons axioms |
| TA | `test_ta_*.smt` | select/store axioms |
| Stress | `test_stress*.smt` | Transitivity chains, deep congruences |
| Axiom | `test_axiom_verify*.smt` | Individual axiom verification |
| Mixed | `test_complex_*.smt` | Combined theories |

Each `.smt` file has a corresponding `.out` file with the expected result.

---

## Author

**Francesco Simbola** — Student ID: VR545665  
Course: Automated Reasoning — January 2026
