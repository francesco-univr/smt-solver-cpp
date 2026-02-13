; Source: Custom stress test - forbidden list propagation
; STRESS TEST 8: Forbidden list propagation
; a != b, c = a, c = b -> UNSAT (conflict via forbidden list)
(assert (not (= a b)))
(assert (= c a))
(assert (= c b))
