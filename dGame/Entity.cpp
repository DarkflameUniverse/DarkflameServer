#include "dCommonVars.h"
#include "Entity.h"
#include "CDClientManager.h"
#include "Game.h"
#include "dLogger.h"
#include <PacketUtils.h>
#include <functional>
#include "CDDestructibleComponentTable.h"
#include "CDClientDatabase.h"
#include <sstream>
#include "dServer.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "Zone.h"
#include "Spawner.h"
#include "UserManager.h"
#include "dpWorld.h"
#include "Player.h"
#include "LUTriggers.h"
#include "User.h"
#include "EntityTimer.h"
#include "EntityCallbackTimer.h"
#include "Loot.h"
#include "eMissionTaskType.h"
#include "eTriggerEventType.h"
#include "eObjectBits.h"

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
#include "RocketLaunchpadControlComponent.h"
#include "PropertyComponent.h"
#include "CollectibleComponent.h"
#include "BaseCombatAIComponent.h"
#include "PropertyManagementComponent.h"
#include "PropertyVendorComponent.h"
#include "ProximityMonitorComponent.h"
#include "PropertyEntranceComponent.h"
#include "ModelBehaviorComponent.h"
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
#include "TriggerComponent.h"
#include "eGameMasterLevel.h"
#include "eReplicaComponentType.h"
#include "eReplicaPacketType.h"
#include "RacingStatsComponent.h"
#include "MinigameControlComponent.h"
#include "ItemComponent.h"

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

const std::vector<ComponentWhitelist> Entity::m_ComponentWhitelists = {
	{ // Unknown use case
		eReplicaComponentType::CONTROLLABLE_PHYSICS,
		eReplicaComponentType::SIMPLE_PHYSICS,
		eReplicaComponentType::RENDER
	},
	{ // Used for BBB
		eReplicaComponentType::RENDER,
		eReplicaComponentType::DESTROYABLE,
		eReplicaComponentType::ITEM,
		eReplicaComponentType::BLUEPRINT,
		eReplicaComponentType::MODEL_BEHAVIOR,
		eReplicaComponentType::CONTROLLABLE_PHYSICS,
		eReplicaComponentType::SIMPLE_PHYSICS,
		eReplicaComponentType::SPAWN
	},
	{ // Unknown use case
		eReplicaComponentType::RENDER,
		eReplicaComponentType::ITEM,
		eReplicaComponentType::BLUEPRINT,
	},
	{ // Used for Pets
		eReplicaComponentType::PET,
		eReplicaComponentType::SKILL,
		eReplicaComponentType::DESTROYABLE,
		eReplicaComponentType::RENDER,
		eReplicaComponentType::CONTROLLABLE_PHYSICS
	},
	{ // Unknown use case
		eReplicaComponentType::CONTROLLABLE_PHYSICS,
		eReplicaComponentType::SIMPLE_PHYSICS,
		eReplicaComponentType::RENDER,
	},
};

Entity::Entity(const LWOOBJID& objectID, EntityInfo info, Entity* parentEntity) {
	m_ObjectID = objectID;
	m_TemplateID = info.lot;
	m_ParentEntity = parentEntity;
	m_Character = nullptr;
	m_GMLevel = eGameMasterLevel::CIVILIAN;
	m_NetworkID = 0;
	m_Observers = 0;
	m_OwnerOverride = LWOOBJID_EMPTY;
	m_Groups.clear();
	m_Timers.clear();
	m_ChildEntities.clear();
	m_TargetsInPhantom.clear();
	m_DieCallbacks.clear();
	m_PhantomCollisionCallbacks.clear();
	m_ScheduleKiller = nullptr;
	m_IsParentChildDirty = true;
	m_IsGhostingCandidate = false;
	m_PlayerIsReadyForUpdates = false;
	m_ShouldDestroyAfterUpdate = false;

	m_Settings = info.settings;
	m_NetworkSettings = info.networkSettings;
	m_DefaultPosition = info.pos;
	m_DefaultRotation = info.rot;
	m_Scale = info.scale;
	m_Spawner = info.spawner;
	m_SpawnerID = info.spawnerID;
	m_HasSpawnerNodeID = info.hasSpawnerNodeID;
	m_SpawnerNodeID = info.spawnerNodeID;

	if (info.lot != 1) m_PlayerIsReadyForUpdates = true;
}

Entity::~Entity() {
	if (m_Character) m_Character->SaveXMLToDatabase();

	CancelAllTimers();
	CancelCallbackTimers();

	for (auto child : m_ChildEntities) if (child) child->RemoveParent();

	if (m_ParentEntity) m_ParentEntity->RemoveChild(this);
}

void Entity::ApplyComponentWhitelist(TemplateComponents& components) const {
	const auto whitelistIndex = GetVar<int32_t>(u"componentWhitelist");
	if (whitelistIndex < 0 || whitelistIndex >= m_ComponentWhitelists.size()) return;

	const auto& whitelist = m_ComponentWhitelists.at(whitelistIndex);
	const auto endRange = std::remove_if(components.begin(), components.end(), [&whitelist](const auto& componentCandidate) {
		return std::find(whitelist.begin(), whitelist.end(), componentCandidate.first) == whitelist.end();
		});
	components.erase(endRange, components.end());
}

void Entity::ApplyComponentBlacklist(TemplateComponents& components) const {
	bool hasPetComponent = std::find_if(components.begin(), components.end(), [](const auto& componentCandidate) {
		return componentCandidate.first == eReplicaComponentType::PET;
		}) != components.end();
		if (hasPetComponent) {
			auto toRemove = std::remove_if(components.begin(), components.end(), [](const auto& componentCandidate) {
				return componentCandidate.first == eReplicaComponentType::MODEL_BEHAVIOR || componentCandidate.first == eReplicaComponentType::ITEM;
				});
			components.erase(toRemove, components.end());
		}
}

void Entity::ApplyComponentConfig(TemplateComponents& components) const {
	if (GetVar<bool>(u"markedAsPhantom")) {
		auto toRemove = std::remove_if(components.begin(), components.end(), [](const auto& componentCandidate) {
			return componentCandidate.first == eReplicaComponentType::SIMPLE_PHYSICS ||
				componentCandidate.first == eReplicaComponentType::PHANTOM_PHYSICS; // Just make sure we dont have phantom physics already
			});
		components.erase(toRemove, components.end());
		components.emplace_back(eReplicaComponentType::PHANTOM_PHYSICS, 0U);
	}
}

void Entity::AddPathComponent(TemplateComponents& components) const {
	const Path* path = dZoneManager::Instance()->GetZone()->GetPath(GetVarAsString(u"attached_path"));
	//Check to see if we have an attached path and add the appropiate component to handle it:
	if (path) {
		// if we have a moving platform path, then we need a moving platform component
		if (path->pathType == PathType::MovingPlatform) {
			bool hasMovingPlatform = std::count_if(components.begin(), components.end(), [](const auto& componentCandidate) {
				return componentCandidate.first == eReplicaComponentType::MOVING_PLATFORM;
				}) > 0;
			if (!hasMovingPlatform) components.emplace_back(eReplicaComponentType::MOVING_PLATFORM, 0U);
		} else if (path->pathType == PathType::Movement) {
			bool hasMovementAi = std::count_if(components.begin(), components.end(), [](const auto& componentCandidate) {
				return componentCandidate.first == eReplicaComponentType::MOVEMENT_AI;
				}) > 0;
			if (!hasMovementAi) {
				components.emplace_back(eReplicaComponentType::MOVEMENT_AI, 0U);
			}
		} else {
			Game::logger->Log("Entity", "Unsupported path type %i provided for lot %i.", path->pathType, GetLOT());
		}
	}
}

void Entity::Initialize() {
	// A few edge cases to tackle first
	const auto triggerInfo = GetVarAsString(u"trigger_id");
	if (!triggerInfo.empty()) AddComponent<TriggerComponent>(triggerInfo);

	const auto groupIDs = GetVarAsString(u"groupID");
	if (!groupIDs.empty()) {
		m_Groups = GeneralUtils::SplitString(groupIDs, ';');
		if (m_Groups.back().empty()) m_Groups.erase(m_Groups.end() - 1);
	}

	if (m_ParentEntity) m_ParentEntity->AddChild(this);

	auto* componentsRegistry = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
	TemplateComponents components = componentsRegistry->GetTemplateComponents(m_TemplateID);
	ApplyComponentWhitelist(components);
	ApplyComponentBlacklist(components);
	AddPathComponent(components);
	// Brick-by-Brick models use custom physics depending on _something_ but the client uses 4246 as the simple
	// physics component id and 4247 for phantom physics. We'll just use the simple physics component for now
	// since we dont know what the phantom physics are for at the moment.
	if (GetLOT() == LOT_MODEL_IN_WORLD) components.emplace_back(eReplicaComponentType::SIMPLE_PHYSICS, 4246U);
	for (const auto& [componentTemplate, componentId] : components) {
		switch (componentTemplate) {
		case eReplicaComponentType::CONTROLLABLE_PHYSICS:
			AddComponent<ControllablePhysicsComponent>();
			break;
		case eReplicaComponentType::RENDER:
			AddComponent<RenderComponent>();
			break;
		case eReplicaComponentType::SIMPLE_PHYSICS:
			AddComponent<SimplePhysicsComponent>(componentId);
			break;
		case eReplicaComponentType::CHARACTER:
			AddComponent<CharacterComponent>(m_Character);
			AddComponent<MissionComponent>();
			AddComponent<PossessorComponent>();
			AddComponent<LevelProgressionComponent>();
			AddComponent<PlayerForcedMovementComponent>();
			break;
		case eReplicaComponentType::SCRIPT: {
			std::string script;
			if (m_TemplateID == LOT_ZONE_CONTROL) {
				script = ScriptComponent::GetZoneScriptName(componentId);
			} else {
				script = ScriptComponent::GetScriptName(this, componentId);
			}
			AddComponent<ScriptComponent>(script); // Technically this should check for if the script name is empty and not create a component if it is.
			break;
		}
		case eReplicaComponentType::BOUNCER:
			AddComponent<BouncerComponent>();
			break;
		case eReplicaComponentType::DESTROYABLE:
			if (!HasComponent(eReplicaComponentType::DESTROYABLE)) AddComponent<DestroyableComponent>(componentId);
			break;
		case eReplicaComponentType::SKILL:
			AddComponent<SkillComponent>();
			break;
		case eReplicaComponentType::ITEM:
			AddComponent<ItemComponent>();
			break;
		case eReplicaComponentType::VENDOR:
			AddComponent<VendorComponent>();
			break;
		case eReplicaComponentType::INVENTORY:
			AddComponent<InventoryComponent>();
			break;
		case eReplicaComponentType::SHOOTING_GALLERY:
			AddComponent<ShootingGalleryComponent>();
			break;
		case eReplicaComponentType::RIGID_BODY_PHANTOM_PHYSICS:
			AddComponent<RigidbodyPhantomPhysicsComponent>();
			break;
		case eReplicaComponentType::COLLECTIBLE:
			AddComponent<CollectibleComponent>();
			if (!HasComponent(eReplicaComponentType::DESTROYABLE)) AddComponent<DestroyableComponent>(componentId);
			break;
		case eReplicaComponentType::MOVING_PLATFORM:
			AddComponent<MovingPlatformComponent>(GetVarAsString(u"attached_path"));
			break;
		case eReplicaComponentType::PET:
			AddComponent<PetComponent>(componentId);
			AddComponent<MovementAIComponent>();
			break;
		case eReplicaComponentType::HAVOK_VEHICLE_PHYSICS: {
			auto* havokVehiclePhysicsComponent = AddComponent<HavokVehiclePhysicsComponent>();
			if (havokVehiclePhysicsComponent) {
				havokVehiclePhysicsComponent->SetPosition(m_DefaultPosition);
				havokVehiclePhysicsComponent->SetRotation(m_DefaultRotation);
			}
			break;
		}
		case eReplicaComponentType::MOVEMENT_AI:
			AddComponent<MovementAIComponent>();
			break;
		case eReplicaComponentType::PROPERTY:
			AddComponent<PropertyComponent>();
			break;
		case eReplicaComponentType::SCRIPTED_ACTIVITY:
			AddComponent<ScriptedActivityComponent>(componentId);
			break;
		case eReplicaComponentType::PHANTOM_PHYSICS: {
			auto* phantomPhysicsComponent = AddComponent<PhantomPhysicsComponent>();
			if (phantomPhysicsComponent) phantomPhysicsComponent->SetPhysicsEffectActive(false);
			break;
		}
		case eReplicaComponentType::MODEL_BEHAVIOR: {
			AddComponent<ModelBehaviorComponent>();
			if (!HasComponent(eReplicaComponentType::DESTROYABLE)) {
				auto* destroyableComponent = AddComponent<DestroyableComponent>(componentId);
				if (destroyableComponent) {
					destroyableComponent->SetHealth(1);
					destroyableComponent->SetMaxHealth(1.0f);
					destroyableComponent->SetFaction(-1, true);
					destroyableComponent->SetIsSmashable(true);
				}
			}
			break;
		}
		case eReplicaComponentType::PROPERTY_ENTRANCE:
			AddComponent<PropertyEntranceComponent>(componentId);
			break;
		case eReplicaComponentType::PROPERTY_MANAGEMENT:
			AddComponent<PropertyManagementComponent>();
			break;
		case eReplicaComponentType::QUICK_BUILD:
			AddComponent<QuickBuildComponent>(componentId);
			if (!HasComponent(eReplicaComponentType::DESTROYABLE)) AddComponent<DestroyableComponent>(componentId);
			break;
		case eReplicaComponentType::SWITCH:
			AddComponent<SwitchComponent>();
			break;
		case eReplicaComponentType::MINIGAME_CONTROL:
			AddComponent<MinigameControlComponent>();
			break;
		case eReplicaComponentType::BASE_COMBAT_AI: {
			auto* baseCombatAiComponent = AddComponent<BaseCombatAIComponent>(componentId);
			if (baseCombatAiComponent && baseCombatAiComponent->GetTetherSpeed() > 0.0f) {
				auto* movementAiComponent = AddComponent<MovementAIComponent>();
				if (!movementAiComponent) break;
				MovementAIInfo movementAiInfo{};
				movementAiInfo.movementType = "";
				movementAiInfo.wanderChance = 0;
				movementAiInfo.wanderRadius = 16;
				movementAiInfo.wanderSpeed = 2.5f;
				movementAiInfo.wanderDelayMax = 5;
				movementAiInfo.wanderDelayMin = 2;
				movementAiComponent->SetMoveInfo(movementAiInfo);
			}
			break;
		}
		case eReplicaComponentType::MODULE_ASSEMBLY:
			AddComponent<ModuleAssemblyComponent>();
			break;
		case eReplicaComponentType::PROPERTY_VENDOR:
			AddComponent<PropertyVendorComponent>();
			break;
		case eReplicaComponentType::ROCKET_LAUNCHPAD_CONTROL:
			AddComponent<RocketLaunchpadControlComponent>(componentId);
			break;
		case eReplicaComponentType::RACING_CONTROL:
			AddComponent<RacingControlComponent>();
			break;
		case eReplicaComponentType::MISSION_OFFER:
			AddComponent<MissionOfferComponent>(GetLOT());
			break;
		case eReplicaComponentType::RACING_STATS:
			AddComponent<RacingStatsComponent>();
			break;
		case eReplicaComponentType::LUP_EXHIBIT:
			AddComponent<LUPExhibitComponent>();
			break;
		case eReplicaComponentType::SOUND_TRIGGER:
			AddComponent<SoundTriggerComponent>();
			break;
		case eReplicaComponentType::PROXIMITY_MONITOR:
			AddComponent<ProximityMonitorComponent>();
			break;
		case eReplicaComponentType::MULTI_ZONE_ENTRANCE:
			AddComponent<MultiZoneEntranceComponent>();
			break;
		case eReplicaComponentType::BUFF:
			AddComponent<BuffComponent>();
			break;
		case eReplicaComponentType::RAIL_ACTIVATOR:
			AddComponent<RailActivatorComponent>(componentId);
			break;
		case eReplicaComponentType::POSSESSABLE:
			AddComponent<PossessableComponent>(componentId);
			break;
		case eReplicaComponentType::BUILD_BORDER:
			AddComponent<BuildBorderComponent>();
			break;
		case eReplicaComponentType::GHOST:
		case eReplicaComponentType::SPAWN:
		case eReplicaComponentType::MODULAR_BUILD:
		case eReplicaComponentType::BUILD_CONTROLLER:
		case eReplicaComponentType::BUILD_ACTIVATOR:
		case eReplicaComponentType::ICON_ONLY:
		case eReplicaComponentType::PROJECTILE_PHYSICS:
		case eReplicaComponentType::DROP_EFFECT:
		case eReplicaComponentType::CHEST:
		case eReplicaComponentType::BLUEPRINT:
		case eReplicaComponentType::PLATFORM_BOUNDARY:
		case eReplicaComponentType::MODULE:
		case eReplicaComponentType::JETPACKPAD:
		case eReplicaComponentType::EXHIBIT:
		case eReplicaComponentType::OVERHEAD_ICON:
		case eReplicaComponentType::PET_CONTROL:
		case eReplicaComponentType::MINIFIG:
		case eReplicaComponentType::PET_CREATOR:
		case eReplicaComponentType::MODEL_BUILDER:
		case eReplicaComponentType::SPRINGPAD:
		case eReplicaComponentType::FX:
		case eReplicaComponentType::VEHICLE_PHYSICS:
		case eReplicaComponentType::PHYSICS_SYSTEM:
		case eReplicaComponentType::CHANGLING_BUILD:
		case eReplicaComponentType::CHOICE_BUILD:
		case eReplicaComponentType::PACKAGE:
		case eReplicaComponentType::SOUND_REPEATER:
		case eReplicaComponentType::SOUND_AMBIENT_2D:
		case eReplicaComponentType::SOUND_AMBIENT_3D:
		case eReplicaComponentType::PRECONDITION:
		case eReplicaComponentType::FLAG:
		case eReplicaComponentType::CUSTOM_BUILD_ASSEMBLY:
		case eReplicaComponentType::SHOWCASE_MODEL_HANDLER:
		case eReplicaComponentType::RACING_MODULE:
		case eReplicaComponentType::GENERIC_ACTIVATOR:
		case eReplicaComponentType::HF_LIGHT_DIRECTION_GADGET:
		case eReplicaComponentType::ROCKET_ANIMATION_CONTROL:
		case eReplicaComponentType::TRIGGER:
		case eReplicaComponentType::DROPPED_LOOT:
		case eReplicaComponentType::FACTION_TRIGGER:
		case eReplicaComponentType::BBB:
		case eReplicaComponentType::RACING_SOUND_TRIGGER:
		case eReplicaComponentType::CHAT_BUBBLE:
		case eReplicaComponentType::FRIENDS_LIST:
		case eReplicaComponentType::GUILD:
		case eReplicaComponentType::LOCAL_SYSTEM:
		case eReplicaComponentType::MISSION:
		case eReplicaComponentType::MUTABLE_MODEL_BEHAVIORS:
		case eReplicaComponentType::PATHFINDING:
		case eReplicaComponentType::PET_TAMING_CONTROL:
		case eReplicaComponentType::PROPERTY_EDITOR:
		case eReplicaComponentType::SKINNED_RENDER:
		case eReplicaComponentType::SLASH_COMMAND:
		case eReplicaComponentType::STATUS_EFFECT:
		case eReplicaComponentType::TEAMS:
		case eReplicaComponentType::TEXT_EFFECT:
		case eReplicaComponentType::TRADE:
		case eReplicaComponentType::USER_CONTROL:
		case eReplicaComponentType::IGNORE_LIST:
		case eReplicaComponentType::INTERACTION_MANAGER:
		case eReplicaComponentType::DONATION_VENDOR:
		case eReplicaComponentType::COMBAT_MEDIATOR:
		case eReplicaComponentType::ACHIEVEMENT_VENDOR:
		case eReplicaComponentType::GATE_RUSH_CONTROL:
		case eReplicaComponentType::ROLLER:
		case eReplicaComponentType::PLAYER_FORCED_MOVEMENT:
		case eReplicaComponentType::CRAFTING:
		case eReplicaComponentType::LEVEL_PROGRESSION:
		case eReplicaComponentType::POSSESSOR:
		case eReplicaComponentType::MOUNT_CONTROL:
		case eReplicaComponentType::UNKNOWN_112:
		case eReplicaComponentType::PROPERTY_PLAQUE:
		case eReplicaComponentType::UNKNOWN_115:
		case eReplicaComponentType::CULLING_PLANE:
		case eReplicaComponentType::NUMBER_OF_COMPONENTS:
		case eReplicaComponentType::INVALID:
		default:
			Game::logger->Log("Entity", "Attempted to create component %i for lot %i but no creator exists. Component will not be created.", componentId, m_TemplateID);
		}
	}

	AddCallbackTimer(0.0f, [this]() {
		GetScript()->OnStartup(this);
		});

	// Load data specific to this LOT first. These act as defaults for the components.
	std::for_each(m_Components.begin(), m_Components.end(), [this](auto& component) {
		component.second->LoadTemplateData();
		});

	// Then load data specific to this Entity. This will vary on an Entity to Entity basis.
	// If there is data you want to override the LOT default value, generally you would attach it via LDF
	// and it would get loaded and overridden here.
	std::for_each(m_Components.begin(), m_Components.end(), [this](auto& component) {
		component.second->LoadConfigData();
		});

	/**
	 * Startup all the components. Some components need or want data from other components so
	 * we want to ensure that
	 * A) Most if not all components are newed and ready to be accessed.
	 * B) All components have their personal data loaded and ready to be used.
	 */
	std::for_each(m_Components.begin(), m_Components.end(), [this](auto& component) {
		component.second->Startup();
		});

	/**
	 * Load the player save data from XML. Ideally we do this after all initialization so the player
	 * save data overrides any defaults that may be applied.
	 */
	if (!IsPlayer()) std::for_each(m_Components.begin(), m_Components.end(), [this](auto& component) {
		component.second->LoadFromXml(m_Character->GetXMLDoc());
		});

	TriggerEvent(eTriggerEventType::CREATE, this);
	if (!m_Character && EntityManager::Instance()->GetGhostingEnabled()) IsGhosted();
}

// Invert this check and build it into the component initialization. The ghosting property is an intrinsic property of which components the Entity has.
// Keep the first check since that is a special case for large scene elements like Brig Rock as a whole.
void Entity::IsGhosted() {
	// Don't ghost what is likely large scene elements
	if (HasComponent(eReplicaComponentType::SIMPLE_PHYSICS) && HasComponent(eReplicaComponentType::RENDER) && (m_Components.size() == 2 || (HasComponent(eReplicaComponentType::TRIGGER) && m_Components.size() == 3))) {
		return;
	}

	/* Filter for ghosting candidates.
	 *
	 * Don't ghost moving platforms, until we've got proper syncing for those.
	 * Don't ghost big phantom physics triggers, as putting those to sleep might prevent interactions.
	 * Don't ghost property related objects, as the client expects those to always be loaded.
	 */
	if (!EntityManager::IsExcludedFromGhosting(GetLOT()) &&
		!HasComponent(eReplicaComponentType::SCRIPTED_ACTIVITY) &&
		!HasComponent(eReplicaComponentType::MOVING_PLATFORM) &&
		!HasComponent(eReplicaComponentType::PHANTOM_PHYSICS) &&
		!HasComponent(eReplicaComponentType::PROPERTY) &&
		!HasComponent(eReplicaComponentType::RACING_CONTROL) &&
		!HasComponent(eReplicaComponentType::VEHICLE_PHYSICS)) {
		m_IsGhostingCandidate = true;
	}

	if (GetLOT() == LOT_3D_AMBIENT_SOUND) m_IsGhostingCandidate = true;

	// Special case for collectibles in Ninjago
	if (HasComponent(eReplicaComponentType::COLLECTIBLE) && Game::server->GetZoneID() == 2000) {
		m_IsGhostingCandidate = true;
	}
}

// Move to header
bool Entity::operator==(const Entity& other) const {
	return other.m_ObjectID == m_ObjectID;
}

// Move to header
bool Entity::operator!=(const Entity& other) const {
	return !(other.m_ObjectID == m_ObjectID);
}

// Move to header
User* Entity::GetParentUser() const {
	return IsPlayer() ? static_cast<const Player*>(this)->GetParentUser() : nullptr;
}

// Move to header
bool Entity::HasComponent(const eReplicaComponentType componentId) const {
	return m_Components.find(componentId) != m_Components.end();
}

// Fine
void Entity::Subscribe(const LWOOBJID& scriptObjId, CppScripts::Script* scriptToAdd, const std::string& notificationName) {
	if (notificationName == "HitOrHealResult" || notificationName == "Hit") {
		auto* destroyableComponent = GetComponent<DestroyableComponent>();
		if (!destroyableComponent) return;
		destroyableComponent->Subscribe(scriptObjId, scriptToAdd);
	}
}

// Fine
void Entity::Unsubscribe(const LWOOBJID& scriptObjId, const std::string& notificationName) {
	if (notificationName == "HitOrHealResult" || notificationName == "Hit") {
		auto* destroyableComponent = GetComponent<DestroyableComponent>();
		if (!destroyableComponent) return;
		destroyableComponent->Unsubscribe(scriptObjId);
	}
}

// Fine
void Entity::SetProximityRadius(const float proxRadius, const std::string& name) {
	auto* proximityMonitorComponent = AddComponent<ProximityMonitorComponent>();
	if (proximityMonitorComponent) proximityMonitorComponent->SetProximityRadius(proxRadius, name);
}

// Remove in favor of a square constructor
void Entity::SetProximityRadius(dpEntity* entity, const std::string& name) {
	auto* proximityMonitorComponent = AddComponent<ProximityMonitorComponent>();
	if (proximityMonitorComponent) proximityMonitorComponent->SetProximityRadius(entity, name);
}

void Entity::SetGMLevel(eGameMasterLevel value) {
	m_GMLevel = value;
	// User m_Character?
	if (GetParentUser()) {
		Character* character = GetParentUser()->GetLastUsedChar();

		if (character) {
			character->SetGMLevel(value);
		}
	}

	auto* character = GetComponent<CharacterComponent>();
	if (character) character->SetGMLevel(value);

	GameMessages::SendGMLevelBroadcast(m_ObjectID, value);
}

void Entity::WriteBaseReplicaData(RakNet::BitStream* outBitStream, const eReplicaPacketType packetType) {
	if (packetType == eReplicaPacketType::CONSTRUCTION) {
		outBitStream->Write(m_ObjectID);
		outBitStream->Write(m_TemplateID);

		if (IsPlayer()) {
			std::string name = m_Character != nullptr ? m_Character->GetName() : "Invalid";
			outBitStream->Write<uint8_t>(uint8_t(name.size()));

			for (size_t i = 0; i < name.size(); ++i) {
				outBitStream->Write<uint16_t>(name[i]);
			}
		} else {
			const auto& name = GetVar<std::string>(u"npcName");
			outBitStream->Write<uint8_t>(uint8_t(name.size()));

			for (size_t i = 0; i < name.size(); ++i) {
				outBitStream->Write<uint16_t>(name[i]);
			}
		}

		outBitStream->Write<uint32_t>(0); //Time since created on server

		const auto& syncLDF = GetVar<std::vector<std::u16string>>(u"syncLDF");

		// Only sync for models.
		// PetComponent check un-needed since we should be removing the component during construction.
		if (m_Settings.size() > 0 && (GetComponent<ModelBehaviorComponent>() && !GetComponent<PetComponent>())) {
			outBitStream->Write1(); //ldf data

			RakNet::BitStream settingStream;
			settingStream.Write<uint32_t>(m_Settings.size());

			for (LDFBaseData* data : m_Settings) {
				if (data) {
					data->WriteToPacket(&settingStream);
				}
			}

			outBitStream->Write(settingStream.GetNumberOfBytesUsed() + 1);
			outBitStream->Write<uint8_t>(0); //no compression used
			outBitStream->Write(settingStream);
		} else if (!syncLDF.empty()) {
			std::vector<LDFBaseData*> ldfData;

			for (const auto& data : syncLDF) {
				ldfData.push_back(GetVarData(data));
			}

			outBitStream->Write1(); //ldf data

			RakNet::BitStream settingStream;
			settingStream.Write<uint32_t>(ldfData.size());

			for (LDFBaseData* data : ldfData) {
				if (data) {
					data->WriteToPacket(&settingStream);
				}
			}

			outBitStream->Write(settingStream.GetNumberOfBytesUsed() + 1);
			outBitStream->Write<uint8_t>(0); //no compression used
			outBitStream->Write(settingStream);
		} else {
			outBitStream->Write0(); //No ldf data
		}

		auto* triggerComponent = GetComponent<TriggerComponent>();
		if (triggerComponent) {
			// has trigger component, check to see if we have events to handle
			auto* trigger = triggerComponent->GetTrigger();
			outBitStream->Write<bool>(trigger && trigger->events.size() > 0);
		} else { // no trigger componenet, so definitely no triggers
			outBitStream->Write0();
		}

		outBitStream->Write<bool>(m_ParentEntity != nullptr || m_SpawnerID != 0);
		if (m_ParentEntity != nullptr || m_SpawnerID != 0) {
			if (m_ParentEntity != nullptr) outBitStream->Write(GeneralUtils::SetBit(m_ParentEntity->GetObjectID(), static_cast<uint32_t>(eObjectBits::CLIENT)));
			else if (m_Spawner != nullptr && m_Spawner->m_Info.isNetwork) outBitStream->Write(m_SpawnerID);
			else outBitStream->Write(GeneralUtils::SetBit(m_SpawnerID, static_cast<uint32_t>(eObjectBits::CLIENT)));
		}

		outBitStream->Write(m_HasSpawnerNodeID);
		if (m_HasSpawnerNodeID) outBitStream->Write(m_SpawnerNodeID);

		//outBitStream->Write0(); //Spawner node id

		outBitStream->Write<bool>(m_Scale != 1.0f || m_Scale != 0.0f);
		if (m_Scale != 1.0f || m_Scale != 0.0f) outBitStream->Write(m_Scale);

		outBitStream->Write0(); //ObjectWorldState

		outBitStream->Write(m_GMLevel != eGameMasterLevel::CIVILIAN);
		if (m_GMLevel != eGameMasterLevel::CIVILIAN) {
			outBitStream->Write(m_GMLevel);
		}
	}

	// Only serialize parent / child info should the info be dirty (changed) or if this is the construction of the entity.
	outBitStream->Write(m_IsParentChildDirty || packetType == eReplicaPacketType::CONSTRUCTION);
	if (m_IsParentChildDirty || packetType == eReplicaPacketType::CONSTRUCTION) {
		m_IsParentChildDirty = false;
		outBitStream->Write(m_ParentEntity != nullptr);
		if (m_ParentEntity) {
			outBitStream->Write(m_ParentEntity->GetObjectID());
			outBitStream->Write0();
		}
		outBitStream->Write(m_ChildEntities.size() > 0);
		if (m_ChildEntities.size() > 0) {
			outBitStream->Write<uint16_t>(m_ChildEntities.size());
			for (Entity* child : m_ChildEntities) {
				outBitStream->Write<LWOOBJID>(child->GetObjectID());
			}
		}
	}
}

// uh
void Entity::WriteComponents(RakNet::BitStream* outBitStream, const eReplicaPacketType packetType) {

}

// We should be able to use this at some point
void Entity::ResetFlags() {
	// Unused
}

// std::for_each
void Entity::UpdateXMLDoc(tinyxml2::XMLDocument* doc) {
	DluAssert(doc != nullptr);
	for (const auto& pair : m_Components) {
		if (pair.second == nullptr) continue;

		pair.second->UpdateXml(doc);
	}
}

CppScripts::Script* Entity::GetScript() const {
	auto* scriptComponent = GetComponent<ScriptComponent>();
	if (!scriptComponent) return CppScripts::invalidScript.get();
	auto* script = scriptComponent->GetScript();
	DluAssert(script != nullptr);
	return script;
}

void Entity::Update(const float deltaTime) {
	auto namedTimerItr = std::remove_if(m_Timers.begin(), m_Timers.end(), [this, &deltaTime](EntityTimer* timer) {
		timer->Update(deltaTime);
		if (timer->GetTime() <= 0) {
			GetScript()->OnTimerDone(this, timer->GetName());
			TriggerEvent(eTriggerEventType::TIMER_DONE, this);
			delete timer;
			return true;
		}
		return false;
		});
	m_Timers.erase(namedTimerItr, m_Timers.end());

	auto callbackTimerItr = std::remove_if(m_CallbackTimers.begin(), m_CallbackTimers.end(), [this, &deltaTime](EntityCallbackTimer* timer) {
		timer->Update(deltaTime);
		if (timer->GetTime() <= 0) {
			timer->ExecuteCallback();
			delete timer;
			return true;
		}
		return false;
		});
	m_CallbackTimers.erase(callbackTimerItr, m_CallbackTimers.end());

	// Add pending timers to the list of timers so they start next tick.
	if (m_PendingTimers.size() > 0) {
		m_Timers.insert(m_Timers.end(), m_PendingTimers.begin(), m_PendingTimers.end());
		m_PendingTimers.clear();
	}

	if (IsSleeping()) {
		Sleep();

		return;
	}
	Wake();

	GetScript()->OnUpdate(this);

	for (const auto& [componentId, component] : m_Components) {
		if (component) component->Update(deltaTime);
	}

	if (m_ShouldDestroyAfterUpdate) EntityManager::Instance()->DestroyEntity(this);
}

void Entity::OnCollisionProximity(LWOOBJID otherEntity, const std::string& proxName, const std::string& status) {
	auto* other = EntityManager::Instance()->GetEntity(otherEntity);
	if (!other) return;

	GetScript()->OnProximityUpdate(this, other, proxName, status);

	auto* rocketLaunchpadControlComponent = GetComponent<RocketLaunchpadControlComponent>();
	if (!rocketLaunchpadControlComponent) return;

	rocketLaunchpadControlComponent->OnProximityUpdate(other, proxName, status);
}

void Entity::OnCollisionPhantom(const LWOOBJID otherEntity) {
	auto* other = EntityManager::Instance()->GetEntity(otherEntity);
	if (!other) return;

	GetScript()->OnCollisionPhantom(this, other);

	std::for_each(m_PhantomCollisionCallbacks.begin(), m_PhantomCollisionCallbacks.end(), [other](const auto& callback) {
		callback(other);
		});

	auto* switchComponent = GetComponent<SwitchComponent>();
	if (switchComponent) switchComponent->EntityEnter(other);

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
		auto* combat = GetComponent<BaseCombatAIComponent>();

		if (combat != nullptr) {
			const auto index = std::find(m_TargetsInPhantom.begin(), m_TargetsInPhantom.end(), otherEntity);

			if (index != m_TargetsInPhantom.end()) return;

			const auto valid = combat->IsEnemy(otherEntity);

			if (!valid) return;

			m_TargetsInPhantom.push_back(otherEntity);
		}
	}
}

void Entity::OnCollisionLeavePhantom(const LWOOBJID otherEntity) {
	auto* other = EntityManager::Instance()->GetEntity(otherEntity);
	if (!other) return;

	GetScript()->OnOffCollisionPhantom(this, other);

	TriggerEvent(eTriggerEventType::EXIT, other);

	auto* switchComponent = GetComponent<SwitchComponent>();
	if (switchComponent) switchComponent->EntityLeave(other);

	const auto index = std::find(m_TargetsInPhantom.begin(), m_TargetsInPhantom.end(), otherEntity);

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
	GameMessages::SendNotifyObject(GetObjectID(), sender->GetObjectID(), GeneralUtils::ASCIIToUTF16(name), UNASSIGNED_SYSTEM_ADDRESS);

	GetScript()->OnNotifyObject(this, sender, name, param1, param2);
}

void Entity::OnEmoteReceived(const int32_t emote, Entity* target) {
	GetScript()->OnEmoteReceived(this, emote, target);
}

void Entity::OnUse(Entity* originator) {
	TriggerEvent(eTriggerEventType::INTERACT, originator);

	GetScript()->OnUse(this, originator);

	for (const auto& [componentId, component] : m_Components) {
		if (component) component->OnUse(originator);
	}
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

void Entity::RequestActivityExit(Entity* sender, const LWOOBJID& player, const bool canceled) {
	GetScript()->OnRequestActivityExit(sender, player, canceled);
}

void Entity::Smash(const LWOOBJID source, const eKillType killType, const std::u16string& deathType) {
	if (!m_PlayerIsReadyForUpdates) return;

	auto* destroyableComponent = GetComponent<DestroyableComponent>();
	if (!destroyableComponent) {
		Kill(EntityManager::Instance()->GetEntity(source));
		return;
	}
	auto* possessorComponent = GetComponent<PossessorComponent>();
	if (possessorComponent) {
		if (possessorComponent->GetPossessable() != LWOOBJID_EMPTY) {
			auto* mount = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());
			if (mount) possessorComponent->Dismount(mount, true);
		}
	}

	destroyableComponent->Smash(source, killType, deathType);
}

void Entity::Kill(Entity* murderer) {
	if (!m_PlayerIsReadyForUpdates) return;

	for (const auto& cb : m_DieCallbacks) cb();

	m_DieCallbacks.clear();

	//OMAI WA MOU, SHINDERIU

	GetScript()->OnDie(this, murderer);

	if (m_Spawner) m_Spawner->NotifyOfEntityDeath(m_ObjectID);

	if (!IsPlayer()) EntityManager::Instance()->DestroyEntity(this);

	const auto& grpNameQBShowBricks = GetVar<std::string>(u"grpNameQBShowBricks");

	if (!grpNameQBShowBricks.empty()) {
		auto spawners = dZoneManager::Instance()->GetSpawnersByName(grpNameQBShowBricks);

		Spawner* spawner = nullptr;

		if (!spawners.empty()) {
			spawner = spawners.front();
		} else {
			spawners = dZoneManager::Instance()->GetSpawnersInGroup(grpNameQBShowBricks);

			if (!spawners.empty()) spawner = spawners.front();
		}

		if (spawner) spawner->Spawn();
	}

	// Track a player being smashed
	auto* characterComponent = GetComponent<CharacterComponent>();
	if (characterComponent) {
		characterComponent->UpdatePlayerStatistic(TimesSmashed);
	}

	// Track a player smashing something else
	if (murderer) {
		auto* murdererCharacterComponent = murderer->GetComponent<CharacterComponent>();
		if (murdererCharacterComponent) {
			murdererCharacterComponent->UpdatePlayerStatistic(SmashablesSmashed);
		}
	}
}

void Entity::AddDieCallback(const std::function<void()>& callback) {
	m_DieCallbacks.push_back(callback);
}

void Entity::AddCollisionPhantomCallback(const std::function<void(Entity* target)>& callback) {
	m_PhantomCollisionCallbacks.push_back(callback);
}

void Entity::AddRebuildCompleteCallback(const std::function<void(Entity* user)>& callback) const {
	auto* quickBuildComponent = GetComponent<QuickBuildComponent>();
	if (quickBuildComponent) quickBuildComponent->AddRebuildCompleteCallback(callback);
}

bool Entity::GetIsDead() const {
	auto* dest = GetComponent<DestroyableComponent>();
	return dest && dest->GetArmor() == 0 && dest->GetHealth() == 0;
}

// Replace static_cast with dynamic_cast
void Entity::AddLootItem(const Loot::Info& info) {
	if (!IsPlayer()) return;
	auto& droppedLoot = static_cast<Player*>(this)->GetDroppedLoot();
	droppedLoot.insert(std::make_pair(info.id, info));
}

// Replace static_cast with dynamic_cast
void Entity::PickupItem(const LWOOBJID& objectID) {
	if (!IsPlayer()) return;
	auto* inventoryComponent = GetComponent<InventoryComponent>();
	if (!inventoryComponent) return;

	auto* objectsTable = CDClientManager::Instance().GetTable<CDObjectsTable>();
	auto* skillsTable = CDClientManager::Instance().GetTable<CDObjectSkillsTable>();

	auto& droppedLoot = static_cast<Player*>(this)->GetDroppedLoot();

	// See if there is some faster way to do this.
	for (const auto& [lootObjId, loot] : droppedLoot) {
		if (lootObjId != objectID) continue;
		auto* characterComponent = GetComponent<CharacterComponent>();
		if (characterComponent) characterComponent->TrackLOTCollection(loot.lot);

		const CDObjects& object = objectsTable->GetByID(loot.lot);
		if (object.id != 0 && object.type == "Powerup") {
			const auto lootLot = loot.lot;
			auto skills = skillsTable->Query([lootLot](CDObjectSkills entry) {return (entry.objectTemplate == lootLot); });
			for (const auto& skill : skills) {
				auto* skillBehaviorTable = CDClientManager::Instance().GetTable<CDSkillBehaviorTable>();
				auto behaviorData = skillBehaviorTable->GetSkillByID(skill.skillID);
				// This should take a skillID, not a behaviorID.
				SkillComponent::HandleUnmanaged(behaviorData.behaviorID, GetObjectID());

				auto* missionComponent = GetComponent<MissionComponent>();

				if (missionComponent) missionComponent->Progress(eMissionTaskType::POWERUP, skill.skillID);
			}
		} else {
			inventoryComponent->AddItem(loot.lot, loot.count, eLootSourceType::PICKUP, eInventoryType::INVALID, {}, LWOOBJID_EMPTY, true, false, LWOOBJID_EMPTY, eInventoryType::INVALID, 1);
		}
	}
	droppedLoot.erase(objectID);
}

// This functions name is misleading and should not modify the number of dropped coins.
// A separate function, PickupCoins should modify that.
// Replace static_cast with dynamic_cast
bool Entity::CanPickupCoins(const uint64_t& count) {
	if (!IsPlayer()) return false;
	auto* player = static_cast<Player*>(this);
	auto droppedCoins = player->GetDroppedCoins();
	if (count > droppedCoins) {
		return false;
	} else {
		player->SetDroppedCoins(droppedCoins - count);
		return true;
	}
}

// Replace static_cast with dynamic_cast
void Entity::RegisterCoinDrop(const uint64_t& coinsDropped) {
	if (!IsPlayer()) return;
	auto* player = static_cast<Player*>(this);
	player->SetDroppedCoins(player->GetDroppedCoins() + coinsDropped);
}

void Entity::AddChild(Entity* child) {
	m_IsParentChildDirty = true;
	if (std::find(m_ChildEntities.begin(), m_ChildEntities.end(), child) == m_ChildEntities.end()) m_ChildEntities.push_back(child);
}

void Entity::RemoveChild(Entity* child) {
	if (!child) return;
	uint32_t entityPosition = 0;
	auto toRemove = std::remove(m_ChildEntities.begin(), m_ChildEntities.end(), child);
	if (toRemove != m_ChildEntities.end()) m_IsParentChildDirty = true;
	m_ChildEntities.erase(toRemove, m_ChildEntities.end());
}

void Entity::RemoveParent() {
	if (m_ParentEntity) m_IsParentChildDirty = true;
	else Game::logger->Log("Entity", "Attempted to remove parent from(objid:lot) (%llu:%i) when no parent existed", GetObjectID(), GetLOT());
	this->m_ParentEntity = nullptr;
}

void Entity::AddTimer(const std::string& name, float time) {
	m_PendingTimers.push_back(new EntityTimer(name, time));
}

void Entity::AddCallbackTimer(const float time, const std::function<void()>& callback) {
	m_CallbackTimers.push_back(new EntityCallbackTimer(time, callback));
}

bool Entity::HasTimer(const std::string& name) {
	auto possibleTimer = std::find_if(m_Timers.begin(), m_Timers.end(), [name](EntityTimer* timer) {
		return timer->GetName() == name;
		});
	return possibleTimer != m_Timers.end();
}

void Entity::CancelCallbackTimers() {
	std::for_each(m_CallbackTimers.begin(), m_CallbackTimers.end(), [](EntityCallbackTimer* timer) {
		delete timer;
		});
	m_CallbackTimers.clear();
}

void Entity::ScheduleKillAfterUpdate(Entity* murderer) {
	EntityManager::Instance()->ScheduleForKill(this);

	if (murderer) m_ScheduleKiller = murderer;
}

void Entity::CancelTimer(const std::string& name) {
	auto toErase = std::remove_if(m_Timers.begin(), m_Timers.end(), [&name](EntityTimer* timer) {
		if (timer->GetName() == name) {
			delete timer;
			return true;
		}
		return false;
	});
	m_Timers.erase(m_Timers.begin(), toErase);
}
// ### LEFT OFF HERE ###
void Entity::CancelAllTimers() {
	for (auto* timer : m_Timers) {
		delete timer;
	}

	m_Timers.clear();

	for (auto* callBackTimer : m_CallbackTimers) {
		delete callBackTimer;
	}

	m_CallbackTimers.clear();
}

bool Entity::IsPlayer() const {
	return m_TemplateID == 1 && GetSystemAddress() != UNASSIGNED_SYSTEM_ADDRESS;
}

void Entity::TriggerEvent(eTriggerEventType event, Entity* optionalTarget) {
	auto* triggerComponent = GetComponent<TriggerComponent>();
	if (triggerComponent) triggerComponent->TriggerEvent(event, optionalTarget);
}

Entity* Entity::GetOwner() const {
	if (m_OwnerOverride != LWOOBJID_EMPTY) {
		auto* other = EntityManager::Instance()->GetEntity(m_OwnerOverride);

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

void Entity::SetOwnerOverride(const LWOOBJID& value) {
	m_OwnerOverride = value;
}

void Entity::SetObservers(int8_t value) {
	if (value < 0) {
		value = 0;
	}

	m_Observers = value;
}

void Entity::Sleep() {
	auto* baseCombatAIComponent = GetComponent<BaseCombatAIComponent>();

	if (baseCombatAIComponent != nullptr) {
		baseCombatAIComponent->Sleep();
	}
}

void Entity::Wake() {
	auto* baseCombatAIComponent = GetComponent<BaseCombatAIComponent>();

	if (baseCombatAIComponent != nullptr) {
		baseCombatAIComponent->Wake();
	}
}

bool Entity::IsSleeping() const {
	return m_IsGhostingCandidate && m_Observers == 0;
}


const NiPoint3& Entity::GetPosition() const {
	auto* controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr) {
		return controllable->GetPosition();
	}

	auto* phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr) {
		return phantom->GetPosition();
	}

	auto* simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr) {
		return simple->GetPosition();
	}

	auto* vehicle = GetComponent<HavokVehiclePhysicsComponent>();

	if (vehicle != nullptr) {
		return vehicle->GetPosition();
	}

	return NiPoint3::ZERO;
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

	auto* vehicle = GetComponent<HavokVehiclePhysicsComponent>();

	if (vehicle != nullptr) {
		return vehicle->GetRotation();
	}

	return NiQuaternion::IDENTITY;
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

	auto* vehicle = GetComponent<HavokVehiclePhysicsComponent>();

	if (vehicle != nullptr) {
		vehicle->SetPosition(position);
	}

	EntityManager::Instance()->SerializeEntity(this);
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

	auto* vehicle = GetComponent<HavokVehiclePhysicsComponent>();

	if (vehicle != nullptr) {
		vehicle->SetRotation(rotation);
	}

	EntityManager::Instance()->SerializeEntity(this);
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
	for (auto* data : m_Settings) {
		if (data->GetKey() == name) {
			return true;
		}
	}

	return false;
}

void Entity::SetNetworkId(const uint16_t id) {
	m_NetworkID = id;
}

std::vector<LWOOBJID>& Entity::GetTargetsInPhantom() {
	std::vector<LWOOBJID> valid;

	// Clean up invalid targets, like disconnected players
	for (auto i = 0u; i < m_TargetsInPhantom.size(); ++i) {
		const auto id = m_TargetsInPhantom.at(i);

		auto* entity = EntityManager::Instance()->GetEntity(id);

		if (entity == nullptr) {
			continue;
		}

		valid.push_back(id);
	}

	m_TargetsInPhantom = valid;

	return m_TargetsInPhantom;
}

void Entity::SendNetworkVar(const std::string& data, const SystemAddress& sysAddr) {
	GameMessages::SendSetNetworkScriptVar(this, sysAddr, data);
}

LDFBaseData* Entity::GetVarData(const std::u16string& name) const {
	for (auto* data : m_Settings) {
		if (data == nullptr) {
			continue;
		}

		if (data->GetKey() != name) {
			continue;
		}

		return data;
	}

	return nullptr;
}

std::string Entity::GetVarAsString(const std::u16string& name) const {
	auto* data = GetVarData(name);

	if (data == nullptr) {
		return "";
	}

	return data->GetValueAsString();
}

void Entity::Resurrect() {
	if (IsPlayer()) {
		GameMessages::SendResurrect(this);
	}
}

void Entity::AddToGroups(const std::string& group) {
	if (std::find(m_Groups.begin(), m_Groups.end(), group) == m_Groups.end()) {
		m_Groups.push_back(group);
	}
}

void Entity::RetroactiveVaultSize() {
	auto* inventoryComponent = GetComponent<InventoryComponent>();
	if (!inventoryComponent) return;

	auto itemsVault = inventoryComponent->GetInventory(eInventoryType::VAULT_ITEMS);
	auto modelVault = inventoryComponent->GetInventory(eInventoryType::VAULT_MODELS);

	if (itemsVault->GetSize() == modelVault->GetSize()) return;

	modelVault->SetSize(itemsVault->GetSize());
}
