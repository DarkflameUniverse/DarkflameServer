set(DNAVIGATION_SOURCES "dNavMesh.cpp")

add_subdirectory(dTerrain)

foreach(file ${DNAVIGATIONS_DTERRAIN_SOURCES})
	set(DNAVIGATION_SOURCES ${DNAVIGATION_SOURCES} "dTerrain/${file}")
endforeach()

add_library(dNavigation STATIC ${DNAVIGATION_SOURCES})
target_link_libraries(dNavigation Detour Recast)

# If using gcc, warn on old-style C casts
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set_source_files_properties(${DNAVIGATION_SOURCES} PROPERTIES COMPILE_FLAGS "-Wold-style-cast")
endif()
