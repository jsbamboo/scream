#ifndef SCREAM_NUDGING_HPP
#define SCREAM_NUDGING_HPP

#include "share/util/eamxx_time_interpolation.hpp"
#include "share/atm_process/atmosphere_process.hpp"
#include "ekat/ekat_parameter_list.hpp"
#include "ekat/util/ekat_lin_interp.hpp"
#include "share/io/scream_output_manager.hpp"
#include "share/io/scorpio_output.hpp"
#include "share/io/scorpio_input.hpp"
#include "share/io/scream_scorpio_interface.hpp"
#include "share/grid/mesh_free_grids_manager.hpp"
#include "share/grid/point_grid.hpp"
#include "share/util/scream_vertical_interpolation.hpp"
#include "share/util/scream_time_stamp.hpp"

#include <string>

namespace scream
{

/*
 * The class responsible to handle the nudging of variables
*/

// enum to track how the source pressure levels are defined
enum SourcePresType {
  TIME_DEPENDENT           = 0,  // DEFAULT - source data should include time/spatially varying p_mid with dimensions (time, col, lev)
  SINGLE_VERTICAL_PROFILE  = 1,  // source data includes p_lev which is a static set of levels in both space and time, with dimensions (lev)
  HORIZ_VARYING            = 2,  // source data includes p_mid as a single snapshot w/ dimensions (col,lev)
};

class Nudging : public AtmosphereProcess
{
public:
  using mPack = ekat::Pack<Real,1>;
  using mMask = ekat::Mask<1>;
  using KT = KokkosTypes<DefaultDevice>;

  template <typename S>
  using view_1d = typename KT::template view_1d<S>;

  template <typename S>
  using view_2d = typename KT::template view_2d<S>;

  using uview_2d_mask  = Unmanaged<view_2d<mMask>>;

  template <typename S, int N>
  using view_Nd_host = typename KT::template view_ND<S,N>::HostMirror;

  template <typename S>
  using view_1d_host = view_Nd_host<S,1>;

  template <typename S>
  using view_2d_host = view_Nd_host<S,2>;

  // Constructors
  Nudging (const ekat::Comm& comm, const ekat::ParameterList& params);

  // The type of subcomponent
  AtmosphereProcessType type () const { return AtmosphereProcessType::Physics; }

  // The name of the subcomponent
  std::string name () const { return "Nudging"; }

  // Set the grid
  void set_grids (const std::shared_ptr<const GridsManager> grids_manager);

  // Structure for storing local variables initialized using the ATMBufferManager
  struct Buffer {
    // 2D view
    uview_2d_mask int_mask_view;
    
    // Total number of 2d views
    static constexpr int num_2d_midpoint_mask_views = 1;
  };

#ifndef KOKKOS_ENABLE_CUDA
  // Cuda requires methods enclosing __device__ lambda's to be public
protected:
#endif

  void run_impl        (const double dt);

protected:

  // The two other main overrides for the subcomponent
  void initialize_impl (const RunType run_type);
  void finalize_impl   ();

  // Computes total number of bytes needed for local variables
  size_t requested_buffer_size_in_bytes() const;

  // Set local variables using memory provided by
  // the ATMBufferManager
  void init_buffers(const ATMBufferManager &buffer_manager);

  // Creates an helper field, not to be shared with the AD's FieldManager
  void create_helper_field (const std::string& name,
                            const FieldLayout& layout,
                            const std::string& grid_name,
                            const int ps=0);

  // Query if a local field exists
  bool has_helper_field (const std::string& name) const { return m_helper_fields.find(name)!=m_helper_fields.end(); }
  // Retrieve a helper field
  Field get_helper_field (const std::string& name) const { return m_helper_fields.at(name); }
  // Internal function to apply nudging at specific timescale
  void apply_tendency(Field& base, const Field& next, const int dt);

  std::shared_ptr<const AbstractGrid>   m_grid;
  // Keep track of field dimensions and the iteration count
  int m_num_cols;
  int m_num_levs;
  int m_num_src_levs;
  int m_timescale;
  std::vector<std::string> m_datafiles;
  SourcePresType m_src_pres_type;
  

  // Some helper fields.
  std::map<std::string,Field> m_helper_fields;

  std::vector<std::string> m_fields_nudge;

  util::TimeInterpolation m_time_interp;

  Buffer m_buffer;
}; // class Nudging

} // namespace scream

#endif // SCREAM_NUDGING_HPP
