#include "ReportViews.h"

#include <algorithm>
#include <ctime>
#include <set>

#include "RouteUtils.h"
#include "Permissions.h"
#include "Scheduler.h"
#include "Background.h"
#include "EmailService.h"
#include "ReportRoutes.h"
#include "ClientAssets.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	constexpr size_t MAX_VIEWS = 20;
	constexpr int64_t DAY_SECONDS = 24 * 60 * 60;
	const std::set<int> RANGES{ 7, 30, 90, 365 };
	const std::set<std::string> TABS{ "coins", "uscore", "items", "activity", "map", "transfers", "trace", "dupes", "flags" };
	const std::set<std::string> EMAILS{ "off", "daily", "weekly" };
	const std::string SUBSCRIBERS = "report_views:subscribers";

	std::string Key(uint32_t accountId) {
		return "report_views:" + std::to_string(accountId);
	}

	nlohmann::json Load(uint32_t accountId) {
		const auto raw = Database::Get()->GetDashboardState(Key(accountId));
		auto views = raw ? nlohmann::json::parse(*raw, nullptr, false) : nlohmann::json::array();
		return views.is_array() ? views : nlohmann::json::array();
	}

	std::set<uint32_t> Subscribers() {
		const auto raw = Database::Get()->GetDashboardState(SUBSCRIBERS);
		const auto list = raw ? nlohmann::json::parse(*raw, nullptr, false) : nlohmann::json::array();
		std::set<uint32_t> ids;
		if (list.is_array()) for (const auto& id : list) if (id.is_number_unsigned()) ids.insert(id.get<uint32_t>());
		return ids;
	}

	// Keep the index of accounts that asked for emails in step with their views
	void Save(uint32_t accountId, const nlohmann::json& views) {
		if (views.empty()) Database::Get()->DeleteDashboardState(Key(accountId));
		else Database::Get()->SetDashboardState(Key(accountId), views.dump());
		auto subscribers = Subscribers();
		const bool emails = std::ranges::any_of(views, [](const auto& v) { return v.value("email", "off") != "off"; });
		if (emails) subscribers.insert(accountId); else subscribers.erase(accountId);
		Database::Get()->SetDashboardState(SUBSCRIBERS, nlohmann::json(subscribers).dump());
	}

	// A view from the page, checked; nullopt with an error message if something is off
	std::optional<nlohmann::json> Clean(const nlohmann::json& body, std::string& error) {
		std::string name = body.value("name", "");
		name.erase(0, name.find_first_not_of(" \t"));
		name.erase(name.find_last_not_of(" \t") + 1);
		const int days = body.value("days", 30);
		const std::string tab = body.value("tab", "coins"), email = body.value("email", "off");
		if (name.empty() || name.size() > 50) { error = "Give the view a name of up to 50 characters"; return std::nullopt; }
		if (!RANGES.contains(days)) { error = "The range must be 7, 30, 90 or 365 days"; return std::nullopt; }
		if (!TABS.contains(tab)) { error = "Unknown tab"; return std::nullopt; }
		if (!EMAILS.contains(email)) { error = "Email must be off, daily or weekly"; return std::nullopt; }
		return nlohmann::json{ {"name", name}, {"days", days}, {"staff", body.value("staff", false)}, {"lot", std::max(0, body.value("lot", 0))},
			{"lot_name", body.value("lot_name", "").substr(0, 100)}, {"tab", tab}, {"email", email} };
	}

	// Views without the one called `name`
	nlohmann::json Without(const nlohmann::json& views, const std::string& name) {
		nlohmann::json kept = nlohmann::json::array();
		for (const auto& view : views) if (view.value("name", "") != name) kept.push_back(view);
		return kept;
	}

	std::string Number(int64_t value) {
		auto text = std::to_string(value < 0 ? -value : value);
		for (int i = static_cast<int>(text.size()) - 3; i > 0; i -= 3) text.insert(static_cast<size_t>(i), ",");
		return (value < 0 ? "-" : "") + text;
	}

	std::string DateText(uint32_t day) {
		const std::time_t time = static_cast<std::time_t>(day) * DAY_SECONDS;
		char buffer[32];
		std::strftime(buffer, sizeof(buffer), "%b %d", std::gmtime(&time));
		return buffer;
	}

	// The numbers a report email needs, gathered on the background worker
	nlohmann::json Gather(GameDatabase& db, uint32_t from, uint32_t to, bool staff) {
		return {
			{"currency", db.GetCurrencyFlows(from, to, !staff)}, {"uscore", db.GetUScoreFlows(from, to, !staff)},
			{"items", db.GetTopItems(from, to, 5, !staff)}, {"earners", db.GetTopEarners(from, to, 5, !staff)}
		};
	}

	std::string Summarize(const nlohmann::json& view, const nlohmann::json& data, uint32_t from, uint32_t to, const nlohmann::json& sources, uint32_t openFlags) {
		const auto sourceName = [&sources](int id) {
			const auto key = std::to_string(id);
			std::string name = sources.contains(key) ? sources[key].get<std::string>() : "source " + key;
			for (size_t i = 1; i < name.size(); i++) name[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
			std::replace(name.begin(), name.end(), '_', ' ');
			return name;
		};
		int64_t gained = 0, spent = 0, uscore = 0;
		std::map<int, int64_t> bySource;
		for (const auto& row : data["currency"]) {
			gained += row.value("gained", 0ll);
			spent += row.value("spent", 0ll);
			bySource[row.value("source", 0)] += row.value("gained", 0ll);
		}
		for (const auto& row : data["uscore"]) uscore += row.value("gained", 0ll);
		std::vector<std::pair<int, int64_t>> top(bySource.begin(), bySource.end());
		std::sort(top.begin(), top.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

		std::string text = "Economy report \"" + view.value("name", "") + "\": the last " + std::to_string(view.value("days", 30)) + " days (" +
			DateText(from) + " to " + DateText(to) + ", UTC), staff " + (view.value("staff", false) ? "included" : "left out") + ".\n\n";
		text += "Coins: " + Number(gained) + " earned, " + Number(spent) + " spent.\n";
		for (size_t i = 0; i < std::min<size_t>(top.size(), 5); i++) text += "  " + sourceName(top[i].first) + ": " + Number(top[i].second) + "\n";
		text += "\nU-score earned: " + Number(uscore) + "\n\nMost created items:\n";
		for (const auto& item : data["items"]) text += "  " + ClientAssets::ItemName(item.value("lot", 0)) + ": " + Number(item.value("created", 0ll)) + "\n";
		if (data["items"].empty()) text += "  None\n";
		text += "\nTop earners:\n";
		for (const auto& earner : data["earners"]) text += "  " + earner.value("name", std::string("?")) + ": " + Number(earner.value("gained", 0ll)) + " coins\n";
		if (data["earners"].empty()) text += "  None\n";
		text += "\nOpen economy flags: " + std::to_string(openFlags) + "\n";
		const auto url = Game::config->GetValue("dashboard_url");
		if (!url.empty()) {
			std::string name;
			for (const unsigned char c : view.value("name", std::string{})) {
				if (std::isalnum(c) || c == '-' || c == '_' || c == '.') name += static_cast<char>(c);
				else { char hex[4]; std::snprintf(hex, sizeof(hex), "%%%02X", c); name += hex; }
			}
			text += "\nOpen the report: " + url + "/reports#view=" + name + "\n";
		}
		text += "\nYou get this because you asked for it on the Economy page. Change or stop it there, under Views.\n";
		return text;
	}

	struct Delivery {
		std::string to;
		nlohmann::json view;
		uint32_t from{};
		uint32_t to_day{};
	};

	// Send the given views' reports; done gets how many were sent
	void SendReports(std::vector<Delivery> deliveries, std::function<void(size_t, const std::string&)> done) {
		if (deliveries.empty()) return done(0, "");
		std::vector<std::tuple<uint32_t, uint32_t, bool>> ranges;
		for (const auto& d : deliveries) ranges.emplace_back(d.from, d.to_day, d.view.value("staff", false));
		const bool queued = Background::Run("report_emails", [ranges](GameDatabase& db) -> nlohmann::json {
			nlohmann::json results = nlohmann::json::array();
			for (const auto& [from, to, staff] : ranges) results.push_back(Gather(db, from, to, staff));
			return results;
		}, [deliveries, done](nlohmann::json results, const std::string& error) {
			if (!error.empty()) return done(0, error);
			const auto sources = EconomySourceNames();
			const auto openFlags = Database::Get()->GetOpenEconomyFlagCount();
			const auto serverName = Game::config->GetValue("smtp_from_name").empty() ? std::string("DarkflameServer") : Game::config->GetValue("smtp_from_name");
			for (size_t i = 0; i < deliveries.size(); i++) {
				const auto& d = deliveries[i];
				EmailService::Send({ d.to, serverName + " economy report: " + d.view.value("name", ""), Summarize(d.view, results[i], d.from, d.to_day, sources, openFlags) });
			}
			done(deliveries.size(), "");
		});
		if (!queued) done(0, "Report emails are already being sent");
	}

	std::optional<std::string> ConfirmedEmail(uint32_t accountId) {
		const auto email = Database::Get()->GetAccountEmail(accountId);
		return email && email->confirmed && !email->email.empty() ? std::optional<std::string>(email->email) : std::nullopt;
	}

	void RunReportEmails(Scheduler::RunPtr run) {
		if (!EmailService::IsConfigured()) return run->Finish(true, "Email is not set up; nothing sent");
		const auto today = static_cast<uint32_t>(std::time(nullptr) / DAY_SECONDS);
		// Days since the epoch started on a Thursday, so Monday is (day + 3) % 7 == 0
		const bool monday = (today + 3) % 7 == 0;
		std::vector<Delivery> deliveries;
		for (const auto accountId : Subscribers()) {
			const auto account = Database::Get()->GetAccountById(accountId);
			if (account.contains("error") || account.value("banned", 0) || !Permissions::Allowed(static_cast<uint8_t>(account.value("gm_level", 0)), "reports_view")) {
				run->Log("Skipping account " + std::to_string(accountId) + ": no longer allowed to see reports");
				continue;
			}
			const auto email = ConfirmedEmail(accountId);
			if (!email) { run->Log("Skipping " + account.value("name", std::string{}) + ": no confirmed email address"); continue; }
			for (const auto& view : Load(accountId)) {
				const auto when = view.value("email", "off");
				if (when == "daily" || (when == "weekly" && monday)) {
					// Up to yesterday, which is complete
					const uint32_t to = today - 1, from = to - static_cast<uint32_t>(view.value("days", 30)) + 1;
					deliveries.push_back({ *email, view, from, to });
					run->Log("Sending \"" + view.value("name", "") + "\" to " + account.value("name", std::string{}));
				}
			}
		}
		SendReports(std::move(deliveries), [run](size_t sent, const std::string& error) {
			if (!error.empty()) return run->Finish(false, error);
			run->Finish(true, std::to_string(sent) + " report email" + (sent == 1 ? "" : "s") + " queued");
		});
	}
}

void RegisterReportViewTask() {
	Scheduler::Register({ "report_emails", "Report emails",
		"Emails saved Economy views to the people who asked for them: daily ones every day, weekly ones on Mondays.",
		"0 7 * * *", RunReportEmails });
}

void RegisterReportViewRoutes() {
	Route(eHTTPMethod::GET, "/api/reports/views", Perm("reports_view"), "Your saved Economy views",
		[](HTTPReply& reply, const HTTPContext& context) {
			JsonSuccess(reply, { {"views", Load(context.accountId)}, {"emailReady", EmailService::IsConfigured() && ConfirmedEmail(context.accountId).has_value()} });
		});

	Route(eHTTPMethod::POST, "/api/reports/views", Perm("reports_view"),
		"Save a view (replaces one with the same name). Body: {name, days: 7|30|90|365, staff, lot, lot_name, tab, email: off|daily|weekly}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			std::string error;
			const auto view = Clean(*body, error);
			if (!view) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
			if ((*view)["email"] != "off" && !ConfirmedEmail(context.accountId)) {
				return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Confirm an email address on your account page first");
			}
			auto views = Load(context.accountId);
			views = Without(views, (*view)["name"].get<std::string>());
			if (views.size() >= MAX_VIEWS) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "You can keep up to 20 views");
			views.push_back(*view);
			Save(context.accountId, views);
			JsonSuccess(reply, { {"message", "Saved \"" + (*view)["name"].get<std::string>() + "\""}, {"views", views} });
		});

	Route(eHTTPMethod::POST, "/api/reports/views/delete", Perm("reports_view"), "Delete a saved view. Body: {name}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			const std::string name = body ? body->value("name", "") : "";
			auto views = Load(context.accountId);
			const auto before = views.size();
			views = Without(views, name);
			if (views.size() == before) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No view with that name");
			Save(context.accountId, views);
			JsonSuccess(reply, { {"message", "Deleted \"" + name + "\""}, {"views", views} });
		});

	Route(eHTTPMethod::POST, "/api/reports/views/send", Perm("reports_view"), "Email yourself a saved view's report now. Body: {name}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			const std::string name = body ? body->value("name", "") : "";
			const auto views = Load(context.accountId);
			const auto it = std::ranges::find_if(views, [&](const auto& v) { return v.value("name", "") == name; });
			if (it == views.end()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No view with that name");
			if (!EmailService::IsConfigured()) return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Email is not set up on this server");
			const auto email = ConfirmedEmail(context.accountId);
			if (!email) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Confirm an email address on your account page first");
			const auto today = static_cast<uint32_t>(std::time(nullptr) / DAY_SECONDS);
			const uint32_t to = today - 1, from = to - static_cast<uint32_t>(it->value("days", 30)) + 1;
			SendReports({ { *email, *it, from, to } }, [](size_t, const std::string& error) {
				if (!error.empty()) LOG("Report email failed: %s", error.c_str());
			});
			JsonSuccess(reply, { {"message", "Sending the report to " + *email} });
		});
}
