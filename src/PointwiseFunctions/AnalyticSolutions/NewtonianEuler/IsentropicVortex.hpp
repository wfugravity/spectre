// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <array>
#include <cstddef>
#include <limits>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/TypeAliases.hpp"
#include "Options/String.hpp"
#include "PointwiseFunctions/AnalyticSolutions/AnalyticSolution.hpp"
#include "PointwiseFunctions/Hydro/EquationsOfState/EquationOfState.hpp"
#include "PointwiseFunctions/Hydro/EquationsOfState/PolytropicFluid.hpp"
#include "PointwiseFunctions/Hydro/Tags.hpp"
#include "PointwiseFunctions/InitialDataUtilities/InitialData.hpp"
#include "Utilities/MakeArray.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TaggedTuple.hpp"

/// \cond
namespace PUP {
class er;
}  // namespace PUP
/// \endcond

namespace NewtonianEuler::Solutions {

/*!
 * \brief Newtonian isentropic vortex in Cartesian coordinates
 *
 * The analytic solution to the 2-D Newtonian Euler system
 * representing the slow advection of an incompressible, isentropic
 * vortex \cite Yee1999. The initial condition is the superposition of a
 * mean uniform flow with a gaussian-profile vortex. When embedded in
 * 3-D space, the isentropic vortex is still a solution to the corresponding 3-D
 * system if the velocity along the third axis is a constant. In Cartesian
 * coordinates \f$(x, y, z)\f$, and using dimensionless units, the primitive
 * quantities at a given time \f$t\f$ are then
 *
 * \f{align*}
 * \rho &= \left[1 - \dfrac{(\gamma - 1)\beta^2}{8\gamma\pi^2}\exp\left(
 * 1 - r^2\right)\right]^{1/(\gamma - 1)}, \\
 * v_x &= U - \dfrac{\beta\tilde y}{2\pi}\exp\left(\dfrac{1 - r^2}{2}\right),\\
 * v_y &= V + \dfrac{\beta\tilde x}{2\pi}\exp\left(\dfrac{1 - r^2}{2}\right),\\
 * v_z &= W,\\
 * \epsilon &= \frac{\rho^{\gamma - 1}}{\gamma - 1},
 * \f}
 *
 * with
 *
 * \f{align*}
 * r^2 &= {\tilde x}^2 + {\tilde y}^2,\\
 * \tilde x &= x - X_0 - U t,\\
 * \tilde y &= y - Y_0 - V t,
 * \f}
 *
 * where \f$(X_0, Y_0)\f$ is the position of the vortex on the \f$(x, y)\f$
 * plane at \f$t = 0\f$, \f$(U, V, W)\f$ are the components of the mean flow
 * velocity, \f$\beta\f$ is the vortex strength, and \f$\gamma\f$ is the
 * adiabatic index. The pressure \f$p\f$ is then obtained from the dimensionless
 * polytropic relation
 *
 * \f{align*}
 * p = \rho^\gamma.
 * \f}
 *
 * On the other hand, if the velocity along the \f$z-\f$axis is not a constant
 * but a function of the \f$z\f$ coordinate, the resulting modified isentropic
 * vortex is still a solution to the Newtonian Euler system, but with source
 * terms that are proportional to \f$dv_z/dz\f$. (See
 * NewtonianEuler::Sources::VortexPerturbation.) For testing purposes,
 * we choose to write the velocity as a uniform field plus a periodic
 * perturbation,
 *
 * \f{align*}
 * v_z(z) = W + \epsilon \sin{z},
 * \f}
 *
 * where \f$\epsilon\f$ is the amplitude of the perturbation. The resulting
 * source for the Newtonian Euler system will then be proportional to
 * \f$\epsilon \cos{z}\f$.
 */
template <size_t Dim>
class IsentropicVortex : public evolution::initial_data::InitialData,
                         public MarkAsAnalyticSolution {
  static_assert(Dim == 2 or Dim == 3,
                "IsentropicVortex solution works in 2 and 3 dimensions");

  template <typename DataType>
  struct IntermediateVariables;

 public:
  using equation_of_state_type = EquationsOfState::PolytropicFluid<false>;

  /// The adiabatic index of the fluid.
  struct AdiabaticIndex {
    using type = double;
    static constexpr Options::String help = {
        "The adiabatic index of the fluid."};
  };

  /// The position of the center of the vortex at \f$t = 0\f$
  struct Center {
    using type = std::array<double, Dim>;
    static constexpr Options::String help = {
        "The coordinates of the center of the vortex at t = 0."};
  };

  /// The mean flow velocity.
  struct MeanVelocity {
    using type = std::array<double, Dim>;
    static constexpr Options::String help = {"The mean flow velocity."};
  };

  /// The amplitude of the perturbation generating a source term.
  struct PerturbAmplitude {
    using type = double;
    static constexpr Options::String help = {
        "The amplitude of the perturbation producing sources."};
  };

  /// The strength of the vortex.
  struct Strength {
    using type = double;
    static constexpr Options::String help = {"The strength of the vortex."};
    static type lower_bound() { return 0.0; }
  };

  using options = tmpl::conditional_t<
      Dim == 3,
      tmpl::list<AdiabaticIndex, Center, MeanVelocity, Strength,
                 PerturbAmplitude>,
      tmpl::list<AdiabaticIndex, Center, MeanVelocity, Strength>>;

  static constexpr Options::String help = {
      "Newtonian Isentropic Vortex. Works in 2 and 3 dimensions."};

  IsentropicVortex() = default;
  IsentropicVortex(const IsentropicVortex& /*rhs*/) = default;
  IsentropicVortex& operator=(const IsentropicVortex& /*rhs*/) = default;
  IsentropicVortex(IsentropicVortex&& /*rhs*/) = default;
  IsentropicVortex& operator=(IsentropicVortex&& /*rhs*/) = default;
  ~IsentropicVortex() override = default;

  auto get_clone() const
      -> std::unique_ptr<evolution::initial_data::InitialData> override;

  /// \cond
  explicit IsentropicVortex(CkMigrateMessage* msg);
  using PUP::able::register_constructor;
  WRAPPED_PUPable_decl_template(IsentropicVortex);
  /// \endcond

  IsentropicVortex(double adiabatic_index,
                   const std::array<double, Dim>& center,
                   const std::array<double, Dim>& mean_velocity,
                   double strength, double perturbation_amplitude = 0.0);

  /// Retrieve a collection of hydrodynamic variables at position x and time t
  template <typename DataType, typename... Tags>
  tuples::TaggedTuple<Tags...> variables(
      const tnsr::I<DataType, Dim, Frame::Inertial>& x,
      const double t,  // NOLINT
      tmpl::list<Tags...> /*meta*/) const {
    static_assert(sizeof...(Tags) > 1,
                  "The generic template will recurse infinitely if only one "
                  "tag is being retrieved.");
    const IntermediateVariables<DataType> vars(x, t, center_, mean_velocity_,
                                               strength_);
    return {tuples::get<Tags>(variables(tmpl::list<Tags>{}, vars))...};
  }

  /// Function of `z` coordinate to compute the perturbation generating
  /// a source term. Public so the corresponding source class can also use it.
  template <typename DataType>
  DataType perturbation_profile(const DataType& z) const;

  template <typename DataType>
  DataType deriv_of_perturbation_profile(const DataType& z) const;

  // To be used by VortexPerturbation source term
  double perturbation_amplitude() const { return perturbation_amplitude_; }

  const EquationsOfState::PolytropicFluid<false>& equation_of_state() const {
    return equation_of_state_;
  }

  // NOLINTNEXTLINE(google-runtime-references)
  void pup(PUP::er& /*p*/) override;

 private:
  /// @{
  /// Retrieve hydro variable at `(x, t)`
  template <typename DataType>
  auto variables(tmpl::list<hydro::Tags::RestMassDensity<DataType>> /*meta*/,
                 const IntermediateVariables<DataType>& vars) const
      -> tuples::TaggedTuple<hydro::Tags::RestMassDensity<DataType>>;

  template <typename DataType>
  auto variables(
      tmpl::list<hydro::Tags::SpatialVelocity<DataType, Dim>> /*meta*/,
      const IntermediateVariables<DataType>& vars) const
      -> tuples::TaggedTuple<hydro::Tags::SpatialVelocity<DataType, Dim>>;

  template <typename DataType>
  auto variables(
      tmpl::list<hydro::Tags::SpecificInternalEnergy<DataType>> /*meta*/,
      const IntermediateVariables<DataType>& vars) const
      -> tuples::TaggedTuple<hydro::Tags::SpecificInternalEnergy<DataType>>;

  template <typename DataType>
  auto variables(tmpl::list<hydro::Tags::Pressure<DataType>> /*meta*/,
                 const IntermediateVariables<DataType>& vars) const
      -> tuples::TaggedTuple<hydro::Tags::Pressure<DataType>>;
  /// @}

  // Intermediate variables needed to compute the primitives
  template <typename DataType>
  struct IntermediateVariables {
    IntermediateVariables(const tnsr::I<DataType, Dim, Frame::Inertial>& x,
                          double t, const std::array<double, Dim>& center,
                          const std::array<double, Dim>& mean_velocity,
                          double strength);
    DataType x_tilde{};
    DataType y_tilde{};
    DataType profile{};
    // (3D only) z-coordinate to compute perturbation term
    DataType z_coord{};
  };

  template <size_t SpatialDim>
  friend bool
  operator==(  // NOLINT (clang-tidy: readability-redundant-declaration)
      const IsentropicVortex<SpatialDim>& lhs,
      const IsentropicVortex<SpatialDim>& rhs);

  double adiabatic_index_ = std::numeric_limits<double>::signaling_NaN();
  std::array<double, Dim> center_ =
      make_array<Dim>(std::numeric_limits<double>::signaling_NaN());
  std::array<double, Dim> mean_velocity_ =
      make_array<Dim>(std::numeric_limits<double>::signaling_NaN());
  double perturbation_amplitude_ = 0.0;
  double strength_ = std::numeric_limits<double>::signaling_NaN();

  // This is an ideal gas undergoing an isentropic process,
  // so the relation between the pressure and the mass density is polytropic,
  // where the polytropic exponent corresponds to the adiabatic index.
  EquationsOfState::PolytropicFluid<false> equation_of_state_{};
};

template <size_t Dim>
bool operator!=(const IsentropicVortex<Dim>& lhs,
                const IsentropicVortex<Dim>& rhs);

}  // namespace NewtonianEuler::Solutions
