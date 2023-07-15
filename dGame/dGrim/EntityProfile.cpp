#include "EntityProfile.h"

#include "tinyxml2.h"

std::map<int32_t, EntityProfile> EntityProfile::s_EntityProfiles;

EntityProfile::EntityProfile(int32_t lot) {
	this->m_Lot = lot;
}

void EntityProfile::LoadEntityProfiles(const std::string& filename) {
	tinyxml2::XMLDocument doc;
	doc.LoadFile(filename.c_str());

	const auto root = doc.FirstChildElement("EntityProfiles");

	for (auto elem = root->FirstChildElement("EntityProfile"); elem != nullptr; elem = elem->NextSiblingElement("EntityProfile")) {
		const auto lot = elem->IntAttribute("lot");

		EntityProfile profile(lot);

		profile.m_Level = elem->IntAttribute("level");
		profile.m_Health = elem->IntAttribute("health");
		profile.m_Armor = elem->IntAttribute("armor");

		s_EntityProfiles.emplace(lot, profile);
	}
}

EntityProfile* EntityProfile::FindEntityProfile(int32_t lot) {
	const auto it = s_EntityProfiles.find(lot);

	if (it != s_EntityProfiles.end()) {
		return &it->second;
	}

	return nullptr;
}
