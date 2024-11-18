# Try and find a clang-16 install, falling back to a generic clang install otherwise
find_program(CLANG_C_COMPILER clang-16 | clang REQUIRED)
find_program(CLANG_CXX_COMPILER clang++-16 | clang++ REQUIRED)

# Debug messages
message(DEBUG "CLANG_C_COMPILER = ${CLANG_C_COMPILER}")
message(DEBUG "CLANG_CXX_COMPILER = ${CLANG_CXX_COMPILER}")

# Set compilers to clang (need to cache for VSCode tools to work correctly)
set(CMAKE_C_COMPILER ${CLANG_C_COMPILER} CACHE STRING "Set C compiler")
set(CMAKE_CXX_COMPILER ${CLANG_CXX_COMPILER} CACHE STRING "Set C++ compiler")

# Set linker to lld
add_link_options("-fuse-ld=lld")
