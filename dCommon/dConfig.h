#pragma once
#include <fstream>
#include <string>
#include <vector>

class dConfig {
public:
	dConfig(const std::string& filepath);
	~dConfig(void);

	const std::string& GetValue(std::string key);

private:
	void ProcessLine(const std::string& line);

private:
	std::vector<std::string> m_Keys;
	std::vector<std::string> m_Values;
	std::string m_EmptyString;
};
