#pragma once

#include <cstdint>
#include <optional>
#include <string>

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
