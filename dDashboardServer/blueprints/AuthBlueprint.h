#pragma once

#include "crow.h"
#include "crow/middlewares/session.h"

namespace AuthBlueprint {

	using Session = crow::SessionMiddleware<crow::InMemoryStore>;
	using DashboardApp = crow::App<crow::CookieParser, Session>;

	/**
	 * Setup authentication routes
	 * Registers login, logout, and auth status endpoints
	 */
	void Setup(DashboardApp& app);

} // namespace AuthBlueprint
