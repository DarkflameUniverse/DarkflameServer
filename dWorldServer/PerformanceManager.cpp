#include "PerformanceManager.h"

#include "UserManager.h"

//Times are 1 / fps, in ms
#define HIGH 16     //60 fps
#define MEDIUM 33   //30 fps
#define LOW 66      //15 fps

#define SOCIAL { LOW }
#define SOCIAL_HUB { MEDIUM } //Added to compensate for the large playercounts in NS and NT
#define BATTLE { HIGH }
#define BATTLE_INSTANCE { MEDIUM }
#define RACE { HIGH }
#define PROPERTY { LOW }

PerformanceProfile PerformanceManager::m_CurrentProfile = SOCIAL;

PerformanceProfile PerformanceManager::m_DefaultProfile = SOCIAL;

PerformanceProfile PerformanceManager::m_InactiveProfile = { LOW };

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


PerformanceManager::PerformanceManager() {
}

PerformanceManager::~PerformanceManager() {
}

void PerformanceManager::SelectProfile(LWOMAPID mapID) {
	const auto pair = m_Profiles.find(mapID);

	if (pair == m_Profiles.end()) {
		m_CurrentProfile = m_DefaultProfile;

		return;
	}

	m_CurrentProfile = pair->second;
}

uint32_t PerformanceManager::GetServerFramerate() {
	if (UserManager::Instance()->GetUserCount() == 0) {
		return m_InactiveProfile.serverFramerate;
	}

	return m_CurrentProfile.serverFramerate;
}
