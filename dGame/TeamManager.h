#pragma once

#include "Entity.h"

struct Team
{
	LWOOBJID teamID = LWOOBJID_EMPTY;
	char lootOption = 0;
	std::vector<LWOOBJID> members{};
	char lootRound = 0;
};

class TeamManager
{
public:
	static TeamManager* Instance() {
		if (!m_Address) {
			m_Address = new TeamManager();
		}

		return m_Address;
	}

	Team* GetTeam(LWOOBJID member) const;
	LWOOBJID GetNextLootOwner(Team* team) const;
	void UpdateTeam(LWOOBJID teamId, char lootOption, const std::vector<LWOOBJID>& members);
	void DeleteTeam(LWOOBJID teamId);

	explicit TeamManager();
	~TeamManager();

private:
	static TeamManager* m_Address; //For singleton method
	std::unordered_map<LWOOBJID, Team*> m_Teams{};
};

