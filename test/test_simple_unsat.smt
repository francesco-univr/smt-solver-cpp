; Source: Custom test - transitivity chain violation
(assert (= a b))
(assert (= b c))
(assert (not (= a c)))
