// Distributed under the MIT License.
// See LICENSE.txt for details.

/// \file
/// Defines class Variables

#pragma once

#include <algorithm>
#include <array>
#include <blaze/math/AlignmentFlag.h>
#include <blaze/math/CustomVector.h>
#include <blaze/math/DenseVector.h>
#include <blaze/math/GroupTag.h>
#include <blaze/math/PaddingFlag.h>
#include <blaze/math/TransposeFlag.h>
#include <blaze/math/Vector.h>
#include <limits>
#include <memory>
#include <ostream>
#include <pup.h>
#include <string>
#include <tuple>

#include "DataStructures/DataBox/PrefixHelpers.hpp"
#include "DataStructures/DataBox/Subitems.hpp"
#include "DataStructures/DataBox/Tag.hpp"
#include "DataStructures/DataBox/TagName.hpp"
#include "DataStructures/DataBox/TagTraits.hpp"
#include "DataStructures/DataVector.hpp"
#include "DataStructures/MathWrapper.hpp"
#include "DataStructures/Tensor/IndexType.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "Utilities/EqualWithinRoundoff.hpp"
#include "Utilities/ErrorHandling/Assert.hpp"
#include "Utilities/ForceInline.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/Literals.hpp"
#include "Utilities/MakeSignalingNan.hpp"
#include "Utilities/MemoryHelpers.hpp"
#include "Utilities/PrettyType.hpp"
#include "Utilities/Requires.hpp"
#include "Utilities/SetNumberOfGridPoints.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TaggedTuple.hpp"
#include "Utilities/TypeTraits.hpp"
#include "Utilities/TypeTraits/IsA.hpp"

/// \cond
template <typename X, typename Symm, typename IndexList>
class Tensor;
template <typename TagsList>
class Variables;
template <typename T, typename VectorType, size_t StaticSize>
class VectorImpl;
namespace Tags {
template <typename TagsList>
class Variables;
}  // namespace Tags
/// \endcond

/*!
 * \ingroup DataStructuresGroup
 * \brief A Variables holds a contiguous memory block with Tensors pointing
 * into it.
 *
 * The `Tags` are `struct`s that must have a public type alias `type` whose
 * value must be a `Tensor<DataVector, ...>`, a `static' method `name()` that
 * returns a `std::string` of the tag name, and must derive off of
 * `db::SimpleTag`. In general, they should be DataBoxTags that are not compute
 * items. For example,
 *
 * \snippet Helpers/DataStructures/TestTags.hpp simple_variables_tag
 *
 * Prefix tags can also be stored and their format is:
 *
 * \snippet Helpers/DataStructures/TestTags.hpp prefix_variables_tag
 *
 * #### Design Decisions
 *
 * The `Variables` class is designed to hold several different `Tensor`s
 * performing one memory allocation for all the `Tensor`s. The advantage is that
 * memory allocations are quite expensive, especially in a parallel environment.
 *
 * In Debug mode, or if the macro `SPECTRE_NAN_INIT` is defined, the contents
 * are initialized with `NaN`s.
 *
 * `Variables` stores the data it owns in a `std::unique_ptr<double[]>`
 * instead of a `std::vector` because `std::vector` value-initializes its
 * contents, which is very slow.
 */
template <typename... Tags>
class Variables<tmpl::list<Tags...>> {
 public:
  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  static constexpr auto transpose_flag = blaze::defaultTransposeFlag;

  /// A typelist of the Tags whose variables are held
  using tags_list = tmpl::list<Tags...>;
  static_assert(sizeof...(Tags) > 0,
                "You must provide at least one tag to the Variables "
                "for type inference");
  static_assert((db::is_simple_tag_v<Tags> and ...));
  static_assert(tmpl2::flat_all_v<tt::is_a_v<Tensor, typename Tags::type>...>);

 private:
  using first_tensors_type = typename tmpl::front<tags_list>::type::type;

 public:
  static_assert(tmpl2::flat_all_v<std::is_same_v<typename Tags::type::type,
                                                 first_tensors_type>...> or
                    tmpl2::flat_all_v<is_spin_weighted_of_same_type_v<
                        typename Tags::type::type, first_tensors_type>...>,
                "All tensors stored in a single Variables must "
                "have the same internal storage type.");

  using vector_type =
      tmpl::conditional_t<is_any_spin_weighted_v<first_tensors_type>,
                          typename first_tensors_type::value_type,
                          first_tensors_type>;
  using value_type = typename vector_type::value_type;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using allocator_type = std::allocator<value_type>;
  using pointer_type =
      blaze::CustomVector<value_type, blaze::AlignmentFlag::unaligned,
                          blaze::PaddingFlag::unpadded, transpose_flag,
                          blaze::GroupTag<0>, vector_type>;

  static_assert(
      std::is_fundamental_v<value_type> or tt::is_a_v<std::complex, value_type>,
      "`value_type` of the Variables (so the storage type of the vector type "
      "within the tensors in the Variables) must be either a fundamental type "
      "or a std::complex. If this constraint is relaxed, the value_type "
      "should be handled differently in the Variables, including pass by "
      "reference.");

  /// The number of variables of the Variables object is holding. E.g.
  /// \f$\psi_{ab}\f$ would be counted as one variable.
  static constexpr auto number_of_variables = sizeof...(Tags);

  /// The total number of independent components of all the variables. E.g.
  /// a rank-2 symmetric spacetime Tensor \f$\psi_{ab}\f$ in 3 spatial
  /// dimensions would have 10 independent components.
  static constexpr size_t number_of_independent_components =
      (... + Tags::type::size());

  /// Default construct an empty Variables class, Charm++ needs this
  Variables();

  explicit Variables(size_t number_of_grid_points);

  Variables(size_t number_of_grid_points, value_type value);

  /// Construct a non-owning Variables that points to `start`. `size` is the
  /// size of the allocation, which must be
  /// `number_of_grid_points * Variables::number_of_independent_components`
  Variables(pointer start, size_t size);

  Variables(Variables&& rhs);
  Variables& operator=(Variables&& rhs);

  Variables(const Variables& rhs);
  Variables& operator=(const Variables& rhs);

  /// @{
  /// Copy and move semantics for wrapped variables
  template <typename... WrappedTags,
            Requires<tmpl2::flat_all<std::is_same<
                db::remove_all_prefixes<WrappedTags>,
                db::remove_all_prefixes<Tags>>::value...>::value> = nullptr>
  explicit Variables(Variables<tmpl::list<WrappedTags...>>&& rhs);
  template <typename... WrappedTags,
            Requires<tmpl2::flat_all<std::is_same<
                db::remove_all_prefixes<WrappedTags>,
                db::remove_all_prefixes<Tags>>::value...>::value> = nullptr>
  Variables& operator=(Variables<tmpl::list<WrappedTags...>>&& rhs);

  template <typename... WrappedTags,
            Requires<tmpl2::flat_all<std::is_same<
                db::remove_all_prefixes<WrappedTags>,
                db::remove_all_prefixes<Tags>>::value...>::value> = nullptr>
  explicit Variables(const Variables<tmpl::list<WrappedTags...>>& rhs);
  template <typename... WrappedTags,
            Requires<tmpl2::flat_all<std::is_same<
                db::remove_all_prefixes<WrappedTags>,
                db::remove_all_prefixes<Tags>>::value...>::value> = nullptr>
  Variables& operator=(const Variables<tmpl::list<WrappedTags...>>& rhs);
  /// @}

  /// \cond HIDDEN_SYMBOLS
  ~Variables() = default;
  /// \endcond

  /// @{
  /// Initialize a Variables to the state it would have after calling
  /// the constructor with the same arguments.
  // this should be updated if we ever use a variables which has a `value_type`
  // larger than ~2 doubles in size.
  void initialize(size_t number_of_grid_points);
  void initialize(size_t number_of_grid_points, value_type value);
  /// @}

  /// @{
  /// Set the VectorImpl to be a reference to another VectorImpl object
  void set_data_ref(const gsl::not_null<Variables*> rhs) {
    set_data_ref(rhs->data(), rhs->size());
  }

  void set_data_ref(pointer const start, const size_t size) {
    variable_data_impl_dynamic_.reset();
    if (start == nullptr) {
      variable_data_ = pointer_type{};
      size_ = 0;
      number_of_grid_points_ = 0;
    } else {
      size_ = size;
      variable_data_.reset(start, size_);
      ASSERT(
          size_ % number_of_independent_components == 0,
          "The size (" << size_
                       << ") must be a multiple of the number of independent "
                          "components ("
                       << number_of_independent_components
                       << ") since we calculate the number of grid points from "
                          "the size and number of independent components.");
      number_of_grid_points_ = size_ / number_of_independent_components;
    }
    owning_ = false;
    add_reference_variable_data();
  }
  /// @}

  constexpr SPECTRE_ALWAYS_INLINE size_t number_of_grid_points() const {
    return number_of_grid_points_;
  }

  /// Number of grid points * number of independent components
  constexpr SPECTRE_ALWAYS_INLINE size_type size() const { return size_; }

  /// @{
  /// Access pointer to underlying data
  pointer data() { return variable_data_.data(); }
  const_pointer data() const { return variable_data_.data(); }
  /// @}

  /// \cond HIDDEN_SYMBOLS
  /// Needed because of limitations and inconsistency between compiler
  /// implementations of friend function templates with auto return type of
  /// class templates
  const auto& get_variable_data() const { return variable_data_; }
  /// \endcond

  /// Returns true if the class owns the data
  bool is_owning() const { return owning_; }

  // clang-tidy: redundant-declaration
  template <typename Tag, typename TagList>
  friend constexpr typename Tag::type& get(  // NOLINT
      Variables<TagList>& v);
  template <typename Tag, typename TagList>
  friend constexpr const typename Tag::type& get(  // NOLINT
      const Variables<TagList>& v);

  /// Serialization for Charm++.
  // NOLINTNEXTLINE(google-runtime-references)
  void pup(PUP::er& p);

  /// @{
  /// \brief Assign a subset of the `Tensor`s from another Variables or a
  /// tuples::TaggedTuple. Any tags that aren't in both containers are
  /// ignored.
  ///
  /// \note There is no need for an rvalue overload because we need to copy into
  /// the contiguous array anyway
  template <typename... SubsetOfTags>
  void assign_subset(const Variables<tmpl::list<SubsetOfTags...>>& vars) {
    EXPAND_PACK_LEFT_TO_RIGHT([this, &vars]() {
      if constexpr (tmpl::list_contains_v<tmpl::list<Tags...>, SubsetOfTags>) {
        get<SubsetOfTags>(*this) = get<SubsetOfTags>(vars);
      } else {
        (void)this;
        (void)vars;
      }
    }());
  }

  template <typename... SubsetOfTags>
  void assign_subset(const tuples::TaggedTuple<SubsetOfTags...>& vars) {
    EXPAND_PACK_LEFT_TO_RIGHT([this, &vars]() {
      if constexpr (tmpl::list_contains_v<tmpl::list<Tags...>, SubsetOfTags>) {
        get<SubsetOfTags>(*this) = get<SubsetOfTags>(vars);
      } else {
        (void)this;
        (void)vars;
      }
    }());
  }
  /// @}

  /// Create a Variables from a subset of the `Tensor`s in this
  /// Variables
  template <typename SubsetOfTags>
  Variables<SubsetOfTags> extract_subset() const {
    Variables<SubsetOfTags> sub_vars(number_of_grid_points());
    tmpl::for_each<SubsetOfTags>([&](const auto tag_v) {
      using tag = tmpl::type_from<decltype(tag_v)>;
      get<tag>(sub_vars) = get<tag>(*this);
    });
    return sub_vars;
  }

  /// Create a non-owning Variables referencing a subset of the
  /// `Tensor`s in this Variables.  The referenced tensors must be
  /// consecutive in this Variables's tags list.
  ///
  /// \warning As with other appearances of non-owning variables, this
  /// method can cast away constness.
  template <typename SubsetOfTags>
  Variables<SubsetOfTags> reference_subset() const {
    if constexpr (std::is_same_v<SubsetOfTags, tmpl::list<>>) {
      return {};
    } else {
      using subset_from_tags_list = tmpl::reverse_find<
          tmpl::find<
              tags_list,
              std::is_same<tmpl::_1, tmpl::pin<tmpl::front<SubsetOfTags>>>>,
          std::is_same<tmpl::_1, tmpl::pin<tmpl::back<SubsetOfTags>>>>;
      static_assert(std::is_same_v<subset_from_tags_list, SubsetOfTags>,
                    "Tags passed to reference_subset must be consecutive tags "
                    "in the original tags_list.");

      using preceeding_tags = tmpl::pop_back<tmpl::reverse_find<
          tags_list,
          std::is_same<tmpl::_1, tmpl::pin<tmpl::front<SubsetOfTags>>>>>;

      static constexpr auto count_components = [](auto... tags) {
        return (0_st + ... + tmpl::type_from<decltype(tags)>::type::size());
      };
      static constexpr size_t number_of_preceeding_components =
          tmpl::as_pack<preceeding_tags>(count_components);
      static constexpr size_t number_of_components_in_subset =
          tmpl::as_pack<SubsetOfTags>(count_components);

      return {const_cast<value_type*>(data()) +
                  number_of_grid_points() * number_of_preceeding_components,
              number_of_grid_points() * number_of_components_in_subset};
    }
  }

  /// Create a non-owning version of this Variables with different
  /// prefixes on the tensors.  Both sets of prefixes must have the
  /// same tensor types.
  ///
  /// \warning As with other appearances of non-owning variables, this
  /// method can cast away constness.
  template <typename WrappedVariables>
  WrappedVariables reference_with_different_prefixes() const {
    static_assert(
        tmpl::all<tmpl::transform<
            typename WrappedVariables::tags_list, tmpl::list<Tags...>,
            std::is_same<tmpl::bind<db::remove_all_prefixes, tmpl::_1>,
                         tmpl::bind<db::remove_all_prefixes, tmpl::_2>>>>::
            value,
        "Unprefixed tags do not match!");
    static_assert(
        tmpl::all<tmpl::transform<
            typename WrappedVariables::tags_list, tmpl::list<Tags...>,
            std::is_same<tmpl::bind<tmpl::type_from, tmpl::_1>,
                         tmpl::bind<tmpl::type_from, tmpl::_2>>>>::value,
        "Tensor types do not match!");
    return {const_cast<value_type*>(data()), size()};
  }

  /// Converting constructor for an expression to a Variables class
  // clang-tidy: mark as explicit (we want conversion to Variables)
  template <typename VT, bool VF>
  Variables(const blaze::DenseVector<VT, VF>& expression);  // NOLINT

  template <typename VT, bool VF>
  Variables& operator=(const blaze::DenseVector<VT, VF>& expression);

  // Prevent the previous two declarations from implicitly converting
  // DataVectors and similar to Variables.
  template <typename T, typename VectorType, size_t StaticSize>
  Variables(const VectorImpl<T, VectorType, StaticSize>&) = delete;
  template <typename T, typename VectorType, size_t StaticSize>
  Variables& operator=(const VectorImpl<T, VectorType, StaticSize>&) = delete;

  template <typename... WrappedTags,
            Requires<tmpl2::flat_all<std::is_same_v<
                db::remove_all_prefixes<WrappedTags>,
                db::remove_all_prefixes<Tags>>...>::value> = nullptr>
  SPECTRE_ALWAYS_INLINE Variables& operator+=(
      const Variables<tmpl::list<WrappedTags...>>& rhs) {
    static_assert(
        (std::is_same_v<typename Tags::type, typename WrappedTags::type> and
         ...),
        "Tensor types do not match!");
    variable_data_ += rhs.variable_data_;
    return *this;
  }
  template <typename VT, bool VF>
  SPECTRE_ALWAYS_INLINE Variables& operator+=(
      const blaze::Vector<VT, VF>& rhs) {
    variable_data_ += rhs;
    return *this;
  }

  template <typename... WrappedTags,
            Requires<tmpl2::flat_all<std::is_same_v<
                db::remove_all_prefixes<WrappedTags>,
                db::remove_all_prefixes<Tags>>...>::value> = nullptr>
  SPECTRE_ALWAYS_INLINE Variables& operator-=(
      const Variables<tmpl::list<WrappedTags...>>& rhs) {
    static_assert(
        (std::is_same_v<typename Tags::type, typename WrappedTags::type> and
         ...),
        "Tensor types do not match!");
    variable_data_ -= rhs.variable_data_;
    return *this;
  }
  template <typename VT, bool VF>
  SPECTRE_ALWAYS_INLINE Variables& operator-=(
      const blaze::Vector<VT, VF>& rhs) {
    variable_data_ -= rhs;
    return *this;
  }

  SPECTRE_ALWAYS_INLINE Variables& operator*=(const value_type& rhs) {
    variable_data_ *= rhs;
    return *this;
  }

  SPECTRE_ALWAYS_INLINE Variables& operator/=(const value_type& rhs) {
    variable_data_ /= rhs;
    return *this;
  }

  template <typename... WrappedTags,
            Requires<tmpl2::flat_all<std::is_same_v<
                db::remove_all_prefixes<WrappedTags>,
                db::remove_all_prefixes<Tags>>...>::value> = nullptr>
  friend SPECTRE_ALWAYS_INLINE decltype(auto) operator+(
      const Variables<tmpl::list<WrappedTags...>>& lhs, const Variables& rhs) {
    static_assert(
        (std::is_same_v<typename Tags::type, typename WrappedTags::type> and
         ...),
        "Tensor types do not match!");
    return lhs.get_variable_data() + rhs.variable_data_;
  }
  template <typename VT, bool VF>
  friend SPECTRE_ALWAYS_INLINE decltype(auto) operator+(
      const blaze::DenseVector<VT, VF>& lhs, const Variables& rhs) {
    return *lhs + rhs.variable_data_;
  }
  template <typename VT, bool VF>
  friend SPECTRE_ALWAYS_INLINE decltype(auto) operator+(
      const Variables& lhs, const blaze::DenseVector<VT, VF>& rhs) {
    return lhs.variable_data_ + *rhs;
  }

  template <typename... WrappedTags,
            Requires<tmpl2::flat_all<std::is_same_v<
                db::remove_all_prefixes<WrappedTags>,
                db::remove_all_prefixes<Tags>>...>::value> = nullptr>
  friend SPECTRE_ALWAYS_INLINE decltype(auto) operator-(
      const Variables<tmpl::list<WrappedTags...>>& lhs, const Variables& rhs) {
    static_assert(
        (std::is_same_v<typename Tags::type, typename WrappedTags::type> and
         ...),
        "Tensor types do not match!");
    return lhs.get_variable_data() - rhs.variable_data_;
  }
  template <typename VT, bool VF>
  friend SPECTRE_ALWAYS_INLINE decltype(auto) operator-(
      const blaze::DenseVector<VT, VF>& lhs, const Variables& rhs) {
    return *lhs - rhs.variable_data_;
  }
  template <typename VT, bool VF>
  friend SPECTRE_ALWAYS_INLINE decltype(auto) operator-(
      const Variables& lhs, const blaze::DenseVector<VT, VF>& rhs) {
    return lhs.variable_data_ - *rhs;
  }

  friend SPECTRE_ALWAYS_INLINE decltype(auto) operator*(const Variables& lhs,
                                                        const value_type& rhs) {
    return lhs.variable_data_ * rhs;
  }
  friend SPECTRE_ALWAYS_INLINE decltype(auto) operator*(const value_type& lhs,
                                                        const Variables& rhs) {
    return lhs * rhs.variable_data_;
  }

  friend SPECTRE_ALWAYS_INLINE decltype(auto) operator/(const Variables& lhs,
                                                        const value_type& rhs) {
    return lhs.variable_data_ / rhs;
  }

  friend SPECTRE_ALWAYS_INLINE decltype(auto) operator-(const Variables& lhs) {
    return -lhs.variable_data_;
  }
  friend SPECTRE_ALWAYS_INLINE decltype(auto) operator+(const Variables& lhs) {
    return lhs.variable_data_;
  }

 private:
  /// @{
  /*!
   * \brief Subscript operator
   *
   * The subscript operator is private since it should not be used directly.
   * Mathematical operations should be done using the math operators provided.
   * Since the internal ordering of variables is implementation defined there
   * is no safe way to perform any operation that is not a linear combination of
   * Variables. Retrieving a Tensor must be done via the `get()` function.
   *
   *  \requires `i >= 0 and i < size()`
   */
  SPECTRE_ALWAYS_INLINE value_type& operator[](const size_type i) {
    return variable_data_[i];
  }
  SPECTRE_ALWAYS_INLINE const value_type& operator[](const size_type i) const {
    return variable_data_[i];
  }
  /// @}

  void add_reference_variable_data();

  friend bool operator==(const Variables& lhs, const Variables& rhs) {
    return blaze::equal<blaze::strict>(lhs.variable_data_, rhs.variable_data_);
  }

  template <typename VT, bool TF>
  friend bool operator==(const Variables& lhs,
                         const blaze::DenseVector<VT, TF>& rhs) {
    return blaze::equal<blaze::strict>(lhs.variable_data_, *rhs);
  }

  template <typename VT, bool TF>
  friend bool operator==(const blaze::DenseVector<VT, TF>& lhs,
                         const Variables& rhs) {
    return blaze::equal<blaze::strict>(*lhs, rhs.variable_data_);
  }

  template <class FriendTags>
  friend class Variables;

  std::array<value_type, number_of_independent_components>
      variable_data_impl_static_;
  std::unique_ptr<value_type[]> variable_data_impl_dynamic_{};
  bool owning_{true};
  size_t size_ = 0;
  size_t number_of_grid_points_ = 0;

  pointer_type variable_data_;
  tuples::TaggedTuple<Tags...> reference_variable_data_;
};

// The above Variables implementation doesn't work for an empty parameter pack,
// so specialize here.
template <>
class Variables<tmpl::list<>> {
 public:
  using tags_list = tmpl::list<>;
  static constexpr size_t number_of_independent_components = 0;
  Variables() = default;
  explicit Variables(const size_t /*number_of_grid_points*/) {}
  template <typename T>
  Variables(const T* /*pointer*/, const size_t /*size*/) {}
  static constexpr size_t size() { return 0; }
};

// gcc8 screams when the empty Variables has pup as a member function, so we
// declare pup as a free function here.
// clang-tidy: runtime-references
SPECTRE_ALWAYS_INLINE void pup(
    PUP::er& /*p*/,                           // NOLINT
    Variables<tmpl::list<>>& /* unused */) {  // NOLINT
}
SPECTRE_ALWAYS_INLINE void operator|(
    PUP::er& /*p*/, Variables<tmpl::list<>>& /* unused */) {  // NOLINT
}

SPECTRE_ALWAYS_INLINE bool operator==(const Variables<tmpl::list<>>& /*lhs*/,
                                      const Variables<tmpl::list<>>& /*rhs*/) {
  return true;
}
SPECTRE_ALWAYS_INLINE bool operator!=(const Variables<tmpl::list<>>& /*lhs*/,
                                      const Variables<tmpl::list<>>& /*rhs*/) {
  return false;
}

inline std::ostream& operator<<(std::ostream& os,
                                const Variables<tmpl::list<>>& /*d*/) {
  return os << "{}";
}

template <typename... Tags>
Variables<tmpl::list<Tags...>>::Variables() {
  // This makes an assertion trigger if one tries to assign to
  // components of a default-constructed Variables.
  const auto set_refs = [](auto& var) {
    for (auto& dv : var) {
      dv.set_data_ref(nullptr, 0);
    }
    return 0;
  };
  (void)set_refs;
  expand_pack(set_refs(tuples::get<Tags>(reference_variable_data_))...);
}

template <typename... Tags>
Variables<tmpl::list<Tags...>>::Variables(const size_t number_of_grid_points) {
  initialize(number_of_grid_points);
}

template <typename... Tags>
Variables<tmpl::list<Tags...>>::Variables(const size_t number_of_grid_points,
                                          const value_type value) {
  initialize(number_of_grid_points, value);
}

template <typename... Tags>
void Variables<tmpl::list<Tags...>>::initialize(
    const size_t number_of_grid_points) {
  if (number_of_grid_points_ == 0) {
    variable_data_impl_dynamic_.reset();
    size_ = 0;
    number_of_grid_points_ = 0;
  }
  if (number_of_grid_points_ == number_of_grid_points) {
    return;
  }
  if (UNLIKELY(not is_owning())) {
    ERROR("Variables::initialize cannot be called on a non-owning Variables.  "
          "This likely happened because of an attempted resize.  The current "
          "number of grid points is " << number_of_grid_points_ << " and the "
          "requested number is " << number_of_grid_points << ".");
  }
  number_of_grid_points_ = number_of_grid_points;
  size_ = number_of_grid_points * number_of_independent_components;
  if (size_ > 0) {
    if (number_of_grid_points_ == 1) {
      variable_data_impl_dynamic_.reset();
    } else {
      variable_data_impl_dynamic_ =
          cpp20::make_unique_for_overwrite<value_type[]>(size_);
    }
    add_reference_variable_data();
#if defined(SPECTRE_DEBUG) || defined(SPECTRE_NAN_INIT)
    std::fill(variable_data_.data(), variable_data_.data() + size_,
              make_signaling_NaN<value_type>());
#endif  // SPECTRE_DEBUG
  }
}

template <typename... Tags>
void Variables<tmpl::list<Tags...>>::initialize(
    const size_t number_of_grid_points, const value_type value) {
  initialize(number_of_grid_points);
  std::fill(variable_data_.data(), variable_data_.data() + size_, value);
}

/// \cond HIDDEN_SYMBOLS
template <typename... Tags>
Variables<tmpl::list<Tags...>>::Variables(
    const Variables<tmpl::list<Tags...>>& rhs) {
  initialize(rhs.number_of_grid_points());
  variable_data_ =
      static_cast<const blaze::Vector<pointer_type, transpose_flag>&>(
          rhs.variable_data_);
}

template <typename... Tags>
Variables<tmpl::list<Tags...>>& Variables<tmpl::list<Tags...>>::operator=(
    const Variables<tmpl::list<Tags...>>& rhs) {
  if (&rhs == this) {
    return *this;
  }
  initialize(rhs.number_of_grid_points());
  variable_data_ =
      static_cast<const blaze::Vector<pointer_type, transpose_flag>&>(
          rhs.variable_data_);
  return *this;
}

template <typename... Tags>
Variables<tmpl::list<Tags...>>::Variables(Variables<tmpl::list<Tags...>>&& rhs)
    : variable_data_impl_dynamic_(std::move(rhs.variable_data_impl_dynamic_)),
      owning_(rhs.owning_),
      size_(rhs.size()),
      number_of_grid_points_(rhs.number_of_grid_points()),
      variable_data_(std::move(rhs.variable_data_)) {
  if (number_of_grid_points_ == 1) {
#if defined(__GNUC__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif  // defined(__GNUC__) and not defined(__clang__)
    variable_data_impl_static_ = std::move(rhs.variable_data_impl_static_);
#if defined(__GNUC__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif  // defined(__GNUC__) and not defined(__clang__)
  }
  rhs.variable_data_impl_dynamic_.reset();
  rhs.owning_ = true;
  rhs.size_ = 0;
  rhs.number_of_grid_points_ = 0;
  add_reference_variable_data();
}

template <typename... Tags>
Variables<tmpl::list<Tags...>>& Variables<tmpl::list<Tags...>>::operator=(
    Variables<tmpl::list<Tags...>>&& rhs) {
  if (this == &rhs) {
    return *this;
  }
  owning_ = rhs.owning_;
  size_ = rhs.size_;
  number_of_grid_points_ = std::move(rhs.number_of_grid_points_);
  variable_data_ = std::move(rhs.variable_data_);
  variable_data_impl_dynamic_ = std::move(rhs.variable_data_impl_dynamic_);
  if (number_of_grid_points_ == 1) {
#if defined(__GNUC__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif  // defined(__GNUC__) and not defined(__clang__)
    variable_data_impl_static_ = std::move(rhs.variable_data_impl_static_);
#if defined(__GNUC__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif  // defined(__GNUC__) and not defined(__clang__)
  }

  rhs.variable_data_impl_dynamic_.reset();
  rhs.owning_ = true;
  rhs.size_ = 0;
  rhs.number_of_grid_points_ = 0;
  add_reference_variable_data();
  return *this;
}

template <typename... Tags>
template <typename... WrappedTags,
          Requires<tmpl2::flat_all<
              std::is_same<db::remove_all_prefixes<WrappedTags>,
                           db::remove_all_prefixes<Tags>>::value...>::value>>
Variables<tmpl::list<Tags...>>::Variables(
    const Variables<tmpl::list<WrappedTags...>>& rhs) {
  static_assert(
      (std::is_same_v<typename Tags::type, typename WrappedTags::type> and ...),
      "Tensor types do not match!");
  initialize(rhs.number_of_grid_points());
  variable_data_ =
      static_cast<const blaze::Vector<pointer_type, transpose_flag>&>(
          rhs.variable_data_);
}

template <typename... Tags>
template <typename... WrappedTags,
          Requires<tmpl2::flat_all<
              std::is_same<db::remove_all_prefixes<WrappedTags>,
                           db::remove_all_prefixes<Tags>>::value...>::value>>
Variables<tmpl::list<Tags...>>& Variables<tmpl::list<Tags...>>::operator=(
    const Variables<tmpl::list<WrappedTags...>>& rhs) {
  static_assert(
      (std::is_same_v<typename Tags::type, typename WrappedTags::type> and ...),
      "Tensor types do not match!");
  initialize(rhs.number_of_grid_points());
  variable_data_ =
      static_cast<const blaze::Vector<pointer_type, transpose_flag>&>(
          rhs.variable_data_);
  return *this;
}

template <typename... Tags>
template <typename... WrappedTags,
          Requires<tmpl2::flat_all<
              std::is_same<db::remove_all_prefixes<WrappedTags>,
                           db::remove_all_prefixes<Tags>>::value...>::value>>
Variables<tmpl::list<Tags...>>::Variables(
    Variables<tmpl::list<WrappedTags...>>&& rhs)
    : variable_data_impl_dynamic_(std::move(rhs.variable_data_impl_dynamic_)),
      owning_(rhs.owning_),
      size_(rhs.size()),
      number_of_grid_points_(rhs.number_of_grid_points()),
      variable_data_(std::move(rhs.variable_data_)) {
  static_assert(
      (std::is_same_v<typename Tags::type, typename WrappedTags::type> and ...),
      "Tensor types do not match!");
  if (number_of_grid_points_ == 1) {
#if defined(__GNUC__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif  // defined(__GNUC__) and not defined(__clang__)
    variable_data_impl_static_ = std::move(rhs.variable_data_impl_static_);
#if defined(__GNUC__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif  // defined(__GNUC__) and not defined(__clang__)
  }
  rhs.variable_data_impl_dynamic_.reset();
  rhs.size_ = 0;
  rhs.owning_ = true;
  rhs.number_of_grid_points_ = 0;
  add_reference_variable_data();
}

template <typename... Tags>
template <typename... WrappedTags,
          Requires<tmpl2::flat_all<
              std::is_same<db::remove_all_prefixes<WrappedTags>,
                           db::remove_all_prefixes<Tags>>::value...>::value>>
Variables<tmpl::list<Tags...>>& Variables<tmpl::list<Tags...>>::operator=(
    Variables<tmpl::list<WrappedTags...>>&& rhs) {
  static_assert(
      (std::is_same_v<typename Tags::type, typename WrappedTags::type> and ...),
      "Tensor types do not match!");
  variable_data_ = std::move(rhs.variable_data_);
  owning_ = rhs.owning_;
  size_ = rhs.size_;
  number_of_grid_points_ = std::move(rhs.number_of_grid_points_);
  variable_data_impl_dynamic_ = std::move(rhs.variable_data_impl_dynamic_);
  if (number_of_grid_points_ == 1) {
#if defined(__GNUC__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif  // defined(__GNUC__) and not defined(__clang__)
    variable_data_impl_static_ = std::move(rhs.variable_data_impl_static_);
#if defined(__GNUC__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif  // defined(__GNUC__) and not defined(__clang__)
  }

  rhs.variable_data_impl_dynamic_.reset();
  rhs.size_ = 0;
  rhs.owning_ = true;
  rhs.number_of_grid_points_ = 0;
  add_reference_variable_data();
  return *this;
}

template <typename... Tags>
Variables<tmpl::list<Tags...>>::Variables(const pointer start,
                                          const size_t size) {
  set_data_ref(start, size);
}

template <typename... Tags>
void Variables<tmpl::list<Tags...>>::pup(PUP::er& p) {
  ASSERT(
      owning_,
      "Cannot pup a non-owning Variables! It may be reasonable to pack a "
      "non-owning Variables, but not to unpack one. This should be discussed "
      "in an issue with the core devs if the feature seems necessary.");
  size_t number_of_grid_points = number_of_grid_points_;
  p | number_of_grid_points;
  if (p.isUnpacking()) {
    initialize(number_of_grid_points);
  }
  PUParray(p, variable_data_.data(), size_);
}
/// \endcond

/// @{
/*!
 * \ingroup DataStructuresGroup
 * \brief Return Tag::type pointing into the contiguous array
 *
 * \tparam Tag the variable to return
 */
template <typename Tag, typename TagList>
constexpr typename Tag::type& get(Variables<TagList>& v) {
  static_assert(tmpl::list_contains_v<TagList, Tag>,
                "Could not retrieve Tag from Variables. See the first "
                "template parameter of the instantiation for what Tag is "
                "being retrieved and the second template parameter for "
                "what Tags are available.");
  return tuples::get<Tag>(v.reference_variable_data_);
}
template <typename Tag, typename TagList>
constexpr const typename Tag::type& get(const Variables<TagList>& v) {
  static_assert(tmpl::list_contains_v<TagList, Tag>,
                "Could not retrieve Tag from Variables. See the first "
                "template parameter of the instantiation for what Tag is "
                "being retrieved and the second template parameter for "
                "what Tags are available.");
  return tuples::get<Tag>(v.reference_variable_data_);
}
/// @}

template <typename... Tags>
template <typename VT, bool VF>
Variables<tmpl::list<Tags...>>::Variables(
    const blaze::DenseVector<VT, VF>& expression) {
  ASSERT((*expression).size() % number_of_independent_components == 0,
         "Invalid size " << (*expression).size() << " for a Variables with "
         << number_of_independent_components << " components.");
  initialize((*expression).size() / number_of_independent_components);
  variable_data_ = expression;
}

/// \cond
template <typename... Tags>
template <typename VT, bool VF>
Variables<tmpl::list<Tags...>>& Variables<tmpl::list<Tags...>>::operator=(
    const blaze::DenseVector<VT, VF>& expression) {
  ASSERT((*expression).size() % number_of_independent_components == 0,
         "Invalid size " << (*expression).size() << " for a Variables with "
         << number_of_independent_components << " components.");
  initialize((*expression).size() / number_of_independent_components);
  variable_data_ = expression;
  return *this;
}
/// \endcond

/// \cond HIDDEN_SYMBOLS
template <typename... Tags>
void Variables<tmpl::list<Tags...>>::add_reference_variable_data() {
  if (size_ == 0) {
    return;
  }
  if (is_owning()) {
    if (number_of_grid_points_ == 1) {
      variable_data_.reset(variable_data_impl_static_.data(), size_);
    } else {
      variable_data_.reset(variable_data_impl_dynamic_.get(), size_);
    }
  }
  ASSERT(variable_data_.size() == size_ and
             size_ == number_of_grid_points_ * number_of_independent_components,
         "Size mismatch: variable_data_.size() = "
             << variable_data_.size() << " size_ = " << size_ << " should be: "
             << number_of_grid_points_ * number_of_independent_components
             << "\nThis is an internal inconsistency bug in Variables. Please "
                "file an issue.");
  size_t variable_offset = 0;
  tmpl::for_each<tags_list>([this, &variable_offset](auto tag_v) {
    using Tag = tmpl::type_from<decltype(tag_v)>;
    auto& var = tuples::get<Tag>(reference_variable_data_);
    for (size_t i = 0; i < Tag::type::size(); ++i) {
      var[i].set_data_ref(
          &variable_data_[variable_offset++ * number_of_grid_points_],
          number_of_grid_points_);
    }
  });
}
/// \endcond

template <typename... Tags>
Variables<tmpl::list<Tags...>>& operator*=(
    Variables<tmpl::list<Tags...>>& lhs,
    const typename Variables<tmpl::list<Tags...>>::vector_type& rhs) {
  using value_type = typename Variables<tmpl::list<Tags...>>::value_type;
  ASSERT(lhs.number_of_grid_points() == rhs.size(),
         "Size mismatch in multiplication: " << lhs.number_of_grid_points()
                                             << " and " << rhs.size());
  value_type* const lhs_data = lhs.data();
  const value_type* const rhs_data = rhs.data();
  for (size_t c = 0; c < lhs.number_of_independent_components; ++c) {
    for (size_t s = 0; s < lhs.number_of_grid_points(); ++s) {
      // clang-tidy: do not use pointer arithmetic
      lhs_data[c * lhs.number_of_grid_points() + s] *= rhs_data[s];  // NOLINT
    }
  }
  return lhs;
}

template <typename... Tags>
Variables<tmpl::list<Tags...>> operator*(
    const Variables<tmpl::list<Tags...>>& lhs,
    const typename Variables<tmpl::list<Tags...>>::vector_type& rhs) {
  auto result = lhs;
  result *= rhs;
  return result;
}

template <typename... Tags>
Variables<tmpl::list<Tags...>> operator*(
    const typename Variables<tmpl::list<Tags...>>::vector_type& lhs,
    const Variables<tmpl::list<Tags...>>& rhs) {
  auto result = rhs;
  result *= lhs;
  return result;
}

template <typename... Tags>
Variables<tmpl::list<Tags...>>& operator/=(
    Variables<tmpl::list<Tags...>>& lhs,
    const typename Variables<tmpl::list<Tags...>>::vector_type& rhs) {
  ASSERT(lhs.number_of_grid_points() == rhs.size(),
         "Size mismatch in multiplication: " << lhs.number_of_grid_points()
                                             << " and " << rhs.size());
  using value_type = typename Variables<tmpl::list<Tags...>>::value_type;
  value_type* const lhs_data = lhs.data();
  const value_type* const rhs_data = rhs.data();
  for (size_t c = 0; c < lhs.number_of_independent_components; ++c) {
    for (size_t s = 0; s < lhs.number_of_grid_points(); ++s) {
      // clang-tidy: do not use pointer arithmetic
      lhs_data[c * lhs.number_of_grid_points() + s] /= rhs_data[s];  // NOLINT
    }
  }
  return lhs;
}

template <typename... Tags>
Variables<tmpl::list<Tags...>> operator/(
    const Variables<tmpl::list<Tags...>>& lhs,
    const typename Variables<tmpl::list<Tags...>>::vector_type& rhs) {
  auto result = lhs;
  result /= rhs;
  return result;
}

template <typename... Tags, Requires<sizeof...(Tags) != 0> = nullptr>
std::ostream& operator<<(std::ostream& os,
                         const Variables<tmpl::list<Tags...>>& d) {
  size_t count = 0;
  const auto helper = [&os, &d, &count](auto tag_v) {
    count++;
    using Tag = typename decltype(tag_v)::type;
    os << db::tag_name<Tag>() << ":\n";
    os << get<Tag>(d);
    if (count < sizeof...(Tags)) {
      os << "\n\n";
    }
  };
  EXPAND_PACK_LEFT_TO_RIGHT(helper(tmpl::type_<Tags>{}));
  return os;
}

template <typename TagsList>
bool operator!=(const Variables<TagsList>& lhs,
                const Variables<TagsList>& rhs) {
  return not(lhs == rhs);
}

template <typename... TagsLhs, typename... TagsRhs,
          Requires<not std::is_same<tmpl::list<TagsLhs...>,
                                    tmpl::list<TagsRhs...>>::value> = nullptr>
void swap(Variables<tmpl::list<TagsLhs...>>& lhs,
          Variables<tmpl::list<TagsRhs...>>& rhs) {
  Variables<tmpl::list<TagsLhs...>> temp{std::move(lhs)};
  lhs = std::move(rhs);
  rhs = std::move(temp);
}

/// \ingroup DataStructuresGroup
/// Construct a variables from the `Tensor`s in a `TaggedTuple`.
template <typename... Tags>
Variables<tmpl::list<Tags...>> variables_from_tagged_tuple(
    const tuples::TaggedTuple<Tags...>& tuple) {
  auto result = make_with_value<Variables<tmpl::list<Tags...>>>(
      get<tmpl::front<tmpl::list<Tags...>>>(tuple), 0.0);
  result.assign_subset(tuple);
  return result;
}

namespace MakeWithValueImpls {
template <typename TagList>
struct MakeWithSize<Variables<TagList>> {
  static SPECTRE_ALWAYS_INLINE Variables<TagList> apply(
      const size_t size, const typename Variables<TagList>::value_type value) {
    return Variables<TagList>(size, value);
  }
};

template <typename TagList>
struct NumberOfPoints<Variables<TagList>> {
  static SPECTRE_ALWAYS_INLINE size_t apply(const Variables<TagList>& input) {
    return input.number_of_grid_points();
  }
};
}  // namespace MakeWithValueImpls

template <typename TagList>
struct SetNumberOfGridPointsImpls::SetNumberOfGridPointsImpl<
    Variables<TagList>> {
  static constexpr bool is_trivial = false;
  static SPECTRE_ALWAYS_INLINE void apply(
      const gsl::not_null<Variables<TagList>*> result, const size_t size) {
    result->initialize(size);
  }
};

namespace EqualWithinRoundoffImpls {
// It would be nice to use `blaze::equal` in these implementations, but it
// doesn't currently allow to specify a tolerance. See upstream issue:
// https://bitbucket.org/blaze-lib/blaze/issues/417/adjust-relaxed-equal-accuracy
template <typename TagList, typename Floating>
struct EqualWithinRoundoffImpl<Variables<TagList>, Floating,
                               Requires<std::is_floating_point_v<Floating>>> {
  static bool apply(const Variables<TagList>& lhs, const Floating& rhs,
                    const double eps, const double scale) {
    for (size_t i = 0; i < lhs.size(); ++i) {
      if (not equal_within_roundoff(lhs.data()[i], rhs, eps, scale)) {
        return false;
      }
    }
    return true;
  }
};
template <typename TagList, typename Floating>
struct EqualWithinRoundoffImpl<Floating, Variables<TagList>,
                               Requires<std::is_floating_point_v<Floating>>> {
  static SPECTRE_ALWAYS_INLINE bool apply(const Floating& lhs,
                                          const Variables<TagList>& rhs,
                                          const double eps,
                                          const double scale) {
    return equal_within_roundoff(rhs, lhs, eps, scale);
  }
};
template <typename LhsTagList, typename RhsTagList>
struct EqualWithinRoundoffImpl<Variables<LhsTagList>, Variables<RhsTagList>> {
  static bool apply(const Variables<LhsTagList>& lhs,
                    const Variables<RhsTagList>& rhs, const double eps,
                    const double scale) {
    ASSERT(lhs.size() == rhs.size(),
           "Can only compare two Variables of the same size, but lhs has size "
               << lhs.size() << " and rhs has size " << rhs.size() << ".");
    for (size_t i = 0; i < lhs.size(); ++i) {
      if (not equal_within_roundoff(lhs.data()[i], rhs.data()[i], eps, scale)) {
        return false;
      }
    }
    return true;
  }
};
}  // namespace EqualWithinRoundoffImpls

namespace db {
// Enable subitems for ::Tags::Variables and derived tags (e.g. compute tags).
// Other tags that hold a `Variables` don't expose the constituent tensors as
// subitems by default.
namespace Variables_detail {
// Check if the argument is a `::Tags::Variables`, or derived from it. Can't use
// `tt:is_a_v` because we also want to match derived classes. Can't use
// `std::is_base_of` because `::Tags::Variables` is a template.
template <typename TagsList>
constexpr std::true_type is_a_variables_tag(::Tags::Variables<TagsList>&&) {
  return {};
}
constexpr std::false_type is_a_variables_tag(...) { return {}; }
template <typename Tag>
static constexpr bool is_a_variables_tag_v =
    decltype(is_a_variables_tag(std::declval<Tag>()))::value;
}  // namespace Variables_detail
template <typename Tag>
struct Subitems<Tag, Requires<Variables_detail::is_a_variables_tag_v<Tag>>> {
  using type = typename Tag::type::tags_list;

  template <typename Subtag, typename LocalTag = Tag>
  static void create_item(
      const gsl::not_null<typename LocalTag::type*> parent_value,
      const gsl::not_null<typename Subtag::type*> sub_value) {
    auto& vars = get<Subtag>(*parent_value);
    // Only update the Tensor if the Variables has changed its allocation
    if constexpr (not is_any_spin_weighted_v<typename Subtag::type::type>) {
      if (vars.begin()->data() != sub_value->begin()->data()) {
        for (auto vars_it = vars.begin(), sub_var_it = sub_value->begin();
             vars_it != vars.end(); ++vars_it, ++sub_var_it) {
          sub_var_it->set_data_ref(make_not_null(&*vars_it));
        }
      }
    } else {
      if (vars.begin()->data().data() != sub_value->begin()->data().data()) {
        for (auto vars_it = vars.begin(), sub_var_it = sub_value->begin();
             vars_it != vars.end(); ++vars_it, ++sub_var_it) {
          sub_var_it->set_data_ref(make_not_null(&*vars_it));
        }
      }
    }
  }

  template <typename Subtag>
  static const typename Subtag::type& create_compute_item(
      const typename Tag::type& parent_value) {
    return get<Subtag>(parent_value);
  }
};
}  // namespace db

namespace Variables_detail {
template <typename Tags>
using MathWrapperVectorType = tmpl::conditional_t<
    std::is_same_v<typename Variables<Tags>::value_type, double>, DataVector,
    ComplexDataVector>;
}  // namespace Variables_detail

template <typename Tags>
auto make_math_wrapper(const gsl::not_null<Variables<Tags>*> data) {
  using Vector = Variables_detail::MathWrapperVectorType<Tags>;
  Vector referencing(data->data(), data->size());
  return make_math_wrapper(&referencing);
}

template <typename Tags>
auto make_math_wrapper(const Variables<Tags>& data) {
  using Vector = Variables_detail::MathWrapperVectorType<Tags>;
  const Vector referencing(
      const_cast<typename Vector::value_type*>(data.data()), data.size());
  return make_math_wrapper(referencing);
}

/// \cond
template <size_t I, class... Tags>
inline constexpr typename tmpl::at_c<tmpl::list<Tags...>, I>::type&& get(
    Variables<tmpl::list<Tags...>>&& t) {
  return get<tmpl::at_c<tmpl::list<Tags...>, I>>(t);
}

template <size_t I, class... Tags>
inline constexpr const typename tmpl::at_c<tmpl::list<Tags...>, I>::type& get(
    const Variables<tmpl::list<Tags...>>& t) {
  return get<tmpl::at_c<tmpl::list<Tags...>, I>>(t);
}

template <size_t I, class... Tags>
inline constexpr typename tmpl::at_c<tmpl::list<Tags...>, I>::type& get(
    Variables<tmpl::list<Tags...>>& t) {
  return get<tmpl::at_c<tmpl::list<Tags...>, I>>(t);
}
/// \endcond

namespace std {
template <typename... Tags>
struct tuple_size<Variables<tmpl::list<Tags...>>>
    : std::integral_constant<int, sizeof...(Tags)> {};
template <size_t I, typename... Tags>
struct tuple_element<I, Variables<tmpl::list<Tags...>>> {
  using type = typename tmpl::at_c<tmpl::list<Tags...>, I>::type;
};
}  // namespace std

template <typename TagsList>
bool contains_allocations(const Variables<TagsList>& value) {
  return value.number_of_grid_points() > 1;
}

inline bool contains_allocations(const Variables<tmpl::list<>>& /*value*/) {
  return false;
}
