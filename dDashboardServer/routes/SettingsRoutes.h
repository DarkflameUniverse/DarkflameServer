#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "PermissionGrants.h"

#include "json.hpp"

struct HTTPContext;

// Server settings stored in the database and edited from the dashboard (GM 9)
void RegisterSettingsRoutes();

/**
 * Save one setting the way the Settings page does: checked, stored, audited, written to the setting history and every
 * server told to reload. Body: {file, name, value (null clears the web value), webWins}. revertOf: the history entry
 * this undoes. Returns why it was refused, if it was.
 */
std::optional<std::string> SaveSetting(const HTTPContext& context, const nlohmann::json& body, uint64_t revertOf = 0);

// A slash command the world servers registered, with the level it needs now (as the Permissions page shows it)
struct SlashCommandNow {
	PermissionGrants::Command rules; // name, level now, floor, fixed, paired permission
	std::vector<std::string> aliases;
	std::string help;
	bool clientHandled{};
};

// Every slash command the world servers registered (empty until a world has started once)
std::vector<SlashCommandNow> CurrentSlashCommands();
