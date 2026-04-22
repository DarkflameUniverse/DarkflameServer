#ifndef __BUGREPORTSBLUEPRINT_H__
#define __BUGREPORTSBLUEPRINT_H__

#include "crow.h"
#include "crow/middlewares/session.h"

namespace BugReportsBlueprint {

	using Session = crow::SessionMiddleware<crow::InMemoryStore>;
	using DashboardApp = crow::App<crow::CookieParser, Session>;

	/**
	 * Setup bug reports management routes
	 * Registers routes for viewing and resolving bug reports
	 */
	void Setup(DashboardApp& app);

} // namespace BugReportsBlueprint

#endif // __BUGREPORTSBLUEPRINT_H__
