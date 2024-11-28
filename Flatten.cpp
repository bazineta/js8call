#include "Flatten.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>
#include <vendor/Eigen/Dense>

/******************************************************************************/
// Flatten Constants
/******************************************************************************/

namespace
{
  // Tunable settings; degree of the polynomial used for the baseline
  // curve fit, and the percentile of the span at which to sample. In
  // general, a 5th degree polynomial and the 10th percentile should
  // be optimal.

  constexpr auto FLATTEN_DEGREE =  5;
  constexpr auto FLATTEN_SAMPLE = 10;

  // We're going to do a pairwise Estrin's evaluation of the polynomial
  // coefficients, so it's critical that the degree of the polynomial is
  // odd, resulting in an even number of coefficients.

  static_assert(FLATTEN_DEGREE &    1, "Degree must be odd");
  static_assert(FLATTEN_SAMPLE >= 0 &&
                FLATTEN_SAMPLE <= 100, "Sample must be a percentage");

  // Since we know the degree of the polynomial, and thus the number of
  // nodes that we're going to use, we can do all the trigonometry work
  // required to calculate the Chebyshev nodes in advance, by computing
  // them over the range [0, 1]; we can then scale these at runtime to
  // a span of any size by simple multiplication.
  //
  // Downside to this with C++17 is that std::cos() is not yet constexpr,
  // as it is in C++20, so a Taylor series approximation to roll our own.￼

  constexpr auto FLATTEN_NODES = []()
  {
    constexpr auto cos = [](double x, double precision = 1e-16)
    {
      constexpr auto factorial = [](auto self,
                                    int  n) noexcept -> double
      {
        return (n <= 1) ? 1.0 : n * self(self, n - 1);
      };

      constexpr auto power = [](auto   self,
                                double base,
                                int    exp) noexcept -> double
      {
        return exp == 0 ? 1.0 : base * self(self, base, exp - 1);
      };

      constexpr auto abs = [](double x)
      {
        return x < 0 ? -x : x;
      };

      auto  term  = 1.0;
      auto  value = term;
      auto  n     = 1;

      while (abs(term) > precision)
      {
        term = power(power, -1.0, n) * power    (power, x,  2 * n) /
                                       factorial(factorial, 2 * n);
        value += term;
        ++n;
      }

      return value;
    };

    auto           nodes = std::array<double, FLATTEN_DEGREE + 1>{};
    constexpr auto slice = M_PI / (2.0 * nodes.size());

    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
      nodes[i] = 0.5 * (1.0 - cos(slice * (2.0 * i + 1)));
    }

    return nodes;
  }();
}

/******************************************************************************/
// Private Implementation
/******************************************************************************/

// Functor that, when provided with a spectrum, performs a flattening
// operation. This is intended to work in a manner similar to that of
// the Fortran flat4() subroutine, though our implementation differs.
//
// Note that this is a functor; it's serially reusable, but it's not
// reentrant. Call it from one thread only.

class Flatten::Impl
{
  using Points       = Eigen::Matrix<double, FLATTEN_NODES.size(), 2>;
  using Vandermonde  = Eigen::Matrix<double, FLATTEN_NODES.size(),
                                             FLATTEN_NODES.size()>;
  using Coefficients = Eigen::Vector<double, FLATTEN_NODES.size()>;

  Points       p;
  Vandermonde  V;
  Coefficients c;

  // Polynomial evaluation using Estrin's method, loop is unrolled at
  // compile time; a compiler should emit SIMD instructions from what
  // it sees here.

  template <Eigen::Index... I>
  inline auto
  evaluate(std::size_t const i,
           std::integer_sequence<Eigen::Index, I...>) const
  {
    auto baseline = 0.0;
    auto exponent = 1.0;

    ((baseline += (c[I * 2] + c[I * 2 + 1] * i) * exponent, exponent *= i * i), ...);

    return static_cast<float>(baseline);
  }

  inline auto
  evaluate(std::size_t const i) const
  {
    return evaluate(i, std::make_integer_sequence<Eigen::Index, Coefficients::SizeAtCompileTime / 2>{});
  }

public:

  void
  operator()(float     * const data,
             std::size_t const size)
  {
    // Loop invariants; sentinel one past the end of the range, and
    // the number of points in each of the arms on either side of a
    // node.

    auto const end = data + size;
    auto const arm = size / (2 * FLATTEN_NODES.size());

    // Collect lower envelope points; use Chebyshev node interpolants
    // to reduce Runge's phenomenon oscillations.

    for (std::size_t i = 0; i < FLATTEN_NODES.size(); ++i)
    {
      auto const node = size * FLATTEN_NODES[i];
      auto const base = data + static_cast<int>(std::round(node));
      auto       span = std::vector<float>(std::clamp(base - arm, data, end),
                                           std::clamp(base + arm, data, end));

      auto const n = span.size() * FLATTEN_SAMPLE / 100;

      std::nth_element(span.begin(), span.begin() + n, span.end());

      p.row(i) << node, span[n];
    }

    // Extract x and y values from points and prepare the Vandermonde
    // matrix, initializing the first column with 1 (x^0); remaining
    // columns are filled with the Schur product.

    Eigen::VectorXd x = p.col(0);
    Eigen::VectorXd y = p.col(1);

    V.col(0).setOnes();
    for (Eigen::Index i = 1; i < V.cols(); ++i)
    {
      V.col(i) = V.col(i - 1).cwiseProduct(x);
    }

    // Solve the least squares problem for polynomial coefficients;
    // evaluate the polynomial and subtract the baseline.

    c = V.colPivHouseholderQr().solve(y);

    for (std::size_t i = 0; i < size; ++i) data[i] -= evaluate(i);
  }
};

/******************************************************************************/
// Public Implementation
/******************************************************************************/

Flatten::Flatten(bool const flatten)
: m_impl(flatten ? std::make_unique<Impl>() : nullptr)
{}

Flatten::~Flatten() = default;

void
Flatten::operator()(bool const flatten)
{
  m_impl.reset(flatten ? new Impl() : nullptr);
}

void
Flatten::operator()(float     * const data,
                    std::size_t const size)
{
  if (m_impl) (*m_impl)(data, size);
}

/******************************************************************************/
