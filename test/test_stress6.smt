; Source: Custom stress test - cons/car/cdr identity
; STRESS TEST 6: cons/car/cdr identity
; x = cons(car(x), cdr(x)) is NOT universally true (only if x is a cons)
; But car(cons(a,b)) = a AND cdr(cons(a,b)) = b
; cons(car(cons(a,b)), cdr(cons(a,b))) = cons(a,b) -> True
; Expected: UNSAT
(assert (not (= (cons (car (cons a b)) (cdr (cons a b))) (cons a b))))
