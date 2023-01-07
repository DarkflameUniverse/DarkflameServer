#pragma once

#include <map>

#include "dCommonVars.h"

struct PerformanceProfile {
	uint32_t serverFrameDelta;
};

class PerformanceManager {
public:
	static void SelectProfile(LWOMAPID mapID);

	static uint32_t GetServerFrameDelta();

private:
	static PerformanceProfile m_CurrentProfile;
	static PerformanceProfile m_DefaultProfile;
	static PerformanceProfile m_InactiveProfile;
	static std::map<LWOMAPID, PerformanceProfile> m_Profiles;
};
