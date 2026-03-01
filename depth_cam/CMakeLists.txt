cmake_minimum_required(VERSION 3.16)
project(footscan CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(realsense2 REQUIRED)
find_package(CURL REQUIRED)

add_executable(footscan
  src/main.cpp
)
target_include_directories(footscan PRIVATE include)
target_link_libraries(footscan PRIVATE realsense2 CURL::libcurl bluetooth)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(footscan PRIVATE -O3 -march=native -Wall -Wextra -Wno-unknown-pragmas)
endif()

