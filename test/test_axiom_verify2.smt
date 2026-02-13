; Source: Axiom verification - TA select-store different index
; Test 2: TA Axiom - i != j => select(store(a, i, v), j) = select(a, j)
; Expected: UNSAT
(assert (not (= i j)))
(assert (not (= (select (store a i v) j) (select a j))))
