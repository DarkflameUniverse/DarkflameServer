add_library(dECS STATIC "ECS.cpp")
target_include_directories(dECS PUBLIC .)
target_link_libraries(dECS PRIVATE dCommon)
target_compile_options(dECS PRIVATE "-Wall")
