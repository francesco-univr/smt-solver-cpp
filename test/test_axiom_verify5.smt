; Source: Axiom verification - TE congruence
; Test 5: TE Congruence - f(a) = f(b) when a = b
; Expected: UNSAT
(assert (= a b))
(assert (not (= (f a) (f b))))
