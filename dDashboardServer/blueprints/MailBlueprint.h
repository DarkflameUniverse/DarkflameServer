#ifndef __MAILBLUEPRINT_H__
#define __MAILBLUEPRINT_H__

#include "crow.h"
#include "crow/middlewares/session.h"

namespace MailBlueprint {

	using Session = crow::SessionMiddleware<crow::InMemoryStore>;
	using DashboardApp = crow::App<crow::CookieParser, Session>;

	/**
	 * Setup mail management routes
	 * Registers routes for sending and viewing mail
	 */
	void Setup(DashboardApp& app);

} // namespace MailBlueprint

#endif // __MAILBLUEPRINT_H__
