set(DZONEMANAGER_SOURCES "dZoneManager.cpp"
	"Level.cpp"
	"Spawner.cpp"
	"Zone.cpp")

#add_library(dZoneManager STATIC ${DZONEMANAGER_SOURCES}) 

set(DZONEMANAGER_SOURCES ${DZONEMANAGER_SOURCES} PARENT_SCOPE)
