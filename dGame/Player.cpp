#include "Player.h"

#include <ctime>

#include "Character.h"
#include "UserManager.h"
#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"
#include "dZoneManager.h"
#include "User.h"
#include "CppScripts.h"
#include "Loot.h"
#include "eReplicaComponentType.h"
#include "PlayerManager.h"

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
	m_DroppedCoins = 0;

	m_GhostReferencePoint = NiPoint3::ZERO;
	m_GhostOverridePoint = NiPoint3::ZERO;
	m_GhostOverride = false;

	int32_t initialObservedEntitiesCapacity = 256;
	m_ObservedEntities.resize(initialObservedEntitiesCapacity);

	m_Character->SetEntity(this);

	PlayerManager::AddPlayer(this);
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

Player::~Player() {
	LOG("Deleted player");

	for (auto& observedEntity : m_ObservedEntities) {
		if (observedEntity == 0) continue;

		auto* entity = Game::entityManager->GetGhostCandidate(observedEntity);
		if (!entity) continue;
		
		entity->SetObservers(entity->GetObservers() - 1);
	}

	m_LimboConstructions.clear();
	
	// Make sure the player exists first.  Remove afterwards to prevent the OnPlayerExist functions from not being able to find the player.
	if (!PlayerManager::GetPlayer(GetObjectID())) {
		LOG("Unable to find player to remove from manager.");
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

	PlayerManager::RemovePlayer(this);
}
