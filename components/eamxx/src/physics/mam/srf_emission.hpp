#ifndef SRF_EMISSION_HPP
#define SRF_EMISSION_HPP

namespace scream::mam_coupling {
namespace {

template <typename ScalarType, typename DeviceType>
struct srfEmissFunctions {
  struct srfEmissData {
    srfEmissData() = default;
    srfEmissData(const int ncol_) { init(ncol_, true); }

    void init(const int ncol_, const bool allocate) {
      ncols = ncol_;

      if(allocate) {
        AGR = view_1d("", ncols);
        RCO = view_1d("", ncols);
        SHP = view_1d("", ncols);
        SLV = view_1d("", ncols);
        TRA = view_1d("", ncols);
        WST = view_1d("", ncols);
      }
    }

    // Basic spatial dimensions of the data
    int ncols;

    view_1d AGR;
    view_1d RCO;
    view_1d SHP;
    view_1d SLV;
    view_1d TRA;
    view_1d WST;
  };  // srfEmissData

  struct srfEmissInput {
    srfEmissInput() = default;
    srfEmissInput(const int ncols_) { init(ncols_); }

    void init(const int ncols_) { data.init(ncols_, true); }
    srfEmissData data;  // All srfEmiss fields
  };                    // srfEmissInput

  // The output is really just srfEmissData, but for clarity it might
  // help to see a srfEmissOutput along a srfEmissInput in functions signatures
  using srfEmissOutput = srfEmissData;

  /* -------------------------------------------------------------------------------------------
   */
  // Surface emissions routines

  static std::shared_ptr<AbstractRemapper> create_horiz_remapper(
      const std::shared_ptr<const AbstractGrid> &model_grid,
      const std::string &spa_data_file, const std::string &map_file,
      const bool use_iop = false);

  static std::shared_ptr<AtmosphereInput> create_srfEmiss_data_reader(
      const std::shared_ptr<AbstractRemapper> &horiz_remapper,
      const std::string &srfEmiss_data_file);

};  // struct srfEmissFunctions
}  // namespace
}  // namespace scream::mam_coupling
#endif  // SRF_EMISSION_HPP

#include "srf_emission_impl.hpp"