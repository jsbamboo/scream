if (COMP_NAME STREQUAL gptl)
  string(APPEND CFLAGS " -DHAVE_SLASHPROC")
endif()
string(APPEND CPPDEFS " -DLINUX")
if (DEBUG)
  string(APPEND FFLAGS " -check all -ftrapuv -init=snan")
endif()
set(PIO_FILESYSTEM_HINTS "lustre")
set(BLA_VENDOR Intel10_64_dyn)
string(APPEND SLIBS " -lpmi ")
if (MPILIB STREQUAL impi)
  set(MPICC "mpiicc")
  set(MPICXX "mpiicpc")
  set(MPIFC "mpiifort")
endif()
