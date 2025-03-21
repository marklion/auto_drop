
cmake_minimum_required(VERSION 3.10)
project(public)

SET(LIBRARY_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/bin)
add_compile_options(-std=gnu++14 -g)

add_subdirectory(hiredis)
find_package(yaml-cpp REQUIRED)

aux_source_directory(utils/ UTILS_SRC)
aux_source_directory(event_sc/ EVENT_SC_SRC)
aux_source_directory(./ MAIN_SRC)

add_library(ad_utils SHARED ${UTILS_SRC} ${EVENT_SC_SRC})
add_executable(public_test ${MAIN_SRC})

target_link_libraries(ad_utils hiredis::hiredis yaml-cpp)
target_link_libraries(public_test ad_utils hiredis::hiredis)
