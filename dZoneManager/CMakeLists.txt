set(DZONEMANAGER_SOURCES "dZoneManager.cpp"
	"Level.cpp"
	"Spawner.cpp"
	"Zone.cpp")

add_library(dZoneManager STATIC ${DZONEMANAGER_SOURCES})
target_link_libraries(dZoneManager dPhysics) 

# If using gcc, warn on old-style C casts
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set_source_files_properties(${DZONEMANAGER_SOURCES} PROPERTIES COMPILE_FLAGS "-Wold-style-cast")
endif()
