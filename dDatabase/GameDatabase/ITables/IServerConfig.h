#ifndef __ISERVERCONFIG__H__
#define __ISERVERCONFIG__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * Server settings kept in the database so they can be edited from the dashboard.
 * Each row is one key of one config file (sharedconfig.ini, worldconfig.ini, ...).
 */
class IServerConfig {
public:
	struct Setting {
		std::string file;
		std::string name;
		std::optional<std::string> fileValue; // from the ini or environment when a server last started (never for secrets)
		std::string fileSource;               // "file", "env", or "" when neither sets it
		std::optional<std::string> webValue;  // set on the dashboard
		bool webWins{};                       // the web value beats the file and environment
		bool secret{};
		std::string description;
		int64_t seenAt{};                     // last time a server reported this key from its files
		int64_t updatedAt{};
		std::string updatedBy;
	};

	// A server reports a key it read from its files or environment (keeps any web value)
	virtual void ReportConfigFromFile(const Setting& setting) = 0;

	// Every setting, or only those of the given files
	virtual std::vector<Setting> GetServerConfig(const std::vector<std::string>& files) = 0;

	// Set or clear (nullopt) the web value of a setting, creating it if needed
	virtual void SetWebConfigValue(const std::string& file, const std::string& name, const std::optional<std::string>& value, bool webWins, const std::string& by) = 0;

	// Forget a setting completely
	virtual void DeleteServerConfig(const std::string& file, const std::string& name) = 0;

	/**
	 * One change of a setting's web value, written when it is saved (never parsed back out of the audit log).
	 * Secrets keep no values: only that they changed.
	 */
	struct SettingChange {
		uint64_t id{};
		std::string file;
		std::string name;
		std::optional<std::string> oldValue; // web value before; nullopt: none
		bool oldWebWins{};
		std::optional<std::string> newValue; // web value after; nullopt: cleared
		bool newWebWins{};
		std::optional<std::string> fileValue; // what the files or environment set at the time (what a cleared value falls back to)
		bool secret{};
		bool removed{};                       // the whole setting was forgotten
		uint64_t revertOf{};                  // the change this one undid
		int64_t changedAt{};
		uint32_t accountId{};
		std::string changedBy;
	};

	virtual uint64_t InsertSettingChange(const SettingChange& change) = 0;

	// Newest first; empty file and name: every setting. total: how many match
	virtual std::vector<SettingChange> GetSettingChanges(const std::string& file, const std::string& name, uint32_t start, uint32_t length, uint64_t& total) = 0;

	virtual std::optional<SettingChange> GetSettingChange(uint64_t id) = 0;
};

#endif  //!__ISERVERCONFIG__H__
