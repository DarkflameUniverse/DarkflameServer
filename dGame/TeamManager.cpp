#include "TeamManager.h"
#include "EntityManager.h"

TeamManager* TeamManager::m_Address = nullptr; //For singleton method

TeamManager::TeamManager() {
}

Team* TeamManager::GetTeam(LWOOBJID member) const {
	for (const auto& pair : m_Teams) {
		for (const auto memberId : pair.second->members) {
			if (memberId == member) {
				return pair.second;
			}
		}
	}

	return nullptr;
}

LWOOBJID TeamManager::GetNextLootOwner(Team* team) const {
	team->lootRound++;

	if (team->lootRound >= team->members.size()) {
		team->lootRound = 0;
	}

	return team->members[team->lootRound];
}

void TeamManager::UpdateTeam(LWOOBJID teamId, char lootOption, const std::vector<LWOOBJID>& members) {
	const auto& pair = m_Teams.find(teamId);

	Team* team;

	if (pair == m_Teams.end()) {
		if (members.size() <= 1) {
			return;
		}

		team = new Team();
		m_Teams[teamId] = team;
	} else {
		team = pair->second;
	}

	team->members = members;
	team->lootOption = lootOption;
}

void TeamManager::DeleteTeam(LWOOBJID teamId) {
	const auto& pair = m_Teams.find(teamId);

	if (pair == m_Teams.end()) return;

	delete pair->second;

	m_Teams.erase(teamId);
}

TeamManager::~TeamManager() {
}
