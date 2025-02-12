cmake_minimum_required(VERSION 3.10)
project(cli)

SET(LIBRARY_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/bin)
add_compile_options(-std=gnu++14 -g)

find_package(yaml-cpp REQUIRED)
find_package(Lua REQUIRED)

aux_source_directory(./ MAIN_SRC)
aux_source_directory(./detail DETAIL_SRC)

add_executable(ad_cli ${MAIN_SRC} ${DETAIL_SRC})

find_library(AD_UTILS_LIB libad_utils.so ${PRJ_INTERNAL_BUILD}/lib)
find_library(AD_RPC_LIB libad_rpc.so ${PRJ_INTERNAL_BUILD}/lib)
find_library(AD_CORE_LIB libad_core.so ${PRJ_INTERNAL_BUILD}/lib)

target_link_libraries(ad_cli ${AD_UTILS_LIB} ${AD_CORE_LIB} ${AD_RPC_LIB} ${LUA_LIBRARIES} thrift yaml-cpp)
