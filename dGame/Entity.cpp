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
#include "RacingStatsComponent.h"
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
}

int32_t Entity::ObjectLoadState::GetComponentID(const eReplicaComponentType type, const int32_t defaultValue) const {
	if (whitelist && std::ranges::find(*whitelist, type) == whitelist->end()) return defaultValue;
	return CDClientManager::GetTable<CDComponentsRegistryTable>()->GetByIDAndType(lot, type, defaultValue);
}

// ObjectLoader2::DoObjectPreLoad (0x01054d30): what the object's components will be before any is made
Entity::ObjectLoadState Entity::DoObjectPreLoad() {
	ObjectLoadState state;
	state.lot = m_TemplateID;

	// The player's own components (HandleLocalPlayer makes them for the local player, not from the registry)
	if (m_Character && m_Character->GetParentUser()) {
		AddComponent<MissionComponent>(-1)->LoadFromSaveData(m_Character->GetXMLDoc());
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
void Entity::DoObjectComponentLoad(ObjectLoadState& state) {


	/**
	 * The components in the client's load order (ObjectLoader2::DoObjectComponentLoad 0x0105ebb0 walks
	 * ComponentOrderVector::Initialize 0x0101f8e0's list): a component that looks at another one finds it made when the
	 * client's does. Ids a component needs from a later one come from the registry.
	 */
	const auto petComponentID = state.GetComponentID(eReplicaComponentType::PET);
	const auto collectibleComponentID = state.GetComponentID(eReplicaComponentType::COLLECTIBLE);
	const auto combatAiID = state.GetComponentID(eReplicaComponentType::BASE_COMBAT_AI);
	// LWOScriptComponent::Allocator (0x0104d710) and LWOBaseCombatAiComponent::Allocator (0x0104c7b0) add a proximity
	// monitor when there is none, and pathfinding control when the object has movement physics (controllable,
	// projectile, vehicle or havok vehicle) and none yet
	const auto addProximityAndPathfinding = [this]() {
		if (!HasComponent(eReplicaComponentType::PROXIMITY_MONITOR)) AddComponent<ProximityMonitorComponent>(-1);
		const bool hasMovementPhysics = HasComponent(eReplicaComponentType::CONTROLLABLE_PHYSICS) || HasComponent(eReplicaComponentType::PROJECTILE_PHYSICS)
			|| HasComponent(eReplicaComponentType::VEHICLE_PHYSICS) || HasComponent(eReplicaComponentType::HAVOK_VEHICLE_PHYSICS);
		if (hasMovementPhysics && !HasComponent(eReplicaComponentType::PATHFINDING)) AddComponent<PathfindingComponent>(-1);
	};

	for (const auto type : ClientComponentLoadOrder) {
		if (state.whitelist && std::ranges::find(*state.whitelist, type) == state.whitelist->end()) continue;
		switch (type) {
		case eReplicaComponentType::POSSESSABLE: {
		const auto possessableComponentID = state.GetComponentID(eReplicaComponentType::POSSESSABLE);
		if (possessableComponentID > 0) {
			AddComponent<PossessableComponent>(possessableComponentID);
		}
			break;
		}
		case eReplicaComponentType::MODULE_ASSEMBLY: {
		const auto moduleAssemblyID = state.GetComponentID(eReplicaComponentType::MODULE_ASSEMBLY);
		if (moduleAssemblyID > 0) {
			AddComponent<ModuleAssemblyComponent>(moduleAssemblyID);
		}
			break;
		}
		case eReplicaComponentType::CONTROLLABLE_PHYSICS: {
		const auto controllablePhysicsComponentID = state.GetComponentID(eReplicaComponentType::CONTROLLABLE_PHYSICS);
		if (controllablePhysicsComponentID > 0) {
			// The component places it: where the level put an object, where a character's save and the zone put it
			auto* controllablePhysics = AddComponent<ControllablePhysicsComponent>(controllablePhysicsComponentID);
			if (m_Character) controllablePhysics->LoadFromSaveData(m_Character->GetXMLDoc());
		}
			break;
		}
		case eReplicaComponentType::HAVOK_VEHICLE_PHYSICS: {
		const auto havokVehiclePhysicsComponentID = state.GetComponentID(eReplicaComponentType::HAVOK_VEHICLE_PHYSICS);
		// The client makes simple physics instead for use_simple_physics, and none for objectWorldState 2
		if (havokVehiclePhysicsComponentID > 0 && HasVar(u"use_simple_physics")) {
			if (!HasComponent(eReplicaComponentType::SIMPLE_PHYSICS)) AddComponent<SimplePhysicsComponent>(havokVehiclePhysicsComponentID);
		} else if (havokVehiclePhysicsComponentID > 0 && !(HasVar(u"objectWorldState") && GetVarAs<int32_t>(u"objectWorldState") == 2)) {
			auto* havokVehiclePhysicsComponent = AddComponent<HavokVehiclePhysicsComponent>(havokVehiclePhysicsComponentID);
			havokVehiclePhysicsComponent->SetPosition(m_DefaultPosition);
			havokVehiclePhysicsComponent->SetRotation(m_DefaultRotation);
		}
			break;
		}
		case eReplicaComponentType::RIGID_BODY_PHANTOM_PHYSICS: {
		const auto rigidBodyPhantomPhysicsComponentID = state.GetComponentID(eReplicaComponentType::RIGID_BODY_PHANTOM_PHYSICS);
		if (rigidBodyPhantomPhysicsComponentID > 0) {
			AddComponent<RigidbodyPhantomPhysicsComponent>(rigidBodyPhantomPhysicsComponentID);
		}
			break;
		}
		case eReplicaComponentType::SIMPLE_PHYSICS: {
		const auto simplePhysicsComponentID = state.GetComponentID(eReplicaComponentType::SIMPLE_PHYSICS);
		if (state.hasModelBehaviors) {
			// LWOSimplePhysicsComponent::Allocator (0x0104ded0): left to the model case
			if (simplePhysicsComponentID > 0) state.modelPhysicsComponentID = simplePhysicsComponentID;
		} else if (simplePhysicsComponentID > 0) {
			// state.markedAsPhantom makes its simple physics phantom physics (with the simple physics row)
			if (state.markedAsPhantom) AddComponent<PhantomPhysicsComponent>(simplePhysicsComponentID)->SetPhysicsEffectActive(false);
			else AddComponent<SimplePhysicsComponent>(simplePhysicsComponentID);
		}
			break;
		}
		case eReplicaComponentType::PHANTOM_PHYSICS: {
		const auto phantomPhysicsComponentID = state.GetComponentID(eReplicaComponentType::PHANTOM_PHYSICS);
		if (phantomPhysicsComponentID > 0 && !HasComponent(eReplicaComponentType::PHANTOM_PHYSICS)) {
			AddComponent<PhantomPhysicsComponent>(phantomPhysicsComponentID)->SetPhysicsEffectActive(false);
		}
			break;
		}
		case eReplicaComponentType::DESTROYABLE: {
		/**
		 * The destroyable component. The client makes one (with a buff component in front of it) for a DESTROYABLE
		 * registry entry, whose id is the DestructibleComponent row. Collectibles and smashable (is_smashable) objects
		 * without that entry get one too, with no row.
		 */
		const auto destroyableComponentID = state.GetComponentID(eReplicaComponentType::DESTROYABLE);
		if (destroyableComponentID > 0) {
			AddComponent<BuffComponent>(destroyableComponentID);
		}

		// A collectible's (right after it) and a smashable object's (after the walk) come later, as the client makes them
		if (destroyableComponentID > 0) {
			// The row, the level config and a character's save, in the client's order
			DestroyableComponent* comp = AddComponent<DestroyableComponent>(destroyableComponentID);
			if (m_Character) comp->LoadFromSaveData(m_Character->GetXMLDoc());
		}
			break;
		}
		case eReplicaComponentType::MINIFIG: {
			const auto minifigID = state.GetComponentID(eReplicaComponentType::MINIFIG);
			if (minifigID > 0) AddComponent<MinifigComponent>(minifigID);
			break;
		}
		case eReplicaComponentType::CHARACTER: {
		const auto characterID = state.GetComponentID(eReplicaComponentType::CHARACTER);
		if (characterID > 0 || m_Character) {
			// LWOCharacterComponent::Factory (0x0104db40): possession control, mount control, pet creator, level
			// progression and forced movement, then the character, all componentID -1
			AddComponent<PossessorComponent>(-1);
			AddComponent<MountControlComponent>(-1);
			AddComponent<PetCreatorComponent>(-1);
			AddComponent<LevelProgressionComponent>(-1)->LoadFromSaveData(m_Character->GetXMLDoc());
			AddComponent<PlayerForcedMovementComponent>(-1);

			auto& systemAddress = m_Character->GetParentUser() ? m_Character->GetParentUser()->GetSystemAddress() : UNASSIGNED_SYSTEM_ADDRESS;
			AddComponent<CharacterComponent>(-1, m_Character, systemAddress)->LoadFromSaveData(m_Character->GetXMLDoc());

		}
			break;
		}
		case eReplicaComponentType::PET: {
		// LWOPetComponent::Factory (0x01031240): no pet in an inventory; otherwise pathfinding control first
		if (petComponentID > 0 && !GetVar<bool>(u"inInventory")) {
			if (!HasComponent(eReplicaComponentType::PATHFINDING)) AddComponent<PathfindingComponent>(-1);
			AddComponent<PetComponent>(petComponentID);
		}
			break;
		}
		case eReplicaComponentType::INVENTORY: {
		const auto inventoryID = state.GetComponentID(eReplicaComponentType::INVENTORY);
		if (inventoryID > 0 || m_Character) {
			AddComponent<InventoryComponent>(inventoryID);
		}
			break;
		}
		case eReplicaComponentType::PROXIMITY_MONITOR: {
		const auto proximityMonitorID = state.GetComponentID(eReplicaComponentType::PROXIMITY_MONITOR);
		if (proximityMonitorID > 0) AddComponent<ProximityMonitorComponent>(proximityMonitorID);
			break;
		}
		case eReplicaComponentType::MOVEMENT_AI: {
		const int movementAIID = state.GetComponentID(eReplicaComponentType::MOVEMENT_AI);
		if (movementAIID > 0) {
			// Its row and the level's wanderRadius load in the component
			AddComponent<MovementAIComponent>(movementAIID, MovementAIInfo{});
		} else if (petComponentID > 0 || combatAiID > 0 && BaseCombatAIComponent::TetherSpeedOf(combatAiID) > 0) {
			MovementAIInfo moveInfo{
				.movementType = "",
				.wanderRadius = 16,
				.wanderSpeed = 2.5f,
				.wanderChance = 0,
				.wanderDelayMin = 2,
				.wanderDelayMax = 5,
			};

			AddComponent<MovementAIComponent>(-1, moveInfo);
		} else if (state.addsMovementAI) {
			// ApplyMovementAiIfNeeded: a scripted object or one with an attached path has one (id 0: no row)
			AddComponent<MovementAIComponent>(0, MovementAIInfo{});
		}
			break;
		}
		case eReplicaComponentType::SCRIPT: {
		// LWOScriptComponent's allocator: a script component for a registry entry, a character, or a custom script in
		// the level config; the component picks its script
		CDScriptComponentTable* const scriptCompTable = CDClientManager::GetTable<CDScriptComponentTable>();
		const int32_t scriptComponentID = state.GetComponentID(eReplicaComponentType::SCRIPT, -1);
		const bool hasCustomScript = !GetVarAsString(u"custom_script_server").empty() || !GetVarAsString(u"custom_script_client").empty();
		if (scriptComponentID >= 0 || m_Character || hasCustomScript) {
			AddComponent<ScriptComponent>(scriptComponentID);
			addProximityAndPathfinding();
		}

		// ZoneControl script
		if (m_TemplateID == 2365) {
			const auto& zoneID = Game::zoneManager->GetZoneID();
			const CDZoneTable* const zoneData = CDZoneTableTable::Query(zoneID.GetMapID());

			if (zoneData != nullptr) {
				const int32_t zoneScriptID = zoneData->scriptID;
				CDScriptComponent zoneScriptData = scriptCompTable->GetByID(zoneScriptID);
				// A server script only: the client has no script component on the zone control object (live sent none)
				AddComponent<ScriptComponent>(zoneScriptID, zoneScriptData.script_name, false, false);
			}
		}
			break;
		}
		case eReplicaComponentType::SKILL: {
		const auto skillID = state.GetComponentID(eReplicaComponentType::SKILL, -1);
		if (skillID > -1 || m_Character) {
			auto* const skillComponent = AddComponent<SkillComponent>(skillID);
			if (m_Character) skillComponent->LoadFromSaveData(m_Character->GetXMLDoc());
		}
			break;
		}
		case eReplicaComponentType::BASE_COMBAT_AI: {
		if (combatAiID > 0) {
			addProximityAndPathfinding();
			AddComponent<BaseCombatAIComponent>(combatAiID);
		}
			break;
		}
		case eReplicaComponentType::ITEM: {
		// A pet takes the item (and model) component away (LWOPetComponent::Factory); models keep theirs (live: an
		// item block before the model's)
		PetComponent* petComponent{};
		const auto itemID = state.GetComponentID(eReplicaComponentType::ITEM);
		if (itemID > 0 && !TryGetComponent(eReplicaComponentType::PET, petComponent)) {
			AddComponent<ItemComponent>(itemID);
		}
			break;
		}
		case eReplicaComponentType::BUILD_BORDER: {
		const auto buildBorderID = state.GetComponentID(eReplicaComponentType::BUILD_BORDER, -1);
		if (buildBorderID > -1) {
			AddComponent<BuildBorderComponent>(buildBorderID);
		}
			break;
		}
		case eReplicaComponentType::QUICK_BUILD: {
		const auto quickBuildComponentID = state.GetComponentID(eReplicaComponentType::QUICK_BUILD);
		if (quickBuildComponentID > 0) {
			AddComponent<QuickBuildComponent>(quickBuildComponentID);

			// A quick build without a destroyable gets one (componentID -1) and is smashable, as the client makes it
			// (DoObjectComponentLoad); live: health 1 / 1.0, armor 1 / 0, faction -1, smashable (AG LOT 7804)
			if (!HasComponent(eReplicaComponentType::DESTROYABLE)) {
				AddComponent<DestroyableComponent>(-1, true);
			}
		}
			break;
		}
		case eReplicaComponentType::MISSION_OFFER: {
		const auto missionOfferComponentID = state.GetComponentID(eReplicaComponentType::MISSION_OFFER, -1);
		if (missionOfferComponentID > -1 || m_Character) {
			AddComponent<MissionOfferComponent>(missionOfferComponentID);
		}
			break;
		}
		case eReplicaComponentType::VENDOR: {
		const auto vendorID = state.GetComponentID(eReplicaComponentType::VENDOR);
		// The vendor allocators add a proximity monitor first when there is none
		if (vendorID > 0) {
			if (!HasComponent(eReplicaComponentType::PROXIMITY_MONITOR)) AddComponent<ProximityMonitorComponent>(-1);
			AddComponent<VendorComponent>(vendorID);
		}
			break;
		}
		case eReplicaComponentType::DONATION_VENDOR: {
		const auto donationVendorID = state.GetComponentID(eReplicaComponentType::DONATION_VENDOR, -1);
		if (donationVendorID > -1) {
			if (!HasComponent(eReplicaComponentType::PROXIMITY_MONITOR)) AddComponent<ProximityMonitorComponent>(-1);
			AddComponent<DonationVendorComponent>(donationVendorID);
		}
			break;
		}
		case eReplicaComponentType::ACHIEVEMENT_VENDOR: {
		const auto achievementVendorID = state.GetComponentID(eReplicaComponentType::ACHIEVEMENT_VENDOR, -1);
		if (achievementVendorID > -1) {
			if (!HasComponent(eReplicaComponentType::PROXIMITY_MONITOR)) AddComponent<ProximityMonitorComponent>(-1);
			AddComponent<AchievementVendorComponent>(achievementVendorID);
		}
			break;
		}
		case eReplicaComponentType::SHOOTING_GALLERY: {
		// Shooting gallery component
		const auto shootingGalleryComponentID = state.GetComponentID(eReplicaComponentType::SHOOTING_GALLERY);
		if (shootingGalleryComponentID > 0) {
			AddComponent<ShootingGalleryComponent>(shootingGalleryComponentID);
		}
			break;
		}
		case eReplicaComponentType::SCRIPTED_ACTIVITY: {
		// Scripted activity component
		const auto scriptedActivityID = state.GetComponentID(eReplicaComponentType::SCRIPTED_ACTIVITY, -1);
		if (scriptedActivityID > -1) {
			AddComponent<ScriptedActivityComponent>(scriptedActivityID);
		}
			break;
		}
		case eReplicaComponentType::COLLECTIBLE: {

		if (collectibleComponentID > 0) {
			AddComponent<CollectibleComponent>(collectibleComponentID, GetVarAs<int32_t>(u"collectible_id"));
			// The client makes a collectible's destroyable right after it when there is none
			if (!HasComponent(eReplicaComponentType::DESTROYABLE)) AddComponent<DestroyableComponent>(-1);
		}
			break;
		}
		case eReplicaComponentType::MOVING_PLATFORM: {
		const std::string pathName = GetVarAsString(u"attached_path");
		const Path* const path = Game::zoneManager->GetZone()->GetPath(pathName);

		//Check to see if we have an attached path and add the appropiate component to handle it:
		if (path) {
			// if we have a moving platform path, then we need a moving platform component
			if (path->pathType == PathType::MovingPlatform) {
				// The registry component ID picks the subcomponent (a simple mover when it has one), path or not
				AddComponent<MovingPlatformComponent>(state.GetComponentID(eReplicaComponentType::MOVING_PLATFORM, -1), pathName);
			} else if (path->pathType == PathType::Movement) {
				auto* const movementAIcomponent = GetComponent<MovementAIComponent>();
				if (movementAIcomponent && combatAiID == 0) {
					movementAIcomponent->SetPath(pathName);
				} else {
					MovementAIInfo moveInfo{
						.movementType = "",
						.wanderRadius = 16,
						.wanderSpeed = 2.5f,
						.wanderChance = 0,
						.wanderDelayMin = 2,
						.wanderDelayMax = 5,
					};

					AddComponent<MovementAIComponent>(-1, moveInfo);
				}
			}
		} else {
			// else we still need to setup moving platform if it has a moving platform comp but no path
			const auto movingPlatformComponentID = state.GetComponentID(eReplicaComponentType::MOVING_PLATFORM, -1);
			if (movingPlatformComponentID >= 0) {
				AddComponent<MovingPlatformComponent>(movingPlatformComponentID, pathName);
			}
		}
			break;
		}
		case eReplicaComponentType::LUP_EXHIBIT: {
		const auto lupExhibitID = state.GetComponentID(eReplicaComponentType::LUP_EXHIBIT, -1);
		if (lupExhibitID >= 0) {
			AddComponent<LUPExhibitComponent>(lupExhibitID);
		}
			break;
		}
		case eReplicaComponentType::PROPERTY: {
		const auto propertyID = state.GetComponentID(eReplicaComponentType::PROPERTY, -1);
		if (propertyID > -1) {
			AddComponent<PropertyComponent>(propertyID);
		}
			break;
		}
		case eReplicaComponentType::MODEL: {
		const auto modelID = state.GetComponentID(eReplicaComponentType::MODEL, -1);
		if (modelID > -1 && !GetComponent<PetComponent>()) {
			// ObjectLoader2::LoadModelBehaviorsComponent (0x0104d1f0): a model without physics gets it here (BBB models,
			// LOT 14, have none in the registry): simple physics 4247 without behaviors; with behaviors controllable
			// (4246) for modelType 0, else simple (4247), unless the registry gave an id. modelType 0 also gets
			// pathfinding control.
			const bool hasPhysics = HasComponent(eReplicaComponentType::CONTROLLABLE_PHYSICS) || HasComponent(eReplicaComponentType::SIMPLE_PHYSICS)
				|| HasComponent(eReplicaComponentType::PROJECTILE_PHYSICS) || HasComponent(eReplicaComponentType::RIGID_BODY_PHANTOM_PHYSICS)
				|| HasComponent(eReplicaComponentType::HAVOK_VEHICLE_PHYSICS) || HasComponent(eReplicaComponentType::PHANTOM_PHYSICS)
				|| HasComponent(eReplicaComponentType::VEHICLE_PHYSICS) || HasComponent(eReplicaComponentType::PHYSICS_SYSTEM);
			const int32_t modelType = HasVar(u"modelType") ? GetVarAs<int32_t>(u"modelType") : -1;
			if (!hasPhysics) {
				if (!state.hasModelBehaviors) AddComponent<SimplePhysicsComponent>(4247);
				else if (modelType == 0) AddComponent<ControllablePhysicsComponent>(state.modelPhysicsComponentID == -1 ? 4246 : state.modelPhysicsComponentID);
				else AddComponent<SimplePhysicsComponent>(state.modelPhysicsComponentID == -1 ? 4247 : state.modelPhysicsComponentID);
			}
			if (modelType == 0 && !HasComponent(eReplicaComponentType::PATHFINDING)) AddComponent<PathfindingComponent>(-1);
			AddComponent<ModelComponent>(modelID)->LoadBehaviors();
			if (!HasComponent(eReplicaComponentType::DESTROYABLE)) {
				AddComponent<DestroyableComponent>(-1, true);
			}
		}
			break;
		}
		case eReplicaComponentType::PROPERTY_ENTRANCE: {
		const auto propertyEntranceComponentID = state.GetComponentID(eReplicaComponentType::PROPERTY_ENTRANCE);
		if (propertyEntranceComponentID > 0) {
			AddComponent<PropertyEntranceComponent>(propertyEntranceComponentID);
		}
			break;
		}
		case eReplicaComponentType::MULTI_ZONE_ENTRANCE: {
		// if this component exists, then we initialize it. it's value is always 0
		const auto multiZoneEntranceID = state.GetComponentID(eReplicaComponentType::MULTI_ZONE_ENTRANCE, -1);
		if (multiZoneEntranceID > -1) {
			AddComponent<MultiZoneEntranceComponent>(multiZoneEntranceID);
		}
			break;
		}
		case eReplicaComponentType::PROPERTY_MANAGEMENT: {
		const auto propertyManagementID = state.GetComponentID(eReplicaComponentType::PROPERTY_MANAGEMENT, -1);
		if (propertyManagementID > -1) {
			AddComponent<PropertyManagementComponent>(propertyManagementID);
		}
			break;
		}
		case eReplicaComponentType::RAIL_ACTIVATOR: {
		const auto railComponentID = state.GetComponentID(eReplicaComponentType::RAIL_ACTIVATOR);
		if (railComponentID > 0) {
			AddComponent<RailActivatorComponent>(railComponentID);
		}
			break;
		}
		case eReplicaComponentType::BOUNCER: {
		const auto bouncerID = state.GetComponentID(eReplicaComponentType::BOUNCER, -1);
		if (bouncerID > -1) { // you have to determine it like this because all bouncers have a componentID of 0
			AddComponent<BouncerComponent>(bouncerID);
		}
			break;
		}
		case eReplicaComponentType::SWITCH: {
		const auto switchID = state.GetComponentID(eReplicaComponentType::SWITCH, -1);
		if (switchID > -1) {
			AddComponent<SwitchComponent>(switchID);
		}
			break;
		}
		case eReplicaComponentType::MINI_GAME_CONTROL: {
		// The client makes it from the registry (the zone control object's id is 0)
		const auto minigameControlID = state.GetComponentID(eReplicaComponentType::MINI_GAME_CONTROL, -1);
		if (minigameControlID > -1) {
			AddComponent<MiniGameControlComponent>(minigameControlID);
		}
			break;
		}
		case eReplicaComponentType::RENDER: {
		const auto renderComponentID = state.GetComponentID(eReplicaComponentType::RENDER);
		if ((renderComponentID > 0 && m_TemplateID != 2365) || m_Character) {
			AddComponent<RenderComponent>(renderComponentID);
		}
			break;
		}
		case eReplicaComponentType::SOUND_TRIGGER: {
		const auto soundTriggerID = state.GetComponentID(eReplicaComponentType::SOUND_TRIGGER, -1);
		const auto racingSoundTriggerID = state.GetComponentID(eReplicaComponentType::RACING_SOUND_TRIGGER, -1);
		if (soundTriggerID > -1) {
			AddComponent<SoundTriggerComponent>(soundTriggerID);
		} else if (racingSoundTriggerID > -1) {
			AddComponent<RacingSoundTriggerComponent>(racingSoundTriggerID);
		}
			break;
		}
		case eReplicaComponentType::PROPERTY_VENDOR: {
		const auto propertyVendorID = state.GetComponentID(eReplicaComponentType::PROPERTY_VENDOR, -1);
		if (propertyVendorID > -1) {
			AddComponent<PropertyVendorComponent>(propertyVendorID);
		}
			break;
		}
		case eReplicaComponentType::ROCKET_LAUNCH: {
		const auto rocketID = state.GetComponentID(eReplicaComponentType::ROCKET_LAUNCH);
		if ((rocketID > 0)) {
			AddComponent<RocketLaunchpadControlComponent>(rocketID);
		}
			break;
		}
		case eReplicaComponentType::RACING_CONTROL: {
		const auto racingControlID = state.GetComponentID(eReplicaComponentType::RACING_CONTROL);
		if (racingControlID > 0) {
			AddComponent<RacingControlComponent>(racingControlID);
		}
			break;
		}
		case eReplicaComponentType::SOUND_REPEATER: {
			// LWOSoundRepeaterComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::SOUND_REPEATER, -1);
			if (id > -1) AddComponent<SoundRepeaterComponent>(id);
			break;
		}
		case eReplicaComponentType::SOUND_AMBIENT_2D: {
			// LWOSoundAmbient2DComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::SOUND_AMBIENT_2D, -1);
			if (id > -1) AddComponent<SoundAmbient2DComponent>(id);
			break;
		}
		case eReplicaComponentType::SOUND_AMBIENT_3D: {
			// LWOSoundAmbient3DComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::SOUND_AMBIENT_3D, -1);
			if (id > -1) AddComponent<SoundAmbient3DComponent>(id);
			break;
		}
		case eReplicaComponentType::CULLING_PLANE: {
			// LoadCullingPlaneComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::CULLING_PLANE, -1);
			if (id > -1) AddComponent<CullingPlaneComponent>(id);
			break;
		}
		case eReplicaComponentType::PLATFORM_BOUNDARY: {
			// LWOPlatformBoundaryComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::PLATFORM_BOUNDARY, -1);
			if (id > -1) AddComponent<PlatformBoundaryComponent>(id);
			break;
		}
		case eReplicaComponentType::REBUILD: {
			// LWOModularBuildComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::REBUILD, -1);
			if (id > -1) AddComponent<ModularBuildComponent>(id);
			break;
		}
		case eReplicaComponentType::REBUILD_START: {
			// LWOBuildControllerComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::REBUILD_START, -1);
			if (id > -1) AddComponent<BuildControllerComponent>(id);
			break;
		}
		case eReplicaComponentType::REBUILD_ACTIVATOR: {
			// LWORebuildActivatorComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::REBUILD_ACTIVATOR, -1);
			if (id > -1) AddComponent<RebuildActivatorComponent>(id);
			break;
		}
		case eReplicaComponentType::CHEST: {
			// LWOChestComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::CHEST, -1);
			if (id > -1) AddComponent<ChestComponent>(id);
			break;
		}
		case eReplicaComponentType::BLUEPRINT: {
			// LWOBlueprintComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::BLUEPRINT, -1);
			if (id > -1) AddComponent<BlueprintComponent>(id);
			break;
		}
		case eReplicaComponentType::MODULE: {
			// LWOModuleComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::MODULE, -1);
			if (id > -1) AddComponent<ModuleComponent>(id);
			break;
		}
		case eReplicaComponentType::ARCADE: {
			// LWOArcadeComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::ARCADE, -1);
			if (id > -1) AddComponent<ArcadeComponent>(id);
			break;
		}
		case eReplicaComponentType::MODEL_BUILDER: {
			// LWOModelBuilderComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::MODEL_BUILDER, -1);
			if (id > -1) AddComponent<ModelBuilderComponent>(id);
			break;
		}
		case eReplicaComponentType::SPRINGPAD: {
			// LWOSpringpadComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::SPRINGPAD, -1);
			if (id > -1) AddComponent<SpringpadComponent>(id);
			break;
		}
		case eReplicaComponentType::CHANGLING: {
			// LWOChangelingComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::CHANGLING, -1);
			if (id > -1) AddComponent<ChangelingComponent>(id);
			break;
		}
		case eReplicaComponentType::CHOICE_BUILD: {
			// LWOChoiceBuildComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::CHOICE_BUILD, -1);
			if (id > -1) AddComponent<ChoiceBuildComponent>(id);
			break;
		}
		case eReplicaComponentType::PACKAGE: {
			// LWOPackageComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::PACKAGE, -1);
			if (id > -1) AddComponent<PackageComponent>(id);
			break;
		}
		case eReplicaComponentType::CUSTOM_BUILD_ASSEMBLY: {
			// LWOCustomBuildAssemblyComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::CUSTOM_BUILD_ASSEMBLY, -1);
			if (id > -1) AddComponent<CustomBuildAssemblyComponent>(id);
			break;
		}
		case eReplicaComponentType::SHOWCASE_MODEL_HANDLER: {
			// LWOShowcaseModelHandlerComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::SHOWCASE_MODEL_HANDLER, -1);
			if (id > -1) AddComponent<ShowcaseModelHandlerComponent>(id);
			break;
		}
		case eReplicaComponentType::GENERIC_ACTIVATOR: {
			// LWOGenericActivatorComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::GENERIC_ACTIVATOR, -1);
			if (id > -1) AddComponent<GenericActivatorComponent>(id);
			break;
		}
		case eReplicaComponentType::PROJECTILE_PHYSICS: {
			// LWOProjectilePhysicsComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::PROJECTILE_PHYSICS, -1);
			if (id > -1) AddComponent<ProjectilePhysicsComponent>(id);
			break;
		}
		case eReplicaComponentType::VEHICLE_PHYSICS: {
			// LWOVehiclePhysicsComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::VEHICLE_PHYSICS, -1);
			if (id > -1) AddComponent<VehiclePhysicsComponent>(id);
			break;
		}
		case eReplicaComponentType::PHYSICS_SYSTEM: {
			// The client's physics system component: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::PHYSICS_SYSTEM, -1);
			if (id > -1) AddComponent<PhysicsSystemComponent>(id);
			break;
		}
		case eReplicaComponentType::SPAWNER: {
			// A spawner object's spawner component, from the registry as the client makes it
			const auto id = state.GetComponentID(eReplicaComponentType::SPAWNER, -1);
			if (id > -1) AddComponent<SpawnerComponent>(id);
			break;
		}
		case eReplicaComponentType::HF_LIGHT_DIRECTION_GADGET: {
			// The client makes it from the registry (an editor object)
			const auto id = state.GetComponentID(eReplicaComponentType::HF_LIGHT_DIRECTION_GADGET, -1);
			if (id > -1) AddComponent<HFLightDirectionGadgetComponent>(id);
			break;
		}
		case eReplicaComponentType::PLAYER_FORCED_MOVEMENT: {
			// From the registry, as the client makes it (a character got one with its character component)
			const auto id = state.GetComponentID(eReplicaComponentType::PLAYER_FORCED_MOVEMENT, -1);
			if (id > -1 && !HasComponent(eReplicaComponentType::PLAYER_FORCED_MOVEMENT)) AddComponent<PlayerForcedMovementComponent>(id);
			break;
		}
		case eReplicaComponentType::EXHIBIT: {
			// 1.10.64's loader has this case but makes nothing (dead code; showcases were cut); DLU makes the exhibit,
			// which sends nothing in a construction, so unchanged clients are unaffected
			const auto id = state.GetComponentID(eReplicaComponentType::EXHIBIT, -1);
			if (id > -1) AddComponent<ExhibitComponent>(id);
			break;
		}
		case eReplicaComponentType::BBB: {
			// DoObjectPreLoad gives a character (LOT 1) one with id 0; any other object the registry's
			const auto id = m_TemplateID == 1 ? 0 : state.GetComponentID(eReplicaComponentType::BBB, -1);
			if (id > -1) AddComponent<BBBComponent>(id);
			break;
		}
		case eReplicaComponentType::ROCKET_LANDING: {
			// LWORocketAnimationControlComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::ROCKET_LANDING, -1);
			if (id > -1) AddComponent<RocketLandingComponent>(id);
			break;
		}
		case eReplicaComponentType::FACTION_TRIGGER: {
			// LWOFactionTriggerComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::FACTION_TRIGGER, -1);
			if (id > -1) AddComponent<FactionTriggerComponent>(id);
			break;
		}
		case eReplicaComponentType::GATE_RUSH_CONTROL: {
			// LWOGateRushComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::GATE_RUSH_CONTROL, -1);
			if (id > -1) AddComponent<GateRushComponent>(id);
			break;
		}
		case eReplicaComponentType::ROLLER: {
			// LWORollerComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::ROLLER, -1);
			if (id > -1) AddComponent<RollerComponent>(id);
			break;
		}
		case eReplicaComponentType::PROPERTY_PLAQUE: {
			// LoadPropertyPlaqueComponent: the client makes it from the registry
			const auto id = state.GetComponentID(eReplicaComponentType::PROPERTY_PLAQUE, -1);
			if (id > -1) AddComponent<PropertyPlaqueComponent>(id);
			break;
		}
		default:
			break;
		}
	}

	/**
	 * Components DLU has that the client's load order doesn't make
	 */
	const auto racingStatsID = state.GetComponentID(eReplicaComponentType::RACING_STATS);
	if (racingStatsID > 0) {
		AddComponent<RacingStatsComponent>(racingStatsID);
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
		AddComponent<GhostComponent>(-1)->LoadFromSaveData(m_Character->GetXMLDoc());
	}
	// A mission offer's object gets a proximity monitor after the walk when it has none (the registry's mission offer;
	// a character's is DLU's own)
	if (!m_Character && HasComponent(eReplicaComponentType::MISSION_OFFER) && !HasComponent(eReplicaComponentType::PROXIMITY_MONITOR)) {
		AddComponent<ProximityMonitorComponent>(-1);
	}
	const auto triggerInfo = GetVarAsString(u"trigger_id");
	if (!triggerInfo.empty()) AddComponent<TriggerComponent>(-1, triggerInfo);
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

	auto loadState = DoObjectPreLoad();
	DoObjectComponentLoad(loadState);

	// Every component exists: the client's Startup step
	for (auto* const component : m_Components | std::views::values) if (component) component->Startup();
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
			outBitStream.Write0(); // Updates position with parent, usually false in live. Haven't seen a case where its supposed to be true
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

bool Entity::HandleMsg(GameMessages::GameMsg& msg) const {
	bool handled = false;
	auto [beg, end] = m_MsgHandlers.equal_range(msg.msgId);
	for (auto& it = beg; it != end; ++it) {
		if (it->second) handled |= it->second(msg);
	}

	return handled;
}

void Entity::RegisterMsg(const MessageType::Game msgId, std::function<bool(GameMessages::GameMsg&)> handler) {
	m_MsgHandlers.emplace(msgId, handler);
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
