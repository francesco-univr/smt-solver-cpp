; Source: Axiom verification - Tcons car projection
; Test 3: Tcons Axiom - car(cons(x,y)) = x
; Expected: UNSAT
(assert (not (= (car (cons x y)) x)))
