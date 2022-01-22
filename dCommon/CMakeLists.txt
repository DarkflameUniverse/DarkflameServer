file(
    GLOB SOURCES_DCOMMON
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    *.cpp
)

add_library(dCommon ${SOURCES_DCOMMON})
target_link_libraries(dCommon ZLIB::ZLIB)
target_link_libraries(dCommon libbcrypt)