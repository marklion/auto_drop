
cmake_minimum_required(VERSION 3.10)
project(public)

SET(LIBRARY_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/bin)
add_compile_options(-std=gnu++14 -g)

aux_source_directory(utils/ UTILS_SRC)
add_library(ad_utils SHARED ${UTILS_SRC})
