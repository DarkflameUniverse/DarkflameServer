#include "dConfig.h"
#include <sstream>
#include "BinaryPathFinder.h"

dConfig::dConfig(const std::string& filepath) {
	m_EmptyString = "";
	std::ifstream in(BinaryPathFinder::GetBinaryDir() / filepath);
	if (!in.good()) return;

	std::string line;
	while (std::getline(in, line)) {
		if (line.length() > 0) {
			if (line[0] != '#') ProcessLine(line);
		}
	}

	std::ifstream sharedConfig(BinaryPathFinder::GetBinaryDir() / "sharedconfig.ini", std::ios::in);
	if (!sharedConfig.good()) return;

	while (std::getline(sharedConfig, line)) {
		if (line.length() > 0) {
			if (line[0] != '#') ProcessLine(line);
		}
	}
}

dConfig::~dConfig(void) {
}

const std::string& dConfig::GetValue(std::string key) {
	for (size_t i = 0; i < m_Keys.size(); ++i) {
		if (m_Keys[i] == key) return m_Values[i];
	}

	return m_EmptyString;
}

void dConfig::ProcessLine(const std::string& line) {
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

	for (const auto& key : m_Keys) {
		if (seglist[0] == key) {
			return; // first loaded key is preferred due to loading shared config secondarily
		}
	}

	m_Keys.push_back(seglist[0]);
	m_Values.push_back(seglist[1]);
}
