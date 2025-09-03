set(DSIMCLIENT_SOURCES
    "SimClient.cpp"
    "SimUser.cpp"
    "SimUser.h"
    "SimWorld.cpp"
    "SimWorld.h"
    "SimBehavior.cpp"
    "SimBehavior.h"
    "SimConfig.cpp"
    "SimConfig.h"
)

add_executable(SimClient ${DSIMCLIENT_SOURCES})

target_link_libraries(SimClient ${COMMON_LIBRARIES})
target_include_directories(SimClient PRIVATE 
    "${PROJECT_SOURCE_DIR}/dCommon"
    "${PROJECT_SOURCE_DIR}/dDatabase"
    "${PROJECT_SOURCE_DIR}/dNet"
    "${PROJECT_SOURCE_DIR}/thirdparty/raknet/Source"
    "${PROJECT_SOURCE_DIR}/dServer"
)

# Copy the configuration file to build directory
configure_file(
    "${PROJECT_SOURCE_DIR}/resources/simclient.ini"
    "${CMAKE_CURRENT_BINARY_DIR}/simclient.ini"
    COPYONLY
)
