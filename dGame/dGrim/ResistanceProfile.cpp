#include "ResistanceProfile.h"

#include "tinyxml2.h"

std::map<int32_t, ResistanceProfile> ResistanceProfile::s_ResistanceProfiles;

void ResistanceProfile::LoadResistanceProfiles(const std::string& filename) {
	tinyxml2::XMLDocument doc;
	doc.LoadFile(filename.c_str());

	auto root = doc.FirstChildElement("ResistanceProfiles");

	for (auto element = root->FirstChildElement("ResistanceProfile"); element != nullptr; element = element->NextSiblingElement("ResistanceProfile")) {
		// lot,lot,...
		auto lots = element->Attribute("lot");

		if (lots == nullptr) {
			continue;
		}
		
		const auto& splits = GeneralUtils::SplitString(lots, ',');

		for (const auto& split : splits) {
			const auto lot = std::stoi(split);

			ResistanceProfile resistanceProfile(lot);

			for (auto resistanceElement = element->FirstChildElement("Resistance"); resistanceElement != nullptr; resistanceElement = resistanceElement->NextSiblingElement("Resistance")) {
				auto statType = resistanceElement->IntAttribute("type");
				auto value = resistanceElement->FloatAttribute("value");

				resistanceProfile.AddResistanceProfile(static_cast<eStatTypes>(statType), value);
			}

			s_ResistanceProfiles.emplace(lot, resistanceProfile);
		}
	}
}

ResistanceProfile* ResistanceProfile::FindResistanceProfile(int32_t lot) {
	const auto& it = s_ResistanceProfiles.find(lot);

	if (it != s_ResistanceProfiles.end()) {
		return &it->second;
	}

	return nullptr;
}

ResistanceProfile::ResistanceProfile(int32_t lot) {
	this->m_Lot = lot;
}

void ResistanceProfile::AddResistanceProfile(eStatTypes statType, float value) {
	m_ResistanceProfile[statType] = value;
}

float ResistanceProfile::GetResistanceProfile(eStatTypes statType) const {
	const auto& it = m_ResistanceProfile.find(statType);

	if (it != m_ResistanceProfile.end()) {
		return it->second;
	}

	return 0.0f;
}

