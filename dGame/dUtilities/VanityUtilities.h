#pragma once

#include "dCommonVars.h"
#include "Entity.h"
#include <map>
#include <set>

struct VanityObjectLocation {
	float m_Chance = 1.0f;
	NiPoint3 m_Position;
	NiQuaternion m_Rotation;
	float m_Scale = 1.0f;
};

struct VanityObject {
	LWOOBJID m_ID = LWOOBJID_EMPTY;
	std::string m_Name;
	LOT m_LOT = LOT_NULL;
	std::vector<LOT> m_Equipment;
	std::vector<std::string> m_Phrases;
	std::map<uint32_t, std::vector<VanityObjectLocation>> m_Locations;
	std::vector<LDFBaseData*> m_Config;
};


namespace VanityUtilities {
	void SpawnVanity();

	VanityObject* GetObject(const std::string& name);

	std::string ParseMarkdown(
		const std::string& file
	);
};
