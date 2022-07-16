set(DAUTHSERVER_SOURCES "AuthServer.cpp")

add_executable(AuthServer ${DAUTHSERVER_SOURCES})
target_link_libraries(AuthServer ${COMMON_LIBRARIES})
