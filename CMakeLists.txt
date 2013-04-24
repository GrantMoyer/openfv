cmake_minimum_required (VERSION 2.6)

project (sa-ptv)

set (PROJECT_VERSION_MAJOR 1)
set (PROJECT_VERSION_MINOR 0)

find_package (OpenCV REQUIRED)

# include directories
include_directories ("${PROJECT_SOURCE_DIR}/lib")
include_directories ("${PROJECT_SOURCE_DIR}/include")

# subdirectories
add_subdirectory (lib)

add_executable (sa-ptv sa-ptv.cpp)

set (CERES_LIBS ceres glog gflags cholmod protobuf cxsparse gomp)

target_link_libraries (sa-ptv ${OpenCV_LIBS} lib ${CERES_LIBS})