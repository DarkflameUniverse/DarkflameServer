#ifndef __CHARACTERSBLUEPRINT_H__
#define __CHARACTERSBLUEPRINT_H__

#include "crow.h"
#include "crow/middlewares/session.h"

namespace CharactersBlueprint {

	using Session = crow::SessionMiddleware<crow::InMemoryStore>;
	using DashboardApp = crow::App<crow::CookieParser, Session>;

	/**
	 * Setup character management routes
	 * Registers routes for viewing, editing, and managing characters
	 */
	void Setup(DashboardApp& app);

} // namespace CharactersBlueprint

#endif // __CHARACTERSBLUEPRINT_H__
