#include "dLocale.h"

#include <clocale>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "tinyxml2.h"
#include "Game.h"
#include "dConfig.h"

dLocale::dLocale() {
	if (Game::config->GetValue("locale_enabled") != "1") {
		return;
	}

	std::ifstream file(m_LocalePath);

	if (!file.good()) {
		return;
	}

	std::stringstream data;
	data << file.rdbuf();

	if (data.str().empty()) {
		return;
	}

	auto* doc = new tinyxml2::XMLDocument();

	if (doc == nullptr) {
		return;
	}

	if (doc->Parse(data.str().c_str(), data.str().size()) != 0) {
		return;
	}

	std::hash<std::string> hash;

	auto* localization = doc->FirstChildElement("localization");
	auto* phrases = localization->FirstChildElement("phrases");

	auto* phrase = phrases->FirstChildElement("phrase");

	while (phrase != nullptr) {
		// Add the phrase hash to the vector
		m_Phrases.push_back(hash(phrase->Attribute("id")));
		phrase = phrase->NextSiblingElement("phrase");
	}

	file.close();

	delete doc;
}

dLocale::~dLocale() = default;

std::string dLocale::GetTemplate(const std::string& phraseID) {
	return "%[" + phraseID + "]";
}

bool dLocale::HasPhrase(const std::string& phraseID) {
	if (Game::config->GetValue("locale_enabled") != "1") {
		return true;
	}

	// Compute the hash and see if it's in the vector
	std::hash<std::string> hash;
	std::size_t hashValue = hash(phraseID);
	return std::find(m_Phrases.begin(), m_Phrases.end(), hashValue) != m_Phrases.end();
}

/*std::string dLocale::GetPhrase(const std::string& phraseID) {
	if (m_Phrases.find(phraseID) == m_Phrases.end()) {
		return "";
	}
	return m_Phrases[phraseID];
}*/
