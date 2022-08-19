set(DPHYSICS_SOURCES "dpCollisionChecks.cpp"
	"dpEntity.cpp"
	"dpGrid.cpp"
	"dpShapeBase.cpp"
	"dpShapeBox.cpp"
	"dpShapeSphere.cpp"
	"dpWorld.cpp")

add_library(dPhysics STATIC ${DPHYSICS_SOURCES})
target_link_libraries(dPhysics Recast Detour)
