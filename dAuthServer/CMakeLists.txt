add_executable(AuthServer "AuthServer.cpp")
target_link_libraries(AuthServer ${COMMON_LIBRARIES})
add_compile_definitions(AuthServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")
