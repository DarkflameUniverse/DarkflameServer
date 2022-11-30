set(DNAVIGATION_SOURCES "dNavMesh.cpp")

add_subdirectory(dTerrain)

foreach(file ${DNAVIGATIONS_DTERRAIN_SOURCES})
	set(DNAVIGATION_SOURCES ${DNAVIGATION_SOURCES} "dTerrain/${file}")
endforeach()

add_library(dNavigation STATIC ${DNAVIGATION_SOURCES})
target_link_libraries(dNavigation Detour Recast)
