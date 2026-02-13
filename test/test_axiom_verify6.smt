; Source: Axiom verification - nested stores
; select(store(store(a, i, v1), j, v2), i) should equal:
;   - v2 if i = j
;   - v1 if i != j
; Expected: UNSAT (we assert both i=j and select != v2)
(assert (= i j))
(assert (not (= (select (store (store a i v1) j v2) i) v2)))
