; Source: SMT-LIB QF_UF benchmark format test
(set-logic QF_UF)
(declare-sort U 0)
(declare-fun x0 () U)
(declare-fun y0 () U)
(declare-fun z0 () U)
(assert (not (= x0 x0)))
(check-sat)
(exit)
