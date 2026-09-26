#pragma once

#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "GeneralUtils.h"

/**
 * Server settings. Values come from, in order of priority:
 *  1. the database, for settings marked "web value wins" on the dashboard
 *  2. an environment variable with the key's name in upper case
 *  3. the server's own config file, then sharedconfig.ini
 *  4. the database, for settings only set on the dashboard
 * Database values are layered in by a sync function set with SetDatabaseSync once the database is connected.
 * Connection settings (and a few security keys) only ever come from files or the environment.
 */
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
	 * Reloads the config file (and database values) to reset values
	 */
	void ReloadConfig();

	// Adds a function to be called when the config is (re)loaded
	void AddConfigHandler(std::function<void()> handler);
	void LogSettings() const;

	// ---- Database layer ----

	struct FileEntry {
		std::string key;
		std::string value;
		std::string file;        // the file it came from
		std::string description; // the comment lines just above it
	};

	// This server's own config file name, e.g. "worldconfig.ini"
	const std::string& GetFileName() const { return m_ConfigFilePath; }

	// Every key read from the files, in the order read
	std::vector<FileEntry> GetFileEntries() const;

	// The environment variable for a key, if set
	static std::optional<std::string> GetEnvironmentValue(const std::string& key);

	// Settings that must never come from the database (connection details, keys that protect the database itself)
	static bool IsFileOnlyKey(const std::string& key);

	// Settings whose values are secret: never copied from files into the database, never shown on the dashboard
	static bool IsSecretKey(const std::string& key);

	/**
	 * Values from the database: overrides beat files and the environment ("web value wins"),
	 * fallbacks are only used when neither sets the key.
	 */
	void SetDatabaseValues(std::map<std::string, std::string> overrides, std::map<std::string, std::string> fallbacks);

	// Run the database sync now and again after every reload
	void SetDatabaseSync(std::function<void(dConfig&)> sync);

private:
	void ProcessLine(const std::string& line, const std::string& file, const std::string& description);
	void LoadFile(const std::string& file);

	std::map<std::string, std::string> m_ConfigValues;
	std::map<std::string, FileEntry> m_FileEntries;
	std::vector<std::string> m_FileOrder;
	std::map<std::string, std::string> m_EnvValues;
	std::map<std::string, std::string> m_DatabaseOverrides;
	std::map<std::string, std::string> m_DatabaseFallbacks;
	std::vector<std::function<void()>> m_ConfigHandlers;
	std::function<void(dConfig&)> m_DatabaseSync;
	std::string m_ConfigFilePath;
};

template<>
inline std::string dConfig::GetValue(const std::string& key, const std::string emptyValue) {
	const auto& value = GetValue(key);
	return value.empty() ? emptyValue : value;
};
