#pragma once

#include "crow.h"
#include "crow/middlewares/session.h"

namespace ApiBlueprint {

	using Session = crow::SessionMiddleware<crow::InMemoryStore>;
	using DashboardApp = crow::App<crow::CookieParser, Session>;

	/**
	 * Setup API routes
	 * Registers all API endpoints for stats, accounts, and moderation
	 */
	void Setup(DashboardApp& app);

} // namespace ApiBlueprint
