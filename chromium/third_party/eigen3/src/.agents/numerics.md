# Numerical Code

Use this guidance when changing scalar math, packet math, decompositions, eigensolvers, linear solvers, matrix
functions, or numerical tests. The nearby implementation, tests, and public documentation in the checked-out tree
are the source of truth; this file defines the review standard rather than an algorithm.

## Standards and Accuracy Contracts

- Follow the applicable ISO C++ and incorporated ISO C library contracts. IEEE 754 requirements apply where the
  platform and API claim IEC 60559 behavior. cppreference is a useful secondary summary, not a normative
  specification.
- Distinguish exact semantic requirements from approximation quality. NaN, infinity, signed zero, domain errors,
  and function-specific boundary behavior must follow the contract. For ordinary finite inputs, the C++ standard
  generally does not promise correctly rounded elementary functions, so Eigen's documented or established ULP
  budget is the relevant target.
- Do not use `VERIFY_IS_APPROX` as the acceptance criterion for a newly designed numerical kernel. Its defaults in
  [`test/numerical_test_helpers.h`](../test/numerical_test_helpers.h) are deliberately loose test-framework
  tolerances, not machine-epsilon or ULP bounds.
- Scale coverage with the change. A narrow fix needs focused regression cases and nearby coverage; a new algorithm
  or shared kernel needs broad conditioning, scalar-type, and backend coverage.

## Scalar Math

Test regular inputs across the full supported domain and concentrate samples near discontinuities, roots, extrema,
range-reduction boundaries, overflow and underflow thresholds, and difficult rounding cases. Use an error metric
that matches the contract:

- Use ULP error when evaluating a floating-point approximation against a correctly rounded or high-precision
  reference. Relative error is not meaningful near zero, and absolute error alone hides scale-dependent failures.
- Use MPFR for ground truth when an accuracy decision depends on the reference. Eigen's C++
  [`ULP accuracy tool`](../test/ulp_accuracy/README.md) supports MPFR and standard-library references. The
  [`coefficient-wise math table`](../doc/CoeffwiseMathFunctionsTable.dox) records existing accuracy expectations.
- Sollya is appropriate for polynomial or rational approximation design. Record the function, domain, precision,
  error objective, tool version, and generation command or script so coefficients are reproducible; verify the
  emitted implementation independently with MPFR.

Test special values explicitly: `+0`, `-0`, positive and negative infinity, quiet NaN, normal/subnormal boundaries,
the smallest subnormal, and values immediately on both sides of each domain boundary. Approximate comparisons can
treat two NaNs as matching and cannot distinguish the sign of zero. Therefore use explicit predicates:

- Check NaN with `(numext::isnan)(value)`.
- Check infinity with `(numext::isinf)(value)` and check its sign separately.
- Check zero by equality and its sign with `(numext::signbit)(value)`.
- Check finite classification when overflow or invalid results are possible.

## Decompositions and Solvers

Prefer backward-error and invariant checks over forward comparison with one reference answer. Depending on the
operation, test normalized reconstruction error, solve residual, eigenpair residual, orthogonality/unitarity, rank,
symmetry, or structure preservation. Express tolerances as named bounds derived from
`NumTraits<RealScalar>::epsilon()`, dimension, and the expected operation count; avoid unexplained decimal literals.

Forward error is condition-dependent. A well-conditioned problem may support a tight result comparison, while a
near-singular problem can have a small residual and a large forward error. Estimate or bound conditioning when a
forward comparison is necessary, and do not reject a stable answer merely because a different stable algorithm
selects different vectors, signs, phases, pivots, or bases for a clustered invariant subspace.

Exercise structures relevant to the algorithm: well-conditioned, ill-conditioned, near-singular, singular,
rank-deficient, clustered/repeated spectra, extreme scaling, and the matrix properties promised by the API. Useful
families include Hilbert, Vandermonde, Wilkinson, Toeplitz/KMS, banded, defective or near-defective, and barely
positive-definite matrices. Check error/status reporting as well as successful results.

For uniform transformations and built-in operations on small structured submatrices, use fixed-size block expressions,
such as `T.template block<2, 2>(i, i).determinant()`, so the mathematical structure and compile-time dimensions remain
explicit. This is especially useful for Schur blocks, pivots, and small panels; use coefficient access instead when the
entries require distinct formulas.

Where LAPACK has a counterpart, require comparable backward stability, conditioning behavior, pivoting robustness,
and test-category coverage. Do not require identical internal steps, pivot order, eigenvector signs/phases, or
roundoff-level output. Higham's *Accuracy and Stability of Numerical Algorithms* and Golub and Van Loan's *Matrix
Computations* are standard references for choosing error measures and adversarial inputs.

## Packet Accuracy

- Test the scalar path, generic packet fallback, and every affected backend specialization that is available. Build
  and run [`test/packetmath.cpp`](../test/packetmath.cpp) and, for special functions,
  [`unsupported/test/special_packetmath.cpp`](../unsupported/test/special_packetmath.cpp). Report backends that were
  not available locally.
- Compare packet results with the scalar contract for special values, but use MPFR rather than assuming the scalar
  standard-library result is accurate enough to set a new finite-input ULP target.
- Cover every lane, mixed regular/special lanes, alignment and tail cases where applicable, and values around
  approximation-region boundaries. A packet implementation must not let one lane's special value affect another.
- Treat a few-ULP performance tradeoff as a measured, documented finite-input decision. It does not waive NaN,
  infinity, signed-zero, or domain semantics unless the API and build mode explicitly document different behavior.

## Subnormals and Flush-to-Zero

Require gradual-underflow behavior when the target and active floating-point mode support it. Some targets or build
modes have fixed or enabled flush-to-zero (FTZ) behavior, so an impossible subnormal expectation must be detected and
conditionalized rather than made flaky. Use the facilities and platform notes in
[`test/fp_control.h`](../test/fp_control.h) and nearby packet tests.

Keep an FTZ exception narrow: document the affected target and operation, preserve and restore controllable FP
state, and still verify normal values, NaN, infinity, signed zero, and scalar/packet consistency in that mode. Do not
use FTZ as a blanket reason to skip underflow tests or to hide accidental compiler flags that changed semantics.

## Provenance

Learn algorithms from published papers, standards, and textbooks, then write an original Eigen implementation. Cite
the specific reference inline by author/year and algorithm, routine, paper, or working-note identifier. If adapting
source code rather than an idea, first confirm that its license and provenance are compatible with Eigen. A citation
does not make copied expression from an incompatible or unknown source permissible, and attribution must never be
invented. Include the numerical rationale for non-obvious scaling, pivoting, stopping, and tolerance choices.
