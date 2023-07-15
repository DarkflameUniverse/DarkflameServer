#pragma once

#ifndef __ERESISTANCEPROFILE__H__
#define __ERESISTANCEPROFILE__H__

#include <cstdint>
#include <vector>
#include <map>
#include "ItemModifierTemplate.h"

class ResistanceProfile {
public:
	ResistanceProfile(int32_t lot);

	~ResistanceProfile() = default;

	void AddResistanceProfile(eStatTypes statType, float value);

	float GetResistanceProfile(eStatTypes statType) const;

	static void LoadResistanceProfiles(const std::string& filename);

	static ResistanceProfile* FindResistanceProfile(int32_t lot);

private:
	int32_t m_Lot;

	std::map<eStatTypes, float> m_ResistanceProfile;

	static std::map<int32_t, ResistanceProfile> s_ResistanceProfiles;
};

#endif  //!__ERESISTANCEPROFILE__H__