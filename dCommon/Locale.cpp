#include "Locale.h"
#include "tinyxml2.h"
#include "Logger.h"

namespace {
	std::unordered_map<std::string, std::string> g_Phrases;
	bool g_Loaded = false;
	const std::string g_Empty;
}

void Locale::LoadFromFile(const std::string& filepath, const std::string& locale) {
	tinyxml2::XMLDocument doc;
	if (doc.LoadFile(filepath.c_str()) != tinyxml2::XML_SUCCESS) {
		LOG("Failed to load locale file: %s", filepath.c_str());
		return;
	}

	auto* root = doc.FirstChildElement("localization");
	if (!root) return;

	auto* phrases = root->FirstChildElement("phrases");
	if (!phrases) return;

	for (auto* phrase = phrases->FirstChildElement("phrase"); phrase; phrase = phrase->NextSiblingElement("phrase")) {
		const char* id = phrase->Attribute("id");
		if (!id) continue;

		for (auto* trans = phrase->FirstChildElement("translation"); trans; trans = trans->NextSiblingElement("translation")) {
			const char* loc = trans->Attribute("locale");
			if (loc && locale == loc) {
				const char* text = trans->GetText();
				if (text) {
					g_Phrases[id] = text;
				}
				break;
			}
		}
	}

	g_Loaded = true;
	LOG("Loaded %zu locale phrases from %s (locale: %s)", g_Phrases.size(), filepath.c_str(), locale.c_str());
}

const std::string& Locale::GetPhrase(const std::string& phraseId) {
	auto it = g_Phrases.find(phraseId);
	if (it != g_Phrases.end()) return it->second;
	return g_Empty;
}

bool Locale::IsLoaded() {
	return g_Loaded;
}

std::vector<std::string> Locale::GetPhraseIdsWithPrefix(const std::string& prefix) {
	std::vector<std::string> result;
	for (const auto& [key, _] : g_Phrases) {
		if (key.compare(0, prefix.size(), prefix) == 0) {
			result.push_back(key);
		}
	}
	return result;
}
