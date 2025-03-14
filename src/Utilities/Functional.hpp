// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <tuple>
#include <utility>

#include "Utilities/ConstantExpressions.hpp"
#include "Utilities/ContainerHelpers.hpp"
#include "Utilities/ErrorHandling/Assert.hpp"
#include "Utilities/ErrorHandling/StaticAssert.hpp"
#include "Utilities/ForceInline.hpp"
#include "Utilities/Math.hpp"

/*!
 * \ingroup UtilitiesGroup
 * \brief Higher order function objects similar to `std::plus`, etc.
 *
 * \details
 * These chaining function objects can be used to represent highly general
 * mathematical operations
 * 1. as types, which can be passed around in template arguments, and
 * 2. such that any time they can be evaluated at compile time, they will be.
 *
 * As an illustrative example, consider the definition of a general sinusoid
 * function object type :
 * \snippet Utilities/Test_Functional.cpp using_sinusoid
 * which then gives a type which when instantiated and evaluated will give the
 * answer \f$ a\times\sin(b + c \times d)\f$ from calling `Sinusoid{}(a,b,c,d)`
 *
 * As a more creative example, we can take advantage of literals to make, for
 * instance, distributions. Let's make a Gaussian with mean at 5.0 and unity
 * variance
 * \snippet Utilities/Test_Functional.cpp using_gaussian
 *
 * This gives us a function object whose call operator takes one argument that
 * gives the value of the desired Gaussian distribution \f$ e^{-(x - 5.0)^2}
 * \f$
 */
namespace funcl {
// using for overload resolution with blaze
using std::max;
using std::min;

/// \cond
template <size_t Arity>
struct Functional {
  static constexpr size_t arity = Arity;

 protected:
  template <class C, size_t Offset, class... Ts, size_t... Is>
  static constexpr decltype(auto) helper(const std::tuple<Ts...>& t,
                                         std::index_sequence<Is...> /*meta*/) {
    return C{}(std::get<Offset + Is>(t)...);
  }
};

struct Identity;
/// \endcond

/// Functional that asserts that the function object `C` applied to the first
/// and second arguments are equal and returns the function object C applied to
/// the first argument
template <class C = Identity>
struct AssertEqual : Functional<2> {
  template <class T>
  const T& operator()(const T& t0, const T& t1) {
    DEBUG_STATIC_ASSERT(
        C::arity == 1,
        "The arity of the functional passed to AssertEqual must be 1");
    ASSERT(C{}(t0) == C{}(t1), "Values are not equal in funcl::AssertEqual "
           << C{}(t0) << " and " << C{}(t1));
    return C{}(t0);
  }
};

#define MAKE_BINARY_FUNCTIONAL(NAME, OPERATOR)                                 \
  /** Functional for computing `OPERATOR` from two objects */                  \
  template <class C0 = Identity, class C1 = C0>                                \
  struct NAME : Functional<C0::arity + C1::arity> {                            \
    using base = Functional<C0::arity + C1::arity>;                            \
    template <class... Ts>                                                     \
    constexpr auto operator()(const Ts&... ts) {                               \
      return OPERATOR(                                                         \
          base::template helper<C0, 0>(std::tuple<const Ts&...>(ts...),        \
                                       std::make_index_sequence<C0::arity>{}), \
          base::template helper<C1, C0::arity>(                                \
              std::tuple<const Ts&...>(ts...),                                 \
              std::make_index_sequence<C1::arity>{}));                         \
    }                                                                          \
  };                                                                           \
  /** \cond */                                                                 \
  template <class C1>                                                          \
  struct NAME<Identity, C1> : Functional<1 + C1::arity> {                      \
    template <class T0, class... Ts>                                           \
    constexpr auto operator()(const T0& t0, const Ts&... ts) {                 \
      return OPERATOR(t0, C1{}(ts...));                                        \
    }                                                                          \
  };                                                                           \
  template <>                                                                  \
  struct NAME<Identity, Identity> : Functional<2> {                            \
    template <class T0, class T1>                                              \
    constexpr auto operator()(const T0& t0, const T1& t1) {                    \
      return OPERATOR(t0, t1);                                                 \
    }                                                                          \
  } /** \endcond */

#define MAKE_BINARY_INPLACE_OPERATOR(NAME, OPERATOR)               \
  /** Functional for computing `OPERATOR` of two objects */        \
  template <class C0 = Identity, class C1 = C0>                    \
  struct NAME : Functional<C0::arity + C1::arity> {                \
    using base = Functional<C0::arity + C1::arity>;                \
    template <class... Ts>                                         \
    constexpr decltype(auto) operator()(Ts&... ts) {               \
      return base::template helper<C0, 0>(                         \
          std::tuple<const Ts&...>(ts...),                         \
          std::make_index_sequence<C0::arity>{})                   \
          OPERATOR base::template helper<C1, C0::arity>(           \
              std::tuple<const Ts&...>(ts...),                     \
              std::make_index_sequence<C1::arity>{});              \
    }                                                              \
  };                                                               \
  /** \cond */                                                     \
  template <class C1>                                              \
  struct NAME<Identity, C1> : Functional<1 + C1::arity> {          \
    template <class T0, class... Ts>                               \
    constexpr decltype(auto) operator()(T0& t0, const Ts&... ts) { \
      return t0 OPERATOR C1{}(ts...);                              \
    }                                                              \
  };                                                               \
  template <>                                                      \
  struct NAME<Identity, Identity> : Functional<2> {                \
    static constexpr size_t arity = 2;                             \
    template <class T0, class T1>                                  \
    constexpr decltype(auto) operator()(T0& t0, const T1& t1) {    \
      return t0 OPERATOR t1;                                       \
    }                                                              \
  } /** \endcond */

#define MAKE_BINARY_OPERATOR(NAME, OPERATOR)                   \
  /** Functional for computing `OPERATOR` of two objects */    \
  template <class C0 = Identity, class C1 = C0>                \
  struct NAME : Functional<C0::arity + C1::arity> {            \
    using base = Functional<C0::arity + C1::arity>;            \
    template <class... Ts>                                     \
    constexpr auto operator()(const Ts&... ts) {               \
      return base::template helper<C0, 0>(                     \
          std::tuple<const Ts&...>(ts...),                     \
          std::make_index_sequence<C0::arity>{})               \
          OPERATOR base::template helper<C1, C0::arity>(       \
              std::tuple<const Ts&...>(ts...),                 \
              std::make_index_sequence<C1::arity>{});          \
    }                                                          \
  };                                                           \
  /** \cond */                                                 \
  template <class C1>                                          \
  struct NAME<Identity, C1> : Functional<1 + C1::arity> {      \
    template <class T0, class... Ts>                           \
    constexpr auto operator()(const T0& t0, const Ts&... ts) { \
      return t0 OPERATOR C1{}(ts...);                          \
    }                                                          \
  };                                                           \
  template <>                                                  \
  struct NAME<Identity, Identity> : Functional<2> {            \
    static constexpr size_t arity = 2;                         \
    template <class T0, class T1>                              \
    constexpr auto operator()(const T0& t0, const T1& t1) {    \
      return t0 OPERATOR t1;                                   \
    }                                                          \
  } /** \endcond */

#define MAKE_LITERAL_VAL(NAME, VAL)                                    \
  /** Functional literal for `VAL` */                                  \
  struct Literal##NAME : Functional<0> {                               \
    constexpr double operator()() { return static_cast<double>(VAL); } \
  }

#define MAKE_UNARY_FUNCTIONAL(NAME, OPERATOR)             \
  /** Functional for computing `OPERATOR` on an object */ \
  template <typename C0 = Identity>                       \
  struct NAME;                                            \
  /** \cond */                                            \
  template <typename C0>                                  \
  struct NAME : Functional<C0::arity> {                   \
    template <class... Ts>                                \
    constexpr auto operator()(const Ts&... ts) {          \
      return OPERATOR(C0{}(ts...));                       \
    }                                                     \
  };                                                      \
  template <>                                             \
  struct NAME<Identity> : Functional<1> {                 \
    template <class T0>                                   \
    constexpr auto operator()(const T0& t0) {             \
      return OPERATOR(t0);                                \
    }                                                     \
  } /** \endcond */

/// Functional to retrieve the `ArgumentIndex`th argument
template <size_t Arity, size_t ArgumentIndex = 0, class C = Identity>
struct GetArgument : Functional<Arity> {
  template <class... Ts>
  constexpr decltype(auto) operator()(const Ts&... ts) {
    static_assert(Arity == sizeof...(Ts),
                  "The arity passed to GetArgument must be the same as the "
                  "actually arity of the function.");
    return C{}(std::get<ArgumentIndex>(std::tuple<const Ts&...>(ts...)));
  }
};

/// The identity higher order function object
struct Identity : Functional<1> {
  template <class T>
  SPECTRE_ALWAYS_INLINE constexpr const T& operator()(const T& t) {
    return t;
  }
};

template <int val, typename Type = double>
struct Literal : Functional<0> {
  constexpr Type operator()() { return static_cast<Type>(val); }
};

MAKE_BINARY_INPLACE_OPERATOR(DivAssign, /=);
MAKE_BINARY_INPLACE_OPERATOR(MinusAssign, -=);
MAKE_BINARY_INPLACE_OPERATOR(MultAssign, *=);
MAKE_BINARY_INPLACE_OPERATOR(PlusAssign, +=);

MAKE_BINARY_OPERATOR(Divides, /);
MAKE_BINARY_OPERATOR(Minus, -);
MAKE_BINARY_OPERATOR(Multiplies, *);
MAKE_BINARY_OPERATOR(Plus, +);
MAKE_BINARY_OPERATOR(And, and);
MAKE_BINARY_OPERATOR(Or, or);

MAKE_BINARY_FUNCTIONAL(Atan2, atan2);
MAKE_BINARY_FUNCTIONAL(Hypot, hypot);
MAKE_BINARY_FUNCTIONAL(Max, max);
MAKE_BINARY_FUNCTIONAL(Min, min);
MAKE_BINARY_FUNCTIONAL(Pow, pow);

MAKE_LITERAL_VAL(Pi, M_PI);
MAKE_LITERAL_VAL(E, M_E);

MAKE_UNARY_FUNCTIONAL(Abs, abs);
MAKE_UNARY_FUNCTIONAL(Acos, acos);
MAKE_UNARY_FUNCTIONAL(Acosh, acosh);
MAKE_UNARY_FUNCTIONAL(Asin, asin);
MAKE_UNARY_FUNCTIONAL(Asinh, asinh);
MAKE_UNARY_FUNCTIONAL(Atan, atan);
MAKE_UNARY_FUNCTIONAL(Atanh, atanh);
MAKE_UNARY_FUNCTIONAL(Cbrt, cbrt);
MAKE_UNARY_FUNCTIONAL(Conj, conj);
MAKE_UNARY_FUNCTIONAL(Cos, cos);
MAKE_UNARY_FUNCTIONAL(Cosh, cosh);
MAKE_UNARY_FUNCTIONAL(Erf, erf);
MAKE_UNARY_FUNCTIONAL(Exp, exp);
MAKE_UNARY_FUNCTIONAL(Exp2, exp2);
MAKE_UNARY_FUNCTIONAL(Fabs, fabs);
MAKE_UNARY_FUNCTIONAL(Imag, imag);
MAKE_UNARY_FUNCTIONAL(InvCbrt, invcbrt);
MAKE_UNARY_FUNCTIONAL(InvSqrt, invsqrt);
MAKE_UNARY_FUNCTIONAL(Log, log);
MAKE_UNARY_FUNCTIONAL(Log10, log10);
MAKE_UNARY_FUNCTIONAL(Log2, log2);
MAKE_UNARY_FUNCTIONAL(Real, real);
MAKE_UNARY_FUNCTIONAL(Sin, sin);
MAKE_UNARY_FUNCTIONAL(Sinh, sinh);
MAKE_UNARY_FUNCTIONAL(Sqrt, sqrt);
MAKE_UNARY_FUNCTIONAL(StepFunction, step_function);
MAKE_UNARY_FUNCTIONAL(Tan, tan);
MAKE_UNARY_FUNCTIONAL(Tanh, tanh);
MAKE_UNARY_FUNCTIONAL(Negate, -);

/// Function for computing an integer power, forwards to template pow<N>()
template <int N, typename C0 = Identity>
struct UnaryPow;

/// \cond
template <int N, typename C0>
struct UnaryPow : Functional<C0::arity> {
  template <class... Ts>
  constexpr auto operator()(const Ts&... ts) {
    return pow<N>(C0{}(ts...));
  }
};

template <int N>
struct UnaryPow<N, Identity> : Functional<1> {
  template <class T0>
  constexpr auto operator()(const T0& t0) {
    return pow<N>(t0);
  }
};
/// \endcond

/// Function for squaring a quantity
template <class C = Identity>
struct Square : Functional<C::arity> {
  template <class... Ts>
  constexpr auto operator()(const Ts&... ts) {
    decltype(auto) result = C{}(ts...);
    return result * result;
  }
};

/// Function that applies `C` to every element of the operands. This function is
/// currently only tested for `std::vector` operands. Operands other than the
/// first may be a single value, which is applied element-wise to the vector. If
/// needed, this function can be generalized further.
template <typename C>
struct ElementWise : Functional<C::arity> {
  template <typename T0, typename... Ts>
  auto operator()(const T0& t0, const Ts&... ts) {
    const size_t size = get_size(t0);
    ASSERT(((get_size(ts) == size or get_size(ts) == 1) and ...),
           "Sizes must be the same but got "
               << (std::vector<size_t>{size, get_size(ts)...}));
    T0 result(size);
    for (size_t i = 0; i < size; ++i) {
      get_element(result, i) = C{}(get_element(t0, i), get_element(ts, i)...);
    }
    return result;
  }
};

/// Function that merges two containers using the `merge` method of the first
/// container. Can be used to collect data in a `std::map` in a reduction.
template <typename C0 = Identity, typename C1 = C0>
struct Merge : Functional<2> {
  template <typename T>
  T operator()(const T& t0, const T& t1) {
    auto result = C0{}(t0);
    auto to_merge = C1{}(t1);
    result.merge(to_merge);
    return result;
  }
};

#undef MAKE_BINARY_FUNCTIONAL
#undef MAKE_BINARY_INPLACE_OPERATOR
#undef MAKE_BINARY_OPERATOR
#undef MAKE_UNARY_FUNCTIONAL
}  // namespace funcl
