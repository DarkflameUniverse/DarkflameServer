add_library(dECS STATIC
"Core.h"
"Core.cpp"
)
target_include_directories(dECS PUBLIC .)
target_link_libraries(dECS PRIVATE dCommon magic_enum::magic_enum)
target_compile_options(dECS PRIVATE "-Wall")
