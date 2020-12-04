// Distributed under the MIT License.
// See LICENSE.txt for details.

/// \file
/// Defines class FrustalCloak.

#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "Domain/Creators/DomainCreator.hpp"  // IWYU pragma: keep
#include "Domain/Domain.hpp"
#include "Options/Options.hpp"
#include "Utilities/TMPL.hpp"

/// \cond
namespace domain {
namespace CoordinateMaps {
class Frustum;
}  // namespace CoordinateMaps

template <typename SourceFrame, typename TargetFrame, typename... Maps>
class CoordinateMap;
}  // namespace domain
/// \endcond

namespace domain {
namespace creators {
/// Create a 3D cubical domain with two equal-sized abutting excised cubes in
/// the center. This is done by combining ten frusta.
class FrustalCloak : public DomainCreator<3> {
 public:
  using maps_list =
      tmpl::list<domain::CoordinateMap<Frame::Logical, Frame::Inertial,
                                       domain::CoordinateMaps::Frustum>>;

  struct InitialRefinement {
    using type = size_t;
    static constexpr Options::String help = {
        "Initial refinement level in each dimension."};
  };

  struct InitialGridPoints {
    using type = std::array<size_t, 2>;
    static constexpr Options::String help = {
        "Initial number of grid points in [r,angular]."};
  };

  struct UseEquiangularMap {
    using type = bool;
    static constexpr Options::String help = {
        "Use equiangular instead of equidistant coordinates."};
  };

  struct ProjectionFactor {
    using type = double;
    static constexpr Options::String help = {"Grid compression factor."};
  };

  struct LengthInnerCube {
    using type = double;
    static constexpr Options::String help = {"Side length of each inner cube."};
    static constexpr type lower_bound() noexcept { return 0.0; }
  };

  struct LengthOuterCube {
    using type = double;
    static constexpr Options::String help = {"Side length of the outer cube."};
    static constexpr type lower_bound() noexcept { return 0.0; }
  };

  struct OriginPreimage {
    using type = std::array<double, 3>;
    static constexpr Options::String help = {"The origin preimage in [x,y,z]."};
  };

  using options = tmpl::list<InitialRefinement, InitialGridPoints,
                             UseEquiangularMap, ProjectionFactor,
                             LengthInnerCube, LengthOuterCube, OriginPreimage>;

  static constexpr Options::String help{
      "Creates a cubical domain with two equal-sized abutting excised cubes\n"
      "in the center. This is done by combining ten frusta. The parameter\n"
      "`UseEquiangularMap` can be used to apply a tangent mapping to the xi\n"
      "and eta logical coordinates of each frustum, while the parameter\n"
      "`ProjectionFactor` can be used to apply a projective map to the zeta\n"
      "logical coordinate of each frustum. Increasing the\n"
      "`ProjectionFactor` value can give better gridpoint spacings in the\n"
      "z direction. The user also specifies values for `LengthInnerCube` and\n"
      "`LengthOuterCube`. This will create a cubical Domain of side"
      "length `LengthOuterCube` with the center excised. The size of the\n"
      "excised region is determined by the value set for `LengthInnerCube`.\n"
      "`OriginPreimage` moves the blocks such that the origin preimage is\n"
      "mapped to the origin. Note that the abutting excised cubes share a\n"
      "face in the x-direction. This Domain is primarily for testing the\n"
      "frustal cloak in the BinaryCompactObject Domain."};

  FrustalCloak(typename InitialRefinement::type initial_refinement_level,
               typename InitialGridPoints::type initial_number_of_grid_points,
               typename UseEquiangularMap::type use_equiangular_map = false,
               typename ProjectionFactor::type projection_factor = 1.0,
               typename LengthInnerCube::type length_inner_cube = 0.0,
               typename LengthOuterCube::type length_outer_cube = 0.0,
               typename OriginPreimage::type origin_preimage = {{0.0, 0.0,
                                                                 0.0}},
               const Options::Context& context = {}) noexcept;

  FrustalCloak() = default;
  FrustalCloak(const FrustalCloak&) = delete;
  FrustalCloak(FrustalCloak&&) noexcept = default;
  FrustalCloak& operator=(const FrustalCloak&) = delete;
  FrustalCloak& operator=(FrustalCloak&&) noexcept = default;
  ~FrustalCloak() noexcept override = default;

  Domain<3> create_domain() const noexcept override;

  std::vector<std::array<size_t, 3>> initial_extents() const noexcept override;

  std::vector<std::array<size_t, 3>> initial_refinement_levels() const
      noexcept override;

 private:
  typename InitialRefinement::type initial_refinement_level_{};
  typename InitialGridPoints::type initial_number_of_grid_points_{};
  typename UseEquiangularMap::type use_equiangular_map_ = false;
  typename ProjectionFactor::type projection_factor_{};
  typename LengthInnerCube::type length_inner_cube_{0.0};
  typename LengthOuterCube::type length_outer_cube_{0.0};
  typename OriginPreimage::type origin_preimage_{{0.0, 0.0, 0.0}};
};
}  // namespace creators
}  // namespace domain
