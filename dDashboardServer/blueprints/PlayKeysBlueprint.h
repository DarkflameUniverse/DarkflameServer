#ifndef __PLAYKEYSBLUEPRINT_H__
#define __PLAYKEYSBLUEPRINT_H__

#include "crow.h"
#include "crow/middlewares/session.h"

namespace PlayKeysBlueprint {

	using Session = crow::SessionMiddleware<crow::InMemoryStore>;
	using DashboardApp = crow::App<crow::CookieParser, Session>;

	/**
	 * Setup play keys management routes
	 * Registers routes for creating, viewing, editing, and deleting play keys
	 */
	void Setup(DashboardApp& app);

} // namespace PlayKeysBlueprint

#endif // __PLAYKEYSBLUEPRINT_H__
