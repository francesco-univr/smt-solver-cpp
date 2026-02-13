; Source: Custom stress test - mixed select/cons
; STRESS TEST 7: Mixed select/cons
; select(store(a, i, cons(x, y)), i) = cons(x, y)
; car(select(store(a, i, cons(x, y)), i)) = x
; Expected: UNSAT
(assert (not (= (car (select (store a i (cons x y)) i)) x)))
