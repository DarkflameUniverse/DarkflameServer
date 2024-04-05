# Try and find a gcc/g++ install, falling back to a generic clang install otherwise
find_program(GNU_C_COMPILER gcc REQUIRED)
find_program(GNU_CXX_COMPILER g++ REQUIRED)

# Debug messages
message("GNU_C_COMPILER = ${GNU_C_COMPILER}")
message("GNU_CXX_COMPILER = ${GNU_C_COMPILER}")

# Set compilers to clang
set(CMAKE_C_COMPILER ${GNU_C_COMPILER})
set(CMAKE_CXX_COMPILER ${GNU_CXX_COMPILER})
