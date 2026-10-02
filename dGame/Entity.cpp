#include "dCommonVars.h"
#include "ChatServerLink.h"
#include "ChatPackets.h"
#include "Entity.h"
#include "EconomyLedger.h"
#include "CDClientManager.h"
#include "Game.h"
#include "Logger.h"
#include <functional>
#include "CDDestructibleComponentTable.h"
#include "CDClientDatabase.h"
#include <sstream>
#include <array>
#include "dServer.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "MovementMessages.h"
#include "ObjectMessages.h"
#include "PlayerMessages.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "Zone.h"
#include "Spawner.h"
#include "UserManager.h"
#include "dpWorld.h"
#include "LUTriggers.h"
#include "User.h"
#include "EntityTimer.h"
#include "EntityCallbackTimer.h"
#include "Loot.h"
#include "eMissionTaskType.h"
#include "eTriggerEventType.h"
#include "eObjectBits.h"
#include "PositionUpdate.h"
#include "MessageType/Chat.h"
#include "PlayerManager.h"

//Component includes:
#include "Component.h"
#include "ControllablePhysicsComponent.h"
#include "RenderComponent.h"
#include "MultiZoneEntranceComponent.h"
#include "CharacterComponent.h"
#include "DestroyableComponent.h"
#include "BuffComponent.h"
#include "BouncerComponent.h"
#include "InventoryComponent.h"
#include "LevelProgressionComponent.h"
#include "PlayerForcedMovementComponent.h"
#include "ScriptComponent.h"
#include "SkillComponent.h"
#include "SimplePhysicsComponent.h"
#include "SwitchComponent.h"
#include "PhantomPhysicsComponent.h"
#include "RigidbodyPhantomPhysicsComponent.h"
#include "MovingPlatformComponent.h"
#include "MissionComponent.h"
#include "MissionOfferComponent.h"
#include "QuickBuildComponent.h"
#include "BuildBorderComponent.h"
#include "MovementAIComponent.h"
#include "VendorComponent.h"
#include "DonationVendorComponent.h"
#include "RocketLaunchpadControlComponent.h"
#include "PropertyComponent.h"
#include "BaseCombatAIComponent.h"
#include "PropertyManagementComponent.h"
#include "PropertyVendorComponent.h"
#include "ProximityMonitorComponent.h"
#include "PropertyEntranceComponent.h"
#include "ModelComponent.h"
#include "ZCompression.h"
#include "PetComponent.h"
#include "HavokVehiclePhysicsComponent.h"
#include "PossessableComponent.h"
#include "PossessorComponent.h"
#include "ModuleAssemblyComponent.h"
#include "RacingControlComponent.h"
#include "SoundTriggerComponent.h"
#include "ShootingGalleryComponent.h"
#include "RailActivatorComponent.h"
#include "LUPExhibitComponent.h"
#include "RacingSoundTriggerComponent.h"
#include "TriggerComponent.h"
#include "eGameMasterLevel.h"
#include "eReplicaComponentType.h"
#include "eReplicaPacketType.h"
#include "MiniGameControlComponent.h"
#include "CollectibleComponent.h"
#include "ItemComponent.h"
#include "MinifigComponent.h"
#include "PropertyPlaqueComponent.h"
#include "RollerComponent.h"
#include "GateRushComponent.h"
#include "FactionTriggerComponent.h"
#include "RocketLandingComponent.h"
#include "MountControlComponent.h"
#include "PetCreatorComponent.h"
#include "HFLightDirectionGadgetComponent.h"
#include "SpawnerComponent.h"
#include "BBBComponent.h"
#include "ExhibitComponent.h"
#include "PathfindingComponent.h"
#include "PhysicsSystemComponent.h"
#include "VehiclePhysicsComponent.h"
#include "ProjectilePhysicsComponent.h"
#include "GenericActivatorComponent.h"
#include "ShowcaseModelHandlerComponent.h"
#include "CustomBuildAssemblyComponent.h"
#include "PackageComponent.h"
#include "ChoiceBuildComponent.h"
#include "ChangelingComponent.h"
#include "SpringpadComponent.h"
#include "ModelBuilderComponent.h"
#include "ArcadeComponent.h"
#include "ModuleComponent.h"
#include "BlueprintComponent.h"
#include "ChestComponent.h"
#include "RebuildActivatorComponent.h"
#include "BuildControllerComponent.h"
#include "ModularBuildComponent.h"
#include "PlatformBoundaryComponent.h"
#include "CullingPlaneComponent.h"
#include "SoundAmbient3DComponent.h"
#include "SoundAmbient2DComponent.h"
#include "SoundRepeaterComponent.h"
#include "GhostComponent.h"
#include "AchievementVendorComponent.h"
#include "VanityUtilities.h"
#include "ObjectIDManager.h"
#include "ePlayerFlag.h"

// Table includes
#include "CDComponentsRegistryTable.h"
#include "CDCurrencyTableTable.h"
#include "CDMovementAIComponentTable.h"
#include "CDProximityMonitorComponentTable.h"
#include "CDRebuildComponentTable.h"
#include "CDObjectSkillsTable.h"
#include "CDObjectsTable.h"
#include "CDScriptComponentTable.h"
#include "CDSkillBehaviorTable.h"
#include "CDZoneTableTable.h"

#include "StringifiedEnum.h"

#include <ranges>
#include "InventoryMessages.h"
#include "ActivityMessages.h"

Observable<Entity*, const PositionUpdate&> Entity::OnPlayerPositionUpdate;

Entity::Entity(const LWOOBJID& objectID, const EntityInfo& info, User* parentUser, Entity* parentEntity) {
	m_ObjectID = objectID;
	m_TemplateID = info.lot;
	m_ParentEntity = parentEntity;
	m_Character = nullptr;
	m_GMLevel = eGameMasterLevel::CIVILIAN;
	m_NetworkID = 0;
	m_Groups = {};
	m_OwnerOverride = LWOOBJID_EMPTY;
	m_Timers = {};
	m_ChildEntities = {};
	m_ScheduleKiller = nullptr;
	m_TargetsInPhantom = {};
	m_Components = {};
	m_DieCallbacks = {};
	m_PhantomCollisionCallbacks = {};
	m_IsParentChildDirty = true;

	m_Settings = info.settings;
	m_NetworkSettings = info.networkSettings;
	m_DefaultPosition = info.pos;
	m_DefaultRotation = info.rot;
	m_Scale = info.scale;
	m_Scene = info.scene;
	m_Spawner = info.spawner;
	m_SpawnerID = info.spawnerID;
	m_HasSpawnerNodeID = info.hasSpawnerNodeID;
	m_SpawnerNodeID = info.spawnerNodeID;

	m_PlayerIsReadyForUpdates = info.lot != 1;

	if (parentUser) {
		m_Character = parentUser->GetLastUsedChar();
		parentUser->SetLoggedInChar(objectID);
		m_GMLevel = m_Character->GetGMLevel();

		m_Character->SetEntity(this);

		PlayerManager::AddPlayer(this);
	}
}

Entity::~Entity() {
	if (IsPlayer()) {
		LOG("Deleted player");

		// Make sure the player exists first.  Remove afterwards to prevent the OnPlayerExist functions from not being able to find the player.
		if (!PlayerManager::RemovePlayer(this)) {
			LOG("Unable to find player to remove from manager.");
			return;
		}

		// Ends their building on the property; the owner leaving can stop their best friends building
		if (auto* property = PropertyManagementComponent::Instance()) property->OnPlayerRemoved(*this);

		auto* zoneControl = Game::entityManager->GetZoneControlEntity();
		if (zoneControl) {
			zoneControl->GetScript()->OnPlayerExit(zoneControl, this);
		}

		std::vector<Entity*> scriptedActs = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SCRIPTED_ACTIVITY);
		for (Entity* scriptEntity : scriptedActs) {
			if (zoneControl && scriptEntity->GetObjectID() != zoneControl->GetObjectID()) { // Don't want to trigger twice on instance worlds
				scriptEntity->GetScript()->OnPlayerExit(scriptEntity, this);
			}
		}
	}

	if (m_Character) {
		m_Character->SaveXMLToDatabase();
		m_Character->SetEntity(nullptr);
	}

	CancelAllTimers();
	CancelCallbackTimers();

	// The client's Preshutdown, then Shutdown, for every component before any is deleted
	for (auto* const component : m_Components | std::views::values) if (component) component->Preshutdown();
	for (auto* const component : m_Components | std::views::values) if (component) component->Shutdown();

	for (auto& component : m_Components | std::views::values) {
		if (component) {
			delete component;
			component = nullptr;
		}
	}

	for (auto* const child : m_ChildEntities) {
		if (child) child->RemoveParent();
	}

	if (m_ParentEntity) {
		GameMessages::ChildRemoved removedMsg{};
		removedMsg.childID = m_ObjectID;
		removedMsg.target = m_ParentEntity->GetObjectID();
		removedMsg.Send();
		m_ParentEntity->RemoveChild(this);
	}
}

namespace {
	// The order the client makes an object's components in (ComponentOrderVector::Initialize 0x0101f8e0, walked by
	// ObjectLoader2::DoObjectComponentLoad 0x0105ebb0; crafting 107 only for game versions above 999)
	constexpr auto ClientComponentLoadOrder = [] {
		constexpr std::array<uint32_t, 79> types{ 108, 24, 61, 1, 18, 47, 46, 30, 20, 3, 40, 7, 35, 4, 106, 110, 111, 26, 17, 78, 31, 5, 9, 60, 10, 11, 12, 114, 28, 13, 14, 48, 73, 16, 100, 102, 19, 105, 39, 23, 25, 51, 52, 53, 27, 66, 116, 32, 75, 36, 113, 42, 43, 97, 45, 104, 6, 41, 49, 50, 38, 54, 22, 62, 29, 2, 55, 56, 77, 79, 59, 64, 65, 67, 68, 72, 71, 103, 76 };
		std::array<eReplicaComponentType, types.size()> order{};
		for (size_t i = 0; i < types.size(); i++) order[i] = static_cast<eReplicaComponentType>(types[i]);
		return order;
	}();

	// A component the client makes from its registry entry alone (most LWOxComponent::Allocator functions): ids from
	// MinID up make one
	template <typename ComponentType, eReplicaComponentType Type, int32_t MinID>
	void RegistryAllocator(Entity& entity, ObjectLoadState& state) {
		const auto id = state.GetComponentID(Type, -1);
		if (id >= MinID) entity.AddComponent<ComponentType>(id);
	}

	// The allocator for each type DoObjectComponentLoad walks (the client's allocator table); a type with none makes
	// nothing
	const std::unordered_map<eReplicaComponentType, ComponentAllocator> ComponentAllocators{
		{ eReplicaComponentType::POSSESSABLE, &RegistryAllocator<PossessableComponent, eReplicaComponentType::POSSESSABLE, 1> },
		{ eReplicaComponentType::MODULE_ASSEMBLY, &RegistryAllocator<ModuleAssemblyComponent, eReplicaComponentType::MODULE_ASSEMBLY, 1> },
		{ eReplicaComponentType::CONTROLLABLE_PHYSICS, &ControllablePhysicsComponent::Allocator },
		{ eReplicaComponentType::HAVOK_VEHICLE_PHYSICS, &HavokVehiclePhysicsComponent::Allocator },
		{ eReplicaComponentType::RIGID_BODY_PHANTOM_PHYSICS, &RegistryAllocator<RigidbodyPhantomPhysicsComponent, eReplicaComponentType::RIGID_BODY_PHANTOM_PHYSICS, 1> },
		{ eReplicaComponentType::SIMPLE_PHYSICS, &SimplePhysicsComponent::Allocator },
		{ eReplicaComponentType::PHANTOM_PHYSICS, &PhantomPhysicsComponent::Allocator },
		{ eReplicaComponentType::DESTROYABLE, &DestroyableComponent::Allocator },
		{ eReplicaComponentType::MINIFIG, &RegistryAllocator<MinifigComponent, eReplicaComponentType::MINIFIG, 1> },
		{ eReplicaComponentType::CHARACTER, &CharacterComponent::Allocator },
		{ eReplicaComponentType::PET, &PetComponent::Allocator },
		{ eReplicaComponentType::INVENTORY, &InventoryComponent::Allocator },
		{ eReplicaComponentType::PROXIMITY_MONITOR, &RegistryAllocator<ProximityMonitorComponent, eReplicaComponentType::PROXIMITY_MONITOR, 1> },
		{ eReplicaComponentType::MOVEMENT_AI, &MovementAIComponent::Allocator },
		{ eReplicaComponentType::SCRIPT, &ScriptComponent::Allocator },
		{ eReplicaComponentType::SKILL, &SkillComponent::Allocator },
		{ eReplicaComponentType::BASE_COMBAT_AI, &BaseCombatAIComponent::Allocator },
		{ eReplicaComponentType::ITEM, &ItemComponent::Allocator },
		{ eReplicaComponentType::BUILD_BORDER, &RegistryAllocator<BuildBorderComponent, eReplicaComponentType::BUILD_BORDER, 0> },
		{ eReplicaComponentType::QUICK_BUILD, &QuickBuildComponent::Allocator },
		{ eReplicaComponentType::MISSION_OFFER, &RegistryAllocator<MissionOfferComponent, eReplicaComponentType::MISSION_OFFER, 0> },
		{ eReplicaComponentType::VENDOR, &VendorComponent::Allocator },
		{ eReplicaComponentType::DONATION_VENDOR, &DonationVendorComponent::Allocator },
		{ eReplicaComponentType::ACHIEVEMENT_VENDOR, &AchievementVendorComponent::Allocator },
		{ eReplicaComponentType::SHOOTING_GALLERY, &RegistryAllocator<ShootingGalleryComponent, eReplicaComponentType::SHOOTING_GALLERY, 1> },
		{ eReplicaComponentType::SCRIPTED_ACTIVITY, &RegistryAllocator<ScriptedActivityComponent, eReplicaComponentType::SCRIPTED_ACTIVITY, 0> },
		{ eReplicaComponentType::COLLECTIBLE, &CollectibleComponent::Allocator },
		{ eReplicaComponentType::MOVING_PLATFORM, &MovingPlatformComponent::Allocator },
		{ eReplicaComponentType::LUP_EXHIBIT, &RegistryAllocator<LUPExhibitComponent, eReplicaComponentType::LUP_EXHIBIT, 0> },
		{ eReplicaComponentType::PROPERTY, &RegistryAllocator<PropertyComponent, eReplicaComponentType::PROPERTY, 0> },
		{ eReplicaComponentType::MODEL, &ModelComponent::Allocator },
		{ eReplicaComponentType::PROPERTY_ENTRANCE, &RegistryAllocator<PropertyEntranceComponent, eReplicaComponentType::PROPERTY_ENTRANCE, 1> },
		{ eReplicaComponentType::MULTI_ZONE_ENTRANCE, &RegistryAllocator<MultiZoneEntranceComponent, eReplicaComponentType::MULTI_ZONE_ENTRANCE, 0> }, // always id 0
		{ eReplicaComponentType::PROPERTY_MANAGEMENT, &RegistryAllocator<PropertyManagementComponent, eReplicaComponentType::PROPERTY_MANAGEMENT, 0> },
		{ eReplicaComponentType::RAIL_ACTIVATOR, &RegistryAllocator<RailActivatorComponent, eReplicaComponentType::RAIL_ACTIVATOR, 1> },
		{ eReplicaComponentType::BOUNCER, &RegistryAllocator<BouncerComponent, eReplicaComponentType::BOUNCER, 0> }, // every bouncer's id is 0
		{ eReplicaComponentType::SWITCH, &RegistryAllocator<SwitchComponent, eReplicaComponentType::SWITCH, 0> },
		{ eReplicaComponentType::MINI_GAME_CONTROL, &RegistryAllocator<MiniGameControlComponent, eReplicaComponentType::MINI_GAME_CONTROL, 0> }, // the zone control object's id is 0
		{ eReplicaComponentType::RENDER, &RenderComponent::Allocator },
		{ eReplicaComponentType::SOUND_TRIGGER, &SoundTriggerComponent::Allocator },
		{ eReplicaComponentType::PROPERTY_VENDOR, &RegistryAllocator<PropertyVendorComponent, eReplicaComponentType::PROPERTY_VENDOR, 0> },
		{ eReplicaComponentType::ROCKET_LAUNCH, &RegistryAllocator<RocketLaunchpadControlComponent, eReplicaComponentType::ROCKET_LAUNCH, 1> },
		{ eReplicaComponentType::RACING_CONTROL, &RegistryAllocator<RacingControlComponent, eReplicaComponentType::RACING_CONTROL, 1> },
		{ eReplicaComponentType::SOUND_REPEATER, &RegistryAllocator<SoundRepeaterComponent, eReplicaComponentType::SOUND_REPEATER, 0> },
		{ eReplicaComponentType::SOUND_AMBIENT_2D, &RegistryAllocator<SoundAmbient2DComponent, eReplicaComponentType::SOUND_AMBIENT_2D, 0> },
		{ eReplicaComponentType::SOUND_AMBIENT_3D, &RegistryAllocator<SoundAmbient3DComponent, eReplicaComponentType::SOUND_AMBIENT_3D, 0> },
		{ eReplicaComponentType::CULLING_PLANE, &RegistryAllocator<CullingPlaneComponent, eReplicaComponentType::CULLING_PLANE, 0> },
		{ eReplicaComponentType::PLATFORM_BOUNDARY, &RegistryAllocator<PlatformBoundaryComponent, eReplicaComponentType::PLATFORM_BOUNDARY, 0> },
		{ eReplicaComponentType::REBUILD, &RegistryAllocator<ModularBuildComponent, eReplicaComponentType::REBUILD, 0> },
		{ eReplicaComponentType::REBUILD_START, &RegistryAllocator<BuildControllerComponent, eReplicaComponentType::REBUILD_START, 0> },
		{ eReplicaComponentType::REBUILD_ACTIVATOR, &RegistryAllocator<RebuildActivatorComponent, eReplicaComponentType::REBUILD_ACTIVATOR, 0> },
		{ eReplicaComponentType::CHEST, &RegistryAllocator<ChestComponent, eReplicaComponentType::CHEST, 0> },
		{ eReplicaComponentType::BLUEPRINT, &RegistryAllocator<BlueprintComponent, eReplicaComponentType::BLUEPRINT, 0> },
		{ eReplicaComponentType::MODULE, &RegistryAllocator<ModuleComponent, eReplicaComponentType::MODULE, 0> },
		{ eReplicaComponentType::ARCADE, &RegistryAllocator<ArcadeComponent, eReplicaComponentType::ARCADE, 0> },
		{ eReplicaComponentType::MODEL_BUILDER, &RegistryAllocator<ModelBuilderComponent, eReplicaComponentType::MODEL_BUILDER, 0> },
		{ eReplicaComponentType::SPRINGPAD, &RegistryAllocator<SpringpadComponent, eReplicaComponentType::SPRINGPAD, 0> },
		{ eReplicaComponentType::CHANGLING, &RegistryAllocator<ChangelingComponent, eReplicaComponentType::CHANGLING, 0> },
		{ eReplicaComponentType::CHOICE_BUILD, &RegistryAllocator<ChoiceBuildComponent, eReplicaComponentType::CHOICE_BUILD, 0> },
		{ eReplicaComponentType::PACKAGE, &RegistryAllocator<PackageComponent, eReplicaComponentType::PACKAGE, 0> },
		{ eReplicaComponentType::CUSTOM_BUILD_ASSEMBLY, &RegistryAllocator<CustomBuildAssemblyComponent, eReplicaComponentType::CUSTOM_BUILD_ASSEMBLY, 0> },
		{ eReplicaComponentType::SHOWCASE_MODEL_HANDLER, &RegistryAllocator<ShowcaseModelHandlerComponent, eReplicaComponentType::SHOWCASE_MODEL_HANDLER, 0> },
		{ eReplicaComponentType::GENERIC_ACTIVATOR, &RegistryAllocator<GenericActivatorComponent, eReplicaComponentType::GENERIC_ACTIVATOR, 0> },
		{ eReplicaComponentType::PROJECTILE_PHYSICS, &RegistryAllocator<ProjectilePhysicsComponent, eReplicaComponentType::PROJECTILE_PHYSICS, 0> },
		{ eReplicaComponentType::VEHICLE_PHYSICS, &RegistryAllocator<VehiclePhysicsComponent, eReplicaComponentType::VEHICLE_PHYSICS, 0> },
		{ eReplicaComponentType::PHYSICS_SYSTEM, &RegistryAllocator<PhysicsSystemComponent, eReplicaComponentType::PHYSICS_SYSTEM, 0> },
		{ eReplicaComponentType::SPAWNER, &RegistryAllocator<SpawnerComponent, eReplicaComponentType::SPAWNER, 0> },
		{ eReplicaComponentType::HF_LIGHT_DIRECTION_GADGET, &RegistryAllocator<HFLightDirectionGadgetComponent, eReplicaComponentType::HF_LIGHT_DIRECTION_GADGET, 0> },
		{ eReplicaComponentType::PLAYER_FORCED_MOVEMENT, &PlayerForcedMovementComponent::Allocator },
		{ eReplicaComponentType::EXHIBIT, &RegistryAllocator<ExhibitComponent, eReplicaComponentType::EXHIBIT, 0> }, // 1.10.64 makes nothing here (showcases were cut); it sends nothing, so unchanged clients are unaffected
		{ eReplicaComponentType::BBB, &BBBComponent::Allocator },
		{ eReplicaComponentType::ROCKET_LANDING, &RegistryAllocator<RocketLandingComponent, eReplicaComponentType::ROCKET_LANDING, 0> },
		{ eReplicaComponentType::FACTION_TRIGGER, &RegistryAllocator<FactionTriggerComponent, eReplicaComponentType::FACTION_TRIGGER, 0> },
		{ eReplicaComponentType::GATE_RUSH_CONTROL, &RegistryAllocator<GateRushComponent, eReplicaComponentType::GATE_RUSH_CONTROL, 0> },
		{ eReplicaComponentType::ROLLER, &RegistryAllocator<RollerComponent, eReplicaComponentType::ROLLER, 0> },
		{ eReplicaComponentType::PROPERTY_PLAQUE, &RegistryAllocator<PropertyPlaqueComponent, eReplicaComponentType::PROPERTY_PLAQUE, 0> },
	};
}

int32_t ObjectLoadState::GetComponentID(const eReplicaComponentType type, const int32_t defaultValue) const {
	if (whitelist && std::ranges::find(*whitelist, type) == whitelist->end()) return defaultValue;
	return CDClientManager::GetTable<CDComponentsRegistryTable>()->GetByIDAndType(lot, type, defaultValue);
}

// ObjectLoader2::DoObjectPreLoad (0x01054d30): what the object's components will be before any is made
ObjectLoadState Entity::DoObjectPreLoad() {
	ObjectLoadState state;
	state.lot = m_TemplateID;

	// The player's own components (HandleLocalPlayer makes them for the local player, not from the registry)
	if (m_Character && m_Character->GetParentUser()) {
		AddComponent<MissionComponent>(-1);
	}

	// markedAsPhantom doesn't count when the registry has phantom physics
	state.markedAsPhantom = HasVar(u"markedAsPhantom") && state.GetComponentID(eReplicaComponentType::PHANTOM_PHYSICS, -1) == -1;

	// A model with behaviors (0x01055667: not inInventory, and modelType and modelBehaviors in its config) gets its
	// physics from the model case, which takes the registry's simple physics id
	state.hasModelBehaviors = !GetVar<bool>(u"inInventory") && HasVar(u"modelType") && HasVar(u"modelBehaviors");

	// ApplyMovementAiIfNeeded (0x01031350): an object with a script (its registry's, or a custom script in the config,
	// ApplyScriptComponentIfHasScript) or an attached path gets movement AI. Not a spawner object: the client never
	// loads one, the server keeps thousands
	const bool hasScript = state.GetComponentID(eReplicaComponentType::SCRIPT, -1) > -1 || HasVar(u"custom_script_server") || HasVar(u"custom_script_client");
	state.addsMovementAI = m_TemplateID != 176 && (hasScript || HasVar(u"attached_path"));

	// ObjectLoader2::ApplyComponentWhitelist (0x0103e300): the config's componentWhitelist keeps only these registry
	// components (the lists ObjectLoader2's constructor builds; property models use 1)
	static constexpr std::array<std::initializer_list<eReplicaComponentType>, 5> COMPONENT_WHITELISTS{ {
		{ eReplicaComponentType::CONTROLLABLE_PHYSICS, eReplicaComponentType::SIMPLE_PHYSICS, eReplicaComponentType::RENDER },
		{ eReplicaComponentType::RENDER, eReplicaComponentType::DESTROYABLE, eReplicaComponentType::ITEM, eReplicaComponentType::BLUEPRINT,
		  eReplicaComponentType::MODEL, eReplicaComponentType::CONTROLLABLE_PHYSICS, eReplicaComponentType::SIMPLE_PHYSICS, eReplicaComponentType::SPAWNER },
		{ eReplicaComponentType::RENDER, eReplicaComponentType::ITEM, eReplicaComponentType::BLUEPRINT },
		{ eReplicaComponentType::PET, eReplicaComponentType::SKILL, eReplicaComponentType::DESTROYABLE, eReplicaComponentType::RENDER, eReplicaComponentType::CONTROLLABLE_PHYSICS },
		{ eReplicaComponentType::CONTROLLABLE_PHYSICS, eReplicaComponentType::SIMPLE_PHYSICS, eReplicaComponentType::RENDER },
	} };
	const auto whitelistIndex = HasVar(u"componentWhitelist") ? GetVarAs<int32_t>(u"componentWhitelist") : -1;
	state.whitelist =
		whitelistIndex >= 0 && whitelistIndex < static_cast<int32_t>(COMPONENT_WHITELISTS.size()) ? &COMPONENT_WHITELISTS[whitelistIndex] : nullptr;

	return state;
}

// ObjectLoader2::DoObjectComponentLoad (0x0105ebb0): the components in the client's order, then the ones it adds after
void ObjectLoadState::AddProximityAndPathfinding(Entity& entity) {
	if (!entity.HasComponent(eReplicaComponentType::PROXIMITY_MONITOR)) entity.AddComponent<ProximityMonitorComponent>(-1);
	const bool hasMovementPhysics = entity.HasComponent(eReplicaComponentType::CONTROLLABLE_PHYSICS) || entity.HasComponent(eReplicaComponentType::PROJECTILE_PHYSICS)
		|| entity.HasComponent(eReplicaComponentType::VEHICLE_PHYSICS) || entity.HasComponent(eReplicaComponentType::HAVOK_VEHICLE_PHYSICS);
	if (hasMovementPhysics && !entity.HasComponent(eReplicaComponentType::PATHFINDING)) entity.AddComponent<PathfindingComponent>(-1);
}

void Entity::DoObjectComponentLoad(ObjectLoadState& state) {
	/**
	 * The components in the client's load order (ObjectLoader2::DoObjectComponentLoad 0x0105ebb0 walks
	 * ComponentOrderVector::Initialize 0x0101f8e0's list), each by its type's allocator: a component that looks at
	 * another one finds it made when the client's does. Ids a component needs from a later one come from the registry.
	 */
	for (const auto type : ClientComponentLoadOrder) {
		if (state.whitelist && std::ranges::find(*state.whitelist, type) == state.whitelist->end()) continue;
		const auto allocator = ComponentAllocators.find(type);
		if (allocator != ComponentAllocators.end()) allocator->second(*this, state);
	}

	/**
	 * Added after the walk, as the client does (DoObjectComponentLoad): the ghost of a character (LWOGhostComponent's
	 * allocator 0x0101c010 makes one for characters with physics; level and plain spawned objects get none), then the
	 * trigger from the level config
	 */
	// LWODestroyableComponent::Allocator (0x0104d070): a smashable object (is_smashable) without one gets a destroyable
	if (!HasComponent(eReplicaComponentType::DESTROYABLE) && m_TemplateID != 176 && GetVarAs<int32_t>(u"is_smashable") != 0) {
		AddComponent<DestroyableComponent>(-1);
	}
	if (m_Character && HasComponent(eReplicaComponentType::CONTROLLABLE_PHYSICS)) {
		AddComponent<GhostComponent>(-1);
	}
	// A mission offer's object gets a proximity monitor after the walk when it has none (the registry's mission offer;
	// a character's is DLU's own)
	if (!m_Character && HasComponent(eReplicaComponentType::MISSION_OFFER) && !HasComponent(eReplicaComponentType::PROXIMITY_MONITOR)) {
		AddComponent<ProximityMonitorComponent>(-1);
	}
	const auto triggerInfo = GetVarAsString(u"trigger_id");
	if (!triggerInfo.empty()) AddComponent<TriggerComponent>(-1, triggerInfo);
}

void Entity::LoadTemplateData() {
	const auto& object = CDClientManager::GetTable<CDObjectsTable>()->GetByID(m_TemplateID);
	if (object.id == m_TemplateID && object.interactionDistance >= 0.0f) m_InteractionDistance = object.interactionDistance;
}

// The client erases interaction_distance from the config once read; server scripts still read it, so it stays
void Entity::LoadConfigData() {
	if (HasVar(u"interaction_distance")) m_InteractionDistance = GetVarAs<float>(u"interaction_distance");
	if (HasVar(u"objectWorldState")) m_ObjectWorldState = GetVarAs<int32_t>(u"objectWorldState");
	if (HasVar(u"CheckPrecondition")) m_CheckPrecondition = GetVar<std::u16string>(u"CheckPrecondition");
}

void Entity::Initialize() {
	RegisterMsg(&Entity::MsgRequestServerObjectInfo);
	RegisterMsg(&Entity::MsgDropClientLoot);
	RegisterMsg(&Entity::MsgGetFactionTokenType);
	RegisterMsg(&Entity::MsgPickupItem);
	RegisterMsg(&Entity::MsgChildRemoved);
	RegisterMsg(&Entity::MsgGetFlag);
	/**
	 * Setup groups
	 */

	const auto groupIDs = GetVarAsString(u"groupID");

	if (!groupIDs.empty()) {
		m_Groups = GeneralUtils::SplitString(groupIDs, ';');
		if (!m_Groups.empty()) {
			if (m_Groups.back().empty()) m_Groups.erase(m_Groups.end() - 1);
		}
	}

	/**
	 * Set ourselves as a child of our parent
	 */

	if (m_ParentEntity != nullptr) {
		m_ParentEntity->AddChild(this);
	}

	LoadTemplateData();

	m_LoadingComponents = true;
	auto loadState = DoObjectPreLoad();
	DoObjectComponentLoad(loadState);
	m_LoadingComponents = false;

	// LWOGameObject::MakeReadyForSerialization (0x0101c230): every component exists; the object loads its config, then
	// each component the config and a character's save, in the order they were made
	LoadConfigData();
	for (auto* const component : m_ComponentList) {
		component->LoadConfigData();
		if (m_Character) component->LoadFromSaveData(m_Character->GetXMLDoc());
	}

	// Every component exists: the client's Startup step
	for (auto* const component : m_ComponentList) component->Startup();
	m_ComponentsStarted = true;

	// Hacky way to trigger these when the object has had a chance to get constructed
	AddCallbackTimer(0, [this]() {
		this->GetScript()->OnStartup(this);
		if (this->m_ParentEntity) {
			GameMessages::ChildLoaded childLoaded;
			childLoaded.childID = this->m_ObjectID;
			childLoaded.templateID = this->GetLOT();
			this->m_ParentEntity->OnChildLoaded(childLoaded);
		}
		});

	if (!m_Character && Game::entityManager->GetGhostingEnabled()) {
		// Don't ghost what is likely large scene elements
		if (HasComponent(eReplicaComponentType::SIMPLE_PHYSICS) && HasComponent(eReplicaComponentType::RENDER) && (m_Components.size() == 2 || (HasComponent(eReplicaComponentType::TRIGGER) && m_Components.size() == 3))) {
			goto no_ghosting;
		}

		/* Filter for ghosting candidates.
		 *
		 * Don't ghost moving platforms, until we've got proper syncing for those.
		 * Don't ghost big phantom physics triggers, as putting those to sleep might prevent interactions.
		 * Don't ghost property related objects, as the client expects those to always be loaded.
		 */
		if (
			!EntityManager::IsExcludedFromGhosting(GetLOT()) &&
			!HasComponent(eReplicaComponentType::SCRIPTED_ACTIVITY) &&
			!HasComponent(eReplicaComponentType::MOVING_PLATFORM) &&
			!HasComponent(eReplicaComponentType::PHANTOM_PHYSICS) &&
			!HasComponent(eReplicaComponentType::PROPERTY) &&
			!HasComponent(eReplicaComponentType::RACING_CONTROL) &&
			!HasComponent(eReplicaComponentType::HAVOK_VEHICLE_PHYSICS)
			)
			//if (HasComponent(eReplicaComponentType::BASE_COMBAT_AI))
		{
			m_IsGhostingCandidate = true;
		}

		if (GetLOT() == 6368) {
			m_IsGhostingCandidate = true;
		}

		// Special case for collectibles in Ninjago
		if (HasComponent(eReplicaComponentType::COLLECTIBLE) && Game::server->GetZoneID() == 2000) {
			m_IsGhostingCandidate = true;
		}
	}

no_ghosting:

	TriggerEvent(eTriggerEventType::CREATE, this);

	if (m_Character) {
		auto* controllablePhysicsComponent = GetComponent<ControllablePhysicsComponent>();
		auto* levelComponent = GetComponent<LevelProgressionComponent>();

		if (controllablePhysicsComponent && levelComponent) {
			controllablePhysicsComponent->SetSpeedMultiplier(levelComponent->GetSpeedBase() / 500.0f);
		}
	}
}

void Entity::OnEnterScope(Entity& player) {
	for (auto* const component : m_Components | std::views::values) if (component) component->OnEnterScope(player);
}

void Entity::OnLeaveScope(Entity& player) {
	for (auto* const component : m_Components | std::views::values) if (component) component->OnLeaveScope(player);
}

bool Entity::operator==(const Entity& other) const {
	return other.m_ObjectID == m_ObjectID;
}

bool Entity::operator!=(const Entity& other) const {
	return !operator==(other);
}

Component* Entity::GetComponent(eReplicaComponentType componentID) const {
	const auto& index = m_Components.find(componentID);
	return index != m_Components.end() ? index->second : nullptr;
}

bool Entity::HasComponent(const eReplicaComponentType componentId) const {
	return m_Components.contains(componentId);
}

void Entity::Subscribe(LWOOBJID scriptObjId, CppScripts::Script* scriptToAdd, const std::string& notificationName) {
	if (notificationName == "HitOrHealResult" || notificationName == "Hit") {
		auto* const  destroyableComponent = GetComponent<DestroyableComponent>();
		if (!destroyableComponent) return;
		destroyableComponent->Subscribe(scriptObjId, scriptToAdd);
	} else if (notificationName == "PlayerResurrectionFinished") {
		LOG("Subscribing to PlayerResurrectionFinished");
		m_Subscriptions[scriptObjId][notificationName] = scriptToAdd;
	}
}

void Entity::Unsubscribe(LWOOBJID scriptObjId, const std::string& notificationName) {
	if (notificationName == "HitOrHealResult" || notificationName == "Hit") {
		auto* const  destroyableComponent = GetComponent<DestroyableComponent>();
		if (!destroyableComponent) return;
		destroyableComponent->Unsubscribe(scriptObjId);
	} else if (notificationName == "PlayerResurrectionFinished") {
		LOG("Unsubscribing from PlayerResurrectionFinished");
		m_Subscriptions[scriptObjId].erase(notificationName);
	}
}

void Entity::SetProximityRadius(float proxRadius, std::string name, uint32_t collisionGroup) {
	auto* proxMon = GetComponent<ProximityMonitorComponent>();
	if (!proxMon) proxMon = AddComponent<ProximityMonitorComponent>(-1);
	proxMon->SetProximityRadius(proxRadius, name, collisionGroup);
}

void Entity::SetProximityRadius(dpEntity* entity, std::string name) {
	auto* proxMon = GetComponent<ProximityMonitorComponent>();
	if (!proxMon) proxMon = AddComponent<ProximityMonitorComponent>(-1);
	proxMon->SetProximityRadius(entity, name);
}

void Entity::SetGMLevel(eGameMasterLevel value) {
	m_GMLevel = value;
	if (m_Character) m_Character->SetGMLevel(value);

	auto* const characterComponent = GetComponent<CharacterComponent>();
	if (!characterComponent) return;

	characterComponent->SetGMLevel(value);

	GameMessages::SetGMLevel setGMLevel;
	setGMLevel.target = m_ObjectID;
	setGMLevel.level = value;
	setGMLevel.Send(UNASSIGNED_SYSTEM_ADDRESS);

	// Update the chat server of our GM Level
	{
		ChatPackets::GMLevelUpdate update;
		update.playerID = m_ObjectID;
		update.gmLevel = m_GMLevel;
		ChatServerLink::Send(update);
	}
}

bool Entity::IsVarSentToClient(const std::u16string& name) const {
	if (GetComponent<ModelComponent>() && !GetComponent<PetComponent>()) return m_Settings.values.contains(name);
	const auto& syncLDF = GetVar<std::vector<std::u16string>>(u"syncLDF");
	return std::ranges::find(syncLDF, name) != syncLDF.end();
}

void Entity::WriteLDFData(const LwoNameValue& ldf, RakNet::BitStream& outBitStream) const {
	RakNet::BitStream settingStream;
	int32_t numberOfValidKeys = ldf.values.size();

	// Writing keys value pairs the client does not expect to receive or interpret will result in undefined behavior,
	// so we need to filter out any keys that are not valid and fix the number of valid keys to be correct.
	for (const auto& data : ldf.values | std::views::values) {
		if (data && data->GetValueType() != eLDFType::LDF_TYPE_UNKNOWN) {
			data->WriteToPacket(settingStream);
		} else {
			numberOfValidKeys--;
		}
	}

	// Now write it to the main bitstream
	outBitStream.Write<uint32_t>(settingStream.GetNumberOfBytesUsed() + 1 + sizeof(uint32_t));
	outBitStream.Write<uint8_t>(0); //no compression used
	outBitStream.Write(numberOfValidKeys);
	outBitStream.Write(settingStream);
}

void Entity::WriteBaseReplicaData(RakNet::BitStream& outBitStream, eReplicaPacketType packetType) {
	if (packetType == eReplicaPacketType::CONSTRUCTION) {
		outBitStream.Write(m_ObjectID);
		outBitStream.Write(m_TemplateID);

		const auto& name = GeneralUtils::ASCIIToUTF16(IsPlayer() ?
			m_Character ? m_Character->GetName() : "Invalid"
			: GetVar<std::string>(u"npcName"));

		outBitStream.Write<uint8_t>(name.size());
		outBitStream.Write(name);

		// Live sent the object's age on this server in milliseconds: 0 for an object constructed as it is created
		// (a player's own construction on load), the time since load for zone objects.
		outBitStream.Write<uint32_t>(GetTimeSinceCreatedMs());

		const auto& syncLDF = GetVar<std::vector<std::u16string>>(u"syncLDF");

		// Only sync for models.
		if (!m_Settings.values.empty() && (GetComponent<ModelComponent>() && !GetComponent<PetComponent>())) {
			outBitStream.Write1(); // Has ldf data
			WriteLDFData(m_Settings, outBitStream);
		} else if (!syncLDF.empty()) {
			// Find all the ldf data we need to write
			LwoNameValue ldfData;

			for (const auto& data : syncLDF) {
				const auto* toInsert = GetVarData(data);
				if (toInsert) ldfData.values.insert_or_assign(data, toInsert->Copy());
			}

			outBitStream.Write1(); // Has ldf data
			WriteLDFData(ldfData, outBitStream);
		} else {
			outBitStream.Write0(); // No ldf data
		}

		const auto* const triggerComponent = GetComponent<TriggerComponent>();
		if (triggerComponent) {
			// Has trigger component, check to see if we have events to handle
			auto* trigger = triggerComponent->GetTrigger();
			outBitStream.Write<bool>(trigger && !trigger->events.empty());
		} else { // No trigger componenet, so definitely no triggers
			outBitStream.Write0();
		}

		const bool hasParent = m_ParentEntity != nullptr || m_SpawnerID != 0;
		outBitStream.Write(hasParent);
		if (hasParent) {
			// 触るな！
			if (m_ParentEntity != nullptr) outBitStream.Write(GeneralUtils::SetBit(m_ParentEntity->GetObjectID(), static_cast<uint32_t>(eObjectBits::CLIENT)));
			else if (m_Spawner != nullptr && m_Spawner->m_Info.isNetwork) outBitStream.Write(m_SpawnerID);
			else outBitStream.Write(GeneralUtils::SetBit(m_SpawnerID, static_cast<uint32_t>(eObjectBits::CLIENT)));
		}

		outBitStream.Write(m_HasSpawnerNodeID);
		if (m_HasSpawnerNodeID) outBitStream.Write(m_SpawnerNodeID);

		// This zero check should not be here?
		const bool hasDefaultScale = m_Scale != 1.0f && m_Scale != 0.0f;
		outBitStream.Write(hasDefaultScale);
		if (hasDefaultScale) outBitStream.Write(m_Scale);

		outBitStream.Write0(); //ObjectWorldState

		const bool hasGMLevel = m_GMLevel != eGameMasterLevel::CIVILIAN;
		outBitStream.Write(hasGMLevel);
		if (hasGMLevel) outBitStream.Write(m_GMLevel);
	}

	// Only serialize parent / child info should the info be dirty (changed) or if this is the construction of the entity.
	const bool writeParentChild = m_IsParentChildDirty || packetType == eReplicaPacketType::CONSTRUCTION;
	outBitStream.Write(writeParentChild);
	if (writeParentChild) {
		m_IsParentChildDirty = false;
		outBitStream.Write(m_ParentEntity != nullptr);
		if (m_ParentEntity) {
			outBitStream.Write(m_ParentEntity->GetObjectID());
			outBitStream.Write(m_UpdatesWithParentPosition);
		}
		outBitStream.Write(!m_ChildEntities.empty());
		if (!m_ChildEntities.empty()) {
			outBitStream.Write<uint16_t>(m_ChildEntities.size());
			for (const auto* const child : m_ChildEntities) {
				if (child) outBitStream.Write<LWOOBJID>(child->GetObjectID());
			}
		}
	}
}

namespace {
	/**
	 * The order the client reads components in (it builds an object's component list in this order in
	 * ObjectLoader2::DoObjectComponentLoad). CHARACTER is preceded by POSSESSOR, LEVEL_PROGRESSION and
	 * PLAYER_FORCED_MOVEMENT, which WriteComponents writes with it.
	 */
	constexpr auto SERIALIZATION_ORDER = [] {
		using enum eReplicaComponentType;
		return std::array{
			POSSESSABLE, MODULE_ASSEMBLY, CONTROLLABLE_PHYSICS, SIMPLE_PHYSICS, RIGID_BODY_PHANTOM_PHYSICS, HAVOK_VEHICLE_PHYSICS,
			PHANTOM_PHYSICS, SOUND_TRIGGER, RACING_SOUND_TRIGGER, BUFF, DESTROYABLE, COLLECTIBLE, PET, PLAYER_FORCED_MOVEMENT, CHARACTER,
			INVENTORY,
			SCRIPT, SKILL, BASE_COMBAT_AI, ITEM, QUICK_BUILD, MOVING_PLATFORM, SWITCH, VENDOR, DONATION_VENDOR, ACHIEVEMENT_VENDOR,
			BOUNCER, SCRIPTED_ACTIVITY, SHOOTING_GALLERY, RACING_CONTROL, LUP_EXHIBIT, MODEL, RENDER, MINI_GAME_CONTROL, BBB,
		};
	}();

	// The component in SERIALIZATION_ORDER the destroyable is written in front of (DESTROYABLE: at its own place)
	eReplicaComponentType DestroyableSerializationSlot(const Entity& entity) {
		using enum eReplicaComponentType;
		// A DESTROYABLE registry entry puts the destroyable (with the buff in front of it) at its own place. Collectibles
		// and quick builds without one get theirs right before themselves, which for a collectible is the same place.
		if (entity.HasComponent(BUFF) || entity.HasComponent(COLLECTIBLE)) return DESTROYABLE;
		if (entity.HasComponent(QUICK_BUILD)) return QUICK_BUILD;
		// Anything else that has one (is_smashable objects, models) writes it after the others
		return MINI_GAME_CONTROL;
	}
}

void Entity::WriteComponents(RakNet::BitStream& outBitStream, eReplicaPacketType packetType) const {
	const bool bIsInitialUpdate = packetType == eReplicaPacketType::CONSTRUCTION;
	auto* const destroyableComponent = GetComponent<DestroyableComponent>();
	const auto destroyableSlot = DestroyableSerializationSlot(*this);

	using enum eReplicaComponentType;
	for (const auto type : SERIALIZATION_ORDER) {
		if (type == destroyableSlot && destroyableComponent) destroyableComponent->Serialize(outBitStream, bIsInitialUpdate);
		if (type == DESTROYABLE) continue;

		auto* const component = GetComponent(type);
		if (!component) continue;
		// A character writes its forced movement with its other parts (below); an object without one (the registry
		// gives it, LOT 16684) writes it on its own, where the client reads it
		if (type == PLAYER_FORCED_MOVEMENT && HasComponent(CHARACTER)) continue;

		if (type == CHARACTER) {
			// Written with the character component; every character has them, a 0 bit stands in should one be missing
			for (const auto characterPart : { POSSESSOR, LEVEL_PROGRESSION, PLAYER_FORCED_MOVEMENT }) {
				auto* const part = GetComponent(characterPart);
				if (part) part->Serialize(outBitStream, bIsInitialUpdate);
				else outBitStream.Write0();
			}
		}
		component->Serialize(outBitStream, bIsInitialUpdate);
	}
}

void Entity::UpdateXMLDoc(tinyxml2::XMLDocument& doc) {
	//This function should only ever be called from within Character, meaning doc should always exist when this is called.
	//Naturally, we don't include any non-player components in this update function.

	for (const auto& component : m_Components | std::views::values) {
		if (!component) continue;

		component->WriteSaveData(doc);
	}
}

void Entity::Update(const float deltaTime) {
	uint32_t timerPosition;
	for (timerPosition = 0; timerPosition < m_Timers.size();) {
		auto& timer = m_Timers[timerPosition];
		timer.Update(deltaTime);
		// If the timer is expired, erase it and dont increment the position because the next timer will be at the same position.
		// Before: [0, 1, 2, 3, ..., n]
		// timerPosition  ^
		// After:  [0, 1, 3, ..., n]
		// timerPosition  ^
		if (timer.GetTime() <= 0) {
			// Remove the timer from the list of timers first so that scripts and events can remove timers without causing iterator invalidation
			auto timerName = timer.GetName();
			m_Timers.erase(m_Timers.begin() + timerPosition);
			Profiler::Scope profile("Script timer", Profiler::Phase::SCRIPTS);
			GetScript()->OnTimerDone(this, timerName);
			VanityUtilities::OnTimerDone(this, timerName);

			TriggerEvent(eTriggerEventType::TIMER_DONE, this);
		} else {
			// If the timer isnt expired, go to the next timer.
			timerPosition++;
		}
	}

	for (timerPosition = 0; timerPosition < m_CallbackTimers.size(); ) {
		// If the timer is expired, erase it and dont increment the position because the next timer will be at the same position.
		// Before: [0, 1, 2, 3, ..., n]
		// timerPosition  ^
		// After:  [0, 1, 3, ..., n]
		// timerPosition  ^
		auto& callbackTimer = m_CallbackTimers[timerPosition];
		callbackTimer.Update(deltaTime);
		if (callbackTimer.GetTime() <= 0) {
			// Remove the timer from the list of timers first so that callbacks can remove timers without causing iterator invalidation
			auto callback = callbackTimer.GetCallback();
			m_CallbackTimers.erase(m_CallbackTimers.begin() + timerPosition);
			callback();
		} else {
			timerPosition++;
		}
	}

	// Add pending timers to the list of timers so they start next tick.
	if (!m_PendingTimers.empty()) {
		m_Timers.reserve(m_Timers.size() + m_PendingTimers.size());
		m_Timers.insert(m_Timers.end(), m_PendingTimers.begin(), m_PendingTimers.end());
		m_PendingTimers.clear();
	}

	if (!m_PendingCallbackTimers.empty()) {
		m_CallbackTimers.reserve(m_CallbackTimers.size() + m_PendingCallbackTimers.size());
		m_CallbackTimers.insert(m_CallbackTimers.end(), m_PendingCallbackTimers.begin(), m_PendingCallbackTimers.end());
		m_PendingCallbackTimers.clear();
	}

	if (IsSleeping()) {
		if (!m_IsSleeping) {
			Sleep();
			m_IsSleeping = true;
		}

		return;
	} else {
		if (m_IsSleeping) {
			m_IsSleeping = false;
			Wake();
		}
	}


	GetScript()->OnUpdate(this);

	for (const auto& component : m_Components | std::views::values) {
		if (!component) continue;

		component->Run(deltaTime);
	}

	if (m_ShouldDestroyAfterUpdate) {
		Game::entityManager->DestroyEntity(this->GetObjectID());
	}
}

void Entity::OnCollisionProximity(LWOOBJID otherEntity, const std::string& proxName, const std::string& status) {
	Entity* const other = Game::entityManager->GetEntity(otherEntity);
	if (!other) return;

	GetScript()->OnProximityUpdate(this, other, proxName, status);
	VanityUtilities::OnProximityUpdate(this, other, proxName, status);

	auto* const rocketComp = GetComponent<RocketLaunchpadControlComponent>();
	if (!rocketComp) return;

	rocketComp->OnProximityUpdate(other, proxName, status);
}

void Entity::OnCollisionPhantom(const LWOOBJID otherEntity) {
	auto* other = Game::entityManager->GetEntity(otherEntity);
	if (!other) return;

	GetScript()->OnCollisionPhantom(this, other);

	for (const auto& callback : m_PhantomCollisionCallbacks) {
		callback(other);
	}

	SwitchComponent* const switchComp = GetComponent<SwitchComponent>();
	if (switchComp) {
		switchComp->EnterIfResponding(other);
	}

	TriggerEvent(eTriggerEventType::ENTER, other);

	// POI system
	const auto& poi = GetVar<std::u16string>(u"POI");

	if (!poi.empty()) {
		auto* missionComponent = other->GetComponent<MissionComponent>();

		if (missionComponent != nullptr) {
			missionComponent->Progress(eMissionTaskType::EXPLORE, 0, 0, GeneralUtils::UTF16ToWTF8(poi));
		}
	}

	if (!other->GetIsDead()) {
		if (GetComponent<BaseCombatAIComponent>() != nullptr) {
			const auto index = std::ranges::find(m_TargetsInPhantom, otherEntity);

			if (index != m_TargetsInPhantom.end()) return;

			m_TargetsInPhantom.push_back(otherEntity);
		}
	}
}

void Entity::OnCollisionLeavePhantom(const LWOOBJID otherEntity) {
	auto* const other = Game::entityManager->GetEntity(otherEntity);
	if (!other) return;

	GetScript()->OnOffCollisionPhantom(this, other);

	TriggerEvent(eTriggerEventType::EXIT, other);

	SwitchComponent* const switchComp = GetComponent<SwitchComponent>();
	if (switchComp) {
		switchComp->EntityLeave(other);
	}

	const auto index = std::ranges::find(m_TargetsInPhantom, otherEntity);

	if (index == m_TargetsInPhantom.end()) return;

	m_TargetsInPhantom.erase(index);
}

void Entity::OnFireEventServerSide(Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	GetScript()->OnFireEventServerSide(this, sender, args, param1, param2, param3);
}

void Entity::OnActivityStateChangeRequest(LWOOBJID senderID, int32_t value1, int32_t value2, const std::u16string& stringValue) {
	GetScript()->OnActivityStateChangeRequest(this, senderID, value1, value2, stringValue);
}

void Entity::OnCinematicUpdate(Entity* self, Entity* sender, eCinematicEvent event, const std::u16string& pathName,
	float_t pathTime, float_t totalTime, int32_t waypoint) {
	GetScript()->OnCinematicUpdate(self, sender, event, pathName, pathTime, totalTime, waypoint);
}

void Entity::NotifyObject(Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	GameMessages::NotifyObject(GetObjectID(), sender->GetObjectID(), GeneralUtils::ASCIIToUTF16(name)).Send(UNASSIGNED_SYSTEM_ADDRESS);

	GetScript()->OnNotifyObject(this, sender, name, param1, param2);
}

void Entity::OnEmoteReceived(const int32_t emote, Entity* target) {
	GetScript()->OnEmoteReceived(this, emote, target);
}

void Entity::RequestUse(const GameMessages::RequestUse& msg, Entity& user) {
	if (!msg.IsMissionChoice()) {
		TriggerEvent(eTriggerEventType::INTERACT, &user);
		GetScript()->OnUse(this, &user);
	}

	GameMessages::RequestUseEvent event(msg, user);
	HandleMsg(event);
}

void Entity::OnUse(Entity* originator) {
	GameMessages::RequestUse msg;
	msg.target = originator->GetObjectID();
	msg.object = m_ObjectID;
	RequestUse(msg, *originator);
}

void Entity::OnHitOrHealResult(Entity* attacker, int32_t damage) {
	GetScript()->OnHitOrHealResult(this, attacker, damage);
}

void Entity::OnHit(Entity* attacker) {
	TriggerEvent(eTriggerEventType::HIT, attacker);
	GetScript()->OnHit(this, attacker);
}

void Entity::OnZonePropertyEditBegin() {
	GetScript()->OnZonePropertyEditBegin(this);
}

void Entity::OnZonePropertyEditEnd() {
	GetScript()->OnZonePropertyEditEnd(this);
}

void Entity::OnZonePropertyModelEquipped() {
	GetScript()->OnZonePropertyModelEquipped(this);
}

void Entity::OnZonePropertyModelPlaced(Entity* player) {
	GetScript()->OnZonePropertyModelPlaced(this, player);
}

void Entity::OnZonePropertyModelPickedUp(Entity* player) {
	GetScript()->OnZonePropertyModelPickedUp(this, player);
}

void Entity::OnZonePropertyModelRemoved(Entity* player) {
	GetScript()->OnZonePropertyModelRemoved(this, player);
}

void Entity::OnZonePropertyModelRemovedWhileEquipped(Entity* player) {
	GetScript()->OnZonePropertyModelRemovedWhileEquipped(this, player);
}

void Entity::OnZonePropertyModelRotated(Entity* player) {
	GetScript()->OnZonePropertyModelRotated(this, player);
}

void Entity::OnMessageBoxResponse(Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	GetScript()->OnMessageBoxResponse(this, sender, button, identifier, userData);
}

void Entity::OnChoiceBoxResponse(Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) {
	GetScript()->OnChoiceBoxResponse(this, sender, button, buttonIdentifier, identifier);
}

void Entity::OnActivityNotify(GameMessages::ActivityNotify& notify) {
	GetScript()->OnActivityNotify(this, notify);
}

void Entity::OnShootingGalleryFire(GameMessages::ShootingGalleryFire& fire) {
	GetScript()->OnShootingGalleryFire(*this, fire);
}

void Entity::OnChildLoaded(GameMessages::ChildLoaded& childLoaded) {
	GetScript()->OnChildLoaded(*this, childLoaded);
}

void Entity::NotifyPlayerResurrectionFinished(GameMessages::PlayerResurrectionFinished& msg) {
	for (const auto& scriptList : m_Subscriptions | std::views::values) {
		auto it = scriptList.find("PlayerResurrectionFinished");
		if (it == scriptList.end()) continue;

		it->second->NotifyPlayerResurrectionFinished(*this, msg);
	}
}

void Entity::RequestActivityExit(Entity* sender, LWOOBJID player, bool canceled) {
	GetScript()->OnRequestActivityExit(sender, player, canceled);
}

CppScripts::Script* const Entity::GetScript() const {
	auto* scriptComponent = GetComponent<ScriptComponent>();
	auto* script = scriptComponent ? scriptComponent->GetScript() : CppScripts::GetInvalidScript();
	DluAssert(script != nullptr);
	return script;
}

void Entity::Smash(const LWOOBJID source, const eKillType killType, const std::u16string& deathType) {
	if (!m_PlayerIsReadyForUpdates) return;

	auto* destroyableComponent = GetComponent<DestroyableComponent>();
	if (destroyableComponent == nullptr) {
		Kill(Game::entityManager->GetEntity(source));
		return;
	}
	auto* possessorComponent = GetComponent<PossessorComponent>();
	if (possessorComponent) {
		if (possessorComponent->GetPossessable() != LWOOBJID_EMPTY) {
			auto* mount = Game::entityManager->GetEntity(possessorComponent->GetPossessable());
			if (mount) possessorComponent->Dismount(mount, true);
		}
	}

	destroyableComponent->Smash(source, killType, deathType);
}

void Entity::Kill(Entity* murderer, const eKillType killType) {
	if (!m_PlayerIsReadyForUpdates) return;

	for (const auto& cb : m_DieCallbacks) {
		cb();
	}

	m_DieCallbacks.clear();

	//お前はもう死んでいる

	GetScript()->OnDie(this, murderer);

	if (m_Spawner != nullptr) {
		m_Spawner->NotifyOfEntityDeath(m_ObjectID);
	}

	if (!IsPlayer()) {
		auto* destroyableComponent = GetComponent<DestroyableComponent>();
		bool waitForDeathAnimation = false;

		if (destroyableComponent) {
			waitForDeathAnimation = !destroyableComponent->GetIsSmashable() && destroyableComponent->GetDeathBehavior() == 0 && killType != eKillType::SILENT;
		}

		// Live waited a hard coded 12 seconds for death animations of type 0 before networking destruction!
		constexpr float DelayDeathTime = 12.0f;
		if (waitForDeathAnimation) AddCallbackTimer(DelayDeathTime, [this]() { Game::entityManager->DestroyEntity(this); });
		else Game::entityManager->DestroyEntity(this);
	}

	const auto& grpNameQBShowBricks = GetVarAsString(u"grpNameQBShowBricks");
	if (!grpNameQBShowBricks.empty()) {
		for (auto* const spawner : Game::zoneManager->GetSpawnersByName(grpNameQBShowBricks)) if (spawner) spawner->Spawn();
		for (auto* const spawner : Game::zoneManager->GetSpawnersInGroup(grpNameQBShowBricks)) if (spawner) spawner->Spawn();
	}

	// Track a player being smashed
	auto* characterComponent = GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->UpdatePlayerStatistic(TimesSmashed);
	}

}

void Entity::AddDieCallback(const std::function<void()>& callback) {
	m_DieCallbacks.push_back(callback);
}

void Entity::AddCollisionPhantomCallback(const std::function<void(Entity* target)>& callback) {
	m_PhantomCollisionCallbacks.push_back(callback);
}

void Entity::AddQuickBuildCompleteCallback(const std::function<void(Entity* user)>& callback) const {
	auto* quickBuildComponent = GetComponent<QuickBuildComponent>();
	if (quickBuildComponent != nullptr) {
		quickBuildComponent->AddQuickBuildCompleteCallback(callback);
	}
}

bool Entity::GetIsDead() const {
	DestroyableComponent* dest = GetComponent<DestroyableComponent>();
	return dest && dest->GetArmor() == 0 && dest->GetHealth() == 0;
}

void Entity::AddLootItem(const Loot::Info& info) const {
	if (!IsPlayer()) return;

	auto* const characterComponent = GetComponent<CharacterComponent>();
	if (!characterComponent) return;
	LOG("Player %llu has been allowed to pickup %i with id %llu", m_ObjectID, info.lot, info.id);
	auto& droppedLoot = characterComponent->GetDroppedLoot();
	droppedLoot[info.id] = info;
}

void Entity::PickupItem(const LWOOBJID& objectID) const {
	if (!IsPlayer()) return;
	auto* const inv = GetComponent<InventoryComponent>();
	auto* const characterComponent = GetComponent<CharacterComponent>();
	if (!inv || !characterComponent) return;

	CDObjectsTable* objectsTable = CDClientManager::GetTable<CDObjectsTable>();

	auto& droppedLoot = characterComponent->GetDroppedLoot();

	const auto itr = droppedLoot.find(objectID);

	if (itr != droppedLoot.end()) {
		const auto& info = itr->second;
		auto* const characterComponent = GetComponent<CharacterComponent>();
		if (characterComponent != nullptr) {
			characterComponent->TrackLOTCollection(info.lot);
		}

		const CDObjects& object = objectsTable->GetByID(info.lot);
		if (object.id != 0 && object.type == "Powerup") {
			auto* const skillsTable = CDClientManager::GetTable<CDObjectSkillsTable>();
			const auto skills = skillsTable->Query([&info](CDObjectSkills entry) {return (entry.objectTemplate == info.lot); });
			for (const auto& skill : skills) {
				const auto [skillComponent, missionComponent] = GetComponentsMut<SkillComponent, MissionComponent>();
				if (skillComponent) skillComponent->CastSkill(skill.skillID, GetObjectID(), GetObjectID(), skill.castOnType);

				if (missionComponent != nullptr) {
					missionComponent->Progress(eMissionTaskType::POWERUP, skill.skillID);
				}
			}
			EconomyLedger::RecordMapEvent(IEconomyLedger::eMapEvent::POWERUP_PICKUPS, info.lot, GetPosition(), 1, this);
		} else {
			// Live told the client which object the item dropped from (all 2,344 live pickups of a dropped item)
			LootMetrics metrics{};
			if (info.sourceLot != 0) metrics.sourceLot = info.sourceLot;
			inv->AddItem(info.lot, info.count, eLootSourceType::PICKUP, eInventoryType::INVALID, {}, LWOOBJID_EMPTY, true, false, LWOOBJID_EMPTY, eInventoryType::INVALID, 1, false, -1, metrics);
		}

		droppedLoot.erase(objectID);
	}
}

bool Entity::PickupCoins(uint64_t count) const {
	if (!IsPlayer()) return false;

	auto* const characterComponent = GetComponent<CharacterComponent>();
	if (!characterComponent) return false;

	const auto droppedCoins = characterComponent->GetDroppedCoins();
	if (count > droppedCoins) {
		return false;
	} else {
		characterComponent->SetDroppedCoins(droppedCoins - count);
		return true;
	}
}

void Entity::RegisterCoinDrop(uint64_t count) const {
	if (!IsPlayer()) return;

	auto* const characterComponent = GetComponent<CharacterComponent>();
	if (!characterComponent) return;

	characterComponent->SetDroppedCoins(characterComponent->GetDroppedCoins() + count);
}

void Entity::AddChild(Entity* child) {
	m_IsParentChildDirty = true;
	m_ChildEntities.push_back(child);
}

void Entity::RemoveChild(Entity* child) {
	if (!child) return;
	uint32_t entityPosition = 0;
	while (entityPosition < m_ChildEntities.size()) {
		auto* const childEntity = m_ChildEntities[entityPosition];
		if (!childEntity || childEntity->GetObjectID() == child->GetObjectID()) {
			m_IsParentChildDirty = true;
			m_ChildEntities.erase(m_ChildEntities.begin() + entityPosition);
		} else {
			entityPosition++;
		}
	}
}

void Entity::RemoveParent() {
	this->m_ParentEntity = nullptr;
	m_IsParentChildDirty = true;
}

void Entity::AddTimer(const std::string& name, float time) {
	m_PendingTimers.emplace_back(name, time);
}

void Entity::AddCallbackTimer(float time, const std::function<void()> callback) {
	m_PendingCallbackTimers.emplace_back(time, callback);
}

bool Entity::HasTimer(const std::string& name) {
	return std::find(m_Timers.begin(), m_Timers.end(), name) != m_Timers.end();
}

void Entity::CancelCallbackTimers() {
	m_CallbackTimers.clear();
	m_PendingCallbackTimers.clear();
}

void Entity::ScheduleKillAfterUpdate(Entity* murderer) {
	Game::entityManager->ScheduleForKill(this);

	if (murderer) m_ScheduleKiller = murderer;
}

void Entity::CancelTimer(const std::string& name) {
	for (int i = 0; i < m_Timers.size(); i++) {
		if (m_Timers[i] == name) {
			m_Timers.erase(m_Timers.begin() + i);
			return;
		}
	}
}

void Entity::CancelAllTimers() {
	m_Timers.clear();
	m_PendingTimers.clear();
	m_CallbackTimers.clear();
	m_PendingCallbackTimers.clear();
}

bool Entity::IsPlayer() const {
	return m_TemplateID == 1 && GetSystemAddress() != UNASSIGNED_SYSTEM_ADDRESS;
}

void Entity::TriggerEvent(eTriggerEventType event, Entity* optionalTarget) const {
	auto* const triggerComponent = GetComponent<TriggerComponent>();
	if (triggerComponent) triggerComponent->TriggerEvent(event, optionalTarget);
}

Entity* Entity::GetOwner() const {
	if (m_OwnerOverride != LWOOBJID_EMPTY) {
		auto* other = Game::entityManager->GetEntity(m_OwnerOverride);

		if (other != nullptr) {
			return other->GetOwner();
		}
	}

	return const_cast<Entity*>(this);
}

const NiPoint3& Entity::GetDefaultPosition() const {
	return m_DefaultPosition;
}

const NiQuaternion& Entity::GetDefaultRotation() const {
	return m_DefaultRotation;
}

float Entity::GetDefaultScale() const {
	return m_Scale;
}

void Entity::SetOwnerOverride(const LWOOBJID value) {
	m_OwnerOverride = value;
}

bool Entity::GetIsGhostingCandidate() const {
	return m_IsGhostingCandidate;
}

int8_t Entity::GetObservers() const {
	return m_Observers;
}

void Entity::SetObservers(int8_t value) {
	if (value < 0) {
		value = 0;
	}

	m_Observers = value;
}

void Entity::Sleep() const {
	auto* baseCombatAIComponent = GetComponent<BaseCombatAIComponent>();

	if (baseCombatAIComponent != nullptr) {
		baseCombatAIComponent->Sleep();
	}
}

void Entity::Wake() const {
	auto* baseCombatAIComponent = GetComponent<BaseCombatAIComponent>();

	if (baseCombatAIComponent != nullptr) {
		baseCombatAIComponent->Wake();
	}
}

bool Entity::IsSleeping() const {
	return m_IsGhostingCandidate && m_Observers == 0;
}


NiPoint3 Entity::GetPosition() const {
	GameMessages::GetPosition posMsg{};
	posMsg.pos = NiPoint3Constant::ZERO;
	HandleMsg(posMsg);
	return posMsg.pos;
}

const NiQuaternion& Entity::GetRotation() const {
	auto* controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr) {
		return controllable->GetRotation();
	}

	auto* phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr) {
		return phantom->GetRotation();
	}

	auto* simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr) {
		return simple->GetRotation();
	}

	auto* vehicel = GetComponent<HavokVehiclePhysicsComponent>();

	if (vehicel != nullptr) {
		return vehicel->GetRotation();
	}

	auto* rigidBodyPhantomPhysicsComponent = GetComponent<RigidbodyPhantomPhysicsComponent>();

	if (rigidBodyPhantomPhysicsComponent != nullptr) {
		return rigidBodyPhantomPhysicsComponent->GetRotation();
	}

	return QuatUtils::IDENTITY;
}

void Entity::SetPosition(const NiPoint3& position) {
	auto* controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr) {
		controllable->SetPosition(position);
	}

	auto* phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr) {
		phantom->SetPosition(position);
	}

	auto* simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr) {
		simple->SetPosition(position);
	}

	auto* vehicel = GetComponent<HavokVehiclePhysicsComponent>();

	if (vehicel != nullptr) {
		vehicel->SetPosition(position);
	}

	auto* rigidBodyPhantomPhysicsComponent = GetComponent<RigidbodyPhantomPhysicsComponent>();

	if (rigidBodyPhantomPhysicsComponent != nullptr) {
		rigidBodyPhantomPhysicsComponent->SetPosition(position);
	}

	Game::entityManager->SerializeEntity(this);
}

void Entity::SetRotation(const NiQuaternion& rotation) {
	auto* controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr) {
		controllable->SetRotation(rotation);
	}

	auto* phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr) {
		phantom->SetRotation(rotation);
	}

	auto* simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr) {
		simple->SetRotation(rotation);
	}

	auto* vehicel = GetComponent<HavokVehiclePhysicsComponent>();

	if (vehicel != nullptr) {
		vehicel->SetRotation(rotation);
	}

	auto* rigidBodyPhantomPhysicsComponent = GetComponent<RigidbodyPhantomPhysicsComponent>();

	if (rigidBodyPhantomPhysicsComponent != nullptr) {
		rigidBodyPhantomPhysicsComponent->SetRotation(rotation);
	}

	Game::entityManager->SerializeEntity(this);
}

void Entity::SetVelocity(const NiPoint3& velocity) {
	auto* controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr) {
		controllable->SetVelocity(velocity);
	}

	auto* simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr) {
		simple->SetVelocity(velocity);
	}

	Game::entityManager->SerializeEntity(this);
}

const NiPoint3& Entity::GetVelocity() const {
	auto* controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr) {
		return controllable->GetVelocity();
	}

	auto* simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr) {
		return simple->GetVelocity();
	}

	return NiPoint3Constant::ZERO;
}

bool Entity::GetBoolean(const std::u16string& name) const {
	return GetVar<bool>(name);
}

int32_t Entity::GetI32(const std::u16string& name) const {
	return GetVar<int32_t>(name);
}

int64_t Entity::GetI64(const std::u16string& name) const {
	return GetVar<int64_t>(name);
}

void Entity::SetBoolean(const std::u16string& name, const bool value) {
	SetVar(name, value);
}

void Entity::SetI32(const std::u16string& name, const int32_t value) {
	SetVar(name, value);
}

void Entity::SetI64(const std::u16string& name, const int64_t value) {
	SetVar(name, value);
}

bool Entity::HasVar(const std::u16string& name) const {
	return m_Settings.values.contains(name);
}

uint16_t Entity::GetNetworkId() const {
	return m_NetworkID;
}

void Entity::SetNetworkId(const uint16_t id) {
	m_NetworkID = id;
}

std::vector<LWOOBJID> Entity::GetTargetsInPhantom() {
	// Clean up invalid targets, like disconnected players
	m_TargetsInPhantom.erase(std::ranges::remove_if(m_TargetsInPhantom, [](const LWOOBJID id) {
		return !Game::entityManager->GetEntity(id);
		}).begin(), m_TargetsInPhantom.end());

	std::vector<LWOOBJID> enemies;
	for (const auto id : m_TargetsInPhantom) {
		const auto* const combat = GetComponent<BaseCombatAIComponent>();
		if (!combat || !combat->IsEnemy(id)) continue;

		enemies.push_back(id);
	}

	return enemies;
}

void Entity::SendNetworkVar(const std::string& data, const SystemAddress& sysAddr) {
	GameMessages::ScriptNetworkVarUpdate scriptVar;
	scriptVar.target = this->GetObjectID();
	scriptVar.tableOfVars = GeneralUtils::ASCIIToUTF16(data);
	scriptVar.Send(sysAddr);
}

const LDFBaseData* const Entity::GetVarData(const std::u16string& name) const {
	const auto itr = m_Settings.values.find(name);
	return itr != m_Settings.values.cend() ? itr->second.get() : nullptr;
}

std::string Entity::GetVarAsString(const std::u16string& name) const {
	const auto* const data = GetVarData(name);
	return data ? data->GetValueAsString() : "";
}

void Entity::Resurrect() {
	if (IsPlayer()) {
		DestroyableComponent::Resurrect(*this);
	}
}

void Entity::AddToGroup(const std::string& group) {
	if (std::ranges::find(m_Groups, group) == m_Groups.end()) {
		m_Groups.push_back(group);
	}
}

void Entity::RemoveFromGroup(const std::string& group) {
	std::erase(m_Groups, group);
}

void Entity::RetroactiveVaultSize() const {
	auto* const inventoryComponent = GetComponent<InventoryComponent>();
	if (!inventoryComponent) return;

	auto itemsVault = inventoryComponent->GetInventory(eInventoryType::VAULT_ITEMS);
	auto modelVault = inventoryComponent->GetInventory(eInventoryType::VAULT_MODELS);

	if (itemsVault->GetSize() == modelVault->GetSize()) return;

	modelVault->SetSize(itemsVault->GetSize());
}

uint8_t Entity::GetCollectibleID() const {
	auto* collectible = GetComponent<CollectibleComponent>();
	return collectible ? collectible->GetCollectibleId() : 0;
}

void Entity::ProcessPositionUpdate(PositionUpdate& update) {
	if (!IsPlayer()) return;
	auto* controllablePhysicsComponent = GetComponent<ControllablePhysicsComponent>();
	if (!controllablePhysicsComponent) return;

	auto* possessorComponent = GetComponent<PossessorComponent>();
	bool updateChar = true;

	if (possessorComponent) {
		auto* possassableEntity = Game::entityManager->GetEntity(possessorComponent->GetPossessable());

		if (possassableEntity) {
			auto* possessableComponent = possassableEntity->GetComponent<PossessableComponent>();

			// While possessing something, only update char if we are attached to the thing we are possessing
			updateChar = possessableComponent && possessableComponent->GetPossessionType() == ePossessionType::ATTACHED_VISIBLE;

			auto* havokVehiclePhysicsComponent = possassableEntity->GetComponent<HavokVehiclePhysicsComponent>();
			if (havokVehiclePhysicsComponent) {
				havokVehiclePhysicsComponent->SetPosition(update.position);
				havokVehiclePhysicsComponent->SetRotation(update.rotation);
				havokVehiclePhysicsComponent->SetIsOnGround(update.onGround);
				havokVehiclePhysicsComponent->SetIsOnRail(update.onRail);
				havokVehiclePhysicsComponent->SetVelocity(update.velocity);
				havokVehiclePhysicsComponent->SetAngularVelocity(update.angularVelocity);
				havokVehiclePhysicsComponent->SetRemoteInputInfo(update.remoteInputInfo);
			} else {
				// Need to get the mount's controllable physics
				auto* possessedControllablePhysicsComponent = possassableEntity->GetComponent<ControllablePhysicsComponent>();
				if (!possessedControllablePhysicsComponent) return;
				possessedControllablePhysicsComponent->SetPosition(update.position);
				possessedControllablePhysicsComponent->SetRotation(update.rotation);
				possessedControllablePhysicsComponent->SetIsOnGround(update.onGround);
				possessedControllablePhysicsComponent->SetIsOnRail(update.onRail);
				possessedControllablePhysicsComponent->SetVelocity(update.velocity);
				possessedControllablePhysicsComponent->SetAngularVelocity(update.angularVelocity);
			}
			Game::entityManager->SerializeEntity(possassableEntity);
		}
	}

	if (!updateChar) {
		update.velocity = NiPoint3Constant::ZERO;
		update.angularVelocity = NiPoint3Constant::ZERO;
	}

	// Handle statistics
	auto* characterComponent = GetComponent<CharacterComponent>();
	if (characterComponent) {
		characterComponent->TrackPositionUpdate(update.position);
	}

	controllablePhysicsComponent->SetPosition(update.position);
	controllablePhysicsComponent->SetRotation(update.rotation);
	controllablePhysicsComponent->SetIsOnGround(update.onGround);
	controllablePhysicsComponent->SetIsOnRail(update.onRail);
	controllablePhysicsComponent->SetVelocity(update.velocity);
	controllablePhysicsComponent->SetAngularVelocity(update.angularVelocity);

	auto* ghostComponent = GetComponent<GhostComponent>();
	if (ghostComponent) ghostComponent->SetGhostReferencePoint(update.position);
	Game::entityManager->QueueGhostUpdate(GetObjectID());

	if (updateChar) Game::entityManager->SerializeEntity(this);

	OnPlayerPositionUpdate.Notify(this, update);
}

const SystemAddress& Entity::GetSystemAddress() const {
	auto* characterComponent = GetComponent<CharacterComponent>();
	return characterComponent ? characterComponent->GetSystemAddress() : UNASSIGNED_SYSTEM_ADDRESS;
}

const NiPoint3& Entity::GetRespawnPosition() const {
	auto* characterComponent = GetComponent<CharacterComponent>();
	return characterComponent ? characterComponent->GetRespawnPosition() : NiPoint3Constant::ZERO;
}

const NiQuaternion& Entity::GetRespawnRotation() const {
	auto* characterComponent = GetComponent<CharacterComponent>();
	return characterComponent ? characterComponent->GetRespawnRotation() : QuatUtils::IDENTITY;
}

void Entity::SetRespawnPos(const NiPoint3& position) const {
	auto* characterComponent = GetComponent<CharacterComponent>();
	if (characterComponent) characterComponent->SetRespawnPos(position);
}

void Entity::SetRespawnRot(const NiQuaternion& rotation) const {
	auto* characterComponent = GetComponent<CharacterComponent>();
	if (characterComponent) characterComponent->SetRespawnRot(rotation);
}

int32_t Entity::GetCollisionGroup() const {
	for (const auto* component : m_Components | std::views::values) {
		auto* compToCheck = dynamic_cast<const PhysicsComponent*>(component);
		if (compToCheck) {
			return compToCheck->GetCollisionGroup();
		}
	}

	return 0;
}

// LWOGameObject::PassMessageToComponents (0x00f62500): every handler registered for the message, in registration
// order; only the ones there when the call starts (the client copies the list first; GetPosition is too hot for a copy)
bool Entity::HandleMsg(GameMessages::GameMsg& msg) const {
	const auto it = m_MsgHandlers.find(msg.msgId);
	if (it == m_MsgHandlers.end()) return false;

	bool handled = false;
	const auto& handlers = it->second;
	for (size_t i = 0, count = handlers.size(); i < count; i++) {
		if (handlers[i]) handled |= handlers[i](msg);
	}

	return handled;
}

void Entity::RegisterMsg(const MessageType::Game msgId, std::function<bool(GameMessages::GameMsg&)> handler) {
	m_MsgHandlers[msgId].push_back(std::move(handler));
}

bool Entity::MsgRequestServerObjectInfo(GameMessages::RequestServerObjectInfoEvent& event) {
	const auto& requestInfo = event.msg;
	AMFArrayValue response;
	response.Insert("visible", true);
	response.Insert("objectID", std::to_string(m_ObjectID));
	response.Insert("serverInfo", true);
	GameMessages::GetObjectReportInfo info{};
	info.clientID = requestInfo.clientId;
	info.bVerbose = requestInfo.bVerbose;
	info.info = response.InsertArray("data");
	auto& objectInfo = info.info->PushDebug("Object Details");
	auto* table = CDClientManager::GetTable<CDObjectsTable>();

	const auto& objTableInfo = table->GetByID(GetLOT());

	objectInfo.PushDebug<AMFStringValue>("Name", "name") = objTableInfo.name;
	objectInfo.PushDebug<AMFIntValue>("Template ID(LOT)", "LOT") = GetLOT();
	objectInfo.PushDebug<AMFStringValue>("Object ID", "LWOOBJID") = std::to_string(GetObjectID());
	objectInfo.PushDebug<AMFStringValue>("Spawner's Object ID", "LWOOBJID") = std::to_string(GetSpawnerID());
	objectInfo.PushDebug<AMFStringValue>("Owner override", "LWOOBJID") = std::to_string(m_OwnerOverride);
	auto& children = objectInfo.PushDebug("Child Objects");
	int i = 1;
	for (const auto* child : m_ChildEntities) {
		if (child) children.PushDebug<AMFStringValue>("Child " + std::to_string(i++), "LWOOBJID") = std::to_string(child->GetObjectID());
	}

	auto& componentDetails = objectInfo.PushDebug("Component Information");
	for (const auto [id, component] : m_Components) {
		componentDetails.PushDebug(StringifiedEnum::ToString(id));
	}

	auto& configData = objectInfo.PushDebug("Config Data");
	for (const auto& config : m_Settings.values | std::views::values) {
		configData.PushDebug<AMFStringValue>(GeneralUtils::UTF16ToWTF8(config->GetKey())) = config->GetValueAsString();
	}

	HandleMsg(info);

	auto* client = Game::entityManager->GetEntity(requestInfo.clientId);
	if (client) {
		GameMessages::UIMessageServerToAllClients uiMessage;
		uiMessage.strMessageName = "ToggleObjectDebugger";
		uiMessage.args = std::move(response);
		uiMessage.SendToClient(client->GetSystemAddress());
	}
	return true;
}

bool Entity::MsgDropClientLoot(GameMessages::DropClientLootEvent& event) {
	const auto& dropLootMsg = event.msg;
	if (dropLootMsg.item != LOT_NULL && dropLootMsg.item != 0) {
		const auto* const source = Game::entityManager->GetEntity(dropLootMsg.sourceID);
		Loot::Info info{
			.id = dropLootMsg.lootID,
			.lot = dropLootMsg.item,
			.count = dropLootMsg.count,
			.sourceLot = source ? source->GetLOT() : 0,
		};
		AddLootItem(info);
	}

	if (dropLootMsg.item == LOT_NULL && dropLootMsg.currency != 0) {
		RegisterCoinDrop(dropLootMsg.currency);
	}

	return true;
}

bool Entity::MsgGetFlag(GameMessages::GetFlag& flagMsg) {
	if (m_Character) flagMsg.flag = m_Character->GetPlayerFlag(flagMsg.flagID);
	return true;
}
bool Entity::MsgGetFactionTokenType(GameMessages::GetFactionTokenType& tokenMsg) {
	GameMessages::GetFlag getFlagMsg{};

	getFlagMsg.flagID = ePlayerFlag::ASSEMBLY_FACTION;
	MsgGetFlag(getFlagMsg);
	if (getFlagMsg.flag) tokenMsg.tokenType = 8318;

	getFlagMsg.flagID = ePlayerFlag::SENTINEL_FACTION;
	MsgGetFlag(getFlagMsg);
	if (getFlagMsg.flag) tokenMsg.tokenType = 8319;

	getFlagMsg.flagID = ePlayerFlag::PARADOX_FACTION;
	MsgGetFlag(getFlagMsg);
	if (getFlagMsg.flag) tokenMsg.tokenType = 8320;

	getFlagMsg.flagID = ePlayerFlag::VENTURE_FACTION;
	MsgGetFlag(getFlagMsg);
	if (getFlagMsg.flag) tokenMsg.tokenType = 8321;

	LOG("Returning token type %i", tokenMsg.tokenType);
	return tokenMsg.tokenType != LOT_NULL;
}

bool Entity::MsgPickupItem(GameMessages::PickupItemEvent& event) {
	const auto& pickupItemMsg = event.msg;
	if (GetObjectID() == pickupItemMsg.lootOwnerID) {
		PickupItem(pickupItemMsg.lootID);
	} else {
		auto* const characterComponent = GetComponent<CharacterComponent>();
		if (!characterComponent) return false;
		auto& droppedLoot = characterComponent->GetDroppedLoot();
		const auto it = droppedLoot.find(pickupItemMsg.lootID);
		if (it != droppedLoot.end()) {
			CDObjectsTable* objectsTable = CDClientManager::GetTable<CDObjectsTable>();
			const CDObjects& object = objectsTable->GetByID(it->second.lot);
			if (object.id != 0 && object.type == "Powerup") {
				return false; // Let powerups be duplicated
			}
		}
		droppedLoot.erase(pickupItemMsg.lootID);
	}

	return true;
}

bool Entity::MsgChildRemoved(GameMessages::ChildRemoved& msg) {
	GetScript()->OnChildRemoved(*this, msg);
	return true;
}
