#include "GameConfig.h"
#include <sstream>

std::map<std::string, std::string> GameConfig::m_Config{};
std::string GameConfig::m_EmptyString{};

void GameConfig::Load(const std::string& filepath) {
	m_EmptyString = "";
	std::ifstream in(filepath);
	if (!in.good()) return;

	std::string line;
	while (std::getline(in, line)) {
		if (line.length() > 0) {
			if (line[0] != '#') ProcessLine(line);
		}
	}
}

const std::string& GameConfig::GetValue(const std::string& key) {
	const auto& it = m_Config.find(key);

	if (it != m_Config.end()) {
		return it->second;
	}

	return m_EmptyString;
}

void GameConfig::SetValue(const std::string& key, const std::string& value) {
	m_Config.insert_or_assign(key, value);
}

void GameConfig::ProcessLine(const std::string& line) {
	std::stringstream ss(line);
	std::string segment;
	std::vector<std::string> seglist;

	while (std::getline(ss, segment, '=')) {
		seglist.push_back(segment);
	}

	if (seglist.size() != 2) return;

	//Make sure that on Linux, we remove special characters:
	if (!seglist[1].empty() && seglist[1][seglist[1].size() - 1] == '\r')
		seglist[1].erase(seglist[1].size() - 1);

	m_Config.insert_or_assign(seglist[0], seglist[1]);
}
