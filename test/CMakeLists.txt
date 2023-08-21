cmake_minimum_required(VERSION 3.9)

project(disruptor_test VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_FLAGS_RELEASE "-Wall -O2")
set(CMAKE_VERBOSE_MAKEFILE on) 

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")

include_directories(${CMAKE_CURRENT_LIST_DIR}/../)

#add_executable(disruptor_benchmark disruptor_benchmark.cpp)
#target_link_libraries(disruptor_benchmark pthread )

add_executable(disruptor_unittest disruptor_unittest.cpp)
target_link_libraries(disruptor_unittest pthread )
