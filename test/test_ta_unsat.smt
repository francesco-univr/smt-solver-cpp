; Source: Custom test - TA read-over-write violation
(assert (= a (store b i v)))
(assert (= (select a i) v))
(assert (not (= v v)))
