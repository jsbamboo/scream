#include <catch2/catch.hpp>

#include "share/grid/remap/horizontal_remap_utility.hpp"

#include "share/io/scorpio_input.hpp"
#include "share/io/scream_output_manager.hpp"

#include "share/grid/mesh_free_grids_manager.hpp"
#include "share/grid/point_grid.hpp"

#include "share/field/field_manager.hpp"

#include "share/util/scream_setup_random_test.hpp"

#include "ekat/util/ekat_units.hpp"
#include "ekat/util/ekat_test_utils.hpp"
#include "ekat/ekat_parse_yaml_file.hpp"
namespace {

using namespace scream;

std::shared_ptr<GridsManager> get_test_gm(const ekat::Comm& comm, const Int num_gcols, const Int num_levs);

std::shared_ptr<FieldManager> get_test_fm(std::shared_ptr<const AbstractGrid> grid);

AtmosphereInput setup_data_input(std::shared_ptr<const FieldManager> field_manager);

ekat::ParameterList setup_output_manager_params();

GSMap get_test_map(const ekat::Comm& comm, const view_1d<gid_type>& dofs_gids, const gid_type min_dof);

void run(std::mt19937_64& engine, const ekat::Comm& comm, const gid_type src_min_dof)
{
  using IPDF = std::uniform_int_distribution<Int>;
  using RPDF = std::uniform_real_distribution<Real>;
  constexpr Real tol  = std::numeric_limits<Real>::epsilon()*10;
// Test configuration
  int num_src_cols = 86;  // Number of columns from source data
  int num_tgt_cols = 21;  // Number of columns on target grid
  int num_levels   = 2;   // Dummy variable needed for initialization of a grid.

  // Construct a target grid for remapped data
  auto gm_tgt           = get_test_gm(comm, num_tgt_cols, num_levels);
  auto grid_tgt         = gm_tgt->get_grid("Point Grid");
  auto num_loc_tgt_cols = grid_tgt->get_num_local_dofs();
  auto tgt_min_dof      = grid_tgt->get_global_min_dof_gid();
  auto dofs_gids        = grid_tgt->get_dofs_gids();

// Step 1: Generate a random remapping (source col, target col, weight) and random source data.
//         Store these in local 1D views and create a remap file.
// Note, randomization is done entirely on ROOT so that we only have one version of the remap
// data.

  // When we create random source data we again only do this on root so that we
  // only have one set of data.
  std::vector<Real> vec_src_data(num_src_cols);
  if (comm.am_i_root()) {
    RPDF pdf_src_data(-1,1);               // Randomizer for generating source data to be mapped from.
    for (int ii=0; ii<num_src_cols; ii++) {
      vec_src_data[ii] = pdf_src_data(engine);
    }
  } // if am_i_root
  comm.barrier();
  comm.broadcast(vec_src_data.data(),vec_src_data.size(),comm.root_rank());
  comm.barrier();
  view_1d_host<Real>     source_remap_data_h(vec_src_data.data(),num_src_cols);
  auto source_remap_data = Kokkos::create_mirror_view(source_remap_data_h);
  Kokkos::deep_copy(source_remap_data, source_remap_data_h);

  // Because we are building the remap  on the fly, use vectors to set up the remap data
  // We can also construct a baseline set of remapped data for testing.
  Int                     n_s_local=0;      // Total number of remapping src->tgt pairs.
  std::vector<Int>        vec_src_col, vec_tgt_col;
  std::vector<Real>       vec_wgt;
  std::map<gid_type,Real> y_baseline;
  std::map<gid_type,Int>  num_src_to_tgt_cols;  // A record of how many source columns map to a specific target.  Will be used later for testing segments.
  IPDF pdf_map_src_num(2,num_src_cols); // Randomizer for number of source columns mapping to a target column
  IPDF pdf_map_src(0,num_src_cols-1);   // Randomizer for which source columns map to a target column
  RPDF pdf_map_wgt(0,1);                // Randomizer for mapping weights
  // Cycle through all tgt_cols and set up a remap
  for (int tcol=0; tcol<num_loc_tgt_cols; tcol++) {
    Int nmap = pdf_map_src_num(engine); // Number of source columns that will map to this target column
    n_s_local += nmap;
    num_src_to_tgt_cols.emplace(dofs_gids(tcol),nmap);
    // We need to treat random weights specially, to ensure sum(wgt) = 1;
    std::vector<Real> temp_wgt;
    std::vector<Int>  temp_src;  
    Real wgt_sum = 0.0;
    Real y_sum   = 0.0;
    for (int ii=0; ii<nmap; ii++) {
      Int src_idx = pdf_map_src(engine);
      vec_src_col.push_back(src_idx+src_min_dof);
      vec_tgt_col.push_back(dofs_gids(tcol)-tgt_min_dof+src_min_dof);
      Real wgt = pdf_map_wgt(engine);
      wgt_sum += wgt;
      temp_wgt.push_back(wgt);
      y_sum += source_remap_data_h(src_idx) * wgt;
    }
    y_sum /= wgt_sum;  // account for the normalization of the weights
    y_baseline.emplace(dofs_gids(tcol),y_sum);
    // Normalize weights
    for (int ii=0; ii<nmap; ii++) {
      temp_wgt[ii] /= wgt_sum;
    }
    // Append normalized weights to vector of overall weights
    vec_wgt.insert(vec_wgt.end(), temp_wgt.begin(), temp_wgt.end());
  }
  // Gather data from all ranks
  Int n_s;
  Int* n_s_all_ranks = new Int[comm.size()]; 
  comm.all_gather(&n_s_local,n_s_all_ranks,1);
  comm.all_reduce(&n_s_local,&n_s,1,MPI_SUM);
  Int n_s_offset = 0; // The offset for this rank, used for writing the remap to output
  for (int ii=0; ii<comm.rank(); ii++) {
    n_s_offset += n_s_all_ranks[ii];
  }
  // Now copy these vectors to views 
  view_1d_host<Int>  map_src_cols_h(vec_src_col.data(),vec_wgt.size());
  view_1d_host<Int>  map_tgt_cols_h(vec_tgt_col.data(),vec_wgt.size());
  view_1d_host<Real> map_wgts_h(vec_wgt.data(),vec_wgt.size());
  auto map_src_cols = Kokkos::create_mirror_view(map_src_cols_h);
  auto map_tgt_cols = Kokkos::create_mirror_view(map_tgt_cols_h);
  auto map_wgts     = Kokkos::create_mirror_view(map_wgts_h    );
  Kokkos::deep_copy(map_src_cols, map_src_cols_h);
  Kokkos::deep_copy(map_tgt_cols, map_tgt_cols_h);
  Kokkos::deep_copy(map_wgts    , map_wgts_h    );

  // Step 2: Use GSMap with remap values and source data, and compare against baseline.
  std::map<gid_type,Real> y_from_views;
  GSMap remap_from_views;
  remap_from_views.source_min_dof = src_min_dof;
  remap_from_views.m_min_dof      = tgt_min_dof;
  for (int iseg=0; iseg<num_loc_tgt_cols; iseg++) {
    gid_type seg_dof = dofs_gids(iseg);
    RemapSegment seg(seg_dof,num_src_to_tgt_cols.at(seg_dof));
    seg.m_dof = seg_dof;
    auto source_dofs_h = Kokkos::create_mirror_view(seg.source_dofs);
    auto weights_h     = Kokkos::create_mirror_view(seg.weights);
    int idx = 0;
    for (int ii=0; ii<map_src_cols_h.extent(0); ii++) {
      if (map_tgt_cols_h(ii)-src_min_dof==seg.m_dof-tgt_min_dof) {
        source_dofs_h(idx) = map_src_cols_h(ii);
        weights_h(idx)     = map_wgts_h(ii);
        idx ++;
      }
    }
    remap_from_views.add_segment(seg);
  }
  remap_from_views.check();
  remap_from_views.set_unique_source_dofs();
  // We need to extract just that source data that is related to this ranks need.
  auto unique_dofs_from_views = remap_from_views.get_unique_dofs();
  view_1d<Real> x_data_from_views("",unique_dofs_from_views.size());
  auto x_data_from_views_h = Kokkos::create_mirror_view(x_data_from_views);
  for (int ii=0; ii<unique_dofs_from_views.size(); ii++) {
    x_data_from_views_h(ii) = source_remap_data_h(unique_dofs_from_views[ii]-src_min_dof);
  }
  Kokkos::deep_copy(x_data_from_views,x_data_from_views_h);
  // Apply the remap
  view_1d<Real> y_data_from_views("",num_loc_tgt_cols);
  remap_from_views.apply_remap(x_data_from_views,y_data_from_views);
  // The remapped values should match the y_baseline
  auto y_data_from_views_h = Kokkos::create_mirror_view(y_data_from_views);
  Kokkos::deep_copy(y_data_from_views_h,y_data_from_views);
  for (int ii=0; ii<num_loc_tgt_cols; ii++) {
    gid_type dof = dofs_gids(ii);
    auto y_base = y_baseline[dof];
    REQUIRE(std::abs(y_data_from_views_h(ii)-y_base)<tol);
  } 
  // Step 3: Write source data and remap data to a file.
  std::string filename = "horizontal_remap_test_data_" + std::to_string(comm.size()) + "_ranks_" + std::to_string(src_min_dof) + "_case.nc";
  // Initialize the pio_subsystem for this test:
  MPI_Fint fcomm = MPI_Comm_c2f(comm.mpi_comm()); 
  scorpio::eam_init_pio_subsystem(fcomm);   // Gather the initial PIO subsystem data creater by component coupler
  // Register the output file
  scorpio::register_file(filename,scorpio::Write);
  //   - Dimensions
  scorpio::register_dimension(filename,"n_s","n_s",n_s);
  scorpio::register_dimension(filename,"n_a","n_a",num_src_cols);
  scorpio::register_dimension(filename,"n_b","n_b",num_tgt_cols);
  scorpio::register_dimension(filename,"ncol","ncol",num_src_cols);
  //   - Variables
  std::string remap_decomp_tag_r = "n_s_real";
  std::string data_decomp_tag_r  = "data_real";
  std::string remap_decomp_tag_i = "n_s_int";
  std::vector<std::string> vec_of_remap_dims = {"n_s"};
  std::vector<std::string> vec_of_data_dims  = {"ncol"};
  scorpio::register_variable(filename,"col","col","unitless",1,vec_of_remap_dims,PIO_INT,remap_decomp_tag_i);
  scorpio::register_variable(filename,"row","row","unitless",1,vec_of_remap_dims,PIO_INT,remap_decomp_tag_i);
  scorpio::register_variable(filename,"S","S",    "unitless",1,vec_of_remap_dims,PIO_REAL,remap_decomp_tag_r);
  scorpio::register_variable(filename,"src_data","src_data", "m",1,vec_of_data_dims,PIO_REAL,data_decomp_tag_r);
  //   - DOFs
  std::vector<int64_t> var_dof(n_s_local);
  std::iota(var_dof.begin(),var_dof.end(),n_s_offset);
  scorpio::set_dof(filename,"col",var_dof.size(),var_dof.data());
  scorpio::set_dof(filename,"row",var_dof.size(),var_dof.data());
  scorpio::set_dof(filename,"S",var_dof.size(),var_dof.data());
  var_dof.resize(unique_dofs_from_views.size());
  for (int ii=0; ii<var_dof.size(); ii++) {
    var_dof[ii] = unique_dofs_from_views[ii]-src_min_dof;
  }
  scorpio::set_dof(filename,"src_data",var_dof.size(),var_dof.data());
  scorpio::eam_pio_enddef(filename);
  // Write data
  scorpio::grid_write_data_array(filename,"col",map_src_cols_h.data());
  scorpio::grid_write_data_array(filename,"row",map_tgt_cols_h.data());
  scorpio::grid_write_data_array(filename,"S",map_wgts_h.data());
  scorpio::grid_write_data_array(filename,"src_data",x_data_from_views_h.data());
  // All done writing 
  scorpio::eam_pio_closefile(filename);

  // Step 4: Load remap information and source data from the file created in step 3
  //         and use this data to test remapping from file.  Compare remapped data
  //         against the baseline. 
  GSMap remap_from_file;
  remap_from_file.set_dofs_gids(dofs_gids);
  remap_from_file.set_segments_from_file(filename,comm,dofs_gids,tgt_min_dof);
  remap_from_file.check();
  remap_from_file.set_unique_source_dofs();
  // Read source data at unique points
  scorpio::register_file(filename,scorpio::Read);
  scorpio::get_variable(filename,"src_data","src_data",1,vec_of_data_dims,PIO_REAL,data_decomp_tag_r);
  var_dof.resize(unique_dofs_from_views.size());
  for (int ii=0; ii<var_dof.size(); ii++) {
    var_dof[ii] = unique_dofs_from_views[ii]-src_min_dof;
  }
  scorpio::set_dof(filename,"src_data",var_dof.size(),var_dof.data());
  scorpio::set_decomp(filename);
  view_1d<Real> x_data_from_file("",unique_dofs_from_views.size());
  auto x_data_from_file_h = Kokkos::create_mirror_view(x_data_from_file);
  scorpio::grid_read_data_array(filename,"src_data",0,x_data_from_file.data()); 
  scorpio::eam_pio_closefile(filename);
  // Apply remap using the data
  view_1d<Real> y_data_from_file("",num_loc_tgt_cols);
  remap_from_file.apply_remap(x_data_from_file,y_data_from_file);
  // The remapped values should match the y_baseline
  auto y_data_from_file_h = Kokkos::create_mirror_view(y_data_from_file);
  Kokkos::deep_copy(y_data_from_file_h,y_data_from_file);
  for (int ii=0; ii<num_loc_tgt_cols; ii++) {
    gid_type dof = dofs_gids(ii);
    auto y_base = y_baseline[dof];
    REQUIRE(std::abs(y_data_from_file_h(ii)-y_base)<tol);
  } 

  // All Done with testing
  scorpio::eam_pio_finalize();
  
} // end function run

//===============================================================================
TEST_CASE("horizontal_remap_test", "[horizontal_remap_test]"){

  using scream::Real;
  using Device = scream::DefaultDevice;

  ekat::Comm comm (MPI_COMM_WORLD);
  auto engine = scream::setup_random_test();

// Run tests of vertically dimensioned-functions for both Real and Pack,
// and for (potentially) different pack sizes
  if (comm.am_i_root()) {
    printf(" -> Testing horizontal remapping for minimum source dof = 0...");
  }
  run(engine, comm, 0);
  if (comm.am_i_root()) {
    printf("ok!\n");
  }

  if (comm.am_i_root()) {
    printf(" -> Testing horizontal remapping for minimum source dof = 1...");
  }
  run(engine, comm, 1);
  if (comm.am_i_root()) {
    printf("ok!\n");
  }

} // TEST_CASE
//===============================================================================

TEST_CASE("horizontal_remap_units", "") {

  using namespace scream;
  using IPDF = std::uniform_int_distribution<int>;
  using RPDF = std::uniform_real_distribution<Real>;
  auto engine = scream::setup_random_test();

  // Test the remap_segment structure
  {
    IPDF pdf_seg_len(2,100);
    RPDF pdf_seg_wgt(0,1);
    RemapSegment test_segment;
    Int len = pdf_seg_len(engine);
    test_segment.m_length = len-1;
    test_segment.source_dofs = view_1d<gid_type>("",len);
    test_segment.weights     = view_1d<Real>("",len);
    test_segment.source_idx  = view_1d<Int>("",len);
    ekat::genRandArray(test_segment.weights,engine,pdf_seg_wgt);
    Real wgt_normalize = 0;
    auto weights_h = Kokkos::create_mirror_view(test_segment.weights);
    Kokkos::deep_copy(weights_h,test_segment.weights);
    for (int ii=0; ii<len; ii++) {
      wgt_normalize += weights_h(ii);
    }
    for (int ii=0; ii<len; ii++) {
      weights_h(ii) /= wgt_normalize;
    };
    Kokkos::deep_copy(test_segment.weights,weights_h);
    // Checking this segment should fail because the weight view is a different length than
    // the reported length for the segment.
    REQUIRE_THROWS(test_segment.check());
    // Now switch to passing by bumping up the length
    test_segment.m_length = len;
    test_segment.check();
    // Make it fail again by adjusting one of the weights so that they no longer add to 1.0
    weights_h(0) += 0.1;
    Kokkos::deep_copy(test_segment.weights,weights_h);
    REQUIRE_THROWS(test_segment.check());
    weights_h(0) -= 0.2;
    Kokkos::deep_copy(test_segment.weights,weights_h);
    REQUIRE_THROWS(test_segment.check());
  }


  // Create a map with two remap segments for some simple map structure testing.
  {
    // Create a GSMap to test
    GSMap test_map;
    test_map.m_min_dof = 1;
    test_map.source_min_dof = 1;
    // Create a remap segment
    RemapSegment test_seg;
    IPDF pdf_seg_len(2,100);
    RPDF pdf_seg_wgt(0,1);
    Int len = pdf_seg_len(engine);
    test_seg.m_length = len;
    test_seg.m_dof    = 1;
    test_seg.weights     = view_1d<Real>("",len);
    test_seg.source_dofs = view_1d<gid_type>("",len);
    test_seg.source_idx  = view_1d<Int>("",len);
    ekat::genRandArray(test_seg.weights,engine,pdf_seg_wgt);
    Real wgt_normalize = 0;
    auto weights_h = Kokkos::create_mirror_view(test_seg.weights);
    Kokkos::deep_copy(weights_h,test_seg.weights);
    for (int ii=0; ii<len; ii++) {
      wgt_normalize += weights_h(ii);
    }
    for (int ii=0; ii<len; ii++) {
      weights_h(ii) /= wgt_normalize;
    };
    Kokkos::deep_copy(test_seg.weights,weights_h);
    // Add segment to map
    test_map.add_segment(test_seg);
    test_map.check();
    REQUIRE(test_map.get_num_of_segs()==1);
    auto check_seg = test_map.get_segment(1);
    REQUIRE(check_seg.m_length==len);
    weights_h = Kokkos::create_mirror_view(check_seg.weights);
    Kokkos::deep_copy(weights_h,check_seg.weights);
    weights_h(0) = weights_h(0)/2.0;
    Real new_wgt = weights_h(0);
    Kokkos::deep_copy(check_seg.weights,weights_h);
    REQUIRE_THROWS(test_map.check());
    REQUIRE_THROWS(test_map.get_segment(2));
    // Create a new segment that has the same DOF as the first segment
    RemapSegment new_seg;
    new_seg.m_length = 1;
    new_seg.m_dof    = 1;
    new_seg.weights     = view_1d<Real>("",1);
    new_seg.source_dofs = view_1d<gid_type>("",1);
    new_seg.source_idx  = view_1d<Int>("",1);
    weights_h = Kokkos::create_mirror_view(new_seg.weights);
    weights_h(0)  = new_wgt;
    Kokkos::deep_copy(new_seg.weights,weights_h);
    test_map.add_segment(new_seg);
    // Because the new segment is the same DOF, add_segment should have
    // appended it to the first segment.
    REQUIRE(test_map.get_num_of_segs()==1);
    test_map.check();
    check_seg = test_map.get_segment(1);
    REQUIRE(check_seg.m_length==(len+1));
    // Create a completely new segment for a different DOF and check
    new_seg.m_dof = 2;
    weights_h(0) = 1.0;
    Kokkos::deep_copy(new_seg.weights,weights_h);
    test_map.add_segment(new_seg);
    REQUIRE(test_map.get_num_of_segs()==2);
    test_map.check();
    check_seg = test_map.get_segment(2);
    REQUIRE(check_seg.m_length==(1));
  }

  // Test retrieving unique columns from a full map 
  {
    // Create a GSMap to test
    GSMap test_map;
    // Create a set of segments each with overlapping source dofs
    IPDF pdf_seg_len(4,10),
         pdf_num_seg(5,15);
    Int num_of_segs = pdf_num_seg(engine);
    gid_type seg_start = 0;
    Int seg_len;
    for (int iseg=0; iseg<num_of_segs; iseg++) {
      seg_len = pdf_seg_len(engine);
      RemapSegment seg(iseg,seg_len);
      Kokkos::parallel_for("", seg_len, KOKKOS_LAMBDA (const int& ii) {
        seg.source_dofs(ii) = seg_start + ii;
      });
      test_map.add_segment(seg);
      seg_start += seg_len-2;
    }
    // All of the segments combined should overlap by 2 on either side.  The set of unique
    // columns for all segments should be 0,1,...,seg_start+seg_len-1.  So we check that.
    const gid_type seg_end = seg_start + 2;
    test_map.set_unique_source_dofs();
    const auto& unique_dofs_vec = test_map.get_unique_dofs();
    view_1d<gid_type> unique_dofs("",unique_dofs_vec.size());
    auto unique_dofs_h = Kokkos::create_mirror_view(unique_dofs);
    Kokkos::deep_copy(unique_dofs_h,unique_dofs);
    REQUIRE(unique_dofs_h.extent(0) == seg_end);
    for (int ii=0; ii<unique_dofs.size(); ii++) {
      unique_dofs_h(ii) = unique_dofs_vec[ii];
    }
    Kokkos::deep_copy(unique_dofs,unique_dofs_h);
    Kokkos::parallel_for("", seg_end, KOKKOS_LAMBDA (const int& ii) {
      EKAT_KERNEL_REQUIRE(unique_dofs(ii) == ii);
    });
    
    

  }
} // TEST_CASE horizontal remap
/*===================================================================================================*/
std::shared_ptr<FieldManager> get_test_fm(std::shared_ptr<const AbstractGrid> grid)
{
  using namespace ekat::units;
  using namespace ShortFieldTagsNames;
  using FL = FieldLayout;
  using FR = FieldRequest;

  // Create a fm
  auto fm = std::make_shared<FieldManager>(grid);

  const int num_lcols = grid->get_num_local_dofs();
  const int num_levs = grid->get_num_vertical_levels();

  // Create some fields for this fm
  std::vector<FieldTag> tag_h  = {COL};
  std::vector<FieldTag> tag_v  = {LEV};
  std::vector<FieldTag> tag_2d = {COL,LEV};

  std::vector<Int>     dims_h  = {num_lcols};
  std::vector<Int>     dims_v  = {num_levs};
  std::vector<Int>     dims_2d = {num_lcols,num_levs};

  const std::string& gn = grid->name();

  FieldIdentifier fid_ps("PS",FL{tag_h,dims_h},Pa,gn);

  // Register fields with fm
  // Make sure packsize isn't bigger than the packsize for this machine, but not so big that we end up with only 1 pack.
  fm->registration_begins();
  fm->register_field(FR{fid_ps});
  fm->registration_ends();

  // Initialize these fields
  auto f1 = fm->get_field(fid_ps);
  auto f1_host = f1.get_view<Real*,Host>(); 

  // Update timestamp
  util::TimeStamp time ({2000,1,1},{0,0,0});
  fm->init_fields_time_stamp(time);
  // Sync back to device
  f1.sync_to_dev();

  return fm;
} // end get_test_fm
/*==========================================================================================================*/
std::shared_ptr<GridsManager> get_test_gm(const ekat::Comm& comm, const Int num_gcols, const Int num_levs)
{
  ekat::ParameterList gm_params;
  gm_params.set("number_of_global_columns",num_gcols);
  gm_params.set("number_of_vertical_levels",num_levs);
  auto gm = create_mesh_free_grids_manager(comm,gm_params);
  gm->build_grids();
  return gm;
} // end get_test_gm
/*==========================================================================================================*/
AtmosphereInput setup_data_input(std::shared_ptr<const FieldManager> field_manager)
{
  std::string fname = "horiz_remap_from_file_control.yaml";
  ekat::ParameterList params("horiz_remap_test");
  REQUIRE_NOTHROW ( parse_yaml_file(fname,params) );
  auto& data_file = params.get<std::string>("Data File");
  using vos_type = std::vector<std::string>;
  ekat::ParameterList in_params("Input Parameters");
  in_params.set<std::string>("Filename",data_file);
  in_params.set<vos_type>("Field Names",{"PS"});
  AtmosphereInput data_input(in_params,field_manager);
  return data_input;
}
/*==========================================================================================================*/
ekat::ParameterList setup_output_manager_params()
{
  using vos_type = std::vector<std::string>;
  ekat::ParameterList om_params;
  om_params.set<std::string>("Casename","horizontal_remap_output");
  om_params.set<std::string>("Averaging Type","Instant");
  om_params.set<Int>("Max Snapshots Per File",12);
  om_params.set<vos_type>("Field Names",{"PS"});
  auto& om_control = om_params.sublist("Output Control");
  om_control.set<Int>("Frequency",1);
  om_control.set<std::string>("Frequency Units","Steps");
  om_control.set<bool>("Timestamp in Filename", false);
  om_control.set<bool>("MPI Ranks in Filename", false);
  om_control.set<bool>("AVG Type in Filename", false);
  om_control.set<bool>("Frequency in Filename", false);
  return om_params;
}
/*==========================================================================================================*/
GSMap get_test_map(const ekat::Comm& comm, const view_1d<gid_type>& dofs_gids, const gid_type min_dof)
{

  GSMap test_map;
  test_map.set_dofs_gids(dofs_gids);
  std::string fname = "horiz_remap_from_file_control.yaml";
  ekat::ParameterList params("horiz_remap_test");
  REQUIRE_NOTHROW ( parse_yaml_file(fname,params) );
  auto& remap_file = params.get<std::string>("Remap File");
  test_map.set_segments_from_file(remap_file,comm,dofs_gids,min_dof);
//  test_map.check();
//  test_map.set_unique_source_dofs();
  return test_map;

}
/*==========================================================================================================*/

} // end namespace
