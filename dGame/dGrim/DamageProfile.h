#pragma once

#ifndef __EDAMAGEPROFILE__H__
#define __EDAMAGEPROFILE__H__

#include <cstdint>
#include <vector>
#include <map>
#include "ItemModifierTemplate.h"

class DamageProfile {
public:
	DamageProfile(int32_t skillID);

	~DamageProfile() = default;

	void AddDamageProfile(eStatTypes statType, float value);

	float GetDamageProfile(eStatTypes statType) const;

	static void LoadDamageProfiles(const std::string& filename);

	static DamageProfile* FindDamageProfile(int32_t skillID);

private:
	int32_t m_SkillID;

	std::map<eStatTypes, float> m_DamageProfile;

	static std::map<int32_t, DamageProfile> s_DamageProfiles;
};

#endif  //!__EDAMAGEPROFILE__H__