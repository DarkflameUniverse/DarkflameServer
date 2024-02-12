set(DNAVIGATION_SOURCES "dNavMesh.h" "dNavMesh.cpp")

add_subdirectory(dTerrain)

foreach(file ${DNAVIGATIONS_DTERRAIN_SOURCES})
	set(DNAVIGATION_SOURCES ${DNAVIGATION_SOURCES} "dTerrain/${file}")
endforeach()

add_library(dNavigation STATIC ${DNAVIGATION_SOURCES})
target_include_directories(dNavigation PUBLIC
.
./dTerrain
)

target_link_libraries(dNavigation PUBLIC Detour Recast dCommon dZoneManager)
