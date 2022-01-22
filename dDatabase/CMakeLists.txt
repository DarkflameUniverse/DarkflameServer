file(
    GLOB SOURCES_DDATABASE
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    *.cpp
    Tables/*.cpp
)

add_library(dDatabase ${SOURCES_DDATABASE})
target_link_libraries(dDatabase sqlite3)