cmake_minimum_required(VERSION 3.10)
project(sm)

SET(LIBRARY_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/bin)
add_compile_options(-std=gnu++14 -g)
find_package(yaml-cpp REQUIRED)
find_package(Lua REQUIRED)

aux_source_directory(lib/ LIB_SRC)
add_library(sm SHARED ${LIB_SRC})

aux_source_directory(./ MAIN_SRC)

find_library(AD_UTILS_LIB libad_utils.so ${PRJ_INTERNAL_BUILD}/lib)

target_link_libraries(sm yaml-cpp ${AD_UTILS_LIB} ${LUA_LIBRARIES})