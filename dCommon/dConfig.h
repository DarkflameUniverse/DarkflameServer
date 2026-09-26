#pragma once

#include <fstream>
#include <functional>
#include <map>
#include <string>

#include "GeneralUtils.h"

class dConfig {
public:
	dConfig(const std::string& filepath);

	/**
	 * Checks whether the specified filepath exists
	 */
	static const bool Exists(const std::string& filepath);

	/**
	 * Gets the specified key from the config.  Returns an empty string if the value is not found.
	 *
	 * @param key Key to find
	 * @return The keys value in the config
	 */
	const std::string& GetValue(std::string key);

	// Gets a value from the config and returns the parsed value, or the default value should parsing have failed.
	template<typename T>
	T GetValue(const std::string& key, const T emptyValue = T()) {
		return GeneralUtils::TryParse<T>(GetValue(key)).value_or(emptyValue);
	}

	std::string GetValue(const std::string& key, const char* emptyValue);

	/**
	 * Loads the config from a file
	 */
	void LoadConfig();

	/**
	 * Reloads the config file to reset values
	 */
	void ReloadConfig();

	// Adds a function to be called when the config is (re)loaded
	void AddConfigHandler(std::function<void()> handler);
	void LogSettings() const;

private:
	void ProcessLine(const std::string& line);

	std::map<std::string, std::string> m_ConfigValues;
	std::vector<std::function<void()>> m_ConfigHandlers;
	std::string m_ConfigFilePath;
};

template<>
inline std::string dConfig::GetValue(const std::string& key, const std::string emptyValue) {
	const auto& value = GetValue(key);
	return value.empty() ? emptyValue : value;
};
