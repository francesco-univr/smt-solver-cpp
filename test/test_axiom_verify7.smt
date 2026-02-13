; Source: Axiom verification - mixed theories
; Test 7: Mixed theories - cons + store + select
; Expected: UNSAT
(assert (= x (car (cons (select (store a i v) i) y))))
(assert (not (= x v)))
