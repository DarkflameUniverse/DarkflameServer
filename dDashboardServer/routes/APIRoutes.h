#pragma once

#include <string>

#include "json.hpp"

void RegisterAPIRoutes();

// Items (Objects of type Loot) whose name or display name contains the text, or whose LOT it is: [{lot, name}], at most 50
nlohmann::json SearchItems(const std::string& query);
