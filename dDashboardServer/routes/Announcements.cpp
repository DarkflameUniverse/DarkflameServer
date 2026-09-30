#include "Announcements.h"
#include "GameText.h"
#include "AnnouncementSchedule.h"
#include "LiveWorld.h"
#include "DashboardRoutes.h"
#include "master/DashboardMessages.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <map>

#include "RouteUtils.h"
#include "WSRoutes.h"
#include "Database.h"
#include "Logger.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	using Row = IServerOperations::ScheduledAnnouncement;
	constexpr auto CHECK_INTERVAL = std::chrono::seconds(1);

	struct State {
		Row row;
		std::optional<int64_t> nextAt;
	};

	std::map<uint64_t, State> g_Announcements;
	bool g_Loaded = false;
	std::chrono::steady_clock::time_point g_NextCheck{};

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	// From now on: a send missed while the dashboard was down isn't made up
	void Reschedule(State& state, int64_t after) {
		state.nextAt.reset();
		if (!state.row.enabled) return;
		const auto schedule = Cron::Parse(state.row.schedule);
		if (!schedule) {
			LOG("Scheduled announcement %llu has an invalid schedule '%s'", static_cast<unsigned long long>(state.row.id), state.row.schedule.c_str());
			return;
		}
		state.nextAt = AnnouncementSchedule::Next(*schedule, state.row.startsAt, state.row.endsAt, after);
	}

	void Load() {
		g_Loaded = true;
		g_Announcements.clear();
		try {
			for (auto& row : Database::Get()->GetScheduledAnnouncements()) {
				State state{ std::move(row) };
				Reschedule(state, Now());
				g_Announcements[state.row.id] = std::move(state);
			}
		} catch (const std::exception& ex) {
			LOG("Could not load scheduled announcements: %s", ex.what());
		}
	}

	std::string ZoneList(const std::vector<uint32_t>& zones) {
		if (zones.empty()) return "every world";
		std::string text;
		const auto& names = GameText::ZoneNames();
		for (const auto zone : zones) {
			const auto key = std::to_string(zone);
			text += (text.empty() ? "" : ", ") + (names.contains(key) ? names[key].get<std::string>() : "zone " + key);
		}
		return text;
	}

	nlohmann::json RowJson(const State& state) {
		const auto& row = state.row;
		return { {"id", row.id}, {"title", row.title}, {"message", row.message}, {"zones", row.zones}, {"zoneNames", ZoneList(row.zones)},
			{"schedule", row.schedule}, {"startsAt", row.startsAt}, {"endsAt", row.endsAt}, {"enabled", row.enabled},
			{"lastSentAt", row.lastSentAt}, {"sentCount", row.sentCount}, {"nextAt", state.nextAt ? nlohmann::json(*state.nextAt) : nlohmann::json(nullptr)},
			{"createdAt", row.createdAt}, {"createdBy", row.createdBy}, {"updatedAt", row.updatedAt}, {"updatedBy", row.updatedBy} };
	}

	// Every zone a message can be limited to, for the picker
	nlohmann::json ZoneOptions() {
		std::vector<std::pair<uint32_t, std::string>> zones;
		for (const auto& [id, name] : GameText::ZoneNames().items()) {
			if (const auto zone = GeneralUtils::TryParse<uint32_t>(id); zone && *zone > 0) zones.emplace_back(*zone, name.get<std::string>());
		}
		std::sort(zones.begin(), zones.end());
		nlohmann::json options = nlohmann::json::array();
		for (const auto& [id, name] : zones) options.push_back({ {"id", id}, {"name", name} });
		return options;
	}

	// Read an announcement from a request body into row; error says what is wrong
	bool ReadBody(const nlohmann::json& body, Row& row, std::string& error) {
		row.title = body.value("title", "");
		row.message = body.value("message", "");
		row.schedule = body.value("schedule", "");
		row.schedule.erase(0, row.schedule.find_first_not_of(" \t"));
		row.schedule.erase(row.schedule.find_last_not_of(" \t") + 1);
		row.startsAt = body.value("startsAt", int64_t{ 0 });
		row.endsAt = body.value("endsAt", int64_t{ 0 });
		row.enabled = body.value("enabled", true);
		row.zones.clear();
		if (body.contains("zones") && body["zones"].is_array()) {
			const auto& known = GameText::ZoneNames();
			for (const auto& zone : body["zones"]) {
				if (!zone.is_number_unsigned() || !known.contains(std::to_string(zone.get<uint32_t>()))) { error = "Pick zones from the list"; return false; }
				if (std::find(row.zones.begin(), row.zones.end(), zone.get<uint32_t>()) == row.zones.end()) row.zones.push_back(zone.get<uint32_t>());
			}
		}
		if (const auto problem = AnnouncementSchedule::Validate(row.title, row.message, row.schedule, row.startsAt, row.endsAt, row.zones, Now())) {
			error = *problem;
			return false;
		}
		return true;
	}

	std::string Describe(const Row& row) {
		return "#" + std::to_string(row.id) + " \"" + (row.title.empty() ? row.message.substr(0, 60) : row.title) + "\", " + row.schedule + " to " + ZoneList(row.zones) +
			(row.enabled ? "" : " (off)");
	}

	State* FindFromPath(const HTTPContext& context, HTTPReply& reply) {
		if (!g_Loaded) Load();
		const auto id = PathId<uint64_t>(context.path, 2);
		const auto it = id ? g_Announcements.find(*id) : g_Announcements.end();
		if (it == g_Announcements.end()) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Announcement not found");
			return nullptr;
		}
		return &it->second;
	}
}

namespace Announcements {
	void Update() {
		const auto steady = std::chrono::steady_clock::now();
		if (steady < g_NextCheck) return;
		g_NextCheck = steady + CHECK_INTERVAL;
		if (!g_Loaded) Load();
		const auto now = Now();
		for (auto& [id, state] : g_Announcements) {
			if (!state.nextAt || *state.nextAt > now) continue;
			if (LiveWorld::Announce(state.row.title, state.row.message, state.row.zones)) {
				state.row.lastSentAt = now;
				state.row.sentCount++;
				try {
					Database::Get()->MarkAnnouncementSent(id, now);
				} catch (const std::exception& ex) {
					LOG("Could not record sending announcement %llu: %s", static_cast<unsigned long long>(id), ex.what());
				}
				BroadcastTableChanged("scheduled_announcements", std::to_string(id));
			} else {
				LOG("Skipped scheduled announcement %llu: not connected to the master server", static_cast<unsigned long long>(id));
			}
			Reschedule(state, now);
		}
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/announcements", Perm("announcements_schedule"), "Scheduled announcements page",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "announcements.jinja2", "announcements"); });

		Route(eHTTPMethod::GET, "/api/announcements", Perm("announcements_schedule"),
			"Scheduled announcements with their next send, and the zones they can be limited to",
			[](HTTPReply& reply, const HTTPContext&) {
				if (!g_Loaded) Load();
				nlohmann::json rows = nlohmann::json::array();
				for (const auto& [id, state] : g_Announcements) rows.push_back(RowJson(state));
				JsonSuccess(reply, { {"announcements", rows}, {"zones", ZoneOptions()}, {"now", Now()},
					{"limits", { {"title", Announcement::MAX_TITLE}, {"message", Announcement::MAX_MESSAGE}, {"minInterval", AnnouncementSchedule::MIN_INTERVAL_SECONDS} }} });
			});

		Route(eHTTPMethod::POST, "/api/announcements/preview", Perm("announcements_schedule"),
			"Check a schedule and list its next sends. Body: {schedule, startsAt, endsAt}. Returns {valid, error, next: [unix times]}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				Row row;
				std::string error;
				auto check = *body;
				if (!check.contains("message")) check["message"] = "-";
				if (!ReadBody(check, row, error)) return JsonSuccess(reply, { {"valid", false}, {"error", error} });
				const auto schedule = Cron::Parse(row.schedule);
				nlohmann::json next = nlohmann::json::array();
				auto at = Now();
				for (int i = 0; i < 5; i++) {
					const auto time = AnnouncementSchedule::Next(*schedule, row.startsAt, row.endsAt, at);
					if (!time) break;
					next.push_back(*time);
					at = *time;
				}
				JsonSuccess(reply, { {"valid", true}, {"next", next} });
			});

		Route(eHTTPMethod::POST, "/api/announcements", Perm("announcements_schedule"),
			"Schedule an announcement. Body: {title, message, zones: [zone IDs] (empty: every world), schedule (cron or @every, UTC), startsAt, endsAt (unix; 0: none), enabled}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				if (!g_Loaded) Load();
				Row row;
				std::string error;
				if (!ReadBody(*body, row, error)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
				row.createdAt = row.updatedAt = Now();
				row.createdBy = row.updatedBy = context.authenticatedUser;
				row.id = Database::Get()->InsertScheduledAnnouncement(row);
				State state{ row };
				Reschedule(state, Now());
				g_Announcements[row.id] = std::move(state);
				Audit(context, "schedule_announcement", Describe(row));
				BroadcastTableChanged("scheduled_announcements", std::to_string(row.id));
				JsonSuccess(reply, { {"message", "Announcement scheduled"}, {"id", row.id} });
			});

		Route(eHTTPMethod::POST, "/api/announcements/:id", Perm("announcements_schedule"), "Change a scheduled announcement. Body as when scheduling one",
			[](HTTPReply& reply, const HTTPContext& context) {
				auto* state = FindFromPath(context, reply);
				if (!state) return;
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				Row row = state->row;
				std::string error;
				if (!ReadBody(*body, row, error)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
				row.updatedAt = Now();
				row.updatedBy = context.authenticatedUser;
				Database::Get()->UpdateScheduledAnnouncement(row);
				state->row = row;
				Reschedule(*state, Now());
				Audit(context, "update_announcement", Describe(row));
				BroadcastTableChanged("scheduled_announcements", std::to_string(row.id));
				JsonSuccess(reply, { {"message", "Saved"} });
			});

		Route(eHTTPMethod::POST, "/api/announcements/:id/delete", Perm("announcements_schedule"), "Delete a scheduled announcement",
			[](HTTPReply& reply, const HTTPContext& context) {
				auto* state = FindFromPath(context, reply);
				if (!state) return;
				const auto row = state->row;
				Database::Get()->DeleteScheduledAnnouncement(row.id);
				g_Announcements.erase(row.id);
				Audit(context, "delete_announcement", Describe(row));
				BroadcastTableChanged("scheduled_announcements", std::to_string(row.id));
				JsonSuccess(reply, { {"message", "Deleted"} });
			});

		Route(eHTTPMethod::POST, "/api/announcements/:id/send", Perm("announcements_schedule"), "Send a scheduled announcement now (its schedule carries on as before)",
			[](HTTPReply& reply, const HTTPContext& context) {
				auto* state = FindFromPath(context, reply);
				if (!state) return;
				if (!LiveWorld::Announce(state->row.title, state->row.message, state->row.zones)) {
					return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Not connected to the master server");
				}
				const auto now = Now();
				state->row.lastSentAt = now;
				state->row.sentCount++;
				Database::Get()->MarkAnnouncementSent(state->row.id, now);
				Audit(context, "announce", (state->row.title.empty() ? "" : state->row.title + ": ") + state->row.message + " (scheduled #" +
					std::to_string(state->row.id) + ", sent by hand to " + ZoneList(state->row.zones) + ")");
				BroadcastTableChanged("scheduled_announcements", std::to_string(state->row.id));
				JsonSuccess(reply, { {"message", "Sent"} });
			});
	}
}
