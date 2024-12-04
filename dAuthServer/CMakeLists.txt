
if (WIN32 AND LOCAL_SERVER)
	add_library(AuthServer SHARED "AuthServer.cpp")
else()
	add_executable(AuthServer "AuthServer.cpp")
endif()

target_link_libraries(AuthServer ${COMMON_LIBRARIES} dServer)

target_include_directories(AuthServer PRIVATE ${PROJECT_SOURCE_DIR}/dServer)

add_compile_definitions(AuthServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")
