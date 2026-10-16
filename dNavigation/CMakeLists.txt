set(DNAVIGATION_SOURCES "dNavMesh.cpp")

add_library(dNavigation OBJECT ${DNAVIGATION_SOURCES})
target_include_directories(dNavigation PUBLIC "."
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dZoneManager"
	"${PROJECT_SOURCE_DIR}/dGame"
	"${PROJECT_SOURCE_DIR}/dGame/dEntity"
	"${PROJECT_SOURCE_DIR}/dNavigation/dTerrain" # via dNavMesh.cpp
)
target_link_libraries(dNavigation PRIVATE Detour Recast dCommon)
