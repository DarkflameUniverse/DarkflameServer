#include "dConfig.h"

#include <sstream>

#include "BinaryPathFinder.h"
#include "GeneralUtils.h"

dConfig::dConfig(const std::string& filepath) {
	m_ConfigFilePath = filepath;
	LoadConfig();
}

void dConfig::LoadConfig() {
	std::ifstream in(BinaryPathFinder::GetBinaryDir() / m_ConfigFilePath);
	if (!in.good()) return;

	std::string line{};
	while (std::getline(in, line)) {
		if (!line.empty() && line.front() != '#') ProcessLine(line);
	}

	std::ifstream sharedConfig(BinaryPathFinder::GetBinaryDir() / "sharedconfig.ini", std::ios::in);
	if (!sharedConfig.good()) return;

	line.clear();
	while (std::getline(sharedConfig, line)) {
		if (!line.empty() && line.front() != '#') ProcessLine(line);
	}
}

void dConfig::ReloadConfig() {
	this->m_ConfigValues.clear();
	LoadConfig();
}

const std::string& dConfig::GetValue(std::string key) {
	static std::string emptyString{};
	
	const auto& it = this->m_ConfigValues.find(key);

	if (it == this->m_ConfigValues.end()) return emptyString;

	return it->second;
}

void dConfig::ProcessLine(const std::string& line) {
	auto splitLine = GeneralUtils::SplitString(line, '=');

	if (splitLine.size() != 2) return;

	//Make sure that on Linux, we remove special characters:
	auto& key = splitLine.at(0);
	auto& value = splitLine.at(1);
	if (!value.empty() && value.at(value.size() - 1) == '\r') value.erase(value.size() - 1);

	if (this->m_ConfigValues.find(key) != this->m_ConfigValues.end()) return;

	this->m_ConfigValues.insert(std::make_pair(key, value));
}
