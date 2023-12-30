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
	std::string upper_key(key);
	std::transform(upper_key.begin(), upper_key.end(), upper_key.begin(), ::toupper);
	if (const char* env_p = std::getenv(upper_key.c_str())) {
		this->m_ConfigValues[key] = env_p;
	}
	return this->m_ConfigValues[key];
}

void dConfig::ProcessLine(const std::string& line) {
	auto splitLoc = line.find('=');
	auto key = line.substr(0, splitLoc);
	auto value = line.substr(splitLoc + 1);

	//Make sure that on Linux, we remove special characters:
	if (!value.empty() && value.at(value.size() - 1) == '\r') value.erase(value.size() - 1);

	if (this->m_ConfigValues.find(key) != this->m_ConfigValues.end()) return;

	this->m_ConfigValues.insert(std::make_pair(key, value));
}
