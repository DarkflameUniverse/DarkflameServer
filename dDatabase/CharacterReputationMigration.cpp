#include "CharacterReputationMigration.h"

#include "Database.h"
#include "Logger.h"
#include "tinyxml2.h"

uint32_t CharacterReputationMigration::Run() {
	uint32_t charactersMigrated = 0;

	const auto allCharIds = Database::Get()->GetAllCharacterIds();
	const bool previousCommitValue = Database::Get()->GetAutoCommit();
	Database::Get()->SetAutoCommit(false);

	for (const auto charId : allCharIds) {
		const auto xmlStr = Database::Get()->GetCharacterXml(charId);
		if (xmlStr.empty()) continue;

		tinyxml2::XMLDocument doc;
		if (doc.Parse(xmlStr.c_str(), xmlStr.size()) != tinyxml2::XML_SUCCESS) {
			LOG("Failed to parse XML for character %llu during reputation migration", charId);
			continue;
		}

		auto* obj = doc.FirstChildElement("obj");
		if (!obj) continue;

		auto* character = obj->FirstChildElement("char");
		if (!character) continue;

		int64_t reputation = 0;
		if (character->QueryInt64Attribute("rpt", &reputation) == tinyxml2::XML_SUCCESS && reputation != 0) {
			Database::Get()->SetCharacterReputation(charId, reputation);
			charactersMigrated++;
		}
	}

	Database::Get()->Commit();
	Database::Get()->SetAutoCommit(previousCommitValue);

	return charactersMigrated;
}
