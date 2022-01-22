file(
    GLOB SOURCES_AUTH
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    *.cpp
)

add_executable(AuthServer ${SOURCES_AUTH})
target_link_libraries(AuthServer ${SHARED_LIBS})