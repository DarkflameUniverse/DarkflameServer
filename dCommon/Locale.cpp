#include "Locale.h"
#include "tinyxml2.h"
#include "Logger.h"

namespace {
	std::unordered_map<std::string, std::string> g_Phrases; // the default locale's
	// The other locales' phrases (only loaded with allLocales), by locale
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> g_Others;
	std::vector<std::string> g_Locales;
	bool g_Loaded = false;
	const std::string g_Empty;
}

void Locale::LoadFromFile(const std::string& filepath, const std::string& locale, bool allLocales) {
	g_Phrases.clear();
	g_Others.clear();
	g_Locales.clear();
	g_Loaded = false;

	tinyxml2::XMLDocument doc;
	if (doc.LoadFile(filepath.c_str()) != tinyxml2::XML_SUCCESS) {
		LOG("Failed to load locale file: %s", filepath.c_str());
		return;
	}

	auto* root = doc.FirstChildElement("localization");
	if (!root) return;

	g_Locales.push_back(locale);
	if (allLocales) {
		if (auto* locales = root->FirstChildElement("locales")) {
			for (auto* entry = locales->FirstChildElement("locale"); entry; entry = entry->NextSiblingElement("locale")) {
				const char* name = entry->GetText();
				if (!name || locale == name || g_Others.contains(name)) continue;
				g_Locales.push_back(name);
				g_Others[name];
			}
		}
	}

	auto* phrases = root->FirstChildElement("phrases");
	if (!phrases) return;

	for (auto* phrase = phrases->FirstChildElement("phrase"); phrase; phrase = phrase->NextSiblingElement("phrase")) {
		const char* id = phrase->Attribute("id");
		if (!id) continue;

		for (auto* trans = phrase->FirstChildElement("translation"); trans; trans = trans->NextSiblingElement("translation")) {
			const char* loc = trans->Attribute("locale");
			const char* text = trans->GetText();
			if (!loc || !text) continue;
			if (locale == loc) {
				g_Phrases[id] = text;
				if (!allLocales) break;
			} else if (allLocales) {
				const auto other = g_Others.find(loc);
				if (other != g_Others.end()) other->second[id] = text;
			}
		}
	}

	g_Loaded = true;
	LOG("Loaded %zu locale phrases from %s (locale: %s, %zu other locale(s))", g_Phrases.size(), filepath.c_str(), locale.c_str(), g_Others.size());
}

const std::string& Locale::GetPhrase(const std::string& phraseId) {
	auto it = g_Phrases.find(phraseId);
	if (it != g_Phrases.end()) return it->second;
	return g_Empty;
}

const std::string& Locale::GetPhrase(const std::string& phraseId, const std::string& locale) {
	if (const auto other = g_Others.find(locale); other != g_Others.end()) {
		if (const auto it = other->second.find(phraseId); it != other->second.end()) return it->second;
	}
	return GetPhrase(phraseId);
}

const std::vector<std::string>& Locale::GetLocales() {
	return g_Locales;
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
