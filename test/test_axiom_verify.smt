; Source: Axiom verification - TA select-store same index
; Test 1: TA Axiom - select(store(a, i, v), i) = v
; Expected: UNSAT (contradiction: select(store(a,i,v),i) = v but we assert != v)
(assert (not (= (select (store a i v) i) v)))
