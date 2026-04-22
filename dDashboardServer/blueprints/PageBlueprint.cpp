#include "PageBlueprint.h"
#include "Logger.h"
#include "Database.h"
#include "eGameMasterLevel.h"

namespace PageBlueprint {

	// Helper to get GM level name
	std::string GetGMLevelName(eGameMasterLevel level) {
		switch (level) {
			case eGameMasterLevel::CIVILIAN: return "Civilian";
			case eGameMasterLevel::FORUM_MODERATOR: return "Forum Moderator";
			case eGameMasterLevel::JUNIOR_MODERATOR: return "Junior Moderator";
			case eGameMasterLevel::MODERATOR: return "Moderator";
			case eGameMasterLevel::SENIOR_MODERATOR: return "Senior Moderator";
			case eGameMasterLevel::LEAD_MODERATOR: return "Lead Moderator";
			case eGameMasterLevel::JUNIOR_DEVELOPER: return "Junior Developer";
			case eGameMasterLevel::INACTIVE_DEVELOPER: return "Inactive Developer";
			case eGameMasterLevel::DEVELOPER: return "Developer";
			case eGameMasterLevel::OPERATOR: return "Operator";
			default: return "Unknown";
		}
	}

	// Helper to get current user's account info from session
	std::optional<IAccounts::Info> GetCurrentUser(const crow::request& req, DashboardApp& app) {
		auto& session = app.get_context<Session>(const_cast<crow::request&>(req));
		std::string username = session.template get<std::string>("username");

		if (username.empty()) {
			return std::nullopt;
		}

		return Database::Get()->GetAccountInfo(username);
	}

	// Helper to get user's GM level
	eGameMasterLevel GetUserGMLevel(const crow::request& req, DashboardApp& app) {
		auto user = GetCurrentUser(req, app);
		if (!user) {
			return eGameMasterLevel::CIVILIAN;
		}
		return user->maxGmLevel;
	}

	// Helper to check if user has minimum GM level
	bool HasMinimumGMLevel(const crow::request& req, DashboardApp& app, eGameMasterLevel required) {
		auto level = GetUserGMLevel(req, app);
		return static_cast<uint8_t>(level) >= static_cast<uint8_t>(required);
	}

	// Helper to create base context for all templates
	crow::mustache::context GetBaseContext(const crow::request& req, DashboardApp& app) {
		crow::mustache::context ctx;

		auto& session = app.get_context<Session>(const_cast<crow::request&>(req));
		std::string username = session.template get<std::string>("username");
		int account_id = session.template get<int>("account_id", -1);
		int gm_level = session.template get<int>("gm_level", -1);

		if (!username.empty() && account_id != -1) {
			LOG("User '%s' (Account ID: %d) is authenticated with GM level %d", username.c_str(), account_id, gm_level);
			ctx["is_authenticated"] = true;
			ctx["show_navbar"] = true;
			ctx["username"] = username;
			ctx["account_id"] = account_id;
			ctx["gm_level"] = gm_level;
			ctx["gm_level_name"] = GetGMLevelName(static_cast<eGameMasterLevel>(gm_level));

			// Set permission flags
			ctx["is_gm_3_plus"] = (gm_level >= 3);
			ctx["is_gm_5_plus"] = (gm_level >= 5);
			ctx["is_gm_8_plus"] = (gm_level >= 8);
			ctx["is_gm_9_plus"] = (gm_level >= 9);
		} else {
			LOG("User is not authenticated");
			ctx["is_authenticated"] = false;
			ctx["show_navbar"] = false;
		}

		return ctx;
	}

	// Helper to render a page with layout
	std::string RenderPage(const crow::request& req, DashboardApp& app, const std::string& template_name, const std::string& page_title, crow::mustache::context& page_ctx) {
		auto base_ctx = GetBaseContext(req, app);

		// Merge base context with page-specific context
		for (const auto& key : page_ctx.keys()) {
			base_ctx[key] = crow::json::wvalue(page_ctx[key]);
		}

		// Load the content template and render to string
		auto content_page = crow::mustache::load(template_name);
		std::string content_html = content_page.render_string(base_ctx);

		// Set content and page title in base context
		base_ctx["content"] = crow::json::wvalue(content_html);
		base_ctx["page_title"] = crow::json::wvalue(page_title);

		// Render with layout
		auto layout = crow::mustache::load("layouts/base.html");
		return layout.render_string(base_ctx);
	}

	void Setup(DashboardApp& app) {
		// Home/Dashboard page
		CROW_ROUTE(app, "/")
			([&](const crow::request& req) {
				crow::mustache::context ctx;
				ctx["nav_home"] = true;

				std::string html = RenderPage(req, app, "index.html", "Dashboard", ctx);
				return crow::response(html);
			});

		// Login page
		CROW_ROUTE(app, "/login")
			([&](const crow::request& req) {
				crow::mustache::context ctx;

				std::string html = RenderPage(req, app, "login.html", "Login", ctx);
				return crow::response(html);
			});

		// Accounts page
		CROW_ROUTE(app, "/accounts")
			([&](const crow::request& req) {
				// Check GM level
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				ctx["nav_accounts"] = true;

				std::string html = RenderPage(req, app, "accounts/index.html", "Accounts", ctx);
				return crow::response(html);
			});

		// Activity Logs page
		CROW_ROUTE(app, "/logs/activities")
			([&](const crow::request& req) {
				// Check GM level - Developers and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::DEVELOPER)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				// Set nav active state if needed

				std::string html = RenderPage(req, app, "logs/activities.html", "Activity Logs", ctx);
				return crow::response(html);
			});

		// Characters page
		CROW_ROUTE(app, "/characters")
			([&](const crow::request& req) {
				// Check GM level - Moderators and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				ctx["nav_characters"] = true;

				std::string html = RenderPage(req, app, "characters/index.html", "Characters", ctx);
				return crow::response(html);
			});

		// Play Keys page
		CROW_ROUTE(app, "/playkeys")
			([&](const crow::request& req) {
				// Check GM level - Lead Moderators and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::LEAD_MODERATOR)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				ctx["nav_playkeys"] = true;

				std::string html = RenderPage(req, app, "playkeys/index.html", "Play Keys", ctx);
				return crow::response(html);
			});

			// Registration page - public
			CROW_ROUTE(app, "/register")
				([&](const crow::request& req) {
					crow::mustache::context ctx;
					std::string html = RenderPage(req, app, "register.html", "Register", ctx);
					return crow::response(html);
				});

		// Mail page
		CROW_ROUTE(app, "/mail/send")
			([&](const crow::request& req) {
				// Check GM level - Moderators and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				ctx["nav_mail"] = true;

				std::string html = RenderPage(req, app, "mail/send.html", "Send Mail", ctx);
				return crow::response(html);
			});

		// Bug Reports page
		CROW_ROUTE(app, "/bugreports")
			([&](const crow::request& req) {
				// Anyone authenticated can view their own bug reports
				// GMs can view all
				auto user = GetCurrentUser(req, app);
				if (!user) {
					return crow::response(403, "Forbidden - Login required");
				}

				crow::mustache::context ctx;
				ctx["nav_bugreports"] = true;

				std::string html = RenderPage(req, app, "bugreports/index.html", "Bug Reports", ctx);
				return crow::response(html);
			});

		// Moderation page - Pet Names
		CROW_ROUTE(app, "/moderation/pets")
			([&](const crow::request& req) {
				// Check GM level - Moderators and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				ctx["nav_moderation"] = true;

				std::string html = RenderPage(req, app, "moderation/pets.html", "Pet Name Moderation", ctx);
				return crow::response(html);
			});

		// Moderation page - Properties
		CROW_ROUTE(app, "/moderation/properties")
			([&](const crow::request& req) {
				// Check GM level - Moderators and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				ctx["nav_moderation"] = true;

				std::string html = RenderPage(req, app, "moderation/properties.html", "Property Moderation", ctx);
				return crow::response(html);
			});

		// Account view page
		CROW_ROUTE(app, "/accounts/view/<int>")
			([&](const crow::request& req, int account_id) {
				// Check GM level - Moderators and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				ctx["nav_accounts"] = true;
				ctx["account_id"] = account_id;

				std::string html = RenderPage(req, app, "accounts/view.html", "View Account", ctx);
				return crow::response(html);
			});

		// Character view page
		CROW_ROUTE(app, "/characters/view/<int>")
			([&](const crow::request& req, int character_id) {
				// Check GM level - Moderators and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				ctx["nav_characters"] = true;
				ctx["character_id"] = character_id;

				std::string html = RenderPage(req, app, "characters/view.html", "View Character", ctx);
				return crow::response(html);
			});

		// Logs - Command Logs page
		CROW_ROUTE(app, "/logs/commands")
			([&](const crow::request& req) {
				// Check GM level - Developers and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::DEVELOPER)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				// Set nav active state if needed

				std::string html = RenderPage(req, app, "logs/commands.html", "Command Logs", ctx);
				return crow::response(html);
			});

		// Logs - Audit Logs page
		CROW_ROUTE(app, "/logs/audits")
			([&](const crow::request& req) {
				// Check GM level - Developers and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::DEVELOPER)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				// Set nav active state if needed

				std::string html = RenderPage(req, app, "logs/audits.html", "Audit Logs", ctx);
				return crow::response(html);
			});

		// About page
		CROW_ROUTE(app, "/about")
			([&](const crow::request& req) {
				auto user = GetCurrentUser(req, app);
				if (!user) {
					return crow::response(403, "Forbidden - Login required");
				}

				crow::mustache::context ctx;

				std::string html = RenderPage(req, app, "about.html", "About", ctx);
				return crow::response(html);
			});

		// Bug Reports page (fix routing)
		CROW_ROUTE(app, "/bugs")
			([&](const crow::request& req) {
				// Anyone authenticated can view their own bug reports
				// GMs can view all
				auto user = GetCurrentUser(req, app);
				if (!user) {
					return crow::response(403, "Forbidden - Login required");
				}

				crow::mustache::context ctx;
				ctx["nav_bugs"] = true;

				std::string html = RenderPage(req, app, "bugreports/index.html", "Bug Reports", ctx);
				return crow::response(html);
			});

		// Moderation page - Pending Pets
		CROW_ROUTE(app, "/moderation/pending")
			([&](const crow::request& req) {
				// Check GM level - Moderators and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				ctx["nav_moderation"] = true;

				std::string html = RenderPage(req, app, "moderation/pets.html", "Pending Pet Names", ctx);
				return crow::response(html);
			});

		// Properties page
		CROW_ROUTE(app, "/properties")
			([&](const crow::request& req) {
				// Check GM level - Moderators and above
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "Forbidden - Insufficient GM level");
				}

				crow::mustache::context ctx;
				ctx["nav_moderation"] = true;

				std::string html = RenderPage(req, app, "moderation/properties.html", "Property Moderation", ctx);
				return crow::response(html);
			});
	}

} // namespace PageBlueprint
