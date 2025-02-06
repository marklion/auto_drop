
cmake_minimum_required(VERSION 3.10)
project(driver)

SET(LIBRARY_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/bin)
add_compile_options(-std=gnu++14 -g)

aux_source_directory(lib/ LIB_SRC)

add_library(ad_driver SHARED ${LIB_SRC} ${EVENT_SC_SRC})
add_executable(mock_driver mock_driver.cpp)

find_library(AD_UTILS_LIB libad_utils.so ${PRJ_INTERNAL_BUILD}/lib)
find_library(AD_RPC_LIB libad_rpc.so ${PRJ_INTERNAL_BUILD}/lib)

target_link_libraries(mock_driver ad_driver ${AD_UTILS_LIB} ${AD_RPC_LIB} thrift)
