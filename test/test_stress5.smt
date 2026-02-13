; Source: Custom stress test - diamond pattern
; STRESS TEST 5: Diamond problem
; f(a,b) = c, f(a,b) = d, c != d -> UNSAT
(assert (= (f a b) c))
(assert (= (f a b) d))
(assert (not (= c d)))
