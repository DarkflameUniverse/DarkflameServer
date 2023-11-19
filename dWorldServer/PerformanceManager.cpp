#include "PerformanceManager.h"
#include "CDZoneTableTable.h"
#include "CDClientManager.h"
#include "UserManager.h"

#define SOCIAL { lowFrameDelta }
#define SOCIAL_HUB { mediumFrameDelta } //Added to compensate for the large playercounts in NS and NT
#define BATTLE { highFrameDelta }
#define BATTLE_INSTANCE { mediumFrameDelta }
#define RACE { highFrameDelta }
#define PROPERTY { lowFrameDelta }

PerformanceProfile PerformanceManager::m_CurrentProfile = SOCIAL;

PerformanceProfile PerformanceManager::m_DefaultProfile = SOCIAL;

PerformanceProfile PerformanceManager::m_InactiveProfile = { lowFrameDelta };

std::map<LWOMAPID, PerformanceProfile> PerformanceManager::m_Profiles = {
	// VE
	{ 1000, SOCIAL },

	// AG
	{ 1100, BATTLE },
	{ 1101, BATTLE_INSTANCE },
	{ 1102, BATTLE_INSTANCE },
	{ 1150, PROPERTY },
	{ 1151, PROPERTY },

	// NS
	{ 1200, SOCIAL_HUB },
	{ 1201, SOCIAL },
	{ 1203, RACE },
	{ 1204, BATTLE_INSTANCE },
	{ 1250, PROPERTY },
	{ 1251, PROPERTY },

	// GF
	{ 1300, BATTLE },
	{ 1302, BATTLE_INSTANCE },
	{ 1303, BATTLE_INSTANCE },
	{ 1350, PROPERTY },

	// FV
	{ 1400, BATTLE },
	{ 1402, BATTLE_INSTANCE },
	{ 1403, RACE },
	{ 1450, PROPERTY },

	// LUP
	{ 1600, SOCIAL },
	{ 1601, SOCIAL },
	{ 1602, SOCIAL },
	{ 1603, SOCIAL },
	{ 1604, SOCIAL },

	// LEGO Club
	{ 1700, SOCIAL },

	// AM
	{ 1800, BATTLE },

	// NT
	{ 1900, SOCIAL_HUB },

	// NJ
	{ 2000, BATTLE },
	{ 2001, BATTLE_INSTANCE },
};

void PerformanceManager::SelectProfile(LWOMAPID mapID) {
	// Try to get it from zoneTable
	CDZoneTableTable* zoneTable = CDClientManager::Instance().GetTable<CDZoneTableTable>();
	if (zoneTable) {
		const CDZoneTable* zone = zoneTable->Query(mapID);
		if (zone) {
			if (zone->serverPhysicsFramerate == "high"){
				m_CurrentProfile = { highFrameDelta };
				return;
			}
			if (zone->serverPhysicsFramerate == "medium"){
				m_CurrentProfile = { mediumFrameDelta };
				return;
			}
			if (zone->serverPhysicsFramerate == "low"){
				m_CurrentProfile = { lowFrameDelta };
				return;
			}
		}
	}

	// Fall back to hardcoded list and defaults
	const auto pair = m_Profiles.find(mapID);
	if (pair == m_Profiles.end()) {
		m_CurrentProfile = m_DefaultProfile;
		return;
	}

	m_CurrentProfile = pair->second;
}

uint32_t PerformanceManager::GetServerFrameDelta() {
	if (UserManager::Instance()->GetUserCount() == 0) {
		return m_InactiveProfile.serverFrameDelta;
	}

	return m_CurrentProfile.serverFrameDelta;
}
