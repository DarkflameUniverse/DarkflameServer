set(DZONEMANAGER_SOURCES "dZoneManager.cpp"
	"Level.cpp"
	"Spawner.cpp"
	"Zone.cpp")

set(DZONEMANAGER_SOURCES ${DZONEMANAGER_SOURCES} PARENT_SCOPE)
add_library(dZoneManager STATIC ${DZONEMANAGER_SOURCES})
# target_link_libraries(dZoneManager dPhysics) 
