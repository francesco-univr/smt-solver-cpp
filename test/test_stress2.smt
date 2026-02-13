; Source: Custom stress test - deep congruence chain
; STRESS TEST 2: Deep function congruence
; f(f(f(a))) = f(f(f(b))), a = b -> SAT (consistent)
; f(f(f(a))) != f(f(f(b))), a = b -> UNSAT
(assert (= a b))
(assert (not (= (f (f (f a))) (f (f (f b))))))
