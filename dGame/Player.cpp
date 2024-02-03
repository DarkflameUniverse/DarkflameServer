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

Player::Player(const LWOOBJID& objectID, const EntityInfo info, User* user, Entity* parentEntity) : Entity(objectID, info, parentEntity) {
	m_ParentUser = user;
	m_Character = m_ParentUser->GetLastUsedChar();
	m_ParentUser->SetLoggedInChar(objectID);
	m_GMLevel = m_Character->GetGMLevel();
	m_DroppedCoins = 0;

	m_Character->SetEntity(this);

	PlayerManager::AddPlayer(this);
}
