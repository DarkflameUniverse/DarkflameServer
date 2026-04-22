#ifndef __MODERATIONBLUEPRINT_H__
#define __MODERATIONBLUEPRINT_H__

#include "crow.h"
#include "crow/middlewares/session.h"

namespace ModerationBlueprint {

	using Session = crow::SessionMiddleware<crow::InMemoryStore>;
	using DashboardApp = crow::App<crow::CookieParser, Session>;

	/**
	 * Setup moderation routes
	 * Registers routes for pet name moderation and property approval
	 */
	void Setup(DashboardApp& app);

} // namespace ModerationBlueprint

#endif // __MODERATIONBLUEPRINT_H__
