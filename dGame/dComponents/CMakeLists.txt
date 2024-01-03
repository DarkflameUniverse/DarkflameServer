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

add_library(dComponents STATIC ${DGAME_DCOMPONENTS_SOURCES})
target_include_directories(dComponents PUBLIC "."
	"${PROJECT_SOURCE_DIR}/dGame/dMission" # via MissionComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dBehaviors" # via InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dInventory" # via InventoryComponent.h
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dScripts/02_server/Map/General" # PetDigServer.h
	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages" # direct
	"${PROJECT_SOURCE_DIR}/dGame/dUtilities" # direct Loot.h
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # via dZoneManager/Spawner.h
)
target_precompile_headers(dComponents REUSE_FROM dGameBase)
target_link_libraries(dComponents
	PUBLIC dPhysics dDatabase
	INTERFACE dUtilities dCommon dBehaviors dChatFilter dMission dInventory)
