include(${CMAKE_CURRENT_LIST_DIR}/ruby.cmake)
set(CMAKE_EXE_LINKER_FLAGS "-L/usr/tce/packages/mkl/mkl-2022.1.0/lib/intel64/ -qmkl" CACHE STRING "" FORCE)
set(PYTHON_LIBRARIES "/usr/lib64/libpython3.9.so.1.0" CACHE STRING "" FORCE)
option (SCREAM_ENABLE_ML_CORRECTION "Whether to enable ML correction parametrization" ON)
set(HDF5_DISABLE_VERSION_CHECK 1 CACHE STRING "" FORCE)
execute_process(COMMAND source /usr/WS1/climdat/python_venv/3.9.2/screamML/bin/activate)
