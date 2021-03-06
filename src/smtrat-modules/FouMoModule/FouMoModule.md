# FouMoModule {#FouMoModule}

Implements the SMT compliant Fourier-Motzkin algorithm.
Hence, this module can decide the consistency of any conjunction
consisting only of linear real arithmetic constraints. Furthermore,
it might also find the consistency of a conjunction of constraints
even if they are not all linear e.g. in the case of a monomial \f$ x^i \f$ 
(where i is a positive integer) only occuring in the shape of this
monomial. Such a monomial is subsequently eliminated as common linear
variables are eliminated. One can tune a threshold parameter in order to
determine when, regarding the size of the considered constraints, 
this module shall call the backends. In the latter case, the backends are called
with the constraint set that is obtained after eliminating a certain
number of variables.  

# Integer arithmetic

One can also use this approach for (linear) integer arithmetic
as unsatisfiability over the real domain implies unsatisfiability over the integer
domain. In addition to that, one can heuristically try to construct integer solutions
by considering the lowest upper and the highest lower bound of a variable that can be derived
from the respective elimination step. Note that this approach is incomplete.  
