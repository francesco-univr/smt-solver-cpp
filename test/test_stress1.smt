; Source: Custom stress test - deep transitivity chain
; a=b, b=c, c=d, d=e, e!=a -> UNSAT
(assert (= a b))
(assert (= b c))
(assert (= c d))
(assert (= d e))
(assert (not (= e a)))
