string(APPEND CMAKE_C_FLAGS " -time")
string(APPEND CMAKE_CXX_FLAGS " -time")
string(APPEND CMAKE_Fortran_FLAGS " -i4 -time -Mstack_arrays  -Mextend -byteswapio -Mflushz -Kieee -Mallocatable=03")
if (compile_threaded)
  string(APPEND CMAKE_C_FLAGS " -mp")
  string(APPEND CMAKE_CXX_FLAGS " -mp")
  string(APPEND CMAKE_Fortran_FLAGS " -mp")
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -mp")
endif()
string(APPEND CPPDEFS " -DFORTRANUNDERSCORE -DNO_SHR_VMATH -DNO_R16 -DCPRPGI")
string(APPEND CMAKE_Fortran_FLAGS_DEBUG " -O0 -g -Mbounds")
string(APPEND CMAKE_C_FLAGS_DEBUG " -O0 -g")
string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O0 -g")
string(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG " -g")
if (COMP_NAME STREQUAL datm)
  string(APPEND CMAKE_Fortran_FLAGS " -Mnovect")
endif()
if (COMP_NAME STREQUAL dlnd)
  string(APPEND CMAKE_Fortran_FLAGS " -Mnovect")
endif()
if (COMP_NAME STREQUAL drof)
  string(APPEND CMAKE_Fortran_FLAGS " -Mnovect")
endif()
if (COMP_NAME STREQUAL dwav)
  string(APPEND CMAKE_Fortran_FLAGS " -Mnovect")
endif()
if (COMP_NAME STREQUAL dice)
  string(APPEND CMAKE_Fortran_FLAGS " -Mnovect")
endif()
if (COMP_NAME STREQUAL docn)
  string(APPEND CMAKE_Fortran_FLAGS " -Mnovect")
endif()
if (COMP_NAME STREQUAL gptl)
  string(APPEND CPPDEFS " -DHAVE_SLASHPROC")
endif()
string(APPEND CMAKE_Fortran_FORMAT_FIXED_FLAG " -Mfixed")
string(APPEND CMAKE_Fortran_FORMAT_FREE_FLAG " -Mfree")
string(APPEND CMAKE_EXE_LINKER_FLAGS " -time -Wl,--allow-multiple-definition")
set(MPICC "mpicc")
set(MPICXX "mpicxx")
set(MPIFC "mpif90")
set(SCC "pgcc")
set(SCXX "pgc++")
set(SFC "pgf95")

# PGI has to link fortran mains with fortran
set(E3SM_LINK_WITH_FORTRAN "TRUE")
