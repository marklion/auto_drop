cmake_minimum_required(VERSION 3.10)
project(ad_rpc)

SET(LIBRARY_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/lib)
add_compile_options(-std=gnu++11 -g)
include_directories(${PROJECT_SOURCE_DIR})

find_package(yaml-cpp REQUIRED)
aux_source_directory(./gen_code/cpp API_SRC)
aux_source_directory(./ LIB_SRC)

add_library(ad_rpc SHARED ${API_SRC} ${LIB_SRC})
target_link_libraries(ad_rpc thrift yaml-cpp)