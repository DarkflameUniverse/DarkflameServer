#include "DamageProfile.h"

#include "tinyxml2.h"

std::map<int32_t, DamageProfile> DamageProfile::s_DamageProfiles;

void DamageProfile::LoadDamageProfiles(const std::string& filename) {
	tinyxml2::XMLDocument doc;
	doc.LoadFile(filename.c_str());

	auto root = doc.FirstChildElement("DamageProfiles");

	for (auto element = root->FirstChildElement("DamageProfile"); element != nullptr; element = element->NextSiblingElement("DamageProfile")) {
		auto skillID = element->IntAttribute("skillID");

		DamageProfile damageProfile(skillID);

		for (auto damageElement = element->FirstChildElement("Damage"); damageElement != nullptr; damageElement = damageElement->NextSiblingElement("Damage")) {
			auto statType = damageElement->IntAttribute("type");
			auto value = damageElement->FloatAttribute("value");

			damageProfile.AddDamageProfile(static_cast<eStatTypes>(statType), value);
		}

		s_DamageProfiles.emplace(skillID, damageProfile);
	}
}

DamageProfile* DamageProfile::FindDamageProfile(int32_t skillID) {
	const auto& it = s_DamageProfiles.find(skillID);

	if (it != s_DamageProfiles.end()) {
		return &it->second;
	}

	return nullptr;
}


DamageProfile::DamageProfile(int32_t skillID) {
	this->m_SkillID = skillID;
}

void DamageProfile::AddDamageProfile(eStatTypes statType, float value) {
	m_DamageProfile[statType] = value;
}

float DamageProfile::GetDamageProfile(eStatTypes statType) const {
	const auto& it = m_DamageProfile.find(statType);

	if (it != m_DamageProfile.end()) {
		return it->second;
	}

	return 0.0f;
}
