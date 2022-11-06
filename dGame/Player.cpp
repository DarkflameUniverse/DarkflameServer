#include "Player.h"

#include <ctime>

#include "Character.h"
#include "Database.h"
#include "MissionComponent.h"
#include "UserManager.h"
#include "EntityManager.h"
#include "dLogger.h"
#include "ZoneInstanceManager.h"
#include "WorldPackets.h"
#include "dZoneManager.h"
#include "CharacterComponent.h"
#include "Mail.h"
#include "CppScripts.h"

std::vector<Player*> Player::m_Players = {};

Player::Player(const LWOOBJID& objectID, const EntityInfo info, User* user, Entity* parentEntity) : Entity(objectID, info, parentEntity) {
	m_ParentUser = user;
	m_Character = m_ParentUser->GetLastUsedChar();
	m_ParentUser->SetLoggedInChar(objectID);
	m_GMLevel = m_Character->GetGMLevel();
	m_SystemAddress = m_ParentUser->GetSystemAddress();
	m_DroppedLoot = {};
	m_DroppedCoins = 0;

	m_GhostReferencePoint = NiPoint3::ZERO;
	m_GhostOverridePoint = NiPoint3::ZERO;
	m_GhostOverride = false;
	m_ObservedEntitiesLength = 256;
	m_ObservedEntitiesUsed = 0;
	m_ObservedEntities.resize(m_ObservedEntitiesLength);

	m_Character->SetEntity(this);

	const auto& iter = std::find(m_Players.begin(), m_Players.end(), this);

	if (iter != m_Players.end()) {
		return;
	}

	m_Players.push_back(this);
}

User* Player::GetParentUser() const {
	return m_ParentUser;
}

SystemAddress Player::GetSystemAddress() const {
	return m_SystemAddress;
}

void Player::SetSystemAddress(const SystemAddress& value) {
	m_SystemAddress = value;
}

void Player::SetRespawnPos(const NiPoint3 position) {
	m_respawnPos = position;

	m_Character->SetRespawnPoint(dZoneManager::Instance()->GetZone()->GetWorldID(), position);
}

void Player::SetRespawnRot(const NiQuaternion rotation) {
	m_respawnRot = rotation;
}

NiPoint3 Player::GetRespawnPosition() const {
	return m_respawnPos;
}

NiQuaternion Player::GetRespawnRotation() const {
	return m_respawnRot;
}

void Player::SendMail(const LWOOBJID sender, const std::string& senderName, const std::string& subject, const std::string& body, LOT attachment, uint16_t attachmentCount) const {
	Mail::SendMail(sender, senderName, this, subject, body, attachment, attachmentCount);
}

void Player::SendToZone(LWOMAPID zoneId, LWOCLONEID cloneId) {
	const auto objid = GetObjectID();

	ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, zoneId, cloneId, false, [objid](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
		auto* entity = EntityManager::Instance()->GetEntity(objid);

		if (entity == nullptr) {
			return;
		}

		const auto sysAddr = entity->GetSystemAddress();

		auto* character = entity->GetCharacter();
		auto* characterComponent = entity->GetComponent<CharacterComponent>();

		if (character != nullptr && characterComponent != nullptr) {
			character->SetZoneID(zoneID);
			character->SetZoneInstance(zoneInstance);
			character->SetZoneClone(zoneClone);

			characterComponent->SetLastRocketConfig(u"");

			character->SaveXMLToDatabase();
		}

		WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);

		EntityManager::Instance()->DestructEntity(entity);
		return;
		});
}

void Player::AddLimboConstruction(LWOOBJID objectId) {
	const auto& iter = std::find(m_LimboConstructions.begin(), m_LimboConstructions.end(), objectId);

	if (iter != m_LimboConstructions.end()) {
		return;
	}

	m_LimboConstructions.push_back(objectId);
}

void Player::RemoveLimboConstruction(LWOOBJID objectId) {
	const auto& iter = std::find(m_LimboConstructions.begin(), m_LimboConstructions.end(), objectId);

	if (iter == m_LimboConstructions.end()) {
		return;
	}

	m_LimboConstructions.erase(iter);
}

void Player::ConstructLimboEntities() {
	for (const auto objectId : m_LimboConstructions) {
		auto* entity = EntityManager::Instance()->GetEntity(objectId);

		if (entity == nullptr) {
			continue;
		}

		EntityManager::Instance()->ConstructEntity(entity, m_SystemAddress);
	}

	m_LimboConstructions.clear();
}

std::map<LWOOBJID, Loot::Info>& Player::GetDroppedLoot() {
	return m_DroppedLoot;
}

const NiPoint3& Player::GetGhostReferencePoint() const {
	return m_GhostOverride ? m_GhostOverridePoint : m_GhostReferencePoint;
}

const NiPoint3& Player::GetOriginGhostReferencePoint() const {
	return m_GhostReferencePoint;
}

void Player::SetGhostReferencePoint(const NiPoint3& value) {
	m_GhostReferencePoint = value;
}

void Player::SetGhostOverridePoint(const NiPoint3& value) {
	m_GhostOverridePoint = value;
}

const NiPoint3& Player::GetGhostOverridePoint() const {
	return m_GhostOverridePoint;
}

void Player::SetGhostOverride(bool value) {
	m_GhostOverride = value;
}

bool Player::GetGhostOverride() const {
	return m_GhostOverride;
}

void Player::ObserveEntity(int32_t id) {
	for (int32_t i = 0; i < m_ObservedEntitiesUsed; i++) {
		if (m_ObservedEntities[i] == 0 || m_ObservedEntities[i] == id) {
			m_ObservedEntities[i] = id;

			return;
		}
	}

	const auto index = m_ObservedEntitiesUsed++;

	if (m_ObservedEntitiesUsed > m_ObservedEntitiesLength) {
		m_ObservedEntities.resize(m_ObservedEntitiesLength + m_ObservedEntitiesLength);

		m_ObservedEntitiesLength = m_ObservedEntitiesLength + m_ObservedEntitiesLength;
	}

	m_ObservedEntities[index] = id;
}

bool Player::IsObserved(int32_t id) {
	for (int32_t i = 0; i < m_ObservedEntitiesUsed; i++) {
		if (m_ObservedEntities[i] == id) {
			return true;
		}
	}

	return false;
}

void Player::GhostEntity(int32_t id) {
	for (int32_t i = 0; i < m_ObservedEntitiesUsed; i++) {
		if (m_ObservedEntities[i] == id) {
			m_ObservedEntities[i] = 0;
		}
	}
}

Player* Player::GetPlayer(const SystemAddress& sysAddr) {
	auto* entity = UserManager::Instance()->GetUser(sysAddr)->GetLastUsedChar()->GetEntity();

	return static_cast<Player*>(entity);
}

Player* Player::GetPlayer(const std::string& name) {
	const auto characters = EntityManager::Instance()->GetEntitiesByComponent(COMPONENT_TYPE_CHARACTER);

	for (auto* character : characters) {
		if (!character->IsPlayer()) continue;

		if (character->GetCharacter()->GetName() == name) {
			return static_cast<Player*>(character);
		}
	}

	return nullptr;
}

Player* Player::GetPlayer(LWOOBJID playerID) {
	for (auto* player : m_Players) {
		if (player->GetObjectID() == playerID) {
			return player;
		}
	}

	return nullptr;
}

const std::vector<Player*>& Player::GetAllPlayers() {
	return m_Players;
}

uint64_t Player::GetDroppedCoins() {
	return m_DroppedCoins;
}

void Player::SetDroppedCoins(uint64_t value) {
	m_DroppedCoins = value;
}

Player::~Player() {
	Game::logger->Log("Player", "Deleted player");

	for (int32_t i = 0; i < m_ObservedEntitiesUsed; i++) {
		const auto id = m_ObservedEntities[i];

		if (id == 0) {
			continue;
		}

		auto* entity = EntityManager::Instance()->GetGhostCandidate(id);

		if (entity != nullptr) {
			entity->SetObservers(entity->GetObservers() - 1);
		}
	}

	m_LimboConstructions.clear();

	const auto& iter = std::find(m_Players.begin(), m_Players.end(), this);

	if (iter == m_Players.end()) {
		return;
	}

	if (IsPlayer()) {
		Entity* zoneControl = EntityManager::Instance()->GetZoneControlEntity();
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(zoneControl)) {
			script->OnPlayerExit(zoneControl, this);
		}

		std::vector<Entity*> scriptedActs = EntityManager::Instance()->GetEntitiesByComponent(COMPONENT_TYPE_SCRIPTED_ACTIVITY);
		for (Entity* scriptEntity : scriptedActs) {
			if (scriptEntity->GetObjectID() != zoneControl->GetObjectID()) { // Don't want to trigger twice on instance worlds
				for (CppScripts::Script* script : CppScripts::GetEntityScripts(scriptEntity)) {
					script->OnPlayerExit(scriptEntity, this);
				}
			}
		}
	}

	m_Players.erase(iter);
}
