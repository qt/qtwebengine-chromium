#include "../Eigen/Core"

// Reproduces the "Dynamic-sized Matrix breaks tuple_size" bug reported on !2336.
// With an enable_if_t base-clause the error is a cryptic non-SFINAE hard error;
// with our static_assert-in-body fix it becomes a friendly diagnostic. Either
// way, the compile must fail for MatrixXd.
#ifdef EIGEN_SHOULD_FAIL_TO_BUILD
#define ROWS Eigen::Dynamic
#define COLS Eigen::Dynamic
#else
#define ROWS 3
#define COLS 1
#endif

#include <tuple>

int main() {
  (void)std::tuple_size<Eigen::Matrix<double, ROWS, COLS>>::value;
  return 0;  // Test should pass.
}
