; Source: Custom test - mixed theory integration
(assert (= c (cons (select (store b i v) i) d)))
(assert (= (car c) v))
(assert (not (= (car c) (select (store b i v) i))))
