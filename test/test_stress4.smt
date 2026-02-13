; Source: Custom stress test - nested cons
; STRESS TEST 4: Nested cons
; car(cdr(cons(a, cons(b, c)))) = b
; Expected: UNSAT
(assert (not (= (car (cdr (cons a (cons b c)))) b)))
