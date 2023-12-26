set(DPHYSICS_SOURCES "dpCollisionChecks.cpp"
	"dpEntity.cpp"
	"dpGrid.cpp"
	"dpShapeBase.cpp"
	"dpShapeBox.cpp"
	"dpShapeSphere.cpp"
	"dpWorld.cpp")

add_library(dPhysics STATIC ${DPHYSICS_SOURCES})
target_link_libraries(dPhysics Recast Detour)

# If using gcc, warn on old-style C casts
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set_source_files_properties(${DPHYSICS_SOURCES} PROPERTIES COMPILE_FLAGS "-Wold-style-cast")
endif()
