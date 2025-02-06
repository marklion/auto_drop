cmake_minimum_required(VERSION 3.10)
project(core)

SET(LIBRARY_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/bin)
add_compile_options(-std=gnu++14 -g)

aux_source_directory(rpc_imp_lib/ LIB_SRC)

find_package(yaml-cpp REQUIRED)
add_executable(core_daemon main.cpp)
add_executable(sm_daemon sm_main.cpp)
add_library(ad_core ${LIB_SRC})

find_library(SM_LIB libsm.so ${PRJ_INTERNAL_BUILD}/lib)
find_library(AD_UTILS_LIB libad_utils.so ${PRJ_INTERNAL_BUILD}/lib)
find_library(AD_RPC_LIB libad_rpc.so ${PRJ_INTERNAL_BUILD}/lib)

target_link_libraries(ad_core yaml-cpp ${SM_LIB} ${AD_UTILS_LIB} ${AD_RPC_LIB} thrift)
target_link_libraries(core_daemon ad_core yaml-cpp ${SM_LIB} ${AD_UTILS_LIB} ${AD_RPC_LIB} thrift)
target_link_libraries(sm_daemon ad_core yaml-cpp ${SM_LIB} ${AD_UTILS_LIB} ${AD_RPC_LIB} thrift)