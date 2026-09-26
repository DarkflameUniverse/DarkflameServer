#pragma once

#include "json.hpp"

class HTTPContext;

void RegisterDashboardRoutes();

// Zone ID (as a string) -> display name, from the client's locale
const nlohmann::json& ZoneNames();
