#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "json.hpp"
#include "IServerConfig.h"

struct HTTPContext;

/**
 * History of setting changes made on the dashboard: every save writes what the web value was and became, who and
 * when (server_config_history), and any change can be undone through the same save path. The inline functions are
 * pure and unit tested.
 */
namespace SettingsHistory {
	// Whether a save changes anything: "web value wins" only matters while there is a value
	inline bool Changes(const std::optional<std::string>& oldValue, bool oldWins, const std::optional<std::string>& newValue, bool newWins) {
		return oldValue != newValue || (oldValue.has_value() && oldWins) != (newValue.has_value() && newWins);
	}

	// Whether the setting still holds what a change gave it, so undoing it doesn't also undo a later change
	inline bool StillCurrent(const IServerConfig::SettingChange& change, const std::optional<std::string>& currentValue, bool currentWins) {
		return !Changes(change.newValue, change.newWebWins, currentValue, currentWins);
	}

	/**
	 * The save that undoes a change, as the Settings page sends it: {file, name, value (null: clear), webWins}.
	 * error says why it can't be undone.
	 */
	inline std::optional<nlohmann::json> RevertBody(const IServerConfig::SettingChange& change, std::string& error) {
		if (change.secret) {
			error = "Secrets aren't kept in the history, so this can't be undone; set it again on the Settings page";
			return std::nullopt;
		}
		return nlohmann::json{ {"file", change.file}, {"name", change.name},
			{"value", change.oldValue ? nlohmann::json(*change.oldValue) : nlohmann::json(nullptr)}, {"webWins", change.oldValue.has_value() && change.oldWebWins} };
	}

	// ---- Below need the database (SettingsHistory.cpp) ----

	// The setting's row as it is now, to pass to Record after saving
	std::optional<IServerConfig::Setting> Current(const std::string& file, const std::string& name);

	/**
	 * Write a change to the history, unless it changed nothing. `before` is the row from Current() taken before saving.
	 * @param removed the setting was forgotten entirely
	 * @param revertOf the history entry this change undoes (0: none)
	 */
	void Record(const HTTPContext& context, const std::string& file, const std::string& name, const std::optional<IServerConfig::Setting>& before,
		const std::optional<std::string>& value, bool webWins, bool removed = false, uint64_t revertOf = 0);

	// The history pages' routes; undoing goes through SaveSetting (SettingsRoutes.h)
	void RegisterRoutes();
}
