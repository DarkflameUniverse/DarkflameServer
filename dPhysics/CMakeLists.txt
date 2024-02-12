set(DPHYSICS_SOURCES
#header
    "dpCollisionChecks.h"
	"dpEntity.h"
	"dpGrid.h"
	"dpShapeBase.h"
	"dpShapeBox.h"
	"dpShapeSphere.h"
	"dpWorld.h"

#source
    "dpCollisionChecks.cpp"
	"dpEntity.cpp"
	"dpGrid.cpp"
	"dpShapeBase.cpp"
	"dpShapeBox.cpp"
	"dpShapeSphere.cpp"
	"dpWorld.cpp")

add_library(dPhysics STATIC ${DPHYSICS_SOURCES})
target_include_directories(dPhysics PUBLIC .)
target_link_libraries(dPhysics
	PUBLIC Recast Detour dCommon dNavigation)
