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
#include "RebuildComponent.h"
#include "BuildBorderComponent.h"
#include "MovementAIComponent.h"
#include "VendorComponent.h"
#include "RocketLaunchpadControlComponent.h"
#include "PropertyComponent.h"
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

Entity::Entity(const LWOOBJID& objectID, EntityInfo info, Entity* parentEntity) {
	m_ObjectID = objectID;
	m_TemplateID = info.lot;
	m_ParentEntity = parentEntity;
	m_Character = nullptr;
	m_GMLevel = eGameMasterLevel::CIVILIAN;
	m_CollectibleID = 0;
	m_NetworkID = 0;
	m_Observers = 0;
	m_Groups = {};
	m_OwnerOverride = LWOOBJID_EMPTY;
	m_Timers = {};
	m_ChildEntities = {};
	m_ScheduleKiller = nullptr;
	m_TargetsInPhantom = {};
	m_DieCallbacks = {};
	m_PhantomCollisionCallbacks = {};
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

void Entity::Initialize() {
	auto* componentsRegistry = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
	auto components = componentsRegistry->GetTemplateComponents(m_TemplateID);
	for (const auto& [componentTemplate, componentId] : components) {
		switch (componentTemplate) {
		case eReplicaComponentType::CONTROLLABLE_PHYSICS:
			AddComponent<ControllablePhysicsComponent>();
			break;
		case eReplicaComponentType::RENDER:
			break;
		case eReplicaComponentType::SIMPLE_PHYSICS:
			break;
		case eReplicaComponentType::CHARACTER:
			break;
		case eReplicaComponentType::SCRIPT:
			break;
		case eReplicaComponentType::BOUNCER:
			break;
		case eReplicaComponentType::DESTROYABLE:
			break;
		case eReplicaComponentType::GHOST:
			break;
		case eReplicaComponentType::SKILL:
			break;
		case eReplicaComponentType::SPAWN:
			break;
		case eReplicaComponentType::ITEM:
			break;
		case eReplicaComponentType::MODULAR_BUILD:
			break;
		case eReplicaComponentType::BUILD_CONTROLLER:
			break;
		case eReplicaComponentType::BUILD_ACTIVATOR:
			break;
		case eReplicaComponentType::ICON_ONLY:
			break;
		case eReplicaComponentType::VENDOR:
			break;
		case eReplicaComponentType::INVENTORY:
			break;
		case eReplicaComponentType::PROJECTILE_PHYSICS:
			break;
		case eReplicaComponentType::SHOOTING_GALLERY:
			break;
		case eReplicaComponentType::RIGID_BODY_PHANTOM_PHYSICS:
			break;
		case eReplicaComponentType::DROP_EFFECT:
			break;
		case eReplicaComponentType::CHEST:
			break;
		case eReplicaComponentType::COLLECTIBLE:
			break;
		case eReplicaComponentType::BLUEPRINT:
			break;
		case eReplicaComponentType::MOVING_PLATFORM:
			break;
		case eReplicaComponentType::PET:
			break;
		case eReplicaComponentType::PLATFORM_BOUNDARY:
			break;
		case eReplicaComponentType::MODULE:
			break;
		case eReplicaComponentType::JETPACKPAD:
			break;
		case eReplicaComponentType::HAVOK_VEHICLE_PHYSICS:
			break;
		case eReplicaComponentType::MOVEMENT_AI:
			break;
		case eReplicaComponentType::EXHIBIT:
			break;
		case eReplicaComponentType::OVERHEAD_ICON:
			break;
		case eReplicaComponentType::PET_CONTROL:
			break;
		case eReplicaComponentType::MINIFIG:
			break;
		case eReplicaComponentType::PROPERTY:
			break;
		case eReplicaComponentType::PET_CREATOR:
			break;
		case eReplicaComponentType::MODEL_BUILDER:
			break;
		case eReplicaComponentType::SCRIPTED_ACTIVITY:
			break;
		case eReplicaComponentType::PHANTOM_PHYSICS:
			break;
		case eReplicaComponentType::SPRINGPAD:
			break;
		case eReplicaComponentType::MODEL_BEHAVIOR:
			break;
		case eReplicaComponentType::PROPERTY_ENTRANCE:
			break;
		case eReplicaComponentType::FX:
			break;
		case eReplicaComponentType::PROPERTY_MANAGEMENT:
			break;
		case eReplicaComponentType::VEHICLE_PHYSICS:
			break;
		case eReplicaComponentType::PHYSICS_SYSTEM:
			break;
		case eReplicaComponentType::QUICK_BUILD:
			break;
		case eReplicaComponentType::SWITCH:
			break;
		case eReplicaComponentType::MINIGAME_CONTROL:
			break;
		case eReplicaComponentType::CHANGLING_BUILD:
			break;
		case eReplicaComponentType::CHOICE_BUILD:
			break;
		case eReplicaComponentType::PACKAGE:
			break;
		case eReplicaComponentType::SOUND_REPEATER:
			break;
		case eReplicaComponentType::SOUND_AMBIENT_2D:
			break;
		case eReplicaComponentType::SOUND_AMBIENT_3D:
			break;
		case eReplicaComponentType::PRECONDITION:
			break;
		case eReplicaComponentType::FLAG:
			break;
		case eReplicaComponentType::CUSTOM_BUILD_ASSEMBLY:
			break;
		case eReplicaComponentType::BASE_COMBAT_AI:
			break;
		case eReplicaComponentType::MODULE_ASSEMBLY:
			break;
		case eReplicaComponentType::SHOWCASE_MODEL_HANDLER:
			break;
		case eReplicaComponentType::RACING_MODULE:
			break;
		case eReplicaComponentType::GENERIC_ACTIVATOR:
			break;
		case eReplicaComponentType::PROPERTY_VENDOR:
			break;
		case eReplicaComponentType::HF_LIGHT_DIRECTION_GADGET:
			break;
		case eReplicaComponentType::ROCKET_LAUNCHPAD_CONTROL:
			break;
		case eReplicaComponentType::ROCKET_ANIMATION_CONTROL:
			break;
		case eReplicaComponentType::TRIGGER:
			break;
		case eReplicaComponentType::DROPPED_LOOT:
			break;
		case eReplicaComponentType::RACING_CONTROL:
			break;
		case eReplicaComponentType::FACTION_TRIGGER:
			break;
		case eReplicaComponentType::MISSION_OFFER:
			break;
		case eReplicaComponentType::RACING_STATS:
			break;
		case eReplicaComponentType::LUP_EXHIBIT:
			break;
		case eReplicaComponentType::BBB:
			break;
		case eReplicaComponentType::SOUND_TRIGGER:
			break;
		case eReplicaComponentType::PROXIMITY_MONITOR:
			break;
		case eReplicaComponentType::RACING_SOUND_TRIGGER:
			break;
		case eReplicaComponentType::CHAT_BUBBLE:
			break;
		case eReplicaComponentType::FRIENDS_LIST:
			break;
		case eReplicaComponentType::GUILD:
			break;
		case eReplicaComponentType::LOCAL_SYSTEM:
			break;
		case eReplicaComponentType::MISSION:
			break;
		case eReplicaComponentType::MUTABLE_MODEL_BEHAVIORS:
			break;
		case eReplicaComponentType::PATHFINDING:
			break;
		case eReplicaComponentType::PET_TAMING_CONTROL:
			break;
		case eReplicaComponentType::PROPERTY_EDITOR:
			break;
		case eReplicaComponentType::SKINNED_RENDER:
			break;
		case eReplicaComponentType::SLASH_COMMAND:
			break;
		case eReplicaComponentType::STATUS_EFFECT:
			break;
		case eReplicaComponentType::TEAMS:
			break;
		case eReplicaComponentType::TEXT_EFFECT:
			break;
		case eReplicaComponentType::TRADE:
			break;
		case eReplicaComponentType::USER_CONTROL:
			break;
		case eReplicaComponentType::IGNORE_LIST:
			break;
		case eReplicaComponentType::MULTI_ZONE_ENTRANCE:
			break;
		case eReplicaComponentType::BUFF:
			break;
		case eReplicaComponentType::INTERACTION_MANAGER:
			break;
		case eReplicaComponentType::DONATION_VENDOR:
			break;
		case eReplicaComponentType::COMBAT_MEDIATOR:
			break;
		case eReplicaComponentType::ACHIEVEMENT_VENDOR:
			break;
		case eReplicaComponentType::GATE_RUSH_CONTROL:
			break;
		case eReplicaComponentType::RAIL_ACTIVATOR:
			break;
		case eReplicaComponentType::ROLLER:
			break;
		case eReplicaComponentType::PLAYER_FORCED_MOVEMENT:
			break;
		case eReplicaComponentType::CRAFTING:
			break;
		case eReplicaComponentType::POSSESSABLE:
			break;
		case eReplicaComponentType::LEVEL_PROGRESSION:
			break;
		case eReplicaComponentType::POSSESSOR:
			break;
		case eReplicaComponentType::MOUNT_CONTROL:
			break;
		case eReplicaComponentType::UNKNOWN_112:
			break;
		case eReplicaComponentType::PROPERTY_PLAQUE:
			break;
		case eReplicaComponentType::BUILD_BORDER:
			break;
		case eReplicaComponentType::UNKNOWN_115:
			break;
		case eReplicaComponentType::CULLING_PLANE:
			break;
		case eReplicaComponentType::NUMBER_OF_COMPONENTS:
			break;
		case eReplicaComponentType::INVALID:
		default:
			Game::logger->Log("Entity", "blah %i %i", componentId, m_TemplateID);
		}
	}
}

bool Entity::operator==(const Entity& other) const {
	return other.m_ObjectID == m_ObjectID;
}

bool Entity::operator!=(const Entity& other) const {
	return !(other.m_ObjectID == m_ObjectID);
}

User* Entity::GetParentUser() const {
	if (!IsPlayer()) {
		return nullptr;
	}

	return static_cast<const Player*>(this)->GetParentUser();
}

bool Entity::HasComponent(const eReplicaComponentType componentId) const {
	return m_Components.find(componentId) != m_Components.end();
}

std::vector<ScriptComponent*> Entity::GetScriptComponents() {
	std::vector<ScriptComponent*> comps;
	for (const auto&[componentType, component] : m_Components) {
		if (componentType == eReplicaComponentType::SCRIPT) {
			comps.push_back(dynamic_cast<ScriptComponent*>(component.get()));
		}
	}

	return comps;
}

void Entity::Subscribe(const LWOOBJID& scriptObjId, CppScripts::Script* scriptToAdd, const std::string& notificationName) {
	if (notificationName == "HitOrHealResult" || notificationName == "Hit") {
		auto destroyableComponent = GetComponent<DestroyableComponent>();
		if (!destroyableComponent) return;
		destroyableComponent->Subscribe(scriptObjId, scriptToAdd);
	}
}

void Entity::Unsubscribe(const LWOOBJID& scriptObjId, const std::string& notificationName) {
	if (notificationName == "HitOrHealResult" || notificationName == "Hit") {
		auto destroyableComponent = GetComponent<DestroyableComponent>();
		if (!destroyableComponent) return;
		destroyableComponent->Unsubscribe(scriptObjId);
	}
}

void Entity::SetProximityRadius(const float proxRadius, const std::string& name) {
	auto proximityMonitorComponent = AddComponent<ProximityMonitorComponent>();
	if (proximityMonitorComponent) proximityMonitorComponent->SetProximityRadius(proxRadius, name);
}

void Entity::SetProximityRadius(dpEntity* entity, const std::string& name) {
	auto proximityMonitorComponent = AddComponent<ProximityMonitorComponent>();
	if (proximityMonitorComponent) proximityMonitorComponent->SetProximityRadius(entity, name);
}

void Entity::SetGMLevel(eGameMasterLevel value) {
	m_GMLevel = value;
	if (GetParentUser()) {
		Character* character = GetParentUser()->GetLastUsedChar();

		if (character) {
			character->SetGMLevel(value);
		}
	}

	auto character = GetComponent<CharacterComponent>();
	if (character) character->SetGMLevel(value);

	GameMessages::SendGMLevelBroadcast(m_ObjectID, value);
}

void Entity::WriteBaseReplicaData(RakNet::BitStream* outBitStream, eReplicaPacketType packetType) {
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

		auto triggerComponent = GetComponent<TriggerComponent>();
		if (triggerComponent) {
			// has trigger component, check to see if we have events to handle
			auto* trigger = triggerComponent->GetTrigger();
			outBitStream->Write<bool>(trigger && trigger->events.size() > 0);
		} else { // no trigger componenet, so definitely no triggers
			outBitStream->Write0();
		}


		if (m_ParentEntity != nullptr || m_SpawnerID != 0) {
			outBitStream->Write1();
			if (m_ParentEntity != nullptr) outBitStream->Write(GeneralUtils::SetBit(m_ParentEntity->GetObjectID(), static_cast<uint32_t>(eObjectBits::CLIENT)));
			else if (m_Spawner != nullptr && m_Spawner->m_Info.isNetwork) outBitStream->Write(m_SpawnerID);
			else outBitStream->Write(GeneralUtils::SetBit(m_SpawnerID, static_cast<uint32_t>(eObjectBits::CLIENT)));
		} else outBitStream->Write0();

		outBitStream->Write(m_HasSpawnerNodeID);
		if (m_HasSpawnerNodeID) outBitStream->Write(m_SpawnerNodeID);

		//outBitStream->Write0(); //Spawner node id

		if (m_Scale == 1.0f || m_Scale == 0.0f) outBitStream->Write0();
		else {
			outBitStream->Write1();
			outBitStream->Write(m_Scale);
		}

		outBitStream->Write0(); //ObjectWorldState

		if (m_GMLevel != eGameMasterLevel::CIVILIAN) {
			outBitStream->Write1();
			outBitStream->Write(m_GMLevel);
		} else outBitStream->Write0(); //No GM Level
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
			outBitStream->Write((uint16_t)m_ChildEntities.size());
			for (Entity* child : m_ChildEntities) {
				outBitStream->Write((uint64_t)child->GetObjectID());
			}
		}
	}
}

void Entity::WriteComponents(RakNet::BitStream* outBitStream, eReplicaPacketType packetType) {

}

void Entity::ResetFlags() {
	// Unused
}

void Entity::UpdateXMLDoc(tinyxml2::XMLDocument* doc) {
	//This function should only ever be called from within Character, meaning doc should always exist when this is called.
	//Naturally, we don't include any non-player components in this update function.

	for (const auto& pair : m_Components) {
		if (pair.second == nullptr) continue;

		pair.second->UpdateXml(doc);
	}
}

void Entity::Update(const float deltaTime) {
	uint32_t timerPosition;
	timerPosition = 0;
	while (timerPosition < m_Timers.size()) {
		m_Timers[timerPosition]->Update(deltaTime);
		if (m_Timers[timerPosition]->GetTime() <= 0) {
			const auto timerName = m_Timers[timerPosition]->GetName();

			delete m_Timers[timerPosition];
			m_Timers.erase(m_Timers.begin() + timerPosition);

			for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
				script->OnTimerDone(this, timerName);
			}
			TriggerEvent(eTriggerEventType::TIMER_DONE, this);
		} else {
			timerPosition++;
		}
	}

	for (int i = 0; i < m_CallbackTimers.size(); i++) {
		m_CallbackTimers[i]->Update(deltaTime);
		if (m_CallbackTimers[i]->GetTime() <= 0) {
			m_CallbackTimers[i]->GetCallback()();
			delete m_CallbackTimers[i];
			m_CallbackTimers.erase(m_CallbackTimers.begin() + i);
		}
	}

	// Add pending timers to the list of timers so they start next tick.
	if (m_PendingTimers.size() > 0) {
		for (auto namedTimer : m_PendingTimers) {
			m_Timers.push_back(namedTimer);
		}
		m_PendingTimers.clear();
	}

	if (IsSleeping()) {
		Sleep();

		return;
	} else {
		Wake();
	}

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnUpdate(this);
	}

	for (const auto& pair : m_Components) {
		if (pair.second == nullptr) continue;

		pair.second->Update(deltaTime);
	}

	if (m_ShouldDestroyAfterUpdate) {
		EntityManager::Instance()->DestroyEntity(this->GetObjectID());
	}
}

void Entity::OnCollisionProximity(LWOOBJID otherEntity, const std::string& proxName, const std::string& status) {
	Entity* other = EntityManager::Instance()->GetEntity(otherEntity);
	if (!other) return;

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnProximityUpdate(this, other, proxName, status);
	}

	auto rocketComp = GetComponent<RocketLaunchpadControlComponent>();
	if (!rocketComp) return;

	rocketComp->OnProximityUpdate(other, proxName, status);
}

void Entity::OnCollisionPhantom(const LWOOBJID otherEntity) {
	auto* other = EntityManager::Instance()->GetEntity(otherEntity);
	if (!other) return;

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnCollisionPhantom(this, other);
	}

	for (const auto& callback : m_PhantomCollisionCallbacks) {
		callback(other);
	}

	auto switchComp = GetComponent<SwitchComponent>();
	if (switchComp) {
		switchComp->EntityEnter(other);
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
		auto combat = GetComponent<BaseCombatAIComponent>();

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

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnOffCollisionPhantom(this, other);
	}

	TriggerEvent(eTriggerEventType::EXIT, other);

	auto switchComp = GetComponent<SwitchComponent>();
	if (switchComp) {
		switchComp->EntityLeave(other);
	}

	const auto index = std::find(m_TargetsInPhantom.begin(), m_TargetsInPhantom.end(), otherEntity);

	if (index == m_TargetsInPhantom.end()) return;

	m_TargetsInPhantom.erase(index);
}

void Entity::OnFireEventServerSide(Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnFireEventServerSide(this, sender, args, param1, param2, param3);
	}
}

void Entity::OnActivityStateChangeRequest(LWOOBJID senderID, int32_t value1, int32_t value2, const std::u16string& stringValue) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnActivityStateChangeRequest(this, senderID, value1, value2, stringValue);
	}
}

void Entity::OnCinematicUpdate(Entity* self, Entity* sender, eCinematicEvent event, const std::u16string& pathName,
	float_t pathTime, float_t totalTime, int32_t waypoint) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnCinematicUpdate(self, sender, event, pathName, pathTime, totalTime, waypoint);
	}
}

void Entity::NotifyObject(Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	GameMessages::SendNotifyObject(GetObjectID(), sender->GetObjectID(), GeneralUtils::ASCIIToUTF16(name), UNASSIGNED_SYSTEM_ADDRESS);

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnNotifyObject(this, sender, name, param1, param2);
	}
}

void Entity::OnEmoteReceived(const int32_t emote, Entity* target) {
	for (auto* script : CppScripts::GetEntityScripts(this)) {
		script->OnEmoteReceived(this, emote, target);
	}
}

void Entity::OnUse(Entity* originator) {
	TriggerEvent(eTriggerEventType::INTERACT, originator);

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnUse(this, originator);
	}

	// component base class when

	for (const auto& pair : m_Components) {
		if (pair.second == nullptr) continue;

		pair.second->OnUse(originator);
	}
}

void Entity::OnHitOrHealResult(Entity* attacker, int32_t damage) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnHitOrHealResult(this, attacker, damage);
	}
}

void Entity::OnHit(Entity* attacker) {
	TriggerEvent(eTriggerEventType::HIT, attacker);
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnHit(this, attacker);
	}
}

void Entity::OnZonePropertyEditBegin() {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyEditBegin(this);
	}
}

void Entity::OnZonePropertyEditEnd() {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyEditEnd(this);
	}
}

void Entity::OnZonePropertyModelEquipped() {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelEquipped(this);
	}
}

void Entity::OnZonePropertyModelPlaced(Entity* player) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelPlaced(this, player);
	}
}

void Entity::OnZonePropertyModelPickedUp(Entity* player) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelPickedUp(this, player);
	}
}

void Entity::OnZonePropertyModelRemoved(Entity* player) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelRemoved(this, player);
	}
}

void Entity::OnZonePropertyModelRemovedWhileEquipped(Entity* player) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelRemovedWhileEquipped(this, player);
	}
}

void Entity::OnZonePropertyModelRotated(Entity* player) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelRotated(this, player);
	}
}

void Entity::OnMessageBoxResponse(Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnMessageBoxResponse(this, sender, button, identifier, userData);
	}
}

void Entity::OnChoiceBoxResponse(Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnChoiceBoxResponse(this, sender, button, buttonIdentifier, identifier);
	}
}

void Entity::RequestActivityExit(Entity* sender, const LWOOBJID& player, const bool canceled) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnRequestActivityExit(sender, player, canceled);
	}
}

void Entity::Smash(const LWOOBJID source, const eKillType killType, const std::u16string& deathType) {
	if (!m_PlayerIsReadyForUpdates) return;

	auto destroyableComponent = GetComponent<DestroyableComponent>();
	if (destroyableComponent == nullptr) {
		Kill(EntityManager::Instance()->GetEntity(source));
		return;
	}
	auto possessorComponent = GetComponent<PossessorComponent>();
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

	for (const auto& cb : m_DieCallbacks) {
		cb();
	}

	m_DieCallbacks.clear();

	//OMAI WA MOU, SHINDERIU

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnDie(this, murderer);
	}

	if (m_Spawner != nullptr) {
		m_Spawner->NotifyOfEntityDeath(m_ObjectID);
	}

	if (!IsPlayer()) {
		EntityManager::Instance()->DestroyEntity(this);
	}

	const auto& grpNameQBShowBricks = GetVar<std::string>(u"grpNameQBShowBricks");

	if (!grpNameQBShowBricks.empty()) {
		auto spawners = dZoneManager::Instance()->GetSpawnersByName(grpNameQBShowBricks);

		Spawner* spawner = nullptr;

		if (!spawners.empty()) {
			spawner = spawners[0];
		} else {
			spawners = dZoneManager::Instance()->GetSpawnersInGroup(grpNameQBShowBricks);

			if (!spawners.empty()) {
				spawner = spawners[0];
			}
		}

		if (spawner != nullptr) {
			spawner->Spawn();
		}
	}

	// Track a player being smashed
	auto characterComponent = GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->UpdatePlayerStatistic(TimesSmashed);
	}

	// Track a player smashing something else
	if (murderer != nullptr) {
		auto* murdererCharacterComponent = murderer->GetComponent<CharacterComponent>();
		if (murdererCharacterComponent != nullptr) {
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
	auto rebuildComponent = GetComponent<RebuildComponent>();
	if (rebuildComponent != nullptr) {
		rebuildComponent->AddRebuildCompleteCallback(callback);
	}
}

bool Entity::GetIsDead() const {
	auto dest = GetComponent<DestroyableComponent>();
	if (dest && dest->GetArmor() == 0 && dest->GetHealth() == 0) return true;

	return false;
}

void Entity::AddLootItem(const Loot::Info& info) {
	if (!IsPlayer()) return;
	auto& droppedLoot = static_cast<Player*>(this)->GetDroppedLoot();
	droppedLoot.insert(std::make_pair(info.id, info));
}

void Entity::PickupItem(const LWOOBJID& objectID) {
	if (!IsPlayer()) return;
	auto inv = GetComponent<InventoryComponent>();
	if (!inv) return;

	CDObjectsTable* objectsTable = CDClientManager::Instance().GetTable<CDObjectsTable>();

	auto& droppedLoot = static_cast<Player*>(this)->GetDroppedLoot();

	for (const auto& p : droppedLoot) {
		if (p.first == objectID) {
			auto characterComponent = GetComponent<CharacterComponent>();
			if (characterComponent != nullptr) {
				characterComponent->TrackLOTCollection(p.second.lot);
			}

			const CDObjects& object = objectsTable->GetByID(p.second.lot);
			if (object.id != 0 && object.type == "Powerup") {
				CDObjectSkillsTable* skillsTable = CDClientManager::Instance().GetTable<CDObjectSkillsTable>();
				std::vector<CDObjectSkills> skills = skillsTable->Query([=](CDObjectSkills entry) {return (entry.objectTemplate == p.second.lot); });
				for (CDObjectSkills skill : skills) {
					CDSkillBehaviorTable* skillBehTable = CDClientManager::Instance().GetTable<CDSkillBehaviorTable>();
					CDSkillBehavior behaviorData = skillBehTable->GetSkillByID(skill.skillID);

					SkillComponent::HandleUnmanaged(behaviorData.behaviorID, GetObjectID());

					auto missionComponent = GetComponent<MissionComponent>();

					if (missionComponent != nullptr) {
						missionComponent->Progress(eMissionTaskType::POWERUP, skill.skillID);
					}
				}
			} else {
				inv->AddItem(p.second.lot, p.second.count, eLootSourceType::PICKUP, eInventoryType::INVALID, {}, LWOOBJID_EMPTY, true, false, LWOOBJID_EMPTY, eInventoryType::INVALID, 1);
			}
		}
	}

	droppedLoot.erase(objectID);
}

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

void Entity::RegisterCoinDrop(const uint64_t& count) {
	if (!IsPlayer()) return;
	auto* player = static_cast<Player*>(this);
	auto droppedCoins = player->GetDroppedCoins();
	droppedCoins += count;
	player->SetDroppedCoins(droppedCoins);
}

void Entity::AddChild(Entity* child) {
	m_IsParentChildDirty = true;
	m_ChildEntities.push_back(child);
}

void Entity::RemoveChild(Entity* child) {
	if (!child) return;
	uint32_t entityPosition = 0;
	while (entityPosition < m_ChildEntities.size()) {
		if (!m_ChildEntities[entityPosition] || (m_ChildEntities[entityPosition])->GetObjectID() == child->GetObjectID()) {
			m_IsParentChildDirty = true;
			m_ChildEntities.erase(m_ChildEntities.begin() + entityPosition);
		} else {
			entityPosition++;
		}
	}
}

void Entity::RemoveParent() {
	this->m_ParentEntity = nullptr;
}

void Entity::AddTimer(const std::string& name, float time) {
	EntityTimer* timer = new EntityTimer(name, time);
	m_PendingTimers.push_back(timer);
}

void Entity::AddCallbackTimer(const float time, const std::function<void()>& callback) {
	EntityCallbackTimer* timer = new EntityCallbackTimer(time, callback);
	m_CallbackTimers.push_back(timer);
}

bool Entity::HasTimer(const std::string& name) {
	for (auto* timer : m_Timers) {
		if (timer->GetName() == name) {
			return true;
		}
	}

	return false;
}

void Entity::CancelCallbackTimers() {
	for (auto* callback : m_CallbackTimers) {
		delete callback;
	}

	m_CallbackTimers.clear();
}

void Entity::ScheduleKillAfterUpdate(Entity* murderer) {
	//if (m_Info.spawner) m_Info.spawner->ScheduleKill(this);
	EntityManager::Instance()->ScheduleForKill(this);

	if (murderer) m_ScheduleKiller = murderer;
}

void Entity::CancelTimer(const std::string& name) {
	for (int i = 0; i < m_Timers.size(); i++) {
		if (m_Timers[i]->GetName() == name) {
			delete m_Timers[i];
			m_Timers.erase(m_Timers.begin() + i);
			return;
		}
	}
}

void Entity::CancelAllTimers() {
	/*for (auto timer : m_Timers) {
		if (timer) delete timer;
	}*/

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
	auto triggerComponent = GetComponent<TriggerComponent>();
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
	auto baseCombatAIComponent = GetComponent<BaseCombatAIComponent>();

	if (baseCombatAIComponent != nullptr) {
		baseCombatAIComponent->Sleep();
	}
}

void Entity::Wake() {
	auto baseCombatAIComponent = GetComponent<BaseCombatAIComponent>();

	if (baseCombatAIComponent != nullptr) {
		baseCombatAIComponent->Wake();
	}
}

bool Entity::IsSleeping() const {
	return m_IsGhostingCandidate && m_Observers == 0;
}


const NiPoint3& Entity::GetPosition() const {
	if (!this) return NiPoint3::ZERO;

	auto controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr) {
		return controllable->GetPosition();
	}

	auto phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr) {
		return phantom->GetPosition();
	}

	auto simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr) {
		return simple->GetPosition();
	}

	auto vehicle = GetComponent<HavokVehiclePhysicsComponent>();

	if (vehicle != nullptr) {
		return vehicle->GetPosition();
	}

	return NiPoint3::ZERO;
}

const NiQuaternion& Entity::GetRotation() const {
	auto controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr) {
		return controllable->GetRotation();
	}

	auto phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr) {
		return phantom->GetRotation();
	}

	auto simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr) {
		return simple->GetRotation();
	}

	auto vehicle = GetComponent<HavokVehiclePhysicsComponent>();

	if (vehicle != nullptr) {
		return vehicle->GetRotation();
	}

	return NiQuaternion::IDENTITY;
}

void Entity::SetPosition(const NiPoint3& position) {
	auto controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr) {
		controllable->SetPosition(position);
	}

	auto phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr) {
		phantom->SetPosition(position);
	}

	auto simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr) {
		simple->SetPosition(position);
	}

	auto vehicle = GetComponent<HavokVehiclePhysicsComponent>();

	if (vehicle != nullptr) {
		vehicle->SetPosition(position);
	}

	EntityManager::Instance()->SerializeEntity(this);
}

void Entity::SetRotation(const NiQuaternion& rotation) {
	auto controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr) {
		controllable->SetRotation(rotation);
	}

	auto phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr) {
		phantom->SetRotation(rotation);
	}

	auto simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr) {
		simple->SetRotation(rotation);
	}

	auto vehicle = GetComponent<HavokVehiclePhysicsComponent>();

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
	auto inventoryComponent = GetComponent<InventoryComponent>();
	if (!inventoryComponent) return;

	auto itemsVault = inventoryComponent->GetInventory(eInventoryType::VAULT_ITEMS);
	auto modelVault = inventoryComponent->GetInventory(eInventoryType::VAULT_MODELS);

	if (itemsVault->GetSize() == modelVault->GetSize()) return;

	modelVault->SetSize(itemsVault->GetSize());
}
