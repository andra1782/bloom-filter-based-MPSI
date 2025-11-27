cmake_minimum_required(VERSION 3.14)
project(BF_MPSI_Baseline)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/xxhash.h")
    message(STATUS "Downloading xxhash.h...")
    file(DOWNLOAD 
        https://raw.githubusercontent.com/Cyan4973/xxHash/v0.8.2/xxhash.h
        ${CMAKE_SOURCE_DIR}/src/xxhash.h
        SHOW_PROGRESS
    )
endif()

add_executable(bf_mpsi_baseline
    src/main.cpp
    src/bloom_filter.cpp
    src/bloom_filter.hpp
    src/xxhash.h 
)

target_include_directories(bf_mpsi_baseline PRIVATE src)