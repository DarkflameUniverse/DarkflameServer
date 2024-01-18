cmake_minimum_required(VERSION 3.18)
project(Darkflame)
include(CTest)

# gcc -O2 -S -fdump-tree-optimized=/dev/stdout z.c

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -S -fdump-tree-optimized=/dev/stdout")

add_library(hello "z.c")
