cmake_minimum_required(VERSION 3.1)
set (CMAKE_CXX_STANDARD 11)

project(
    nhvd
)

# use fPIC for all libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(hardware-video-decoder)
add_subdirectory(minimal-latency-streaming-protocol)

# this is our main target
add_library(nhvd nhvd.c)
target_include_directories(nhvd PRIVATE hardware-video-decoder)
target_include_directories(nhvd PRIVATE minimal-latency-streaming-protocol)

# note that nhvd depends through hvd on FFMpeg avcodec and avutil, at least 3.4 version
target_link_libraries(nhvd hvd mlsp)

add_executable(nhvd-frame-example examples/nhvd_frame_example.c)
target_link_libraries(nhvd-frame-example nhvd)

add_executable(nhvd-frame-multi-example examples/nhvd_frame_multi_example.c)
target_link_libraries(nhvd-frame-multi-example nhvd)


