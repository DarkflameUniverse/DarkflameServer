cmake_minimum_required(VERSION 3.18)
project(Darkflame)
include(CTest)

set(CMAKE_CXX_STANDARD 20)

# Compiler flags:
# Disabled deprecated warnings as the MySQL includes have deprecated code in them.
# Disabled misleading indentation as DL_LinkedList from RakNet has a weird indent.
# Disabled no-register
# Disabled unknown pragmas because Linux doesn't understand Windows pragmas.
if(UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wuninitialized -fPIC -Werror")
	add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0 _GLIBCXX_USE_CXX17_ABI=0)

	if(NOT APPLE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -lstdc++fs")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -O2 -fPIC")
endif()

add_executable(hello "test.cc")
