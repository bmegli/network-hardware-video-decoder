cmake_minimum_required(VERSION 3.1)
set (CMAKE_CXX_STANDARD 11)

project(
    nhvd
)

# use fPIC for all libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# compile dependencies as static libraries
add_library(hvd hardware-video-decoder/hvd.c)
add_library(mlsp minimal-latency-streaming-protocol/mlsp.c)
add_library(hdu hardware-depth-unprojector/hdu.c)

# this is our main target
add_library(nhvd SHARED nhvd.cpp)
target_include_directories(nhvd PRIVATE hardware-video-decoder)
target_include_directories(nhvd PRIVATE minimal-latency-streaming-protocol)
target_include_directories(nhvd PRIVATE hardware-depth-unprojector)

# note that nhvd depends through hvd on FFMpeg avcodec and avutil, at least 3.4 version
target_link_libraries(nhvd hvd mlsp hdu avcodec avutil)

add_executable(nhvd-frame-example examples/nhvd_frame_example.cpp)
target_link_libraries(nhvd-frame-example nhvd hdu)

add_executable(nhvd-cloud-example examples/nhvd_cloud_example.cpp)
target_link_libraries(nhvd-cloud-example nhvd hdu)

