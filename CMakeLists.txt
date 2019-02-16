cmake_minimum_required(VERSION 3.0)

project(
    nhvd
)

# use fPIC for all libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# compile dependencies as static libraries
add_library(hvd hardware-video-decoder/hvd.c)
add_library(mlsp minimal-latency-streaming-protocol/mlsp.c)

# this is our main target
add_library(nhvd SHARED nhvd.cpp)
target_include_directories(nhvd PRIVATE hardware-video-decoder)
target_include_directories(nhvd PRIVATE minimal-latency-streaming-protocol)

# note that nhvd depends through hvd on FFMpeg avcodec and avutil, at least 3.4 version
target_link_libraries(nhvd hvd mlsp avcodec avutil)


