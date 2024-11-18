# Try and find a gcc/g++ install
find_program(GNU_C_COMPILER cc | gcc REQUIRED)
find_program(GNU_CXX_COMPILER c++ | g++ REQUIRED)

# Debug messages
message(DEBUG "GNU_C_COMPILER = ${GNU_C_COMPILER}")
message(DEBUG "GNU_CXX_COMPILER = ${GNU_CXX_COMPILER}")

# Set compilers to GNU (need to cache for VSCode tools to work correctly)
set(CMAKE_C_COMPILER ${GNU_C_COMPILER} CACHE STRING "Set C compiler")
set(CMAKE_CXX_COMPILER ${GNU_CXX_COMPILER} CACHE STRING "Set C++ compiler")
