#include "PlayerManager.h"

#include "Character.h"
#include "Player.h"
#include "User.h"
#include "UserManager.h"
#include "eReplicaComponentType.h"

namespace {
	std::vector<Player*> m_Players;
};

const std::vector<Player*>& PlayerManager::GetAllPlayers() {
	return m_Players;
}

void PlayerManager::AddPlayer(Player* player) {
	const auto& iter = std::find(m_Players.begin(), m_Players.end(), player);

	if (iter == m_Players.end()) {
		m_Players.push_back(player);
	}
}

bool PlayerManager::RemovePlayer(Player* player) {
	const auto iter = std::find(m_Players.begin(), m_Players.end(), player);

	const bool toReturn = iter != m_Players.end();
	if (toReturn) {
		m_Players.erase(iter);
	}

	return toReturn;
}

Player* PlayerManager::GetPlayer(const SystemAddress& sysAddr) {
	auto* entity = UserManager::Instance()->GetUser(sysAddr)->GetLastUsedChar()->GetEntity();

	return static_cast<Player*>(entity);
}

Player* PlayerManager::GetPlayer(const std::string& name) {
	const auto characters = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::CHARACTER);

	Player* player = nullptr;
	for (auto* character : characters) {
		if (!character->IsPlayer()) continue;
		
		if (GeneralUtils::CaseInsensitiveStringCompare(name, character->GetCharacter()->GetName())) {
			player = dynamic_cast<Player*>(character);
			break;
		}
	}

	return player;
}

Player* PlayerManager::GetPlayer(LWOOBJID playerID) {
	Player* playerToReturn = nullptr;
	for (auto* player : m_Players) {
		if (player->GetObjectID() == playerID) {
			playerToReturn = player;
			break;
		}
	}

	return playerToReturn;
}
