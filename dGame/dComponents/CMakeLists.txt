set(DGAME_DCOMPONENTS_SOURCES
	"ActivityComponent.cpp"
	"BaseCombatAIComponent.cpp"
	"BouncerComponent.cpp"
	"BuffComponent.cpp"
	"BuildBorderComponent.cpp"
	"CharacterComponent.cpp"
	"CollectibleComponent.cpp"
	"Component.cpp"
	"ControllablePhysicsComponent.cpp"
	"DestroyableComponent.cpp"
	"DonationVendorComponent.cpp"
	"GhostComponent.cpp"
	"InventoryComponent.cpp"
	"ItemComponent.cpp"
	"LevelProgressionComponent.cpp"
	"LUPExhibitComponent.cpp"
	"MissionComponent.cpp"
	"MissionOfferComponent.cpp"
	"ModelComponent.cpp"
	"ModuleAssemblyComponent.cpp"
	"MovementAIComponent.cpp"
	"MovingPlatformComponent.cpp"
	"PetComponent.cpp"
	"PhantomPhysicsComponent.cpp"
	"PhysicsComponent.cpp"
	"PlayerForcedMovementComponent.cpp"
	"PossessableComponent.cpp"
	"PossessorComponent.cpp"
	"PropertyComponent.cpp"
	"PropertyEntranceComponent.cpp"
	"PropertyManagementComponent.cpp"
	"PropertyVendorComponent.cpp"
	"ProximityMonitorComponent.cpp"
	"RacingControlComponent.cpp"
	"RailActivatorComponent.cpp"
	"QuickBuildComponent.cpp"
	"RenderComponent.cpp"
	"RigidbodyPhantomPhysicsComponent.cpp"
	"MultiZoneEntranceComponent.cpp"
	"RocketLaunchpadControlComponent.cpp"
	"ShootingGalleryComponent.cpp"
	"SimplePhysicsComponent.cpp"
	"SkillComponent.cpp"
	"SoundTriggerComponent.cpp"
	"SwitchComponent.cpp"
	"TriggerComponent.cpp"
	"HavokVehiclePhysicsComponent.cpp"
	"VendorComponent.cpp"
	"MiniGameControlComponent.cpp"
)

add_library(dComponents OBJECT ${DGAME_DCOMPONENTS_SOURCES})
target_include_directories(dComponents PUBLIC "."
	"${PROJECT_SOURCE_DIR}/dGame/dPropertyBehaviors" # via ModelComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dPropertyBehaviors/ControlBehaviorMessages"
	"${PROJECT_SOURCE_DIR}/dGame/dMission" # via MissionComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dBehaviors" # via InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dInventory" # via InventoryComponent.h
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dCommon"
	"${PROJECT_SOURCE_DIR}/dCommon/dEnums"
	"${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase/CDClientTables"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/ITables"
	"${PROJECT_SOURCE_DIR}/thirdparty/mariadb-connector-cpp/include"
	# dPhysics (via dpWorld.h)
	"${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Include"
	"${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Detour/Include"

	"${PROJECT_SOURCE_DIR}/dScripts/02_server/Map/General" # PetDigServer.h
	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages" # direct
	"${PROJECT_SOURCE_DIR}/dGame/dUtilities" # direct Loot.h
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # via dZoneManager/Spawner.h
	"${PROJECT_SOURCE_DIR}/dZoneManager" # via BouncerComponent.cpp, ActivityComponent.cpp
)
target_precompile_headers(dComponents REUSE_FROM dGameBase)

# INTERFACE link w/o dependency
#set_property(TARGET dComponents APPEND PROPERTY INTERFACE_LINK_LIBRARIES
#	dUtilities dCommon dBehaviors dChatFilter dMission dInventory
#	dPhysics
#)

target_link_libraries(dComponents INTERFACE dBehaviors)