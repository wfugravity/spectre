// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Elliptic/Systems/Xcts/Events/ObserveAdmIntegrals.hpp"

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/EagerMath/Magnitude.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "Domain/AreaElement.hpp"
#include "Domain/Tags.hpp"
#include "NumericalAlgorithms/DiscontinuousGalerkin/ProjectToBoundary.hpp"
#include "NumericalAlgorithms/LinearOperators/DefiniteIntegral.hpp"
#include "NumericalAlgorithms/Spectral/Mesh.hpp"
#include "PointwiseFunctions/Xcts/AdmLinearMomentum.hpp"
#include "PointwiseFunctions/Xcts/AdmMass.hpp"

namespace Events {

void local_adm_integrals(
    gsl::not_null<Scalar<double>*> adm_mass,
    gsl::not_null<tnsr::I<double, 3>*> adm_linear_momentum,
    const Scalar<DataVector>& conformal_factor,
    const tnsr::i<DataVector, 3>& deriv_conformal_factor,
    const tnsr::ii<DataVector, 3>& conformal_metric,
    const tnsr::II<DataVector, 3>& inv_conformal_metric,
    const tnsr::Ijj<DataVector, 3>& conformal_christoffel_second_kind,
    const tnsr::i<DataVector, 3>& conformal_christoffel_contracted,
    const tnsr::ii<DataVector, 3>& spatial_metric,
    const tnsr::II<DataVector, 3>& inv_spatial_metric,
    const tnsr::ii<DataVector, 3>& extrinsic_curvature,
    const Scalar<DataVector>& trace_extrinsic_curvature,
    const InverseJacobian<DataVector, 3, Frame::ElementLogical,
                          Frame::Inertial>& inv_jacobian,
    const Mesh<3>& mesh, const Element<3>& element,
    const DirectionMap<3, tnsr::i<DataVector, 3>>& conformal_face_normals) {
  // Initialize integrals to 0
  adm_mass->get() = 0;
  for (int I = 0; I < 3; I++) {
    adm_linear_momentum->get(I) = 0;
  }

  // Skip elements not at the outer boundary
  const size_t radial_dimension = 2;
  const auto radial_segment_id = element.id().segment_id(radial_dimension);
  if (radial_segment_id.index() !=
      two_to_the(radial_segment_id.refinement_level()) - 1) {
    return;
  }

  // Loop over external boundaries
  for (const auto boundary_direction : element.external_boundaries()) {
    // Skip interfaces not at the outer boundary
    if (boundary_direction != Direction<3>::upper_zeta()) {
      continue;
    }

    // Project fields to the boundary
    const auto face_conformal_factor = dg::project_tensor_to_boundary(
        conformal_factor, mesh, boundary_direction);
    const auto face_deriv_conformal_factor = dg::project_tensor_to_boundary(
        deriv_conformal_factor, mesh, boundary_direction);
    const auto face_conformal_metric = dg::project_tensor_to_boundary(
        conformal_metric, mesh, boundary_direction);
    const auto face_inv_conformal_metric = dg::project_tensor_to_boundary(
        inv_conformal_metric, mesh, boundary_direction);
    const auto face_conformal_christoffel_second_kind =
        dg::project_tensor_to_boundary(conformal_christoffel_second_kind, mesh,
                                       boundary_direction);
    const auto face_conformal_christoffel_contracted =
        dg::project_tensor_to_boundary(conformal_christoffel_contracted, mesh,
                                       boundary_direction);
    const auto face_spatial_metric = dg::project_tensor_to_boundary(
        spatial_metric, mesh, boundary_direction);
    const auto face_inv_spatial_metric = dg::project_tensor_to_boundary(
        inv_spatial_metric, mesh, boundary_direction);
    const auto face_extrinsic_curvature = dg::project_tensor_to_boundary(
        extrinsic_curvature, mesh, boundary_direction);
    const auto face_trace_extrinsic_curvature = dg::project_tensor_to_boundary(
        trace_extrinsic_curvature, mesh, boundary_direction);
    // This projection could be avoided by using
    // domain::Tags::DetSurfaceJacobian from the DataBox, which is computed
    // directly on the face (not projected). That would be better on Gauss
    // meshes that have no grid point at the boundary. The DetSurfaceJacobian
    // could then be multiplied by the (conformal) metric determinant to form
    // the area element. Note that the DetSurfaceJacobian is computed using the
    // conformal metric.
    const auto face_inv_jacobian =
        dg::project_tensor_to_boundary(inv_jacobian, mesh, boundary_direction);

    // Compute the inverse extrinsic curvature
    tnsr::II<DataVector, 3> face_inv_extrinsic_curvature;
    tenex::evaluate<ti::I, ti::J>(make_not_null(&face_inv_extrinsic_curvature),
                                  face_inv_spatial_metric(ti::I, ti::K) *
                                      face_inv_spatial_metric(ti::J, ti::L) *
                                      face_extrinsic_curvature(ti::k, ti::l));

    // Compute curved area element
    const auto face_sqrt_det_conformal_metric =
        Scalar<DataVector>(sqrt(get(determinant(face_conformal_metric))));
    const auto curved_area_element =
        area_element(face_inv_jacobian, boundary_direction,
                     face_inv_conformal_metric, face_sqrt_det_conformal_metric);

    // Get face mesh and face normal from data box
    const auto& face_mesh = mesh.slice_away(boundary_direction.dimension());
    const auto& conformal_face_normal =
        conformal_face_normals.at(boundary_direction);

    // Compute surface integrands
    const auto mass_integrand = Xcts::adm_mass_surface_integrand(
        face_deriv_conformal_factor, face_inv_conformal_metric,
        face_conformal_christoffel_second_kind,
        face_conformal_christoffel_contracted);
    const auto linear_momentum_integrand =
        Xcts::adm_linear_momentum_surface_integrand(
            face_conformal_factor, face_inv_spatial_metric,
            face_inv_extrinsic_curvature, face_trace_extrinsic_curvature);

    // Contract surface integrands with face normal
    const auto contracted_mass_integrand =
        tenex::evaluate(mass_integrand(ti::I) * conformal_face_normal(ti::i));
    const auto contracted_linear_momentum_integrand = tenex::evaluate<ti::I>(
        linear_momentum_integrand(ti::I, ti::J) * conformal_face_normal(ti::j));

    // Take integrals
    adm_mass->get() += definite_integral(
        get(contracted_mass_integrand) * get(curved_area_element), face_mesh);
    for (int I = 0; I < 3; I++) {
      adm_linear_momentum->get(I) +=
          definite_integral(contracted_linear_momentum_integrand.get(I) *
                                get(curved_area_element),
                            face_mesh);
    }
  }
}

PUP::able::PUP_ID ObserveAdmIntegrals::my_PUP_ID = 0;  // NOLINT

}  // namespace Events
