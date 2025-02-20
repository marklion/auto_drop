cmake_minimum_required(VERSION 3.10)
project(rd_driver)

SET(LIBRARY_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PRJ_INTERNAL_BUILD}/bin)
add_compile_options(-std=gnu++14 -g)
aux_source_directory(./ MAIN_SRC)
add_executable(rd_driver ${MAIN_SRC})

find_package(PCL 1.10 REQUIRED)
include_directories(${PCL_INCLUDE_DIRS})
link_directories(${PCL_LIBRARY_DIRS})
add_definitions(${PCL_DEFINITIONS})

add_subdirectory(${PROJECT_SOURCE_DIR}/rs_driver)
find_package(rs_driver REQUIRED)
include_directories(${rs_driver_INCLUDE_DIRS})

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

find_library(AD_DRIVER_LIB libad_driver.so ${PRJ_INTERNAL_BUILD}/lib)
find_library(AD_UTILS_LIB libad_utils.so ${PRJ_INTERNAL_BUILD}/lib)
find_library(AD_RPC_LIB libad_rpc.so ${PRJ_INTERNAL_BUILD}/lib)

target_link_libraries(rd_driver ${AD_DRIVER_LIB} ${rs_driver_LIBRARIES} ${PCL_LIBRARIES} ${VTK_LIBRARIES} ${AD_UTILS_LIB} ${AD_RPC_LIB} thrift)