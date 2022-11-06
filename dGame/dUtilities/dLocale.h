#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstdint>

class dLocale {
public:
	dLocale();
	~dLocale();
	static std::string GetTemplate(const std::string& phraseID);
	bool HasPhrase(const std::string& phraseID);
	//std::string GetPhrase(const std::string& phraseID);

private:
	std::string m_LocalePath = "./locale/locale.xml";
	std::string m_Locale = "en_US"; // TODO: add to config
	std::vector<std::size_t> m_Phrases;
};
