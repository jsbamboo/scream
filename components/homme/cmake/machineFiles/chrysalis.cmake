# CMake initial cache file
#
EXECUTE_PROCESS(COMMAND which mpiifort RESULT_VARIABLE MPIIFORT_RESULT)
EXECUTE_PROCESS(COMMAND which ifort    RESULT_VARIABLE IFORT_RESULT)
IF (${MPIIFORT_RESULT} EQUAL 0 AND ${IFORT_RESULT} EQUAL 0)
  MESSAGE(STATUS "Found Intel compiler and Intel MPI: building with mpiifort/mpiicc/mpiicpc")
  SET (CMAKE_Fortran_COMPILER mpiifort CACHE FILEPATH "")
  SET (CMAKE_C_COMPILER       mpiicc   CACHE FILEPATH "")
  SET (CMAKE_CXX_COMPILER     mpiicpc  CACHE FILEPATH "")
ELSE()
  MESSAGE(STATUS "Did not detect ifort or mpiifort: building with mpif90/mpicc/mpicxx")
  SET (CMAKE_Fortran_COMPILER mpif90   CACHE FILEPATH "")
  SET (CMAKE_C_COMPILER       mpicc    CACHE FILEPATH "")
  SET (CMAKE_CXX_COMPILER     mpicxx   CACHE FILEPATH "")
ENDIF()

SET (USE_MPIEXEC "srun" CACHE STRING "")
SET (USE_MPI_OPTIONS "-K --cpu_bind=cores" CACHE STRING "")

# Set kokkos arch, to get correct avx flags
SET (Kokkos_ARCH_ZEN2 ON CACHE BOOL "")

EXECUTE_PROCESS(COMMAND pnetcdf-config --prefix
  RESULT_VARIABLE PNCCONFIG_RESULT
  OUTPUT_VARIABLE PNCCONFIG_OUTPUT
  ERROR_VARIABLE  PNCCONFIG_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
SET (PnetCDF_PATH "${PNCCONFIG_OUTPUT}" CACHE STRING "")

EXECUTE_PROCESS(COMMAND nf-config --prefix
  RESULT_VARIABLE NFCONFIG_RESULT
  OUTPUT_VARIABLE NFCONFIG_OUTPUT
  ERROR_VARIABLE  NFCONFIG_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
SET (NetCDF_Fortran_PATH "${NFCONFIG_OUTPUT}" CACHE STRING "")

EXECUTE_PROCESS(COMMAND nc-config --prefix
  RESULT_VARIABLE NCCONFIG_RESULT
  OUTPUT_VARIABLE NCCONFIG_OUTPUT
  ERROR_VARIABLE  NCCONFIG_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
SET (NetCDF_C_PATH "${NCCONFIG_OUTPUT}" CACHE STRING "")

SET (USE_QUEUING FALSE CACHE BOOL "")
# for standalone HOMME builds:
SET (CPRNC_DIR /lcrc/group/e3sm/tools/cprnc CACHE FILEPATH "")

IF (${IFORT_RESULT} EQUAL 0)
  SET (HOMME_USE_MKL "TRUE" CACHE FILEPATH "")
  # turn on additional intel compiler flags
  SET (ADD_Fortran_FLAGS "-traceback" CACHE STRING "")
  SET (ADD_C_FLAGS       "-traceback" CACHE STRING "")
  SET (ADD_CXX_FLAGS     "-traceback" CACHE STRING "")
ELSE()
  SET (MKLROOT $ENV{MKLROOT} CACHE FILEPATH "")
  SET (HOMME_FIND_BLASLAPACK TRUE CACHE BOOL "")
ENDIF()

