#include "PublicRoutes.h"

#include <chrono>
#include <cmath>
#include <ctime>
#include <map>
#include <optional>

#include "RouteUtils.h"
#include "PublicStatus.h"
#include "ServerState.h"
#include "LiveWorld.h"
#include "LeaderboardRoutes.h"
#include "ChallengeRoutes.h"
#include "LiveEventRoutes.h"
#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	using Clock = std::chrono::steady_clock;
	constexpr int64_t UPTIME_BUCKET = 5 * 60;
	constexpr int64_t UPTIME_HISTORY = 7 * 24 * 60 * 60;

	// Generous for people, tight enough that a script can't keep the web server busy
	RateLimiter g_Limiter(120, std::chrono::seconds(60));

	struct Snapshot {
		nlohmann::json status;
		std::string body; // status as JSON text, for the API
		Clock::time_point made;
	};
	std::optional<Snapshot> g_Snapshot;

	bool Enabled() { return ConfigFlag("public_status", false); }

	int64_t Setting(const std::string& key, int64_t fallback, int64_t min, int64_t max) {
		const auto value = Game::config ? GeneralUtils::TryParse<int64_t>(Game::config->GetValue(key)) : std::nullopt;
		return std::clamp(value.value_or(fallback), min, max);
	}

	int64_t CacheSeconds() { return Setting("public_status_cache_seconds", 60, 10, 3600); }

	std::string ServerName() {
		const auto name = Game::config ? Game::config->GetValue("public_server_name") : "";
		return name.empty() ? "DarkflameServer" : name;
	}

	// Share of time up (0-1) as a percentage with one decimal, or null when there's no history yet
	nlohmann::json Percent(double share) {
		if (share < 0) return nullptr;
		return std::round(share * 1000.0) / 10.0;
	}

	// Character names online by zone, leaving out staff: they may be invisible in game, and the count still has them
	std::map<uint32_t, std::vector<std::string>> NamesByZone() {
		std::map<uint32_t, std::vector<std::string>> names;
		for (const auto& player : LiveWorld::OnlinePlayers()) {
			const auto id = GeneralUtils::TryParse<LWOOBJID>(player.value("id", std::string{}));
			const auto info = id ? Database::Get()->GetCharacterInfo(*id) : std::nullopt;
			if (!info || info->name.empty()) continue;
			if (Database::Get()->GetAccountById(info->accountId).value("gm_level", 0) > 0) continue;
			names[player.value("zone", 0u)].push_back(info->name);
		}
		for (auto& [zone, list] : names) std::sort(list.begin(), list.end());
		return names;
	}

	// Everything the public may see, built from what the settings allow. Nothing here is per visitor.
	nlohmann::json BuildStatus() {
		const auto now = static_cast<int64_t>(std::time(nullptr));
		std::vector<PublicStatus::World> worlds;
		bool loginUp = false, chatUp = false;
		{
			std::lock_guard lock(ServerState::g_StatusMutex);
			loginUp = ServerState::g_AuthStatus.online;
			chatUp = ServerState::g_ChatStatus.online;
			for (const auto& world : ServerState::g_WorldInstances) worlds.push_back({ world.mapID, world.zoneName, world.players });
		}

		nlohmann::json status{ {"name", ServerName()}, {"online", loginUp}, {"generated_at", now}, {"refresh_seconds", CacheSeconds()} };

		if (ConfigFlag("public_status_players", true)) {
			const auto names = ConfigFlag("public_status_names", false) ? NamesByZone() : std::map<uint32_t, std::vector<std::string>>{};
			uint32_t total = 0;
			nlohmann::json zones = nlohmann::json::array();
			for (const auto& zone : PublicStatus::CountByZone(worlds)) {
				total += zone.players;
				if (zone.zoneId == 0) continue; // character select counts as online but isn't a world to list
				nlohmann::json entry{ {"zone_id", zone.zoneId}, {"zone", zone.zoneName}, {"players", zone.players}, {"instances", zone.instances} };
				if (ConfigFlag("public_status_names", false)) {
					const auto it = names.find(zone.zoneId);
					entry["names"] = it == names.end() ? std::vector<std::string>{} : it->second;
				}
				zones.push_back(std::move(entry));
			}
			status["players"] = { {"online", total}, {"worlds", zones} };
		}

		if (ConfigFlag("public_status_uptime", true)) {
			const auto uptime = PublicStatus::Summarize(Database::Get()->GetHealthSamples(now - UPTIME_HISTORY, now, UPTIME_BUCKET), now, UPTIME_BUCKET);
			status["uptime"] = {
				{"up_since", uptime.upSince ? nlohmann::json(uptime.upSince) : nlohmann::json(nullptr)},
				{"seconds", uptime.upSince ? now - uptime.upSince : 0},
				{"text", uptime.upSince ? PublicStatus::DurationText(now - uptime.upSince) : ""},
				{"last_day_percent", Percent(uptime.availability24h)},
				{"last_week_percent", Percent(uptime.availability7d)}
			};
		}

		if (ConfigFlag("public_status_health", true)) {
			status["health"] = { {"login", loginUp}, {"chat", chatUp}, {"worlds", worlds.size()} };
		}

		if (const auto top = Setting("public_status_leaderboard_top", 3, 0, 10); top > 0) {
			status["leaderboards"] = LeaderboardTops(static_cast<size_t>(top));
		}
		if (ConfigFlag("public_status_challenges", true)) status["challenges"] = ChallengeRoutes::PublicJson();
		if (ConfigFlag("public_status_events", true)) status["events"] = LiveEventRoutes::PublicJson();
		return status;
	}

	const Snapshot& Current() {
		const auto now = Clock::now();
		if (!g_Snapshot || now - g_Snapshot->made >= std::chrono::seconds(CacheSeconds())) {
			auto status = BuildStatus();
			auto body = status.dump();
			g_Snapshot = Snapshot{ std::move(status), std::move(body), now };
		}
		return *g_Snapshot;
	}

	// Lets shared caches and browsers keep the reply until the next snapshot is due
	void CacheHeaders(HTTPReply& reply, const Snapshot& snapshot) {
		const auto age = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - snapshot.made).count();
		reply.headers.push_back("Cache-Control: public, max-age=" + std::to_string(std::max<int64_t>(0, CacheSeconds() - age)));
	}

	// Off (not found, so it doesn't say the page exists) or too many requests: writes the reply and returns false
	bool Admit(const HTTPContext& context, HTTPReply& reply, bool api, bool enabled) {
		if (!enabled) {
			if (api) JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Not Found");
			else RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "This page is not available on this server.");
			return false;
		}
		if (!g_Limiter.Allow(ClientAddress(context))) {
			if (api) JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many requests, try again in a minute");
			else RenderError(reply, context, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many requests, try again in a minute.");
			return false;
		}
		return true;
	}
}

nlohmann::json PublicPageJson() {
	return { {"name", ServerName()}, {"status", Enabled()}, {"showcase", ConfigFlag("showcase_public", false)} };
}

void RegisterPublicRoutes() {
	Route(eHTTPMethod::GET, "/api/public/status", PUBLIC,
		"Public server status for server lists (public_status=1): name, online, players per world, uptime, health and top leaderboard places, "
		"each as the settings allow. Refreshed at most every public_status_cache_seconds",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Admit(context, reply, true, Enabled())) return;
			const auto& snapshot = Current();
			reply.status = eHTTPStatusCode::OK;
			reply.contentType = eContentType::APPLICATION_JSON;
			reply.message = snapshot.body;
			CacheHeaders(reply, snapshot);
			// Read-only and the same for everyone (no cookies are used), so any site may fetch it
			reply.headers.push_back("Access-Control-Allow-Origin: *");
		});

	Route(eHTTPMethod::GET, "/status", PUBLIC, "Public server status page (public_status=1)",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Admit(context, reply, false, Enabled())) return;
			const auto& snapshot = Current();
			RenderPage(reply, context, "status.jinja2", "status", { {"public_page", PublicPageJson()}, {"status_json", snapshot.body} });
		});

	Route(eHTTPMethod::GET, "/status/widget", PUBLIC,
		"A small status box other sites may show in an iframe (public_status=1 and public_status_widget=1). Query: ?theme=light|dark",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Admit(context, reply, false, Enabled() && ConfigFlag("public_status_widget", true))) return;
			const auto& snapshot = Current();
			const auto& status = snapshot.status;
			const bool players = status.contains("players");
			const bool uptime = status.contains("uptime");
			RenderPage(reply, context, "status_widget.jinja2", "status_widget", {
				{"name", status.value("name", "")},
				{"online", status.value("online", false)},
				{"show_players", players},
				{"players", players ? status["players"].value("online", 0u) : 0u},
				{"show_uptime", uptime},
				{"uptime_text", uptime ? status["uptime"].value("text", "") : ""},
				{"refresh", CacheSeconds()},
				{"light", QueryValue(context.queryString, "theme") == "light"}
			});
			CacheHeaders(reply, snapshot);
			// Framable by any site (browsers that know frame-ancestors ignore the default X-Frame-Options: DENY);
			// the widget has no scripts, forms or actions, so there is nothing to click-jack
			reply.headers.push_back("Content-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'; frame-ancestors *");
		});
}
