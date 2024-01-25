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

	m_Character->SetEntity(this);

	PlayerManager::AddPlayer(this);
}

Player::~Player() {
	LOG("Deleted player");
	
	// Make sure the player exists first.  Remove afterwards to prevent the OnPlayerExist functions from not being able to find the player.
	if (!PlayerManager::RemovePlayer(this)) {
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
}
