#include "BugReportsBlueprint.h"
#include "Database.h"
#include "eGameMasterLevel.h"
#include "Logger.h"
#include <ctime>

namespace BugReportsBlueprint {

	// Helper function to get current user's account info from session
	std::optional<IAccounts::Info> GetCurrentUser(const crow::request& req, DashboardApp& app) {
		auto& session = app.get_context<Session>(const_cast<crow::request&>(req));
		std::string username = session.template get<std::string>("username");

		if (username.empty()) {
			return std::nullopt;
		}

		return Database::Get()->GetAccountInfo(username);
	}

	// Helper function to get user's GM level
	eGameMasterLevel GetUserGMLevel(const crow::request& req, DashboardApp& app) {
		auto user = GetCurrentUser(req, app);
		if (!user) {
			return eGameMasterLevel::CIVILIAN;
		}
		return user->maxGmLevel;
	}

	// Helper function to check if user has minimum GM level
	bool HasMinimumGMLevel(const crow::request& req, DashboardApp& app, eGameMasterLevel required) {
		auto level = GetUserGMLevel(req, app);
		return static_cast<uint8_t>(level) >= static_cast<uint8_t>(required);
	}

	void Setup(DashboardApp& app) {
		// Get all bug reports (filtered by status)
		CROW_ROUTE(app, "/api/bugreports")
			.methods("GET"_method)
			([&](const crow::request& req) {
				// Anyone authenticated can view their own bug reports
				// GMs can view all
				auto user = GetCurrentUser(req, app);
				if (!user) {
					return crow::response(401, "{\"error\": \"Not authenticated\"}");
				}

				crow::json::wvalue response;
				crow::json::wvalue::list data;

				try {
					auto statusParam = req.url_params.get("status");
					std::string status = statusParam ? statusParam : "all";

					std::vector<IBugReports::DetailedInfo> reports;

					if (status == "resolved") {
						reports = Database::Get()->GetResolvedBugReports();
					} else if (status == "unresolved") {
						reports = Database::Get()->GetUnresolvedBugReports();
					} else {
						reports = Database::Get()->GetAllBugReports();
					}

					bool isGM = static_cast<uint8_t>(user->maxGmLevel) >= static_cast<uint8_t>(eGameMasterLevel::MODERATOR);

					for (const auto& report : reports) {
						// If not a GM, only show reports from user's own characters
						if (!isGM) {
							auto charInfo = Database::Get()->GetCharacterInfo(report.characterId);
							if (!charInfo || charInfo->accountId != user->id) {
								continue;
							}
						}

						crow::json::wvalue item;
						item["id"] = report.id;
						item["body"] = report.body;
						item["client_version"] = report.clientVersion;
						item["other_player"] = report.otherPlayer;
						item["selection"] = report.selection;
						item["character_id"] = static_cast<uint64_t>(report.characterId);
						item["submitted"] = report.submitted;
						item["resolved_time"] = report.resolved_time;
						item["resolved_by_id"] = report.resolved_by_id;
						item["resolution"] = report.resolution;

						// Get character name
						auto charInfo = Database::Get()->GetCharacterInfo(report.characterId);
						if (charInfo) {
							item["character_name"] = charInfo->name;
						} else {
							item["character_name"] = "Unknown";
						}

						data.push_back(std::move(item));
					}

					response["data"] = std::move(data);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Get a single bug report by ID
		CROW_ROUTE(app, "/api/bugreports/<uint>")
			.methods("GET"_method)
			([&](const crow::request& req, uint64_t report_id) {
				auto user = GetCurrentUser(req, app);
				if (!user) {
					return crow::response(401, "{\"error\": \"Not authenticated\"}");
				}

				crow::json::wvalue response;

				try {
					auto report = Database::Get()->GetBugReportById(report_id);
					if (!report) {
						response["success"] = false;
						response["error"] = "Bug report not found";
						return crow::response(404, response);
					}

					// Check access rights
					bool canAccess = false;
					if (static_cast<uint8_t>(user->maxGmLevel) >= static_cast<uint8_t>(eGameMasterLevel::MODERATOR)) {
						canAccess = true;
					} else {
						auto charInfo = Database::Get()->GetCharacterInfo(report->characterId);
						if (charInfo && charInfo->accountId == user->id) {
							canAccess = true;
						}
					}

					if (!canAccess) {
						response["success"] = false;
						response["error"] = "Access denied";
						return crow::response(403, response);
					}

					response["success"] = true;
					response["id"] = report->id;
					response["body"] = report->body;
					response["client_version"] = report->clientVersion;
					response["other_player"] = report->otherPlayer;
					response["selection"] = report->selection;
					response["character_id"] = static_cast<uint64_t>(report->characterId);
					response["submitted"] = report->submitted;
					response["resolved_time"] = report->resolved_time;
					response["resolved_by_id"] = report->resolved_by_id;
					response["resolution"] = report->resolution;

					// Get character name
					auto charInfo = Database::Get()->GetCharacterInfo(report->characterId);
					if (charInfo) {
						response["character_name"] = charInfo->name;
					}

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Resolve a bug report
		CROW_ROUTE(app, "/api/bugreports/<uint>/resolve")
			.methods("POST"_method)
			([&](const crow::request& req, uint64_t report_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				auto body = crow::json::load(req.body);
				if (!body) {
					return crow::response(400, "{\"error\": \"Invalid JSON\"}");
				}

				crow::json::wvalue response;

				try {
					auto user = GetCurrentUser(req, app);
					if (!user) {
						response["success"] = false;
						response["error"] = "Not authenticated";
						return crow::response(401, response);
					}

					std::string resolution;
					if (body.has("resolution"))
						resolution = std::string(body["resolution"].s());
					else
						resolution = "";

					if (resolution.empty()) {
						response["success"] = false;
						response["error"] = "Resolution message is required";
						return crow::response(response);
					}

					// Check if report exists and is not already resolved
					auto report = Database::Get()->GetBugReportById(report_id);
					if (!report) {
						response["success"] = false;
						response["error"] = "Bug report not found";
						return crow::response(404, response);
					}

					if (report->resolved_time > 0) {
						response["success"] = false;
						response["error"] = "Bug report already resolved";
						return crow::response(response);
					}

					Database::Get()->ResolveBugReport(report_id, user->id, resolution);

					response["success"] = true;
					response["message"] = "Bug report resolved successfully";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});
	}

} // namespace BugReportsBlueprint
