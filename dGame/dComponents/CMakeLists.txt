set(DGAME_DCOMPONENTS_SOURCES
	"Vendor/AchievementVendorComponent.cpp"
	"Activity/ActivityComponent.cpp"
	"BaseCombatAIComponent.cpp"
	"BouncerComponent.cpp"
	"BuffComponent.cpp"
	"Property/BuildBorderComponent.cpp"
	"Character/CharacterComponent.cpp"
	"CollectibleComponent.cpp"
	"Physics/ControllablePhysicsComponent.cpp"
	"DestroyableComponent.cpp"
	"Vendor/DonationVendorComponent.cpp"
	"Character/GhostComponent.cpp"
	"InventoryComponent.cpp"
	"ItemComponent.cpp"
	"Character/LevelProgressionComponent.cpp"
	"LUPExhibitComponent.cpp"
	"Character/MissionComponent.cpp"
	"MissionOfferComponent.cpp"
	"ModelComponent.cpp"
	"ModuleAssemblyComponent.cpp"
	"MovementAIComponent.cpp"
	"MovingPlatformComponent.cpp"
	"PetComponent.cpp"
	"Physics/PhantomPhysicsComponent.cpp"
	"Physics/PhysicsComponent.cpp"
	"Character/PlayerForcedMovementComponent.cpp"
	"PossessableComponent.cpp"
	"Character/PossessorComponent.cpp"
	"Property/PropertyEntranceComponent.cpp"
	"Property/PropertyManagementComponent.cpp"
	"Property/PropertyVendorComponent.cpp"
	"ProximityMonitorComponent.cpp"
	"Activity/RacingControlComponent.cpp"
	"RailActivatorComponent.cpp"
	"Activity/QuickBuildComponent.cpp"
	"RenderComponent.cpp"
	"Physics/RigidbodyPhantomPhysicsComponent.cpp"
	"MultiZoneEntranceComponent.cpp"
	"RocketLaunchpadControlComponent.cpp"
	"Activity/ShootingGalleryComponent.cpp"
	"Physics/SimplePhysicsComponent.cpp"
	"SkillComponent.cpp"
	"Sound/SoundTriggerComponent.cpp"
	"ExhibitComponent.cpp"
	"SpawnerComponent.cpp"
	"SwitchComponent.cpp"
	"TriggerComponent.cpp"
	"Physics/HavokVehiclePhysicsComponent.cpp"
	"Vendor/BaseVendorComponent.cpp"
	"ScriptComponent.cpp"
)

add_library(dComponents OBJECT ${DGAME_DCOMPONENTS_SOURCES})
target_include_directories(dComponents PUBLIC ${DLU_COMPONENT_INCLUDE_DIRS}
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
	# dPhysics (via dpWorld.h)
	"${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Include"
	"${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Detour/Include"

	"${PROJECT_SOURCE_DIR}/dScripts/02_server/Map/General" # PetDigServer.h
	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages" # direct
	"${PROJECT_SOURCE_DIR}/dGame/dUtilities" # direct Loot.h
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # via dZoneManager/Spawner.h
	"${PROJECT_SOURCE_DIR}/dZoneManager" # via BouncerComponent.cpp, ActivityComponent.cpp
	"${PROJECT_SOURCE_DIR}/dChatFilter" # via PetComponent.cpp
)
target_precompile_headers(dComponents REUSE_FROM dGameBase)

target_link_libraries(dComponents INTERFACE dBehaviors PRIVATE glm::glm)
