set(DNAVIGATION_SOURCES "dNavMesh.cpp")

add_library(dNavigation STATIC ${DNAVIGATION_SOURCES})
target_link_libraries(dNavigation dPhysics)
