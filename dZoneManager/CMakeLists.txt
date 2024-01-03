set(DZONEMANAGER_SOURCES "dZoneManager.cpp"
	"Level.cpp"
	"Spawner.cpp"
	"Zone.cpp")

add_library(dZoneManager STATIC ${DZONEMANAGER_SOURCES})
target_link_libraries(dZoneManager
	PRIVATE dDatabaseCDClient
	PUBLIC dPhysics
	INTERFACE dWorldServer)
target_include_directories(dZoneManager PUBLIC "."
	${PROJECT_SOURCE_DIR}/dGame # Entity.h
	${PROJECT_SOURCE_DIR}/dGame/dEntity # EntityInfo.h
	PRIVATE
	${PROJECT_SOURCE_DIR}/dGame/dComponents #InventoryComponent.h
	${PROJECT_SOURCE_DIR}/dGame/dInventory #InventoryComponent.h (transitive)
	${PROJECT_SOURCE_DIR}/dGame/dBehaviors #BehaviorSlot.h
	${PROJECT_SOURCE_DIR}/dGame/dGameMessages #GameMessages.h
	${PROJECT_SOURCE_DIR}/dGame/dUtilities #VanityUtilities.h
)