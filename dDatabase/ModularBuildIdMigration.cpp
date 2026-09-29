#include "ModularBuildIdMigration.h"

#include <optional>

#include "tinyxml2.h"

#include "Database.h"
#include "eObjectBits.h"
#include "GeneralUtils.h"
#include "Logger.h"

namespace {
	// A persistent id, as ObjectIDManager::GetPersistentID hands them out: from a range reserved in object_id_tracker,
	// with the CHARACTER bit
	class PersistentIds {
	public:
		LWOOBJID Next() {
			if (!m_Range || m_Range->minID > m_Range->maxID) m_Range = Database::Get()->GetPersistentIdRange();
			LWOOBJID id = m_Range->minID++;
			GeneralUtils::SetBit(id, eObjectBits::CHARACTER);
			return id;
		}
	private:
		std::optional<IObjectIdTracker::Range> m_Range;
	};
}

std::vector<ModularBuildIdMigration::NewBuild> ModularBuildIdMigration::AssignIds(tinyxml2::XMLDocument& document, const std::function<LWOOBJID()>& nextId) {
	std::vector<NewBuild> builds;
	auto* obj = document.FirstChildElement("obj");
	auto* inv = obj ? obj->FirstChildElement("inv") : nullptr;
	auto* items = inv ? inv->FirstChildElement("items") : nullptr;
	for (auto* bag = items ? items->FirstChildElement("in") : nullptr; bag; bag = bag->NextSiblingElement("in")) {
		for (auto* item = bag->FirstChildElement("i"); item; item = item->NextSiblingElement("i")) {
			if (item->Int64Attribute("sk", 0) != 0) continue;
			const auto* config = item->FirstChildElement("x");
			const char* saved = config ? config->Attribute("ma") : nullptr;
			if (!saved) continue;
			// Saved as the LDF type and value ("0:1:8129+1:8130"); the build row keeps the value
			std::string modules(saved);
			if (const auto colon = modules.find(':'); colon != std::string::npos && GeneralUtils::TryParse<int32_t>(modules.substr(0, colon))) modules = modules.substr(colon + 1);
			if (modules.empty()) continue;
			const auto id = nextId();
			item->SetAttribute("sk", id);
			builds.push_back({ id, std::move(modules) });
		}
	}
	return builds;
}

void ModularBuildIdMigration::Run() {
	std::vector<LWOOBJID> characters;
	Database::Get()->ForEachCharacterXmlContaining("ma=\"", [&characters](LWOOBJID id, const std::string&) { characters.push_back(id); });

	PersistentIds ids;
	uint32_t builds = 0, changed = 0;
	for (const auto characterId : characters) {
		const auto xml = Database::Get()->GetCharacterXml(characterId);
		tinyxml2::XMLDocument document;
		if (xml.empty() || document.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
			LOG("Character %llu: its saved XML can't be read, its cars and rockets keep no build id", characterId);
			continue;
		}
		const auto made = AssignIds(document, [&ids] { return ids.Next(); });
		if (made.empty()) continue;
		for (const auto& build : made) Database::Get()->InsertUgcBuild(build.modules, build.id, characterId);
		tinyxml2::XMLPrinter printer(0, true, 0);
		document.Print(&printer);
		Database::Get()->UpdateCharacterXml(characterId, printer.CStr());
		builds += static_cast<uint32_t>(made.size());
		changed++;
	}
	LOG("Gave %u cars and rockets of %u characters (made before builds were recorded) a build id", builds, changed);
}
