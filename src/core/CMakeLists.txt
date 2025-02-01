cmake_minimum_required(VERSION 3.10)
project(core)

SET(LIBRARY_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/bin)
add_compile_options(-std=gnu++14 -g)

find_package(yaml-cpp REQUIRED)
aux_source_directory(./ MAIN_SRC)
add_executable(core_daemon ${MAIN_SRC})

find_library(SM_LIB libsm.so ${PRJ_INTERNAL_BUILD}/lib)
find_library(AD_UTILS_LIB libad_utils.so ${PRJ_INTERNAL_BUILD}/lib)
find_library(AD_RPC_LIB libad_rpc.so ${PRJ_INTERNAL_BUILD}/lib)

target_link_libraries(core_daemon yaml-cpp ${SM_LIB} ${AD_UTILS_LIB} ${AD_RPC_LIB} thrift)