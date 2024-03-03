set(DNAVIGATION_SOURCES "dNavMesh.cpp")

add_subdirectory(dTerrain)

foreach(file ${DNAVIGATIONS_DTERRAIN_SOURCES})
	set(DNAVIGATION_SOURCES ${DNAVIGATION_SOURCES} "dTerrain/${file}")
endforeach()

add_library(dNavigation OBJECT ${DNAVIGATION_SOURCES})
target_include_directories(dNavigation PUBLIC "."
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dZoneManager"
	"${PROJECT_SOURCE_DIR}/dGame"
	"${PROJECT_SOURCE_DIR}/dGame/dEntity"
)
target_link_libraries(dNavigation PRIVATE Detour Recast dCommon)
