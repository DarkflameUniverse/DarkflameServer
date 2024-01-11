#include "Player.h"

#include <ctime>

#include "Character.h"
#include "Database.h"
#include "MissionComponent.h"
#include "UserManager.h"
#include "EntityManager.h"
#include "Logger.h"
#include "ZoneInstanceManager.h"
#include "WorldPackets.h"
#include "dZoneManager.h"
#include "CharacterComponent.h"
#include "Mail.h"
#include "User.h"
#include "CppScripts.h"
#include "Loot.h"
#include "eReplicaComponentType.h"

namespace {
	std::vector<Player*> m_Players;
};

const std::vector<Player*>& Player::GetAllPlayers() {
	return m_Players;
}

void Player::SetGhostReferencePoint(const NiPoint3& value) {
	m_GhostReferencePoint = value;
}

void Player::SetGhostOverridePoint(const NiPoint3& value) {
	m_GhostOverridePoint = value;
}

void Player::SetRespawnPos(const NiPoint3& position) {
	if (!m_Character) return;

	m_respawnPos = position;

	m_Character->SetRespawnPoint(Game::zoneManager->GetZone()->GetWorldID(), position);

}

void Player::SetRespawnRot(const NiQuaternion& rotation) {
	m_respawnRot = rotation;
}

void Player::SetSystemAddress(const SystemAddress& value) {
	m_SystemAddress = value;
}

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

	int32_t initialObservedEntitiesCapacity = 256;
	m_ObservedEntities.resize(initialObservedEntitiesCapacity);

	m_Character->SetEntity(this);

	const auto& iter = std::find(m_Players.begin(), m_Players.end(), this);

	if (iter == m_Players.end()) {
		m_Players.push_back(this);
	}
}

void Player::AddLimboConstruction(LWOOBJID objectId) {
	const auto iter = std::find(m_LimboConstructions.begin(), m_LimboConstructions.end(), objectId);
	if (iter == m_LimboConstructions.end()) {
		m_LimboConstructions.push_back(objectId);
	}
}

void Player::RemoveLimboConstruction(LWOOBJID objectId) {
	const auto iter = std::find(m_LimboConstructions.begin(), m_LimboConstructions.end(), objectId);
	if (iter != m_LimboConstructions.end()) {
		m_LimboConstructions.erase(iter);
	}	
}

void Player::ConstructLimboEntities() {
	for (const auto& objectId : m_LimboConstructions) {
		auto* entity = Game::entityManager->GetEntity(objectId);
		if (!entity) continue;

		Game::entityManager->ConstructEntity(entity, m_SystemAddress);
	}

	m_LimboConstructions.clear();
}

void Player::ObserveEntity(int32_t id) {
	for (auto& observedEntity : m_ObservedEntities) {
		if (observedEntity == 0 || observedEntity == id) {
			observedEntity = id;

			return;
		}
	}

	m_ObservedEntities.reserve(m_ObservedEntities.size() + 1);

	m_ObservedEntities.push_back(id);
}

bool Player::IsObserved(int32_t id) {
	return std::find(m_ObservedEntities.begin(), m_ObservedEntities.end(), id) != m_ObservedEntities.end();
}

void Player::GhostEntity(int32_t id) {
	for (auto& observedEntity : m_ObservedEntities) {
		if (observedEntity == id) {
			observedEntity = 0;
		}
	}
}

Player* Player::GetPlayer(const SystemAddress& sysAddr) {
	auto* entity = UserManager::Instance()->GetUser(sysAddr)->GetLastUsedChar()->GetEntity();

	return static_cast<Player*>(entity);
}

Player* Player::GetPlayer(const std::string& name) {
	const auto characters = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::CHARACTER);

	Player* player = nullptr;
	for (auto* character : characters) {
		if (!character->IsPlayer()) continue;
		
		if (GeneralUtils::CaseInsensitiveStringCompare(name, character->GetCharacter()->GetName())) {
			player = dynamic_cast<Player*>(character);
		}
	}

	return player;
}

Player* Player::GetPlayer(LWOOBJID playerID) {
	Player* playerToReturn = nullptr;
	for (auto* player : m_Players) {
		if (player->GetObjectID() == playerID) {
			playerToReturn = player;
		}
	}

	return playerToReturn;
}

Player::~Player() {
	LOG("Deleted player");

	for (auto& observedEntity : m_ObservedEntities) {
		if (observedEntity == 0) continue;

		auto* entity = Game::entityManager->GetGhostCandidate(observedEntity);
		if (!entity) continue;
		
		entity->SetObservers(entity->GetObservers() - 1);
	}

	m_LimboConstructions.clear();

	const auto iter = std::find(m_Players.begin(), m_Players.end(), this);

	if (iter == m_Players.end()) {
		return;
	}

	if (IsPlayer()) {
		Entity* zoneControl = Game::entityManager->GetZoneControlEntity();
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(zoneControl)) {
			script->OnPlayerExit(zoneControl, this);
		}

		std::vector<Entity*> scriptedActs = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SCRIPTED_ACTIVITY);
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
