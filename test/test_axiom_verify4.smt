; Source: Axiom verification - Tcons cdr projection
; Test 4: Tcons Axiom - cdr(cons(x,y)) = y  
; Expected: UNSAT
(assert (not (= (cdr (cons x y)) y)))
