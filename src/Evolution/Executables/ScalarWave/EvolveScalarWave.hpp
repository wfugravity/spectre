// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Domain/Creators/RegisterDerivedWithCharm.hpp"
#include "Domain/Creators/TimeDependence/RegisterDerivedWithCharm.hpp"
#include "Domain/FunctionsOfTime/RegisterDerivedWithCharm.hpp"
#include "Domain/Tags.hpp"
#include "Evolution/ComputeTags.hpp"
#include "Evolution/DiscontinuousGalerkin/Actions/ApplyBoundaryCorrections.hpp"
#include "Evolution/DiscontinuousGalerkin/Actions/ComputeTimeDerivative.hpp"
#include "Evolution/DiscontinuousGalerkin/DgElementArray.hpp"  // IWYU pragma: keep
#include "Evolution/DiscontinuousGalerkin/Initialization/Mortars.hpp"
#include "Evolution/DiscontinuousGalerkin/Initialization/QuadratureTag.hpp"
#include "Evolution/Initialization/DgDomain.hpp"
#include "Evolution/Initialization/Evolution.hpp"
#include "Evolution/Initialization/NonconservativeSystem.hpp"
#include "Evolution/Initialization/SetVariables.hpp"
#include "Evolution/Systems/ScalarWave/BoundaryConditions/Factory.hpp"
#include "Evolution/Systems/ScalarWave/BoundaryConditions/RegisterDerivedWithCharm.hpp"
#include "Evolution/Systems/ScalarWave/BoundaryCorrections/Factory.hpp"
#include "Evolution/Systems/ScalarWave/BoundaryCorrections/RegisterDerived.hpp"
#include "Evolution/Systems/ScalarWave/Equations.hpp"
#include "Evolution/Systems/ScalarWave/Initialize.hpp"
#include "Evolution/Systems/ScalarWave/System.hpp"
#include "Evolution/TypeTraits.hpp"
#include "IO/Observer/Actions/RegisterEvents.hpp"
#include "IO/Observer/Helpers.hpp"            // IWYU pragma: keep
#include "IO/Observer/ObserverComponent.hpp"  // IWYU pragma: keep
#include "NumericalAlgorithms/DiscontinuousGalerkin/Formulation.hpp"
#include "NumericalAlgorithms/DiscontinuousGalerkin/Tags.hpp"
#include "NumericalAlgorithms/LinearOperators/ExponentialFilter.hpp"
#include "NumericalAlgorithms/LinearOperators/FilterAction.hpp"  // IWYU pragma: keep
#include "Options/Options.hpp"
#include "Options/Protocols/FactoryCreation.hpp"
#include "Parallel/Actions/SetupDataBox.hpp"
#include "Parallel/Actions/TerminatePhase.hpp"
#include "Parallel/InitializationFunctions.hpp"
#include "Parallel/PhaseControl/ExecutePhaseChange.hpp"
#include "Parallel/PhaseControl/PhaseControlTags.hpp"
#include "Parallel/PhaseControl/VisitAndReturn.hpp"
#include "Parallel/PhaseDependentActionList.hpp"
#include "Parallel/Reduction.hpp"
#include "Parallel/RegisterDerivedClassesWithCharm.hpp"
#include "ParallelAlgorithms/Actions/MutateApply.hpp"
#include "ParallelAlgorithms/Events/ObserveErrorNorms.hpp"  // IWYU pragma: keep
#include "ParallelAlgorithms/Events/ObserveFields.hpp"      // IWYU pragma: keep
#include "ParallelAlgorithms/Events/ObserveTimeStep.hpp"
#include "ParallelAlgorithms/EventsAndTriggers/Actions/RunEventsAndTriggers.hpp"  // IWYU pragma: keep
#include "ParallelAlgorithms/EventsAndTriggers/Event.hpp"
#include "ParallelAlgorithms/EventsAndTriggers/EventsAndTriggers.hpp"  // IWYU pragma: keep
#include "ParallelAlgorithms/EventsAndTriggers/LogicalTriggers.hpp"
#include "ParallelAlgorithms/EventsAndTriggers/Tags.hpp"
#include "ParallelAlgorithms/EventsAndTriggers/Trigger.hpp"
#include "ParallelAlgorithms/Initialization/Actions/AddComputeTags.hpp"
#include "ParallelAlgorithms/Initialization/Actions/RemoveOptionsAndTerminatePhase.hpp"
#include "PointwiseFunctions/AnalyticSolutions/Tags.hpp"
#include "PointwiseFunctions/AnalyticSolutions/WaveEquation/PlaneWave.hpp"  // IWYU pragma: keep
#include "PointwiseFunctions/AnalyticSolutions/WaveEquation/RegularSphericalWave.hpp"  // IWYU pragma: keep
#include "PointwiseFunctions/MathFunctions/MathFunction.hpp"
#include "Time/Actions/AdvanceTime.hpp"                // IWYU pragma: keep
#include "Time/Actions/ChangeSlabSize.hpp"             // IWYU pragma: keep
#include "Time/Actions/ChangeStepSize.hpp"             // IWYU pragma: keep
#include "Time/Actions/RecordTimeStepperData.hpp"      // IWYU pragma: keep
#include "Time/Actions/SelfStartActions.hpp"           // IWYU pragma: keep
#include "Time/Actions/UpdateU.hpp"                    // IWYU pragma: keep
#include "Time/StepChoosers/ByBlock.hpp"               // IWYU pragma: keep
#include "Time/StepChoosers/Cfl.hpp"                   // IWYU pragma: keep
#include "Time/StepChoosers/Constant.hpp"              // IWYU pragma: keep
#include "Time/StepChoosers/Increase.hpp"              // IWYU pragma: keep
#include "Time/StepChoosers/PreventRapidIncrease.hpp"  // IWYU pragma: keep
#include "Time/StepChoosers/StepChooser.hpp"
#include "Time/StepChoosers/StepToTimes.hpp"
#include "Time/StepControllers/Factory.hpp"
#include "Time/StepControllers/StepController.hpp"
#include "Time/Tags.hpp"
#include "Time/TimeSequence.hpp"
#include "Time/TimeSteppers/TimeStepper.hpp"
#include "Time/Triggers/TimeTriggers.hpp"
#include "Utilities/Blas.hpp"
#include "Utilities/ErrorHandling/Error.hpp"
#include "Utilities/ErrorHandling/FloatingPointExceptions.hpp"
#include "Utilities/Functional.hpp"
#include "Utilities/ProtocolHelpers.hpp"
#include "Utilities/TMPL.hpp"

/// \cond
namespace Frame {
// IWYU pragma: no_forward_declare MathFunction
struct Inertial;
}  // namespace Frame
namespace Parallel {
template <typename Metavariables>
class CProxy_GlobalCache;
}  // namespace Parallel
namespace PUP {
class er;
}  // namespace PUP
/// \endcond

template <size_t Dim, typename InitialData>
struct EvolutionMetavars {
  static constexpr size_t volume_dim = Dim;
  // Customization/"input options" to simulation
  using initial_data_tag = Tags::AnalyticSolution<InitialData>;
  static_assert(
      evolution::is_analytic_data_v<InitialData> xor
          evolution::is_analytic_solution_v<InitialData>,
      "initial_data must be either an analytic_data or an analytic_solution");

  using system = ScalarWave::System<Dim>;
  static constexpr dg::Formulation dg_formulation =
      dg::Formulation::StrongInertial;
  using temporal_id = Tags::TimeStepId;
  static constexpr bool local_time_stepping = true;
  using time_stepper_tag = Tags::TimeStepper<
      tmpl::conditional_t<local_time_stepping, LtsTimeStepper, TimeStepper>>;

  using step_choosers_common = tmpl::list<
      StepChoosers::Registrars::ByBlock<volume_dim>,
      StepChoosers::Registrars::Cfl<Frame::Inertial, system>,
      StepChoosers::Registrars::Constant, StepChoosers::Registrars::Increase>;
  using step_choosers_for_step_only =
      tmpl::list<StepChoosers::Registrars::PreventRapidIncrease>;
  using step_choosers_for_slab_only =
      tmpl::list<StepChoosers::Registrars::StepToTimes>;
  using step_choosers = tmpl::conditional_t<
      local_time_stepping,
      tmpl::append<step_choosers_common, step_choosers_for_step_only>,
      tmpl::list<>>;
  using slab_choosers = tmpl::conditional_t<
      local_time_stepping,
      tmpl::append<step_choosers_common, step_choosers_for_slab_only>,
      tmpl::append<step_choosers_common, step_choosers_for_step_only,
                   step_choosers_for_slab_only>>;

  // public for use by the Charm++ registration code
  using observe_fields = typename system::variables_tag::tags_list;
  using analytic_solution_fields = observe_fields;
  using events =
      tmpl::list<dg::Events::Registrars::ObserveFields<
                     Dim, Tags::Time, observe_fields, analytic_solution_fields>,
                 dg::Events::Registrars::ObserveErrorNorms<
                     Tags::Time, analytic_solution_fields>,
                 Events::Registrars::ObserveTimeStep<EvolutionMetavars>,
                 Events::Registrars::ChangeSlabSize<slab_choosers>>;

  using observed_reduction_data_tags = observers::collect_reduction_data_tags<
      typename Event<events>::creatable_classes>;

  struct factory_creation
      : tt::ConformsTo<Options::protocols::FactoryCreation> {
    using factory_classes = tmpl::map<
        tmpl::pair<StepController, StepControllers::standard_step_controllers>,
        tmpl::pair<Trigger, tmpl::append<Triggers::logical_triggers,
                                         Triggers::time_triggers>>>;
  };

  // The scalar wave system generally does not require filtering, except
  // possibly on certain deformed domains.  Here a filter is added in 2D for
  // testing purposes.  When performing numerical experiments with the scalar
  // wave system, the user should determine whether this filter can be removed.
  static constexpr bool use_filtering = (2 == volume_dim);

  using step_actions = tmpl::flatten<tmpl::list<
      evolution::dg::Actions::ComputeTimeDerivative<EvolutionMetavars>,
      evolution::dg::Actions::ApplyBoundaryCorrections<EvolutionMetavars>,
      tmpl::conditional_t<
          local_time_stepping, tmpl::list<>,
          tmpl::list<Actions::RecordTimeStepperData<>, Actions::UpdateU<>>>,
      tmpl::conditional_t<
          use_filtering,
          dg::Actions::Filter<Filters::Exponential<0>,
                              tmpl::list<ScalarWave::Pi, ScalarWave::Psi,
                                         ScalarWave::Phi<Dim>>>,
          tmpl::list<>>>>;

  enum class Phase {
    Initialization,
    RegisterWithObserver,
    InitializeTimeStepperHistory,
    LoadBalancing,
    Evolve,
    Exit
  };

  static std::string phase_name(Phase phase) noexcept {
    if (phase == Phase::LoadBalancing) {
      return "LoadBalancing";
    }
    ERROR(
        "Passed phase that should not be used in input file. Integer "
        "corresponding to phase is: "
        << static_cast<int>(phase));
  }

  using phase_changes = tmpl::list<PhaseControl::Registrars::VisitAndReturn<
      EvolutionMetavars, Phase::LoadBalancing>>;

  using initialize_phase_change_decision_data =
      PhaseControl::InitializePhaseChangeDecisionData<phase_changes>;

  using phase_change_tags_and_combines_list =
      PhaseControl::get_phase_change_tags<phase_changes>;

  using const_global_cache_tags = tmpl::list<
      initial_data_tag, time_stepper_tag,
      Tags::EventsAndTriggers<events>,
      PhaseControl::Tags::PhaseChangeAndTriggers<phase_changes>>;

  using dg_registration_list =
      tmpl::list<observers::Actions::RegisterEventsWithObservers>;

  using initialization_actions =
      tmpl::list<Actions::SetupDataBox,
                 Initialization::Actions::TimeAndTimeStep<EvolutionMetavars>,
                 evolution::dg::Initialization::Domain<volume_dim>,
                 Initialization::Actions::NonconservativeSystem<system>,
                 evolution::Initialization::Actions::SetVariables<
                     domain::Tags::Coordinates<Dim, Frame::Logical>>,
                 Initialization::Actions::TimeStepperHistory<EvolutionMetavars>,
                 ScalarWave::Actions::InitializeConstraints<volume_dim>,
                 Initialization::Actions::AddComputeTags<tmpl::push_back<
                     StepChoosers::step_chooser_compute_tags<EvolutionMetavars>,
                     evolution::Tags::AnalyticCompute<
                         Dim, initial_data_tag, analytic_solution_fields>>>,
                 ::evolution::dg::Initialization::Mortars<volume_dim, system>,
                 Initialization::Actions::RemoveOptionsAndTerminatePhase>;

  using dg_element_array = DgElementArray<
      EvolutionMetavars,
      tmpl::list<
          Parallel::PhaseActions<Phase, Phase::Initialization,
                                 initialization_actions>,

          Parallel::PhaseActions<
              Phase, Phase::InitializeTimeStepperHistory,
              SelfStart::self_start_procedure<step_actions, system>>,

          Parallel::PhaseActions<Phase, Phase::RegisterWithObserver,
                                 tmpl::list<dg_registration_list,
                                            Parallel::Actions::TerminatePhase>>,

          Parallel::PhaseActions<
              Phase, Phase::Evolve,
              tmpl::list<Actions::RunEventsAndTriggers, Actions::ChangeSlabSize,
                         step_actions, Actions::AdvanceTime,
                         PhaseControl::Actions::ExecutePhaseChange<
                             phase_changes>>>>>;

  template <typename ParallelComponent>
  struct registration_list {
    using type =
        std::conditional_t<std::is_same_v<ParallelComponent, dg_element_array>,
      dg_registration_list, tmpl::list<>>;
  };

  using component_list =
      tmpl::list<observers::Observer<EvolutionMetavars>,
                 observers::ObserverWriter<EvolutionMetavars>,
                 dg_element_array>;

  static constexpr Options::String help{
      "Evolve a Scalar Wave in Dim spatial dimension.\n\n"
      "The numerical flux is:    UpwindFlux\n"};

  template <typename... Tags>
  static Phase determine_next_phase(
      const gsl::not_null<tuples::TaggedTuple<Tags...>*>
          phase_change_decision_data,
      const Phase& current_phase,
      const Parallel::CProxy_GlobalCache<EvolutionMetavars>&
          cache_proxy) noexcept {
    const auto next_phase =
        PhaseControl::arbitrate_phase_change<phase_changes>(
            phase_change_decision_data, current_phase,
            *(cache_proxy.ckLocalBranch()));
    if (next_phase.has_value()) {
      return next_phase.value();
    }
    switch (current_phase) {
      case Phase::Initialization:
        return Phase::InitializeTimeStepperHistory;
      case Phase::InitializeTimeStepperHistory:
        return Phase::RegisterWithObserver;
      case Phase::RegisterWithObserver:
        return Phase::Evolve;
      case Phase::Evolve:
        return Phase::Exit;
      case Phase::Exit:
        ERROR(
            "Should never call determine_next_phase with the current phase "
            "being 'Exit'");
      default:
        ERROR(
            "Unknown type of phase. Did you static_cast<Phase> an integral "
            "value?");
    }
  }

  // NOLINTNEXTLINE(google-runtime-references)
  void pup(PUP::er& /*p*/) noexcept {}
};

static const std::vector<void (*)()> charm_init_node_funcs{
    &setup_error_handling,
    &disable_openblas_multithreading,
    &domain::creators::register_derived_with_charm,
    &domain::creators::time_dependence::register_derived_with_charm,
    &domain::FunctionsOfTime::register_derived_with_charm,
    &ScalarWave::BoundaryConditions::register_derived_with_charm,
    &ScalarWave::BoundaryCorrections::register_derived_with_charm,
    &Parallel::register_derived_classes_with_charm<
        Event<metavariables::events>>,
    &Parallel::register_derived_classes_with_charm<
        MathFunction<1, Frame::Inertial>>,
    &Parallel::register_derived_classes_with_charm<
        StepChooser<metavariables::slab_choosers>>,
    &Parallel::register_derived_classes_with_charm<
        StepChooser<metavariables::step_choosers>>,
    &Parallel::register_derived_classes_with_charm<TimeSequence<double>>,
    &Parallel::register_derived_classes_with_charm<TimeSequence<std::uint64_t>>,
    &Parallel::register_derived_classes_with_charm<TimeStepper>,
    &Parallel::register_derived_classes_with_charm<
        PhaseChange<metavariables::phase_changes>>,
    &Parallel::register_factory_classes_with_charm<metavariables>};

static const std::vector<void (*)()> charm_init_proc_funcs{
    &enable_floating_point_exceptions};
