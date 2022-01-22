file(
    GLOB SOURCES_DNET
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    *.cpp
)

add_library(dNet ${SOURCES_DNET})
target_link_libraries(dNet dCommon) # Needed because otherwise linker errors occur.