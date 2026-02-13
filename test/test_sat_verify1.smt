; Source: Satisfiability test - uncorrelated TA
; Test SAT 1: select(store(a, i, v), j) with i e j not bounded
; Expected: SAT (no contradiction)
(assert (= x (select (store a i v) j)))
