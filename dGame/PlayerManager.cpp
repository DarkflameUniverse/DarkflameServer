#include "PlayerManager.h"

#include "Character.h"
#include "User.h"
#include "UserManager.h"
#include "eReplicaComponentType.h"

namespace {
	std::vector<Entity*> m_Players;
};

const std::vector<Entity*>& PlayerManager::GetAllPlayers() {
	return m_Players;
}

void PlayerManager::AddPlayer(Entity* player) {
	const auto& iter = std::find(m_Players.begin(), m_Players.end(), player);

	if (iter == m_Players.end()) {
		m_Players.push_back(player);
	}
}

bool PlayerManager::RemovePlayer(Entity* player) {
	const auto iter = std::find(m_Players.begin(), m_Players.end(), player);

	const bool toReturn = iter != m_Players.end();
	if (toReturn) {
		m_Players.erase(iter);
	}

	return toReturn;
}

Entity* PlayerManager::GetPlayer(const SystemAddress& sysAddr) {
	auto* entity = UserManager::Instance()->GetUser(sysAddr)->GetLastUsedChar()->GetEntity();

	return entity;
}

Entity* PlayerManager::GetPlayer(const std::string& name) {
	const auto characters = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::CHARACTER);

	Entity* player = nullptr;
	for (auto* character : characters) {
		if (!character->IsPlayer()) continue;
		
		if (GeneralUtils::CaseInsensitiveStringCompare(name, character->GetCharacter()->GetName())) {
			player = character;
			break;
		}
	}

	return player;
}

Entity* PlayerManager::GetPlayer(LWOOBJID playerID) {
	Entity* playerToReturn = nullptr;
	for (auto* player : m_Players) {
		if (player->GetObjectID() == playerID) {
			playerToReturn = player;
			break;
		}
	}

	return playerToReturn;
}
