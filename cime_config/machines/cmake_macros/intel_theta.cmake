string(APPEND CONFIG_ARGS " --host=cray")
string(APPEND CPPDEFS " -DARCH_MIC_KNL")
string(APPEND FFLAGS " -fp-model consistent -fimf-use-svml=true")
if (NOT DEBUG)
  string(APPEND FFLAGS " -qno-opt-dynamic-align -fp-speculation=off")
endif()
string(APPEND FFLAGS " -DHAVE_ERF_INTRINSICS")
set(SCC "icc")
set(SCXX "icpc")
set(SFC "ifort")
set(BLA_VENDOR Intel10_64_dyn)
string(APPEND SLIBS " -lpthread")
