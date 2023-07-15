#pragma once

#ifndef __EENTITYPROFILE__H__
#define __EENTITYPROFILE__H__

#include <cstdint>
#include <vector>
#include <map>
#include "ItemModifierTemplate.h"

class EntityProfile {
public:
	EntityProfile(int32_t lot);

	~EntityProfile() = default;

	int32_t GetLot() const {
		return this->m_Lot;
	}

	int32_t GetLevel() const {
		return this->m_Level;
	}

	int32_t GetHealth() const {
		return this->m_Health;
	}

	int32_t GetArmor() const {
		return this->m_Armor;
	}

	static void LoadEntityProfiles(const std::string& filename);

	static EntityProfile* FindEntityProfile(int32_t lot);

private:
	int32_t m_Lot;

	int32_t m_Level;

	int32_t m_Health;

	int32_t m_Armor;

	static std::map<int32_t, EntityProfile> s_EntityProfiles;
};

#endif  //!__EENTITYPROFILE__H__