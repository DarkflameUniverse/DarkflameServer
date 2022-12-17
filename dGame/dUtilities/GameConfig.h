#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include "GeneralUtils.h"

class GameConfig {
public:
	static void Load(const std::string& filepath);

	static const std::string& GetValue(const std::string& key);

	static void SetValue(const std::string& key, const std::string& value);

	template <typename T>
	static T GetValue(const std::string& key) {
		T value;

		if (GeneralUtils::TryParse(GetValue(key), value)) {
			return value;
		}

		return T();
	}

	template <typename T>
	static void SetValue(const std::string& key, const T& value) {
		SetValue(key, std::to_string(value));
	}

private:
	static void ProcessLine(const std::string& line);

	static std::map<std::string, std::string> m_Config;
	static std::string m_EmptyString;
};
