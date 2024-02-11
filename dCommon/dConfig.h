#pragma once
#include <fstream>
#include <map>
#include <string>

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

	/**
	 * Loads the config from a file
	 */
	void LoadConfig();

	/**
	 * Reloads the config file to reset values
	 */
	void ReloadConfig();
private:
	void ProcessLine(const std::string& line);

private:
	std::map<std::string, std::string> m_ConfigValues;
	std::string m_ConfigFilePath;
};
