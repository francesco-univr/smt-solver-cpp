; Source: Custom stress test - multiple nested stores
; STRESS TEST 3: Multiple store operations
; a1 = store(a, i1, v1)
; a2 = store(a1, i2, v2)
; select(a2, i1) should be v1 if i1 != i2
; Expected: UNSAT
(assert (= a1 (store a i1 v1)))
(assert (= a2 (store a1 i2 v2)))
(assert (not (= i1 i2)))
(assert (not (= (select a2 i1) v1)))
