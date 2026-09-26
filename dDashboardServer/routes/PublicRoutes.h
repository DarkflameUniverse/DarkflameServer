#pragma once

#include "json.hpp"

/**
 * The public server status (off until public_status=1): a page at /status, JSON at /api/public/status for server
 * lists, and a widget at /status/widget other sites may frame. What it shows is chosen in Settings (Public pages).
 * The status is worked out at most every public_status_cache_seconds, however many people ask.
 */
void RegisterPublicRoutes();

// The top bar of public pages: {name, status, showcase} (which public pages are on)
nlohmann::json PublicPageJson();
