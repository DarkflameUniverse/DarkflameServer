cmake_minimum_required (VERSION 3.8)
project (HijackKit)

add_library(HijackKit "include/utils.h" "include/memory.h" "include/tricks.h" "source/memory.cpp" "source/utils.cpp")
target_include_directories(HijackKit PUBLIC "include/")

if (CMAKE_VERSION VERSION_GREATER 3.12)
	set_property(TARGET HijackKit PROPERTY CXX_STANDARD 20)
endif()

set_target_properties(HijackKit PROPERTIES LINKER_LANGUAGE CXX)