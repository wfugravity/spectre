// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include <array>
#include <climits>
#include <cstddef>
#include <random>

#include "DataStructures/ComplexDataVector.hpp"
#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/IndexType.hpp"
#include "DataStructures/Tensor/Symmetry.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "Framework/TestHelpers.hpp"
#include "Helpers/DataStructures/MakeWithRandomValues.hpp"
#include "Helpers/DataStructures/Tensor/Expressions/ComponentPlaceholder.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/MakeWithValue.hpp"

namespace {

// Checks that the number of ops in the expressions match what is expected
void test_tensor_ops_properties() {
  const tnsr::Ij<double, 3> R{};
  const Tensor<double, Symmetry<4, 3, 2, 1>,
               index_list<SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                          SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                          SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                          SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>>>
      S{};

  // Expected: (TotalDim - 1) adds (3 - 1) adds = 2 total ops
  const auto R_contracted = R(ti::I, ti::i);
  // Expected: (TotalDim - 1) adds = (4 - 1) adds = 3 total ops
  const auto S_contract_one_pair = S(ti::A, ti::B, ti::c, ti::a);
  // Expected:
  //   (SpatialDim * TotalDim - 1) adds = (3 * 4 - 1) adds = 11 total ops
  const auto S_contract_both_pairs = S(ti::K, ti::A, ti::a, ti::k);

  CHECK(R_contracted.num_ops_subtree == 2);
  CHECK(S_contract_one_pair.num_ops_subtree == 3);
  CHECK(S_contract_both_pairs.num_ops_subtree == 11);
}

// Contractions are performed by summing over multi-indices in an order that is
// implementation defined. What is considered the "next lowest" and
// "next highest" multi-indices should be opposites of each other. This test
// checks this, as well as checking that the "lowest" and "highest"
// multi-indices being summed are correctly determined.
void test_contraction_summation_consistency() {
  const tnsr::II<double, 3, Frame::Inertial> R{};
  const tnsr::iab<double, 3, Frame::Inertial> S{};

  // L is a `TensorContract`, not a `Tensor`
  const auto L = R(ti::J, ti::I) * S(ti::i, ti::a, ti::j);
  // multi-index for L_2
  const std::array<size_t, 1> L_multi_index = {2};

  const std::array<size_t, 5> lowest_multi_index =
      L.get_lowest_multi_index_to_sum(L_multi_index);
  const std::array<size_t, 5> expected_lowest_multi_index = {0, 0, 0, 2, 1};
  CHECK(lowest_multi_index == expected_lowest_multi_index);

  const std::array<size_t, 5> highest_multi_index =
      L.get_highest_multi_index_to_sum(L_multi_index);
  const std::array<size_t, 5> expected_highest_multi_index = {2, 2, 2, 2, 3};
  CHECK(highest_multi_index == expected_highest_multi_index);

  std::array<size_t, 5> current_multi_index = expected_lowest_multi_index;
  while (current_multi_index != expected_highest_multi_index) {
    const auto next_lowest_multi_index =
        L.get_next_lowest_multi_index_to_sum(current_multi_index);
    CHECK(L.get_next_highest_multi_index_to_sum(next_lowest_multi_index) ==
          current_multi_index);
    current_multi_index = next_lowest_multi_index;
  }
}

template <typename Generator, typename DataType>
void test_contractions_rank2(const gsl::not_null<Generator*> generator,
                             const DataType& used_for_size) {
  std::uniform_real_distribution<> distribution(-1.0, 1.0);

  // Contract (upper, lower) tensor
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  const auto Rul =
      make_with_random_values<tnsr::Ij<DataType, 3, Frame::Inertial>>(
          generator, distribution, used_for_size);

  const Tensor<DataType> RIi_contracted = tenex::evaluate(Rul(ti::I, ti::i));

  DataType expected_RIi_sum = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t i = 0; i < 3; i++) {
    expected_RIi_sum += Rul.get(i, i);
  }
  CHECK_ITERABLE_APPROX(RIi_contracted.get(), expected_RIi_sum);

  // Contract (lower, upper) tensor
  const auto Rlu = make_with_random_values<tnsr::aB<DataType, 3, Frame::Grid>>(
      generator, distribution, used_for_size);

  const Tensor<DataType> RgG_contracted = tenex::evaluate(Rlu(ti::g, ti::G));

  DataType expected_RgG_sum = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t g = 0; g < 4; g++) {
    expected_RgG_sum += Rlu.get(g, g);
  }
  CHECK_ITERABLE_APPROX(RgG_contracted.get(), expected_RgG_sum);
}

template <typename Generator, typename DataType>
void test_contractions_rank3(const gsl::not_null<Generator*> generator,
                             const DataType& used_for_size) {
  std::uniform_real_distribution<> distribution(-1.0, 1.0);

  // Contract first and second indices of (lower, upper, lower) tensor
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  const auto Rlul = make_with_random_values<
      Tensor<DataType, Symmetry<3, 2, 1>,
             index_list<SpatialIndex<2, UpLo::Lo, Frame::Grid>,
                        SpatialIndex<2, UpLo::Up, Frame::Grid>,
                        SpatialIndex<3, UpLo::Lo, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<1>,
               index_list<SpatialIndex<3, UpLo::Lo, Frame::Grid>>>
      RiIj_contracted = tenex::evaluate<ti::j>(Rlul(ti::i, ti::I, ti::j));

  for (size_t j = 0; j < 3; j++) {
    DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
    for (size_t i = 0; i < 2; i++) {
      expected_sum += Rlul.get(i, i, j);
    }
    CHECK_ITERABLE_APPROX(RiIj_contracted.get(j), expected_sum);
  }

  // Contract first and third indices of (upper, upper, lower) tensor
  const auto Ruul =
      make_with_random_values<tnsr::IJk<DataType, 3, Frame::Grid>>(
          generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<1>,
               index_list<SpatialIndex<3, UpLo::Up, Frame::Grid>>>
      RJLj_contracted = tenex::evaluate<ti::L>(Ruul(ti::J, ti::L, ti::j));

  for (size_t l = 0; l < 3; l++) {
    DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
    for (size_t j = 0; j < 3; j++) {
      expected_sum += Ruul.get(j, l, j);
    }
    CHECK_ITERABLE_APPROX(RJLj_contracted.get(l), expected_sum);
  }

  // Contract second and third indices of (upper, lower, upper) tensor
  const auto Rulu = make_with_random_values<
      Tensor<DataType, Symmetry<2, 1, 2>,
             index_list<SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Inertial>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<1>,
               index_list<SpacetimeIndex<3, UpLo::Up, Frame::Inertial>>>
      RBfF_contracted = tenex::evaluate<ti::B>(Rulu(ti::B, ti::f, ti::F));

  for (size_t b = 0; b < 4; b++) {
    DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
    for (size_t f = 0; f < 4; f++) {
      expected_sum += Rulu.get(b, f, f);
    }
    CHECK_ITERABLE_APPROX(RBfF_contracted.get(b), expected_sum);
  }

  // Contract first and third indices of (lower, lower, upper) tensor with mixed
  // TensorIndexTypes
  const auto Rllu = make_with_random_values<
      Tensor<DataType, Symmetry<3, 2, 1>,
             index_list<SpatialIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                        SpatialIndex<3, UpLo::Up, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<1>,
               index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Grid>>>
      RiaI_contracted = tenex::evaluate<ti::a>(Rllu(ti::i, ti::a, ti::I));

  for (size_t a = 0; a < 4; a++) {
    DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
    for (size_t i = 0; i < 3; i++) {
      expected_sum += Rllu.get(i, a, i);
    }
    CHECK_ITERABLE_APPROX(RiaI_contracted.get(a), expected_sum);
  }
}

template <typename Generator, typename DataType>
void test_contractions_rank4(const gsl::not_null<Generator*> generator,
                             const DataType& used_for_size) {
  std::uniform_real_distribution<> distribution(-1.0, 1.0);

  // Contract first and second indices of (lower, upper, upper, lower) tensor to
  // rank 2 tensor
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  const auto Rluul = make_with_random_values<
      Tensor<DataType, Symmetry<4, 3, 2, 1>,
             index_list<SpatialIndex<2, UpLo::Lo, Frame::Inertial>,
                        SpatialIndex<2, UpLo::Up, Frame::Inertial>,
                        SpatialIndex<3, UpLo::Up, Frame::Inertial>,
                        SpatialIndex<2, UpLo::Lo, Frame::Inertial>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpatialIndex<3, UpLo::Up, Frame::Inertial>,
                          SpatialIndex<2, UpLo::Lo, Frame::Inertial>>>
      RiIKj_contracted =
          tenex::evaluate<ti::K, ti::j>(Rluul(ti::i, ti::I, ti::K, ti::j));

  for (size_t k = 0; k < 3; k++) {
    for (size_t j = 0; j < 2; j++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t i = 0; i < 2; i++) {
        expected_sum += Rluul.get(i, i, k, j);
      }
      CHECK_ITERABLE_APPROX(RiIKj_contracted.get(k, j), expected_sum);
    }
  }

  // Contract first and third indices of (upper, upper, lower, lower) tensor to
  // rank 2 tensor
  const auto Ruull = make_with_random_values<
      Tensor<DataType, Symmetry<4, 3, 2, 1>,
             index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<2, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpacetimeIndex<2, UpLo::Up, Frame::Grid>,
                          SpacetimeIndex<3, UpLo::Lo, Frame::Grid>>>
      RABac_contracted =
          tenex::evaluate<ti::B, ti::c>(Ruull(ti::A, ti::B, ti::a, ti::c));

  for (size_t b = 0; b < 3; b++) {
    for (size_t c = 0; c < 4; c++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t a = 0; a < 4; a++) {
        expected_sum += Ruull.get(a, b, a, c);
      }
      CHECK_ITERABLE_APPROX(RABac_contracted.get(b, c), expected_sum);
    }
  }

  // Contract first and fourth indices of (upper, upper, upper, lower) tensor to
  // rank 2 tensor
  const auto Ruuul = make_with_random_values<
      Tensor<DataType, Symmetry<3, 2, 3, 1>,
             index_list<SpatialIndex<2, UpLo::Up, Frame::Grid>,
                        SpatialIndex<3, UpLo::Up, Frame::Grid>,
                        SpatialIndex<2, UpLo::Up, Frame::Grid>,
                        SpatialIndex<2, UpLo::Lo, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpatialIndex<3, UpLo::Up, Frame::Grid>,
                          SpatialIndex<2, UpLo::Up, Frame::Grid>>>
      RLJIl_contracted =
          tenex::evaluate<ti::J, ti::I>(Ruuul(ti::L, ti::J, ti::I, ti::l));

  for (size_t j = 0; j < 3; j++) {
    for (size_t i = 0; i < 2; i++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t l = 0; l < 2; l++) {
        expected_sum += Ruuul.get(l, j, i, l);
      }
      CHECK_ITERABLE_APPROX(RLJIl_contracted.get(j, i), expected_sum);
    }
  }

  // Contract second and third indices of (upper, upper, lower, upper) tensor to
  // rank 2 tensor
  const auto Ruulu = make_with_random_values<
      Tensor<DataType, Symmetry<2, 2, 1, 2>,
             index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<1, 1>,
               index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                          SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>
      REDdA_contracted =
          tenex::evaluate<ti::E, ti::A>(Ruulu(ti::E, ti::D, ti::d, ti::A));

  for (size_t e = 0; e < 4; e++) {
    for (size_t a = 0; a < 4; a++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t d = 0; d < 4; d++) {
        expected_sum += Ruulu.get(e, d, d, a);
      }
      CHECK_ITERABLE_APPROX(REDdA_contracted.get(e, a), expected_sum);
    }
  }

  // Contract second and fourth indices of (lower, upper, lower, lower) tensor
  // to rank 2 tensor
  const auto Rlull = make_with_random_values<
      Tensor<DataType, Symmetry<4, 3, 2, 1>,
             index_list<SpatialIndex<2, UpLo::Lo, Frame::Inertial>,
                        SpatialIndex<2, UpLo::Up, Frame::Inertial>,
                        SpatialIndex<3, UpLo::Lo, Frame::Inertial>,
                        SpatialIndex<2, UpLo::Lo, Frame::Inertial>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpatialIndex<2, UpLo::Lo, Frame::Inertial>,
                          SpatialIndex<3, UpLo::Lo, Frame::Inertial>>>
      RkJij_contracted =
          tenex::evaluate<ti::k, ti::i>(Rlull(ti::k, ti::J, ti::i, ti::j));

  for (size_t k = 0; k < 2; k++) {
    for (size_t i = 0; i < 3; i++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t j = 0; j < 2; j++) {
        expected_sum += Rlull.get(k, j, i, j);
      }
      CHECK_ITERABLE_APPROX(RkJij_contracted.get(k, i), expected_sum);
    }
  }

  // Contract third and fourth indices of (upper, lower, lower, upper) tensor to
  // rank 2 tensor
  const auto Rullu = make_with_random_values<
      Tensor<DataType, Symmetry<3, 2, 2, 1>,
             index_list<SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                        SpacetimeIndex<2, UpLo::Lo, Frame::Inertial>,
                        SpacetimeIndex<2, UpLo::Lo, Frame::Inertial>,
                        SpacetimeIndex<2, UpLo::Up, Frame::Inertial>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                          SpacetimeIndex<2, UpLo::Lo, Frame::Inertial>>>
      RFcgG_contracted =
          tenex::evaluate<ti::F, ti::c>(Rullu(ti::F, ti::c, ti::g, ti::G));

  for (size_t f = 0; f < 4; f++) {
    for (size_t c = 0; c < 3; c++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t g = 0; g < 3; g++) {
        expected_sum += Rullu.get(f, c, g, g);
      }
      CHECK_ITERABLE_APPROX(RFcgG_contracted.get(f, c), expected_sum);
    }
  }

  // Contract first and second indices of (upper, lower, upper, upper) tensor to
  // rank 2 tensor and reorder indices
  const auto Ruluu = make_with_random_values<
      Tensor<DataType, Symmetry<3, 2, 3, 1>,
             index_list<SpatialIndex<3, UpLo::Up, Frame::Grid>,
                        SpatialIndex<3, UpLo::Lo, Frame::Grid>,
                        SpatialIndex<3, UpLo::Up, Frame::Grid>,
                        SpatialIndex<2, UpLo::Up, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpatialIndex<2, UpLo::Up, Frame::Grid>,
                          SpatialIndex<3, UpLo::Up, Frame::Grid>>>
      RKkIJ_contracted_to_JI =
          tenex::evaluate<ti::J, ti::I>(Ruluu(ti::K, ti::k, ti::I, ti::J));

  for (size_t j = 0; j < 2; j++) {
    for (size_t i = 0; i < 3; i++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t k = 0; k < 3; k++) {
        expected_sum += Ruluu.get(k, k, i, j);
      }
      CHECK_ITERABLE_APPROX(RKkIJ_contracted_to_JI.get(j, i), expected_sum);
    }
  }

  // Contract first and third indices of (lower, upper, upper, upper) tensor to
  // rank 2 tensor and reorder indices
  const auto Rluuu = make_with_random_values<
      Tensor<DataType, Symmetry<3, 2, 1, 2>,
             index_list<SpacetimeIndex<2, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<2, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<2, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<2, UpLo::Up, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<1, 1>,
               index_list<SpacetimeIndex<2, UpLo::Up, Frame::Grid>,
                          SpacetimeIndex<2, UpLo::Up, Frame::Grid>>>
      RbCBE_contracted_to_EC =
          tenex::evaluate<ti::E, ti::C>(Rluuu(ti::b, ti::C, ti::B, ti::E));

  for (size_t e = 0; e < 3; e++) {
    for (size_t c = 0; c < 3; c++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t b = 0; b < 3; b++) {
        expected_sum += Rluuu.get(b, c, b, e);
      }
      CHECK_ITERABLE_APPROX(RbCBE_contracted_to_EC.get(e, c), expected_sum);
    }
  }

  // Contract first and fourth indices of (upper, lower, lower, lower) tensor to
  // rank 2 tensor and reorder indices
  const auto Rulll = make_with_random_values<
      Tensor<DataType, Symmetry<2, 1, 1, 1>,
             index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<1, 1>,
               index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                          SpacetimeIndex<3, UpLo::Lo, Frame::Grid>>>
      RAdba_contracted_to_bd =
          tenex::evaluate<ti::b, ti::d>(Rulll(ti::A, ti::d, ti::b, ti::a));

  for (size_t b = 0; b < 4; b++) {
    for (size_t d = 0; d < 4; d++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t a = 0; a < 4; a++) {
        expected_sum += Rulll.get(a, d, b, a);
      }
      CHECK_ITERABLE_APPROX(RAdba_contracted_to_bd.get(b, d), expected_sum);
    }
  }

  // Contract second and third indices of (lower, lower, upper, lower) tensor to
  // rank 2 tensor and reorder indices
  const auto Rllul = make_with_random_values<
      Tensor<DataType, Symmetry<4, 3, 2, 1>,
             index_list<SpatialIndex<2, UpLo::Lo, Frame::Grid>,
                        SpatialIndex<2, UpLo::Lo, Frame::Grid>,
                        SpatialIndex<2, UpLo::Up, Frame::Grid>,
                        SpatialIndex<3, UpLo::Lo, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpatialIndex<3, UpLo::Lo, Frame::Grid>,
                          SpatialIndex<2, UpLo::Lo, Frame::Grid>>>
      RljJi_contracted_to_il =
          tenex::evaluate<ti::i, ti::l>(Rllul(ti::l, ti::j, ti::J, ti::i));

  for (size_t i = 0; i < 3; i++) {
    for (size_t l = 0; l < 2; l++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t j = 0; j < 2; j++) {
        expected_sum += Rllul.get(l, j, j, i);
      }
      CHECK_ITERABLE_APPROX(RljJi_contracted_to_il.get(i, l), expected_sum);
    }
  }

  // Contract second and fourth indices of (lower, lower, upper, upper) tensor
  // to rank 2 tensor and reorder indices
  const auto Rlluu = make_with_random_values<
      Tensor<DataType, Symmetry<2, 2, 1, 1>,
             index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Inertial>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                          SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>>>
      RagDG_contracted_to_Da =
          tenex::evaluate<ti::D, ti::a>(Rlluu(ti::a, ti::g, ti::D, ti::G));

  for (size_t d = 0; d < 4; d++) {
    for (size_t a = 0; a < 4; a++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t g = 0; g < 4; g++) {
        expected_sum += Rlluu.get(a, g, d, g);
      }
      CHECK_ITERABLE_APPROX(RagDG_contracted_to_Da.get(d, a), expected_sum);
    }
  }

  // Contract third and fourth indices of (lower, upper, lower, upper) tensor to
  // rank 2 tensor and reorder indices
  const auto Rlulu = make_with_random_values<
      Tensor<DataType, Symmetry<2, 1, 2, 1>,
             index_list<SpatialIndex<3, UpLo::Lo, Frame::Inertial>,
                        SpatialIndex<3, UpLo::Up, Frame::Inertial>,
                        SpatialIndex<3, UpLo::Lo, Frame::Inertial>,
                        SpatialIndex<3, UpLo::Up, Frame::Inertial>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpatialIndex<3, UpLo::Up, Frame::Inertial>,
                          SpatialIndex<3, UpLo::Lo, Frame::Inertial>>>
      RlJiI_contracted_to_Jl =
          tenex::evaluate<ti::J, ti::l>(Rlulu(ti::l, ti::J, ti::i, ti::I));

  for (size_t j = 0; j < 3; j++) {
    for (size_t l = 0; l < 3; l++) {
      DataType expected_sum = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t i = 0; i < 3; i++) {
        expected_sum += Rlulu.get(l, j, i, i);
      }
      CHECK_ITERABLE_APPROX(RlJiI_contracted_to_Jl.get(j, l), expected_sum);
    }
  }

  // Contract first and second indices AND third and fourth indices to rank 0
  // tensor
  const auto Rulul = make_with_random_values<
      Tensor<DataType, Symmetry<4, 3, 2, 1>,
             index_list<SpatialIndex<2, UpLo::Up, Frame::Grid>,
                        SpatialIndex<2, UpLo::Lo, Frame::Grid>,
                        SpatialIndex<3, UpLo::Up, Frame::Grid>,
                        SpatialIndex<3, UpLo::Lo, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  const Tensor<DataType> RKkLl_contracted =
      tenex::evaluate(Rulul(ti::K, ti::k, ti::L, ti::l));

  DataType expected_RKkLl_sum = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t k = 0; k < 2; k++) {
    for (size_t l = 0; l < 3; l++) {
      expected_RKkLl_sum += Rulul.get(k, k, l, l);
    }
  }
  CHECK_ITERABLE_APPROX(RKkLl_contracted.get(), expected_RKkLl_sum);

  // Contract first and third indices and second and fourth indices to rank 0
  // tensor
  const Tensor<DataType> RcaCA_contracted =
      tenex::evaluate(Rlluu(ti::c, ti::a, ti::C, ti::A));

  DataType expected_RcaCA_sum = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t c = 0; c < 4; c++) {
    for (size_t a = 0; a < 4; a++) {
      expected_RcaCA_sum += Rlluu.get(c, a, c, a);
    }
  }
  CHECK_ITERABLE_APPROX(RcaCA_contracted.get(), expected_RcaCA_sum);

  // Contract first and fourth indices and second and third indices to rank 0
  // tensor
  const Tensor<DataType> RjIiJ_contracted =
      tenex::evaluate(Rlulu(ti::j, ti::I, ti::i, ti::J));

  DataType expected_RjIiJ_sum = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t j = 0; j < 3; j++) {
    for (size_t i = 0; i < 3; i++) {
      expected_RjIiJ_sum += Rlulu.get(j, i, i, j);
    }
  }
  CHECK_ITERABLE_APPROX(RjIiJ_contracted.get(), expected_RjIiJ_sum);
}

template <typename Generator, typename DataType>
void test_spatial_spacetime_index(const gsl::not_null<Generator*> generator,
                                  const DataType& used_for_size) {
  std::uniform_real_distribution<> distribution(-1.0, 1.0);

  // Contract (spatial, spacetime) tensor
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  const auto R = make_with_random_values<
      Tensor<DataType, Symmetry<2, 1>,
             index_list<SpatialIndex<3, UpLo::Up, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>>>>(
      generator, distribution, used_for_size);
  const Tensor<DataType> R_contracted = tenex::evaluate(R(ti::I, ti::i));

  // Contract (spacetime, spatial) tensor
  const auto S = make_with_random_values<
      Tensor<DataType, Symmetry<2, 1>,
             index_list<SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                        SpatialIndex<3, UpLo::Lo, Frame::Inertial>>>>(
      generator, distribution, used_for_size);
  const Tensor<DataType> S_contracted = tenex::evaluate(S(ti::K, ti::k));

  // Contract (spacetime, spacetime) tensor using generic spatial indices
  const auto T = make_with_random_values<tnsr::aB<DataType, 3, Frame::Grid>>(
      generator, distribution, used_for_size);
  const Tensor<DataType> T_contracted = tenex::evaluate(T(ti::j, ti::J));

  DataType expected_R_sum = make_with_value<DataType>(used_for_size, 0.0);
  DataType expected_S_sum = make_with_value<DataType>(used_for_size, 0.0);
  DataType expected_T_sum = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t i = 0; i < 3; i++) {
    expected_R_sum += R.get(i, i + 1);
    expected_S_sum += S.get(i + 1, i);
    expected_T_sum += T.get(i + 1, i + 1);
  }
  CHECK_ITERABLE_APPROX(R_contracted.get(), expected_R_sum);
  CHECK_ITERABLE_APPROX(S_contracted.get(), expected_S_sum);
  CHECK_ITERABLE_APPROX(T_contracted.get(), expected_T_sum);

  const auto G = make_with_random_values<
      Tensor<DataType, Symmetry<4, 3, 2, 1>,
             index_list<SpatialIndex<3, UpLo::Up, Frame::Grid>,
                        SpatialIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  // Contract one (spatial, spacetime) pair of indices of a tensor that also
  // takes a generic spatial index for a single non-contracted spacetime index
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  const Tensor<DataType, Symmetry<2, 1>,
               index_list<SpatialIndex<3, UpLo::Lo, Frame::Grid>,
                          SpatialIndex<3, UpLo::Up, Frame::Grid>>>
      G_contracted_1 =
          tenex::evaluate<ti::i, ti::K>(G(ti::K, ti::j, ti::i, ti::J));

  for (size_t i = 0; i < 3; i++) {
    for (size_t k = 0; k < 3; k++) {
      DataType expected_G_sum_1 = make_with_value<DataType>(used_for_size, 0.0);
      for (size_t j = 0; j < 3; j++) {
        expected_G_sum_1 += G.get(k, j, i + 1, j + 1);
      }
      CHECK_ITERABLE_APPROX(G_contracted_1.get(i, k), expected_G_sum_1);
    }
  }

  // Contract one (spacetime, spacetime) pair of indices using generic spatial
  // indices and then one (spatial, spatial) pair of indices
  const Tensor<DataType> G_contracted_2 =
      tenex::evaluate(G(ti::I, ti::i, ti::j, ti::J));

  DataType expected_G_sum_2 = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t i = 0; i < 3; i++) {
    for (size_t j = 0; j < 3; j++) {
      expected_G_sum_2 += G.get(i, i, j + 1, j + 1);
    }
  }
  CHECK_ITERABLE_APPROX(G_contracted_2.get(), expected_G_sum_2);

  // Contract two (spatial, spacetime) pairs of indices
  const Tensor<DataType> G_contracted_3 =
      tenex::evaluate(G(ti::I, ti::j, ti::i, ti::J));

  DataType expected_G_sum_3 = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t i = 0; i < 3; i++) {
    for (size_t j = 0; j < 3; j++) {
      expected_G_sum_3 += G.get(i, j, i + 1, j + 1);
    }
  }
  CHECK_ITERABLE_APPROX(G_contracted_3.get(), expected_G_sum_3);

  const auto H = make_with_random_values<
      Tensor<DataType, Symmetry<4, 3, 2, 1>,
             index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>>(
      generator, distribution, used_for_size);

  // Contract one (spacetime, spacetime) pair of indices using generic spacetime
  // indices and then one (spacetime, spacetime) pair of indices using generic
  // spatial indices
  const Tensor<DataType> H_contracted_1 =
      tenex::evaluate(H(ti::i, ti::I, ti::a, ti::A));

  DataType expected_H_sum_1 = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t i = 0; i < 3; i++) {
    for (size_t a = 0; a < 4; a++) {
      expected_H_sum_1 += H.get(i + 1, i + 1, a, a);
    }
  }
  CHECK_ITERABLE_APPROX(H_contracted_1.get(), expected_H_sum_1);

  // Contract two (spacetime, spacetime) pair of indices using generic spatial
  // indices
  const Tensor<DataType> H_contracted_2 =
      tenex::evaluate(H(ti::j, ti::I, ti::i, ti::J));

  DataType expected_H_sum_2 = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t j = 0; j < 3; j++) {
    for (size_t i = 0; i < 3; i++) {
      expected_H_sum_2 += H.get(j + 1, i + 1, i + 1, j + 1);
    }
  }
  CHECK_ITERABLE_APPROX(H_contracted_2.get(), expected_H_sum_2);
}

template <typename Generator, typename DataType>
void test_time_index(const gsl::not_null<Generator*> generator,
                     const DataType& used_for_size) {
  std::uniform_real_distribution<> distribution(-1.0, 1.0);

  const auto R = make_with_random_values<
      Tensor<DataType, Symmetry<4, 3, 2, 1>,
             index_list<SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>>>>(
      generator, distribution, used_for_size);
  // Contract RHS tensor with time index to a LHS tensor without a time index
  // Use explicit type (vs auto) for LHS Tensor so the compiler checks the
  // return type of `evaluate`
  // \f$L_{b} = R^{at}{}_{ab}\f$
  const Tensor<DataType, Symmetry<1>,
               index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>>>
      R_contracted_1 = tenex::evaluate<ti::b>(R(ti::A, ti::T, ti::a, ti::b));

  for (size_t b = 0; b < 4; b++) {
    DataType expected_R_sum_1 = make_with_value<DataType>(used_for_size, 0.0);
    for (size_t a = 0; a < 4; a++) {
      expected_R_sum_1 += R.get(a, 0, a, b);
    }
    CHECK_ITERABLE_APPROX(R_contracted_1.get(b), expected_R_sum_1);
  }

  // Contract RHS tensor with upper and lower time index to a LHS tensor without
  // time indices
  // \f$L = R^{at}{}_{at}\f$
  //
  // Makes sure that `TensorContract` does not get confused by the presence of
  // an upper and lower time index in the RHS tensor, which is different than
  // the presence of an upper and lower generic index
  const Tensor<DataType> R_contracted_2 =
      tenex::evaluate(R(ti::A, ti::T, ti::a, ti::t));
  DataType expected_R_sum_2 = make_with_value<DataType>(used_for_size, 0.0);
  for (size_t a = 0; a < 4; a++) {
    expected_R_sum_2 += R.get(a, 0, a, 0);
  }
  CHECK_ITERABLE_APPROX(R_contracted_2.get(), expected_R_sum_2);

  const auto S = make_with_random_values<
      Tensor<DataType, Symmetry<3, 2, 1>,
             index_list<SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>>>>(
      generator, distribution, used_for_size);
  // Assign a placeholder to the LHS tensor's components before it is computed
  // so that when test expressions below only compute time components, we can
  // check that LHS spatial components haven't changed
  const auto spatial_component_placeholder_value =
      TestHelpers::tenex::component_placeholder_value<DataType>::value;
  auto S_contracted = make_with_value<
      Tensor<DataType, Symmetry<4, 3, 2, 1>,
             index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Inertial>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Inertial>>>>(
      used_for_size, spatial_component_placeholder_value);

  // Contract a RHS tensor without time indices to a LHS tensor with time
  // indices
  // \f$L_{tt}{}^{bt} = R^{ba}{}_{a}\f$
  //
  // Makes sure that `TensorContract` does not get confused by the presence of
  // an upper and lower time index in the LHS tensor, which is different than
  // the presence of an upper and lower generic index. Also makes sure that
  // a contraction can be evaluated to a LHS tensor of higher rank than the
  // rank that results from contracting the RHS
  ::tenex::evaluate<ti::t, ti::t, ti::B, ti::T>(make_not_null(&S_contracted),
                                                S(ti::B, ti::A, ti::a));

  for (size_t b = 0; b < 4; b++) {
    DataType expected_S_sum = make_with_value<DataType>(used_for_size, 0.0);
    for (size_t a = 0; a < 4; a++) {
      expected_S_sum += S.get(b, a, a);
    }
    CHECK_ITERABLE_APPROX(S_contracted.get(0, 0, b, 0), expected_S_sum);

    for (size_t i = 0; i < 3; i++) {
      for (size_t j = 0; j < 3; j++) {
        for (size_t k = 0; k < 3; k++) {
          CHECK(S_contracted.get(i + 1, j + 1, b, k + 1) ==
                spatial_component_placeholder_value);
        }
      }
    }
  }

  const auto T = make_with_random_values<
      Tensor<DataType, Symmetry<4, 3, 2, 1>,
             index_list<SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Grid>>>>(
      generator, distribution, used_for_size);
  auto T_contracted = make_with_value<
      Tensor<DataType, Symmetry<3, 2, 1>,
             index_list<SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Up, Frame::Grid>,
                        SpacetimeIndex<3, UpLo::Lo, Frame::Grid>>>>(
      used_for_size, spatial_component_placeholder_value);

  // Contract a RHS tensor with time indices to a LHS tensor with time indices
  // \f$L^{tb}{}_{t} = R_{at}{}^{ab}\f$
  ::tenex::evaluate<ti::T, ti::B, ti::t>(make_not_null(&T_contracted),
                                         T(ti::a, ti::t, ti::A, ti::B));

  for (size_t b = 0; b < 4; b++) {
    DataType expected_T_sum = make_with_value<DataType>(used_for_size, 0.0);
    for (size_t a = 0; a < 4; a++) {
      expected_T_sum += T.get(a, 0, a, b);
    }
    CHECK_ITERABLE_APPROX(T_contracted.get(0, b, 0), expected_T_sum);

    for (size_t i = 0; i < 3; i++) {
      for (size_t j = 0; j < 3; j++) {
        CHECK(T_contracted.get(i + 1, b, j + 1) ==
              spatial_component_placeholder_value);
      }
    }
  }
}

template <typename Generator, typename DataType>
void test_contractions(const gsl::not_null<Generator*> generator,
                       const DataType& used_for_size) {
  test_contractions_rank2(generator, used_for_size);
  test_contractions_rank3(generator, used_for_size);
  test_contractions_rank4(generator, used_for_size);
  test_spatial_spacetime_index(generator, used_for_size);
  test_time_index(generator, used_for_size);
}
}  // namespace

SPECTRE_TEST_CASE("Unit.DataStructures.Tensor.Expression.Contract",
                  "[DataStructures][Unit]") {
  MAKE_GENERATOR(generator);

  test_tensor_ops_properties();
  test_contraction_summation_consistency();
  test_contractions(make_not_null(&generator),
                    std::numeric_limits<double>::signaling_NaN());
  test_contractions(
      make_not_null(&generator),
      DataVector(5, std::numeric_limits<double>::signaling_NaN()));
  test_contractions(
      make_not_null(&generator),
      ComplexDataVector(5, std::numeric_limits<double>::signaling_NaN()));
}
