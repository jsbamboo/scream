#ifndef EAMXX_AODVIS_DIAG
#define EAMXX_AODVIS_DIAG

#include "share/atm_process/atmosphere_diagnostic.hpp"

namespace scream {

/*
 * This diagnostic will compute the visible aerosol optical depth.
 */

class AODVis : public AtmosphereDiagnostic {
 public:
  // Constructors
  AODVis(const ekat::Comm &comm, const ekat::ParameterList &params);

  // The name of the diagnostic
  std::string name() const override { return "aodvis"; }

  // Set the grid
  void set_grids(
      const std::shared_ptr<const GridsManager> grids_manager) override;

 protected:
#ifdef KOKKOS_ENABLE_CUDA
 public:
#endif
  void compute_diagnostic_impl();

  int m_ncols;
  int m_nlevs;

};

}  // namespace scream

#endif  // EAMXX_AODVIS_DIAG
