set(DPHYSICS_SOURCES "dpCollisionChecks.cpp"
	"dpEntity.cpp"
	"dpGrid.cpp"
	"dpShapeBase.cpp"
	"dpShapeBox.cpp"
	"dpShapeSphere.cpp"
	"dpWorld.cpp")

add_library(dPhysics STATIC ${DPHYSICS_SOURCES})
target_include_directories(dPhysics PUBLIC "."
	"${PROJECT_SOURCE_DIR}/dCommon"
	"${PROJECT_SOURCE_DIR}/dCommon/dEnums"
)
target_link_libraries(dPhysics
	PUBLIC Recast Detour
	INTERFACE dNavigation dCommon)
