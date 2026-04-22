#pragma once

#include "crow.h"
#include "crow/middlewares/session.h"

namespace PageBlueprint {

	using Session = crow::SessionMiddleware<crow::InMemoryStore>;
	using DashboardApp = crow::App<crow::CookieParser, Session>;

	/**
	 * Setup page rendering routes
	 * Registers routes that render HTML pages (dashboard, login, accounts, etc.)
	 */
	void Setup(DashboardApp& app);

} // namespace PageBlueprint
