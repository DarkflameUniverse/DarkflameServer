set(gcc_like_cxx "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>")
set(msvc_cxx "$<COMPILE_LANG_AND_ID:CXX,MSVC>")

add_library(dECS STATIC
    "Core.h"
    "Iter.h"
    "Core.cpp"
    "System.cpp"
)
target_include_directories(dECS PUBLIC .)
target_link_libraries(dECS PRIVATE dCommon magic_enum::magic_enum)
target_compile_options(dECS PRIVATE
  "$<${gcc_like_cxx}:$<BUILD_INTERFACE:-Wall;-Wextra>>"
  "$<${msvc_cxx}:$<BUILD_INTERFACE:/W3>>"
)
