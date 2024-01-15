set(DZONEMANAGER_SOURCES "dZoneManager.cpp"
	"Level.cpp"
	"Spawner.cpp"
	"Zone.cpp")

add_library(dZoneManager STATIC ${DZONEMANAGER_SOURCES})
target_link_libraries(dZoneManager
	PUBLIC dPhysics
	INTERFACE dWorldServer)
