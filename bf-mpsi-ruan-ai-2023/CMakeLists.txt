cmake_minimum_required(VERSION 3.14)
project(ruan_mpsi LANGUAGES CXX)

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

file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(ruan_mpsi ${SOURCES})

find_path(NTL_INCLUDE_DIR NTL/ZZ.h)
find_library(NTL_LIBRARY ntl)

if(NTL_INCLUDE_DIR AND NTL_LIBRARY)
	message(STATUS "Found NTL: ${NTL_LIBRARY}")
else()
	message(FATAL_ERROR "Could not find NTL.")
endif()

target_include_directories(ruan_mpsi PRIVATE ${CMAKE_SOURCE_DIR}/src ${NTL_INCLUDE_DIR})

target_link_libraries(ruan_mpsi PRIVATE ${NTL_LIBRARY})

install(TARGETS ruan_mpsi RUNTIME DESTINATION bin)