#include "EventsCalendar.h"
#include "GameText.h"
#include "EventSchedule.h"
#include "AnnouncementSchedule.h"
#include "LevelGating.h"
#include "ZonePaths.h"
#include "ClientAssets.h"
#include "DashboardRoutes.h"
#include "SettingsRoutes.h"
#include "LiveEventRoutes.h"
#include "LiveWorld.h"
#include "LiveOpsRules.h"
#include "PlayerActions.h"
#include "master/PlayerAction.h"
#include "ServerState.h"
#include "GameLabels.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <tuple>

#include "RouteUtils.h"
#include "WSRoutes.h"
#include "Alerts.h"
#include "Background.h"
#include "BinaryPathFinder.h"
#include "CDClientDatabase.h"
#include "ClientVersion.h"
#include "Database.h"
#include "EventParts.h"
#include "Game.h"
#include "Logger.h"
#include "ScheduleRules.h"
#include "VanityEvents.h"
#include "dConfig.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	using eEventState = IServerOperations::eEventState;
	using Event = IServerOperations::ScheduledEvent;
	using EventParts::Part;
	using EventParts::eKind;
	using EventParts::eStep;
	using ScheduleRules::eMode;
	constexpr auto CHECK_INTERVAL = std::chrono::seconds(5);
	constexpr size_t MAX_NAME_LENGTH = 100;
	constexpr size_t MAX_NOTE_LENGTH = 500;
	constexpr size_t MAX_PARTS = 20;
	constexpr size_t MAX_REMOVALS = 200;
	constexpr size_t MAX_MESSAGE_LENGTH = 1000;
	constexpr int32_t MAX_PRIORITY = 1000;
	constexpr int64_t MAX_RESTART_MINUTES = 1440;
	// The longest stretch the calendar asks about at once
	constexpr int64_t MAX_RANGE_SECONDS = 400 * 86400LL;
	constexpr const char* VANITY_ROOT = "root.xml";

	std::chrono::steady_clock::time_point g_NextCheck{};

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	std::filesystem::path VanityFolder() { return BinaryPathFinder::GetBinaryDir() / "vanity"; }

	struct Feature {
		std::string description;
		std::optional<std::tuple<int32_t, int32_t, int32_t>> version; // FeatureGating: unlocked from this client version
		std::map<uint32_t, uint32_t> zones;                            // zone -> objects gated on it
	};

	// FeatureGating, read once
	std::map<std::string, Feature> g_Features;
	enum class eScan { NOT_STARTED, RUNNING, DONE };
	eScan g_Scan = eScan::NOT_STARTED;

	/**
	 * Find which zones have objects gated on which features: the scene files are read here (the client path is a
	 * setting) and parsed on a worker thread, since that takes seconds. Their zones join g_Features when it finishes.
	 */
	void StartFeatureScan() {
		g_Scan = eScan::RUNNING;
		try {
			auto result = CDClientDatabase::ExecuteQuery("SELECT featureName, major, current, minor, description FROM FeatureGating;");
			for (; !result.eof(); result.nextRow()) {
				const std::string name = result.getStringField(0, "");
				if (name.empty()) continue;
				auto& feature = g_Features[name];
				feature.version = std::tuple{ result.getIntField(1, -1), result.getIntField(2, -1), result.getIntField(3, -1) };
				feature.description = result.getStringField(4, "");
			}
		} catch (const std::exception& ex) {
			LOG("Could not read FeatureGating: %s", ex.what());
		}

		// Several zone IDs can share a .luz
		std::map<std::string, std::vector<uint32_t>> zonesByFile;
		try {
			auto result = CDClientDatabase::ExecuteQuery("SELECT zoneID, zoneName FROM ZoneTable;");
			for (; !result.eof(); result.nextRow()) {
				const std::string luz = result.getStringField(1, "");
				if (!luz.empty()) zonesByFile[luz].push_back(static_cast<uint32_t>(result.getIntField(0, 0)));
			}
		} catch (const std::exception& ex) {
			LOG("Could not read ZoneTable: %s", ex.what());
		}
		auto scenes = std::make_shared<std::vector<std::pair<std::vector<uint32_t>, std::string>>>(); // (zones, .lvl bytes)
		for (const auto& [luzPath, zoneIds] : zonesByFile) {
			const auto luz = ClientAssets::ReadResFile("maps/" + luzPath);
			if (!luz) continue;
			const auto folder = luzPath.substr(0, luzPath.find_last_of('/') + 1);
			for (const auto& scene : ZonePaths::ReadSceneFiles(*luz)) {
				if (auto lvl = ClientAssets::ReadResFile("maps/" + folder + scene)) scenes->emplace_back(zoneIds, std::move(*lvl));
			}
		}
		Background::Run("events_feature_scan", [scenes](GameDatabase&) -> nlohmann::json {
			// feature -> zone -> objects
			std::map<std::string, std::map<uint32_t, uint32_t>> found;
			for (const auto& [zoneIds, lvl] : *scenes) {
				for (const auto& [name, objects] : LevelGating::ReadGatedFeatures(lvl)) {
					for (const auto zone : zoneIds) found[name][zone] += objects;
				}
			}
			nlohmann::json result = nlohmann::json::object();
			for (const auto& [name, zones] : found) {
				auto& list = result[name] = nlohmann::json::array();
				for (const auto& [zone, objects] : zones) list.push_back({ zone, objects });
			}
			return result;
		}, [count = scenes->size()](nlohmann::json result, const std::string& error) {
			g_Scan = eScan::DONE;
			if (!error.empty()) {
				LOG("Events calendar: reading the scene files failed: %s", error.c_str());
				return;
			}
			for (const auto& [name, zones] : result.items()) {
				for (const auto& zone : zones) g_Features[name].zones[zone[0].get<uint32_t>()] = zone[1].get<uint32_t>();
			}
			LOG("Events calendar: %zu features, from %zu scene files", g_Features.size(), count);
			BroadcastTableChanged("scheduled_events");
		});
	}

	const std::map<std::string, Feature>& Features() {
		if (g_Scan == eScan::NOT_STARTED) StartFeatureScan();
		return g_Features;
	}

	std::tuple<int32_t, int32_t, int32_t> ClientVersionSetting() {
		return { GeneralUtils::TryParse<int32_t>(Game::config->GetValue("version_major")).value_or(ClientVersion::major),
			GeneralUtils::TryParse<int32_t>(Game::config->GetValue("version_current")).value_or(ClientVersion::current),
			GeneralUtils::TryParse<int32_t>(Game::config->GetValue("version_minor")).value_or(ClientVersion::minor) };
	}


	std::string ZoneList(const std::vector<uint32_t>& zones) {
		if (zones.empty()) return "every world";
		std::string text;
		for (const auto zone : zones) text += (text.empty() ? "" : ", ") + GameText::ZoneName(zone);
		return text;
	}

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

	// Running public worlds of the zones with objects gated on a feature: these keep what they loaded until restarted
	nlohmann::json RunningWorlds(const std::map<uint32_t, uint32_t>& zones) {
		nlohmann::json worlds = nlohmann::json::array();
		std::lock_guard lock(ServerState::g_StatusMutex);
		for (const auto& world : ServerState::g_WorldInstances) {
			if (!zones.contains(world.mapID)) continue;
			worlds.push_back({ {"zone", world.mapID}, {"zoneName", GameText::ZoneName(world.mapID)}, {"instance", world.instanceID}, {"clone", world.cloneID}, {"players", world.players} });
		}
		return worlds;
	}

	nlohmann::json FeatureJson(const std::string& name, const Feature& feature) {
		nlohmann::json zones = nlohmann::json::array();
		for (const auto& [zone, objects] : feature.zones) zones.push_back({ {"id", zone}, {"name", GameText::ZoneName(zone)}, {"objects", objects} });
		nlohmann::json json{ {"name", name}, {"description", feature.description}, {"zones", zones}, {"version", nullptr}, {"unlocked", false} };
		if (feature.version) {
			const auto& [major, current, minor] = *feature.version;
			json["version"] = std::to_string(major) + "." + std::to_string(current) + "." + std::to_string(minor);
			// As CDFeatureGatingTable::FeatureUnlocked: already on for this client version, so an event changes nothing in the levels
			json["unlocked"] = ClientVersionSetting() >= *feature.version;
		}
		return json;
	}

	std::map<uint32_t, uint32_t> ZonesOf(const std::string& feature) {
		const auto& features = Features();
		const auto it = features.find(feature);
		return it == features.end() ? std::map<uint32_t, uint32_t>{} : it->second.zones;
	}

	std::string RestartAdvice(const std::string& feature) {
		const auto zones = ZonesOf(feature);
		if (zones.empty()) return "No zone has objects gated on it; players see client-side changes the next time they log in.";
		std::string names;
		for (const auto& [zone, objects] : zones) names += (names.empty() ? "" : ", ") + GameText::ZoneName(zone);
		return "Worlds of " + names + " that are already running keep what they loaded until they are restarted; new logins get the change straight away.";
	}

	// Features staff may pick: the known ones, plus whatever an event_N setting already holds
	bool KnownFeature(const std::string& feature) {
		if (Features().contains(feature)) return true;
		return EventSchedule::SlotHolding(Database::Get()->GetServerConfig({}), feature) != 0;
	}

	std::string Text(const nlohmann::json& json, const char* key) {
		return json.contains(key) && json[key].is_string() ? json[key].get<std::string>() : "";
	}

	// ---------------------------------------------------------------- permissions

	// What a part needs: the permission of the page that does the same thing by hand
	const char* PermissionFor(eKind kind) {
		switch (kind) {
		case eKind::FEATURE: return "events_manage";
		case eKind::VANITY: return "vanity_manage";
		case eKind::LIVE_EVENT: return "live_events_manage";
		case eKind::ANNOUNCEMENT: return "announcements_schedule";
		case eKind::RESTART: return "server_restart";
		}
		return "events_manage";
	}

	// Anyone who may add some kind of part may see the events
	bool CanView(const HTTPContext& context) {
		return std::ranges::any_of(magic_enum::enum_values<eKind>(), [&](eKind kind) { return Can(context, PermissionFor(kind)); });
	}

	// The first kind of part in these lists the user may not add, change or remove, if any
	std::optional<eKind> Forbidden(const HTTPContext& context, const std::vector<Part>& a, const std::vector<Part>& b = {}) {
		for (const auto* list : { &a, &b }) {
			for (const auto& part : *list) if (!Can(context, PermissionFor(part.kind))) return part.kind;
		}
		return std::nullopt;
	}

	std::string ForbiddenMessage(eKind kind) {
		return "You may not change events with a " + EventParts::KindName(kind) + " part (needs " + PermissionFor(kind) + ")";
	}

	// ---------------------------------------------------------------- events

	eMode ModeOf(const Event& event) { return static_cast<eMode>(event.mode); }

	bool Once(const Event& event) { return event.schedule.empty(); }

	bool IsOn(const Event& event, int64_t now) { return ScheduleRules::IsOn(ModeOf(event), event.schedule, event.startsAt, event.endsAt, now); }

	std::vector<Part> PartsOf(const Event& event) {
		std::string error;
		auto parts = EventParts::Parse(event.parts, error);
		if (!parts) LOG("Scheduled event %llu: %s", static_cast<unsigned long long>(event.id), error.c_str());
		return parts.value_or(std::vector<Part>{});
	}

	// When the stretch it is on in now ends (nullopt: not known, or not in the next years)
	std::optional<int64_t> CurrentEnd(const Event& event, int64_t now) {
		if (ModeOf(event) != eMode::SCHEDULED) return std::nullopt;
		if (Once(event)) return event.endsAt;
		std::string error;
		const auto schedule = ScheduleRules::ParseSchedule(event.schedule, error);
		return schedule ? ScheduleRules::NextChange(*schedule, now) : std::nullopt;
	}

	// The next start and end of its schedule after now (for the list)
	std::pair<std::optional<int64_t>, std::optional<int64_t>> NextTimes(const Event& event, int64_t now) {
		if (Once(event)) {
			if (now < event.startsAt) return { event.startsAt, event.endsAt };
			if (now < event.endsAt) return { std::nullopt, event.endsAt };
			return {};
		}
		std::string error;
		const auto schedule = ScheduleRules::ParseSchedule(event.schedule, error);
		if (!schedule) return {};
		const bool on = ScheduleRules::Active(*schedule, now);
		const auto first = ScheduleRules::NextChange(*schedule, now);
		const auto second = first ? ScheduleRules::NextChange(*schedule, *first) : std::nullopt;
		return on ? std::pair{ second, first } : std::pair{ first, second };
	}

	std::string Describe(const Event& event) {
		std::string parts;
		for (const auto& part : PartsOf(event)) parts += (parts.empty() ? "" : ", ") + EventParts::KindName(part.kind);
		return "#" + std::to_string(event.id) + " " + event.name + " (" + GameLabels::Name(ModeOf(event)) + (parts.empty() ? "" : "; " + parts) + ")";
	}

	void Save(Event& event, const std::string& by) {
		event.updatedAt = Now();
		event.updatedBy = by;
		Database::Get()->UpdateScheduledEvent(event);
		BroadcastTableChanged("scheduled_events", std::to_string(event.id));
	}

	std::optional<Event> FindEvent(uint64_t id) {
		for (auto& event : Database::Get()->GetScheduledEvents()) {
			if (event.id == id) return std::move(event);
		}
		return std::nullopt;
	}

	// ---------------------------------------------------------------- parts: starting and ending

	struct Run {
		const HTTPContext& actor;
		Event& event;
		int64_t now;
		bool vanityChanged{}; // a vanity part started or ended: the worlds respawn their vanity NPCs
	};

	// Put the feature in a free slot. false: still waiting (the reason is in status)
	bool StartFeature(Part& part, Run& run) {
		const auto feature = Text(part.config, "feature");
		const auto rows = Database::Get()->GetServerConfig({});
		// Already in a slot (a setting, or another event): on without taking another one
		if (const auto holding = EventSchedule::SlotHolding(rows, feature)) {
			part.state = { {"shared", true}, {"slot", holding} };
			part.status = "Already on in " + EventSchedule::SlotSetting(holding);
			return true;
		}
		const auto slot = EventSchedule::FreeSlot(EventSchedule::BusySlots(rows));
		if (!slot) {
			const std::string status = "Waiting: event_1 to event_8 all have a value. Clear one on the Settings page or end another event.";
			if (part.status != status) {
				part.status = status;
				Alerts::Emit("server", "Event waiting for a slot", run.event.name + " should have switched " + feature + " on, but every event setting is in use.", {}, "/events");
			}
			return false;
		}
		const auto* previous = EventSchedule::SlotRow(rows, *slot);
		const nlohmann::json change{ {"file", EventSchedule::FILE}, {"name", EventSchedule::SlotSetting(*slot)}, {"value", feature}, {"webWins", true} };
		if (const auto error = SaveSetting(run.actor, change)) {
			part.status = "Couldn't set " + EventSchedule::SlotSetting(*slot) + ": " + *error;
			return false;
		}
		part.state = { {"slot", *slot}, {"previousWebWins", previous && previous->webWins},
			{"previousValue", previous && previous->webValue ? nlohmann::json(*previous->webValue) : nlohmann::json(nullptr)} };
		part.status = "On in " + EventSchedule::SlotSetting(*slot);
		Audit(run.actor, "start_event", run.event.name + ": " + feature + " in " + EventSchedule::SlotSetting(*slot));
		Alerts::Emit("server", "Event started", run.event.name + ": " + feature + " is on (" + EventSchedule::SlotSetting(*slot) + "). " + RestartAdvice(feature), {}, "/events");
		return true;
	}

	// Put the slot back as it was, unless someone changed it by hand in the meantime. false: try again
	bool EndFeature(Part& part, Run& run) {
		const auto feature = Text(part.config, "feature");
		const auto slot = static_cast<uint8_t>(part.state.value("slot", 0));
		if (part.state.value("shared", false) || slot == 0) {
			part.status = "Off";
			return true;
		}
		const auto name = EventSchedule::SlotSetting(slot);
		const auto rows = Database::Get()->GetServerConfig({ EventSchedule::FILE });
		const auto* row = EventSchedule::SlotRow(rows, slot);
		std::string status = "Off";
		if (row && row->webValue == feature) {
			const auto& previous = part.state.contains("previousValue") ? part.state["previousValue"] : nlohmann::json(nullptr);
			const nlohmann::json change{ {"file", EventSchedule::FILE}, {"name", name}, {"value", previous.is_string() ? previous : nlohmann::json(nullptr)},
				{"webWins", part.state.value("previousWebWins", false)} };
			if (const auto error = SaveSetting(run.actor, change)) {
				part.status = "Couldn't put " + name + " back: " + *error;
				return false;
			}
		} else {
			status += "; " + name + " had been changed by hand, so it was left as it was";
		}
		part.status = status;
		Audit(run.actor, "end_event", run.event.name + ": " + feature + " (" + name + ")");
		Alerts::Emit("server", "Event ended", run.event.name + ": " + feature + " is off. " + RestartAdvice(feature), {}, "/events");
		return true;
	}

	bool StartLiveEvent(Part& part, Run& run) {
		const auto end = CurrentEnd(run.event, run.now).value_or(run.now + LiveOpsRules::MAX_DURATION);
		std::string error, message;
		const auto id = LiveEventRoutes::StartPart(part.config, end, run.actor, error, message);
		if (!id) {
			part.status = "Couldn't start: " + error;
			return false;
		}
		part.state = { {"id", *id} };
		part.status = message;
		return true;
	}

	bool EndLiveEvent(Part& part, Run& run) {
		const auto id = part.state.value("id", uint64_t{ 0 });
		part.status = id && LiveEventRoutes::EndPart(id, run.actor, run.event.name + " ended") ? "Ended with the event" : "Had already ended";
		return true;
	}

	void Announce(const nlohmann::json& config, const std::string& message) {
		std::vector<uint32_t> zones;
		if (config.contains("zones") && config["zones"].is_array()) for (const auto& zone : config["zones"]) if (zone.is_number_unsigned()) zones.push_back(zone.get<uint32_t>());
		LiveWorld::Announce(Text(config, "title"), message, zones);
	}

	// The next repeat of an announcement part after `after`, while the event is on
	std::optional<int64_t> NextRepeat(const Part& part, const Run& run, int64_t after) {
		const auto repeat = Text(part.config, "repeat");
		if (repeat.empty()) return std::nullopt;
		const auto schedule = Cron::Parse(repeat);
		if (!schedule) return std::nullopt;
		return AnnouncementSchedule::Next(*schedule, 0, CurrentEnd(run.event, run.now).value_or(0), after);
	}

	bool StartAnnouncement(Part& part, Run& run) {
		const bool atStart = part.config.value("atStart", true);
		if (atStart) Announce(part.config, Text(part.config, "message"));
		const auto next = NextRepeat(part, run, run.now);
		part.state = next ? nlohmann::json{ {"nextAt", *next} } : nlohmann::json::object();
		part.status = atStart ? "Said when it started" : "On";
		return true;
	}

	void ContinueAnnouncement(Part& part, Run& run) {
		const auto at = part.state.value("nextAt", int64_t{ 0 });
		// A repeat added while the event was on: the first one from now
		if (at == 0 && !Text(part.config, "repeat").empty() && !part.state.contains("lastAt")) {
			if (const auto next = NextRepeat(part, run, run.now)) part.state["nextAt"] = *next;
			return;
		}
		if (at == 0 || run.now < at) return;
		Announce(part.config, Text(part.config, "message"));
		const auto next = NextRepeat(part, run, run.now);
		const auto sent = part.state.value("sent", 0) + 1;
		part.state = { {"sent", sent}, {"lastAt", run.now} };
		if (next) part.state["nextAt"] = *next;
		part.status = "Repeated " + std::to_string(sent) + (sent == 1 ? " time" : " times");
	}

	bool EndAnnouncement(Part& part, Run&) {
		const auto message = Text(part.config, "endMessage");
		if (!message.empty()) Announce(part.config, message);
		part.state = nlohmann::json::object();
		part.status = message.empty() ? "Off" : "Said when it ended";
		return true;
	}

	// A restart when the event starts or ends; one already scheduled is left alone (the server restarts anyway)
	bool Restart(Part& part, Run& run, const std::string& when) {
		if (Text(part.config, "when") != when) {
			if (when == "start") part.status = "Restarts when the event ends";
			return true;
		}
		const auto error = LiveWorld::ScheduleRestart(part.config.value("minutes", int64_t{ 15 }) * 60, Text(part.config, "reason"), run.actor, false);
		part.status = error ? "Not scheduled: " + *error : "Restart scheduled when it " + std::string(when == "start" ? "started" : "ended");
		return true;
	}

	// Start or end one part; false when it has to be tried again
	bool Step(Part& part, eStep step, Run& run) {
		switch (part.kind) {
		case eKind::FEATURE:
			return step == eStep::START ? StartFeature(part, run) : EndFeature(part, run);
		case eKind::VANITY:
			run.vanityChanged = true;
			part.status = step == eStep::START ? "On" : "Off";
			return true;
		case eKind::LIVE_EVENT:
			return step == eStep::START ? StartLiveEvent(part, run) : EndLiveEvent(part, run);
		case eKind::ANNOUNCEMENT:
			return step == eStep::START ? StartAnnouncement(part, run) : EndAnnouncement(part, run);
		case eKind::RESTART:
			return Restart(part, run, step == eStep::START ? "start" : "end");
		}
		return true;
	}

	// A part taken out of an event (or changed) while it was on: undo what it switched on, without what it does when
	// its event ends (no end announcement, no restart)
	void Retire(Part& part, Run& run) {
		switch (part.kind) {
		case eKind::FEATURE: EndFeature(part, run); break;
		case eKind::VANITY: run.vanityChanged = true; break;
		case eKind::LIVE_EVENT: EndLiveEvent(part, run); break;
		case eKind::ANNOUNCEMENT:
		case eKind::RESTART: break;
		}
	}

	void RespawnVanity(const HTTPContext& actor) {
		PlayerActionRequest request;
		request.action = ePlayerAction::RELOAD_VANITY;
		PlayerActions::Request(request, actor.accountId, [](const PlayerActionResult& result) {
			return PlayerActions::Outcome{ true, result.affected ? "Vanity NPCs respawned in " + std::to_string(result.affected) + " world(s)" : "No world with vanity NPCs is running" };
		});
	}

	/**
	 * Bring one event's parts in line with whether it is on: start what should be on, end what shouldn't, carry on
	 * repeating announcements. `retired`: parts taken out of the event that are still on, ended here. Saves the event
	 * if anything changed; true when a vanity part started or ended.
	 */
	bool SyncEvent(Event& event, const HTTPContext& actor, int64_t now, std::vector<Part> retired = {}) {
		auto parts = PartsOf(event);
		const auto before = EventParts::Write(parts);
		const auto stateBefore = event.state;
		Run run{ actor, event, now };
		for (auto& part : retired) Retire(part, run);
		const bool on = IsOn(event, now);
		for (auto& part : parts) {
			const auto step = EventParts::StepFor(part, on);
			if (step == eStep::CONTINUE && part.kind == eKind::ANNOUNCEMENT) ContinueAnnouncement(part, run);
			if (step != eStep::START && step != eStep::END) continue;
			try {
				if (Step(part, step, run)) part.applied = step == eStep::START;
			} catch (const std::exception& ex) {
				part.status = "Failed: " + std::string(ex.what());
				LOG("Scheduled event %s: %s part failed: %s", event.name.c_str(), EventParts::KindName(part.kind).c_str(), ex.what());
			}
		}
		const bool applied = std::ranges::any_of(parts, [](const Part& part) { return part.applied; });
		event.state = EventSchedule::NextState(event.state, Once(event), event.endsAt, on, applied, ModeOf(event) == eMode::OFF, now);
		if (event.state != stateBefore && (event.state == eEventState::ACTIVE || stateBefore == eEventState::ACTIVE)) {
			const bool started = event.state == eEventState::ACTIVE;
			event.status = started ? "Switched on" : "Switched off";
			Audit(actor, started ? "event_on" : "event_off", Describe(event));
			if (!parts.empty() || !retired.empty()) {
				Alerts::Emit("server", started ? "Scheduled event started" : "Scheduled event ended", event.name + (started ? " is on." : " is off."), {}, "/events");
			}
		} else if (event.state == eEventState::MISSED && stateBefore != eEventState::MISSED) {
			event.status = "The dashboard wasn't running for any of it";
		}
		event.parts = EventParts::Write(parts);
		if (event.parts != before || event.state != stateBefore || !retired.empty()) Save(event, actor.authenticatedUser);
		return run.vanityChanged;
	}

	// Every event, now: the main loop, and straight after staff change one
	void UpdateAll(const HTTPContext& actor) {
		const auto now = Now();
		bool vanity = false;
		for (auto& event : Database::Get()->GetScheduledEvents()) {
			try {
				vanity = SyncEvent(event, actor, now) || vanity;
			} catch (const std::exception& ex) {
				LOG("Scheduled event %llu failed: %s", static_cast<unsigned long long>(event.id), ex.what());
			}
		}
		if (vanity) RespawnVanity(actor);
	}

	// ---------------------------------------------------------------- reading an event from the page

	std::optional<nlohmann::json> CheckFeature(const nlohmann::json& config, std::string& error) {
		const auto feature = Text(config, "feature");
		if (const auto problem = EventSchedule::ValidateFeature(feature)) { error = *problem; return std::nullopt; }
		if (!KnownFeature(feature)) { error = "Pick a feature from the list"; return std::nullopt; }
		return nlohmann::json{ {"feature", feature} };
	}

	std::optional<nlohmann::json> CheckVanity(const nlohmann::json& config, std::string& error) {
		auto file = Text(config, "file");
		if (!file.empty() && !file.ends_with(".xml")) file += ".xml";
		if (!file.empty() && (!VanityEvents::ValidFileName(file) || file == VANITY_ROOT)) { error = "The overlay file is a vanity file name (letters, digits, - and _)"; return std::nullopt; }
		std::string removals;
		if (config.contains("removals") && config["removals"].is_array()) {
			for (const auto& name : config["removals"]) if (name.is_string()) removals += name.get<std::string>() + "\n";
		} else {
			removals = Text(config, "removals");
		}
		const auto names = VanityEvents::SplitNames(removals);
		if (names.size() > MAX_REMOVALS) { error = "Take out at most 200 NPCs"; return std::nullopt; }
		for (const auto& name : names) if (name.size() > MAX_NAME_LENGTH) { error = "NPC names are up to 100 characters"; return std::nullopt; }
		VanityEvents::FileSwitches switches;
		if (config.contains("fileSwitches")) {
			const auto& input = config["fileSwitches"];
			const auto parsed = input.is_string() ? VanityEvents::ParseFileSwitches(input.get<std::string>(), error) : VanityEvents::ParseFileSwitches(input, error);
			if (!parsed) { error = "File switches: " + error; return std::nullopt; }
			switches = *parsed;
		}
		if (switches.contains(VANITY_ROOT)) { error = "File switches: root.xml is always loaded"; return std::nullopt; }
		if (!file.empty() && switches.contains(file)) { error = "File switches: " + file + " is the overlay file, laid on while the event is on; don't switch it too"; return std::nullopt; }
		if (file.empty() && names.empty() && switches.empty()) { error = "Switch a vanity file on or off, pick an overlay file or list NPCs to take out"; return std::nullopt; }
		return nlohmann::json{ {"file", file}, {"removals", names}, {"fileSwitches", VanityEvents::ToJson(switches)} };
	}

	std::optional<nlohmann::json> CheckAnnouncement(const nlohmann::json& config, std::string& error) {
		const auto title = Text(config, "title"), message = Text(config, "message"), endMessage = Text(config, "endMessage"), repeat = Text(config, "repeat");
		const bool atStart = !config.contains("atStart") || !config["atStart"].is_boolean() || config["atStart"].get<bool>();
		if (title.size() > 100) { error = "The title can be at most 100 characters"; return std::nullopt; }
		if (message.size() > MAX_MESSAGE_LENGTH || endMessage.size() > MAX_MESSAGE_LENGTH) { error = "Messages are up to 1000 characters"; return std::nullopt; }
		if ((atStart || !repeat.empty()) && message.empty()) { error = "Write the message"; return std::nullopt; }
		if (!atStart && repeat.empty() && endMessage.empty()) { error = "Say it when the event starts, repeat it, or write a message for the end"; return std::nullopt; }
		if (!repeat.empty()) {
			if (const auto problem = AnnouncementSchedule::Validate(title, message, repeat, 0, 0, {}, Now())) { error = "Repeat: " + *problem; return std::nullopt; }
		}
		nlohmann::json zones = nlohmann::json::array();
		if (config.contains("zones") && config["zones"].is_array()) {
			const auto& known = GameText::ZoneNames();
			for (const auto& zone : config["zones"]) {
				if (!zone.is_number_unsigned() || !known.contains(std::to_string(zone.get<uint32_t>()))) { error = "Pick zones from the list"; return std::nullopt; }
				if (std::find(zones.begin(), zones.end(), zone) == zones.end()) zones.push_back(zone);
			}
		}
		return nlohmann::json{ {"title", title}, {"message", message}, {"zones", zones}, {"atStart", atStart}, {"repeat", repeat}, {"endMessage", endMessage} };
	}

	std::optional<nlohmann::json> CheckRestart(const nlohmann::json& config, std::string& error) {
		const auto when = Text(config, "when");
		if (when != "start" && when != "end") { error = "Restart when the event starts or when it ends"; return std::nullopt; }
		const auto minutes = config.contains("minutes") && config["minutes"].is_number_integer() ? config["minutes"].get<int64_t>() : 0;
		if (minutes < 1 || minutes > MAX_RESTART_MINUTES) { error = "Warn players 1 minute to 24 hours before the restart"; return std::nullopt; }
		const auto reason = Text(config, "reason");
		if (reason.size() > 300) { error = "The reason can be at most 300 characters"; return std::nullopt; }
		return nlohmann::json{ {"when", when}, {"minutes", minutes}, {"reason", reason} };
	}

	// The parts from the page, each checked as its own page would; the error names the part
	std::optional<std::vector<Part>> ReadParts(const nlohmann::json& input, std::string& error) {
		if (!input.is_array()) { error = "Send parts: [{kind, config}]"; return std::nullopt; }
		if (input.size() > MAX_PARTS) { error = "An event has at most 20 parts"; return std::nullopt; }
		auto parts = EventParts::Parse(input, error);
		if (!parts) return std::nullopt;
		for (size_t i = 0; i < parts->size(); i++) {
			auto& part = (*parts)[i];
			std::optional<nlohmann::json> config;
			std::string problem;
			switch (part.kind) {
			case eKind::FEATURE: config = CheckFeature(part.config, problem); break;
			case eKind::VANITY: config = CheckVanity(part.config, problem); break;
			case eKind::LIVE_EVENT: config = LiveEventRoutes::CheckPart(part.config, problem); break;
			case eKind::ANNOUNCEMENT: config = CheckAnnouncement(part.config, problem); break;
			case eKind::RESTART: config = CheckRestart(part.config, problem); break;
			}
			if (!config) { error = "Part " + std::to_string(i + 1) + " (" + EventParts::KindName(part.kind) + "): " + problem; return std::nullopt; }
			part.config = std::move(*config);
		}
		return parts;
	}

	// Fill an event from the page's body (every field it has); the error names what is wrong
	std::optional<std::string> FromBody(const nlohmann::json& body, Event& event, std::vector<Part>& parts, int64_t now) {
		const auto text = [&body](const char* key, const std::string& fallback) {
			return body.contains(key) && body[key].is_string() ? body[key].get<std::string>() : fallback;
		};
		event.name = text("name", event.name);
		event.name.erase(0, event.name.find_first_not_of(" \t"));
		event.name.erase(event.name.find_last_not_of(" \t") + 1);
		if (event.name.empty() || event.name.size() > MAX_NAME_LENGTH) return "Give it a name of up to 100 characters";
		event.note = text("note", event.note);
		if (event.note.size() > MAX_NOTE_LENGTH) return "The note can be at most 500 characters";
		if (body.contains("priority")) {
			if (!body["priority"].is_number_integer() || std::abs(body["priority"].get<int64_t>()) > MAX_PRIORITY) return "Priority is a whole number from -1000 to 1000";
			event.priority = body["priority"].get<int32_t>();
		}
		if (body.contains("mode")) {
			const auto value = body["mode"].is_number_integer() ? body["mode"].get<int64_t>() : -1;
			const auto mode = value >= 0 && value <= UINT8_MAX ? magic_enum::enum_cast<eMode>(static_cast<uint8_t>(value)) : std::nullopt;
			if (!mode) return "Pick whether it is off, on by its schedule or always on";
			event.mode = static_cast<uint8_t>(*mode);
		}
		// Recurring rules, or null / "" for once between startsAt and endsAt
		if (body.contains("schedule")) {
			const auto& input = body["schedule"];
			if (input.is_null() || (input.is_string() && input.get<std::string>().empty())) {
				event.schedule.clear();
			} else {
				std::string error;
				const auto schedule = input.is_string() ? ScheduleRules::ParseSchedule(input.get<std::string>(), error) : ScheduleRules::ParseSchedule(input, error);
				if (!schedule) return "Schedule: " + error;
				event.schedule = ScheduleRules::ToJson(*schedule).dump();
			}
		}
		if (Once(event)) {
			const auto startsAt = body.contains("startsAt") && body["startsAt"].is_number_integer() ? body["startsAt"].get<int64_t>() : event.startsAt;
			const auto endsAt = body.contains("endsAt") && body["endsAt"].is_number_integer() ? body["endsAt"].get<int64_t>() : event.endsAt;
			// Times are only checked when they change, so an event that is over can still get a new note
			if (startsAt != event.startsAt || endsAt != event.endsAt || event.id == 0) {
				if (const auto error = EventSchedule::ValidateOnce(startsAt, endsAt, now)) return *error;
			}
			event.startsAt = startsAt;
			event.endsAt = endsAt;
		} else {
			event.startsAt = event.endsAt = 0;
		}
		if (body.contains("parts")) {
			std::string error;
			auto read = ReadParts(body["parts"], error);
			if (!read) return error;
			parts = std::move(*read);
		}
		if (parts.empty()) return "Add at least one part: what the event switches on";
		return std::nullopt;
	}

	// An empty vanity file for each vanity part's overlay file that doesn't exist yet, to fill in the NPC editor. The
	// files it made
	std::string CreateOverlays(const std::vector<Part>& parts) {
		std::string made;
		for (const auto& part : parts) {
			const auto file = part.kind == eKind::VANITY ? Text(part.config, "file") : "";
			std::error_code ec;
			if (file.empty() || std::filesystem::exists(VanityFolder() / file, ec)) continue;
			std::ofstream out(VanityFolder() / file, std::ios::binary);
			out << VanityXml::Write({});
			if (out) made += (made.empty() ? "" : ", ") + file;
		}
		return made;
	}

	// ---------------------------------------------------------------- JSON for the page

	nlohmann::json PartJson(const Part& part) {
		auto json = EventParts::ToJson(part);
		json["permission"] = PermissionFor(part.kind);
		if (part.kind == eKind::FEATURE) json["affected"] = RunningWorlds(ZonesOf(Text(part.config, "feature")));
		if (part.kind == eKind::LIVE_EVENT && part.state.contains("id")) json["running"] = LiveEventRoutes::Running(part.state.value("id", uint64_t{ 0 }));
		return json;
	}

	nlohmann::json EventJson(const Event& event, int64_t now) {
		nlohmann::json parts = nlohmann::json::array();
		std::string partsError;
		if (const auto list = EventParts::Parse(event.parts, partsError)) for (const auto& part : *list) parts.push_back(PartJson(part));
		const auto [nextStart, nextEnd] = NextTimes(event, now);
		nlohmann::json json{ {"id", event.id}, {"name", event.name}, {"note", event.note}, {"mode", event.mode}, {"modeName", GameLabels::Name(ModeOf(event))},
			{"once", Once(event)}, {"startsAt", event.startsAt}, {"endsAt", event.endsAt}, {"schedule", nullptr}, {"scheduleError", ""},
			{"priority", event.priority}, {"parts", parts}, {"partsError", partsError}, {"on", IsOn(event, now)},
			{"state", static_cast<uint8_t>(event.state)}, {"stateName", GameLabels::Name(event.state)}, {"status", event.status},
			{"nextStart", nextStart ? nlohmann::json(*nextStart) : nlohmann::json(nullptr)}, {"nextEnd", nextEnd ? nlohmann::json(*nextEnd) : nlohmann::json(nullptr)},
			{"createdAt", event.createdAt}, {"createdBy", event.createdBy}, {"updatedAt", event.updatedAt}, {"updatedBy", event.updatedBy} };
		if (!Once(event)) {
			std::string error;
			if (const auto schedule = ScheduleRules::ParseSchedule(event.schedule, error)) json["schedule"] = ScheduleRules::ToJson(*schedule);
			else json["scheduleError"] = error;
		}
		return json;
	}

	// [{value, name}] for every event state
	nlohmann::json States() {
		nlohmann::json states = nlohmann::json::array();
		for (const auto state : magic_enum::enum_values<eEventState>()) states.push_back({ {"value", static_cast<uint8_t>(state)}, {"name", GameLabels::Name(state)} });
		return states;
	}

	// Every kind of part, whether this user may add it
	nlohmann::json Kinds(const HTTPContext& context) {
		nlohmann::json kinds = nlohmann::json::array();
		for (const auto kind : magic_enum::enum_values<eKind>()) {
			kinds.push_back({ {"name", EventParts::KindName(kind)}, {"permission", PermissionFor(kind)}, {"allowed", Can(context, PermissionFor(kind))} });
		}
		return kinds;
	}

	// What vanity changes the events that are on now make, and which events change the same NPCs or files
	nlohmann::json VanityJson(const std::vector<Event>& events, int64_t now) {
		auto sorted = events;
		VanityEvents::SortForMerge(sorted);
		std::vector<VanityEvents::Changes> on;
		std::vector<std::pair<uint64_t, std::set<std::string>>> touched, switched;
		for (const auto& event : sorted) {
			std::set<std::string> npcs, files;
			for (const auto& change : EventParts::VanityChanges(event.name, event.parts)) {
				if (IsOn(event, now)) on.push_back(change);
				for (const auto& name : VanityEvents::SplitNames(change.removals)) npcs.insert(name);
				if (!change.file.empty()) {
					for (const auto& object : VanityEvents::LoadFiles(VanityFolder(), change.file).objects) if (!object.name.empty()) npcs.insert(object.name);
				}
				std::string error;
				for (const auto& [file, switchOn] : VanityEvents::ParseFileSwitches(change.fileSwitches, error).value_or(VanityEvents::FileSwitches{})) files.insert(file);
			}
			if (npcs.empty() && files.empty()) continue;
			touched.emplace_back(event.id, std::move(npcs));
			switched.emplace_back(event.id, std::move(files));
		}
		// Pairs that change the same NPCs or switch the same files; whichever is laid on last (see SortForMerge) wins when both are on
		nlohmann::json overlaps = nlohmann::json::array();
		for (size_t a = 0; a < touched.size(); a++) {
			for (size_t b = a + 1; b < touched.size(); b++) {
				std::vector<std::string> npcs, files;
				std::set_intersection(touched[a].second.begin(), touched[a].second.end(), touched[b].second.begin(), touched[b].second.end(), std::back_inserter(npcs));
				std::set_intersection(switched[a].second.begin(), switched[a].second.end(), switched[b].second.begin(), switched[b].second.end(), std::back_inserter(files));
				if (!npcs.empty() || !files.empty()) overlaps.push_back({ {"a", touched[a].first}, {"b", touched[b].first}, {"winner", touched[b].first}, {"npcs", npcs}, {"files", files} });
			}
		}
		const auto world = VanityEvents::LoadWorld(VanityFolder(), VANITY_ROOT, on);
		nlohmann::json conflicts = nlohmann::json::array(), fileConflicts = nlohmann::json::array();
		for (const auto& conflict : world.conflicts) conflicts.push_back({ {"npc", conflict.npc}, {"events", conflict.events}, {"winner", conflict.events.back()} });
		for (const auto& conflict : world.fileConflicts) {
			nlohmann::json switches = nlohmann::json::array();
			for (const auto& [name, switchOn] : conflict.switches) switches.push_back({ {"event", name}, {"on", switchOn} });
			fileConflicts.push_back({ {"file", conflict.file}, {"switches", switches}, {"winner", conflict.switches.back().first}, {"on", conflict.switches.back().second} });
		}
		return { {"conflicts", conflicts}, {"fileConflicts", fileConflicts}, {"overlaps", overlaps} };
	}

	// The standalone things that also happen at set times, for the calendar: live events and the scheduled restart
	void OtherOccurrences(nlohmann::json& occurrences, int64_t from, int64_t to, const HTTPContext& context) {
		if (Can(context, "live_events_manage")) {
			for (const auto& live : Database::Get()->GetLiveEvents(false, 100)) {
				if (live.endsAt <= from || live.startsAt >= to) continue;
				occurrences.push_back({ {"kind", "live_event"}, {"id", live.id}, {"name", live.title}, {"start", live.startsAt}, {"end", live.endedAt ? std::min(live.endedAt, live.endsAt) : live.endsAt},
					{"link", "/live_events"} });
			}
		}
		if (Can(context, "announcements_schedule")) {
			const auto now = Now();
			for (const auto& row : Database::Get()->GetScheduledAnnouncements()) {
				if (!row.enabled) continue;
				const auto schedule = Cron::Parse(row.schedule);
				if (!schedule) continue;
				// Every send in the stretch, from now on (a few per day at most are shown)
				auto at = AnnouncementSchedule::Next(*schedule, row.startsAt, row.endsAt, std::max(from, now) - 1);
				for (int count = 0; at && *at < to && count < 100; count++) {
					occurrences.push_back({ {"kind", "announcement"}, {"id", row.id}, {"name", row.title.empty() ? row.message.substr(0, 40) : row.title}, {"start", *at}, {"end", *at},
						{"link", "/announcements"} });
					at = AnnouncementSchedule::Next(*schedule, row.startsAt, row.endsAt, *at);
				}
			}
		}
		if (Can(context, "server_restart")) {
			const auto restart = LiveWorld::RestartStatus(false);
			if (restart.contains("at") && restart["at"].is_number_integer()) {
				const auto at = restart["at"].get<int64_t>();
				if (at >= from && at < to) occurrences.push_back({ {"kind", "restart"}, {"id", 0}, {"name", "Restart"}, {"start", at}, {"end", at}, {"link", "/#restart"} });
			}
		}
	}

	std::optional<uint64_t> IdFrom(const HTTPContext& context) { return PathId<uint64_t>(context.path, 2); }
}

namespace EventsCalendar {
	void Update() {
		const auto steady = std::chrono::steady_clock::now();
		if (steady < g_NextCheck) return;
		g_NextCheck = steady + CHECK_INTERVAL;
		if (g_Scan == eScan::NOT_STARTED) StartFeatureScan();
		try {
			UpdateAll(SystemContext());
		} catch (const std::exception& ex) {
			LOG_DEBUG("Could not update the scheduled events: %s", ex.what());
		}
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/events", 0, "Events page: scheduled events and the calendar (needs one of the permissions a part needs)",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!CanView(context)) return RenderError(reply, context, eHTTPStatusCode::FORBIDDEN, "You may not see the scheduled events");
				RenderPage(reply, context, "events.jinja2", "events");
			});

		// Vanity events are scheduled events now; old links still work
		Route(eHTTPMethod::GET, "/vanity_events", 0, "The Events page (vanity events are scheduled events with a vanity part)",
			[](HTTPReply& reply, const HTTPContext&) {
				reply.status = eHTTPStatusCode::FOUND;
				reply.location = "/events";
				reply.message = "";
			});

		Route(eHTTPMethod::GET, "/api/events", 0,
			"Every scheduled event with its parts (and what each did), whether it is on and its next start and end; the kinds of part and whether you may "
			"add each; every feature a feature part can switch on (with the zones it is in), what is in event_1..event_8 now, the zones, and the vanity "
			"conflicts between the events that are on",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!CanView(context)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may not see the scheduled events");
				const auto rows = Database::Get()->GetServerConfig({});
				const auto events = Database::Get()->GetScheduledEvents();
				const auto now = Now();
				nlohmann::json list = nlohmann::json::array();
				for (const auto& event : events) list.push_back(EventJson(event, now));
				nlohmann::json features = nlohmann::json::array();
				for (const auto& [name, feature] : Features()) features.push_back(FeatureJson(name, feature));
				// What each slot holds for the world servers: their worldconfig.ini, then sharedconfig.ini (see ConfigSync)
				nlohmann::json slots = nlohmann::json::array();
				const auto busy = EventSchedule::BusySlots(rows);
				for (uint8_t slot = 1; slot <= EventSchedule::SLOTS; slot++) {
					const auto name = EventSchedule::SlotSetting(slot);
					nlohmann::json values = nlohmann::json::array();
					for (const auto& row : rows) {
						if (row.name != name) continue;
						if (row.fileValue && !row.fileValue->empty()) values.push_back({ {"file", row.file}, {"source", row.fileSource}, {"value", *row.fileValue} });
						if (row.webValue && !row.webValue->empty()) values.push_back({ {"file", row.file}, {"source", "web"}, {"value", *row.webValue}, {"webWins", row.webWins} });
					}
					// The event whose feature part holds it
					nlohmann::json owner = nullptr;
					for (const auto& event : events) {
						for (const auto& part : PartsOf(event)) {
							if (part.kind == eKind::FEATURE && part.applied && !part.state.value("shared", false) && part.state.value("slot", 0) == slot) owner = event.id;
						}
					}
					slots.push_back({ {"slot", slot}, {"setting", name}, {"busy", busy[slot]}, {"values", values}, {"event", owner} });
				}
				const auto [major, current, minor] = ClientVersionSetting();
				JsonSuccess(reply, { {"events", list}, {"features", features}, {"scanning", g_Scan != eScan::DONE}, {"slots", slots}, {"now", now},
					{"clientVersion", std::to_string(major) + "." + std::to_string(current) + "." + std::to_string(minor)},
					{"states", States()}, {"modes", GameLabels::List<eMode>()}, {"kinds", Kinds(context)}, {"zones", ZoneOptions()},
					{"vanity", Can(context, "vanity_manage") ? VanityJson(events, now) : nlohmann::json(nullptr)} });
			});

		Route(eHTTPMethod::POST, "/api/events", 0,
			"Create a scheduled event. Body: {name, note, mode (0 off, 1 by its schedule, 2 always on), schedule (recurring rules, or null for once), "
			"startsAt, endsAt (unix, for once), priority, parts: [{kind: feature|vanity|live_event|announcement|restart, config}]}. Each part needs the "
			"permission of its own page",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body || !body->is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				Event event;
				std::vector<Part> parts;
				if (const auto error = FromBody(*body, event, parts, Now())) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
				if (const auto kind = Forbidden(context, parts)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, ForbiddenMessage(*kind));
				event.parts = EventParts::Write(parts);
				const auto made = CreateOverlays(parts);
				event.createdAt = event.updatedAt = Now();
				event.createdBy = event.updatedBy = context.authenticatedUser;
				event.id = Database::Get()->InsertScheduledEvent(event);
				Audit(context, "schedule_event", Describe(event));
				BroadcastTableChanged("scheduled_events", std::to_string(event.id));
				UpdateAll(context); // start it now if it's on
				JsonSuccess(reply, { {"message", made.empty() ? "Event scheduled" : "Event scheduled, with an empty " + made + " to put its NPCs in"}, {"id", event.id} });
			});

		ReadRoute(eHTTPMethod::POST, "/api/events/check", 0,
			"Check a schedule and list when it is on. Body: {schedule (recurring rules), from (unix, default now), count (default 5)}. Returns {valid, error, schedule, on, windows: [{start, end}]}",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!CanView(context)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may not see the scheduled events");
				const auto body = ParseBody(context);
				if (!body || !body->is_object() || !body->contains("schedule")) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Send {schedule}");
				std::string error;
				const auto& input = (*body)["schedule"];
				const auto schedule = input.is_string() ? ScheduleRules::ParseSchedule(input.get<std::string>(), error) : ScheduleRules::ParseSchedule(input, error);
				if (!schedule) return JsonSuccess(reply, { {"valid", false}, {"error", error} });
				const auto from = body->contains("from") && (*body)["from"].is_number_integer() ? (*body)["from"].get<int64_t>() : Now();
				const auto count = std::clamp<int64_t>(body->contains("count") && (*body)["count"].is_number_integer() ? (*body)["count"].get<int64_t>() : 5, 1, 50);
				nlohmann::json windows = nlohmann::json::array();
				for (const auto& window : ScheduleRules::Windows(*schedule, from, from + ScheduleRules::HORIZON_SECONDS, static_cast<size_t>(count))) {
					// The first may have started before `from`: say when it really did, if that's within a year
					auto start = window.start;
					if (start == from && ScheduleRules::Active(*schedule, from)) {
						for (const auto& earlier : ScheduleRules::Windows(*schedule, from - 366 * 86400LL, from + 1)) if (earlier.end > from) start = earlier.start;
					}
					windows.push_back({ {"start", start}, {"end", window.end} });
				}
				JsonSuccess(reply, { {"valid", true}, {"schedule", ScheduleRules::ToJson(*schedule)}, {"on", ScheduleRules::Active(*schedule, from)}, {"windows", windows} });
			});

		Route(eHTTPMethod::GET, "/api/events/occurrences", 0,
			"When each scheduled event is on between ?from and ?to (unix, at most 400 days apart), for the calendar, with the standalone live events, "
			"repeating announcements and the scheduled restart you may see: {occurrences: [{kind, id, name, start, end, mode, link}]}",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!CanView(context)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may not see the scheduled events");
				const auto from = GeneralUtils::TryParse<int64_t>(QueryValue(context.queryString, "from"));
				const auto to = GeneralUtils::TryParse<int64_t>(QueryValue(context.queryString, "to"));
				if (!from || !to || *to <= *from || *to - *from > MAX_RANGE_SECONDS) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Send ?from and ?to, at most 400 days apart");
				nlohmann::json occurrences = nlohmann::json::array();
				for (const auto& event : Database::Get()->GetScheduledEvents()) {
					for (const auto& window : ScheduleRules::WindowsOf(ModeOf(event), event.schedule, event.startsAt, event.endsAt, *from, *to, 200)) {
						occurrences.push_back({ {"kind", "event"}, {"id", event.id}, {"name", event.name}, {"start", window.start}, {"end", window.end}, {"mode", event.mode},
							{"state", static_cast<uint8_t>(event.state)} });
					}
				}
				OtherOccurrences(occurrences, *from, *to, context);
				JsonSuccess(reply, { {"occurrences", occurrences} });
			});

		Route(eHTTPMethod::POST, "/api/events/:id", 0,
			"Change a scheduled event. Body as when creating it (fields left out stay). Parts that stay the same carry on; changed or removed parts that "
			"are on are ended first. Needs the permission of every kind of part it has, before and after",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = IdFrom(context);
				const auto body = ParseBody(context);
				if (!body || !body->is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				auto event = id ? FindEvent(*id) : std::nullopt;
				if (!event) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Event not found");
				const auto before = PartsOf(*event);
				auto parts = before;
				if (const auto error = FromBody(*body, *event, parts, Now())) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
				if (const auto kind = Forbidden(context, before, parts)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, ForbiddenMessage(*kind));
				// A cancelled event switched back on waits for its times again
				if (event->state == eEventState::CANCELLED && ModeOf(*event) != eMode::OFF) event->state = eEventState::SCHEDULED;
				const auto made = CreateOverlays(parts);
				auto reconciled = EventParts::Reconcile(before, std::move(parts));
				event->parts = EventParts::Write(reconciled.parts);
				Save(*event, context.authenticatedUser);
				Audit(context, "update_event", Describe(*event));
				if (SyncEvent(*event, context, Now(), std::move(reconciled.retired))) RespawnVanity(context);
				UpdateAll(context);
				JsonSuccess(reply, { {"message", made.empty() ? "Saved" : "Saved, with an empty " + made + " to put its NPCs in"} });
			});

		Route(eHTTPMethod::POST, "/api/events/:id/mode", 0,
			"Switch a scheduled event off, to its schedule or always on. Body: {mode}. Needs the permission of every kind of part it has",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = IdFrom(context);
				const auto body = ParseBody(context);
				auto event = id ? FindEvent(*id) : std::nullopt;
				if (!event) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Event not found");
				if (const auto kind = Forbidden(context, PartsOf(*event))) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, ForbiddenMessage(*kind));
				const auto value = body && body->contains("mode") && (*body)["mode"].is_number_integer() ? (*body)["mode"].get<int64_t>() : -1;
				const auto mode = value >= 0 && value <= UINT8_MAX ? magic_enum::enum_cast<eMode>(static_cast<uint8_t>(value)) : std::nullopt;
				if (!mode) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick whether it is off, on by its schedule or always on");
				event->mode = static_cast<uint8_t>(*mode);
				if (event->state == eEventState::CANCELLED && *mode != eMode::OFF) event->state = eEventState::SCHEDULED;
				Save(*event, context.authenticatedUser);
				Audit(context, "update_event", Describe(*event));
				if (SyncEvent(*event, context, Now())) RespawnVanity(context);
				JsonSuccess(reply, { {"message", "Now " + GameLabels::Name(*mode)} });
			});

		Route(eHTTPMethod::POST, "/api/events/:id/cancel", 0,
			"Cancel a scheduled event: it is switched off and whatever it switched on is ended now. Needs the permission of every kind of part it has",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = IdFrom(context);
				auto event = id ? FindEvent(*id) : std::nullopt;
				if (!event) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Event not found");
				if (const auto kind = Forbidden(context, PartsOf(*event))) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, ForbiddenMessage(*kind));
				event->mode = static_cast<uint8_t>(eMode::OFF);
				const bool vanity = SyncEvent(*event, context, Now());
				event->state = eEventState::CANCELLED;
				event->status = "Cancelled by " + context.authenticatedUser;
				Save(*event, context.authenticatedUser);
				Audit(context, "cancel_event", Describe(*event));
				Alerts::Emit("server", "Event cancelled", event->name + " was cancelled.", {}, "/events");
				if (vanity) RespawnVanity(context);
				JsonSuccess(reply, { {"message", "Event cancelled"} });
			});

		Route(eHTTPMethod::POST, "/api/events/:id/delete", 0,
			"Delete a scheduled event; whatever it switched on is undone first (without its end announcement or restart). Needs the permission of every kind of part it has",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = IdFrom(context);
				auto event = id ? FindEvent(*id) : std::nullopt;
				if (!event) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Event not found");
				if (const auto kind = Forbidden(context, PartsOf(*event))) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, ForbiddenMessage(*kind));
				// Undone as if its parts were taken out: no end announcement, no restart
				Run run{ context, *event, Now() };
				std::string problems;
				for (auto& part : PartsOf(*event)) {
					if (!part.applied) continue;
					Retire(part, run);
					if (part.status.starts_with("Couldn't")) problems += " " + part.status + ".";
				}
				const bool vanity = run.vanityChanged;
				Database::Get()->DeleteScheduledEvent(event->id);
				Audit(context, "delete_event", Describe(*event));
				BroadcastTableChanged("scheduled_events", std::to_string(event->id));
				if (vanity) RespawnVanity(context);
				JsonSuccess(reply, { {"message", "Deleted." + problems} });
			});
	}

	nlohmann::json VanityPreview(const nlohmann::json& body) {
		const auto events = Database::Get()->GetScheduledEvents();
		const auto at = body.contains("at") && body["at"].is_number_integer() ? body["at"].get<int64_t>() : Now();
		std::vector<Event> chosen;
		if (body.contains("ids") && body["ids"].is_array()) {
			std::set<uint64_t> ids;
			for (const auto& id : body["ids"]) if (id.is_number_unsigned()) ids.insert(id.get<uint64_t>());
			for (const auto& event : events) if (ids.contains(event.id)) chosen.push_back(event);
		} else {
			for (const auto& event : events) if (IsOn(event, at)) chosen.push_back(event);
		}
		VanityEvents::SortForMerge(chosen);
		std::vector<VanityEvents::Changes> changes;
		nlohmann::json order = nlohmann::json::array();
		for (const auto& event : chosen) {
			auto list = EventParts::VanityChanges(event.name, event.parts);
			if (list.empty()) continue;
			nlohmann::json parts = nlohmann::json::array();
			for (const auto& change : list) {
				std::string error;
				parts.push_back({ {"file", change.file}, {"removals", VanityEvents::SplitNames(change.removals)},
					{"fileSwitches", VanityEvents::ToJson(VanityEvents::ParseFileSwitches(change.fileSwitches, error).value_or(VanityEvents::FileSwitches{}))} });
				changes.push_back(change);
			}
			order.push_back({ {"id", event.id}, {"name", event.name}, {"priority", event.priority}, {"parts", parts} });
		}
		const auto world = VanityEvents::LoadWorld(VanityFolder(), VANITY_ROOT, changes);
		const auto alone = VanityEvents::LoadWorld(VanityFolder(), VANITY_ROOT, std::vector<VanityEvents::Changes>{});
		const auto files = [](const std::vector<VanityEvents::FileLoad>& list) {
			nlohmann::json json = nlohmann::json::array();
			for (const auto& file : list) json.push_back({ {"name", file.name}, {"includedBy", file.includedBy}, {"enabled", file.enabled}, {"switchedBy", file.switchedBy}, {"loaded", file.loaded} });
			return json;
		};
		VanityXml::Document document;
		document.objects = world.objects;
		nlohmann::json conflicts = nlohmann::json::array(), fileConflicts = nlohmann::json::array();
		for (const auto& conflict : world.conflicts) conflicts.push_back({ {"npc", conflict.npc}, {"events", conflict.events}, {"winner", conflict.events.back()} });
		for (const auto& conflict : world.fileConflicts) {
			nlohmann::json switches = nlohmann::json::array();
			for (const auto& [name, switchOn] : conflict.switches) switches.push_back({ {"event", name}, {"on", switchOn} });
			fileConflicts.push_back({ {"file", conflict.file}, {"switches", switches}, {"winner", conflict.switches.back().first}, {"on", conflict.switches.back().second} });
		}
		return { {"at", at}, {"events", order}, {"xml", VanityXml::Write(document)}, {"npcs", world.objects.size()}, {"fileNpcs", world.fileNpcs}, {"baseNpcs", alone.objects.size()},
			{"files", files(world.files)}, {"baseFiles", files(alone.files)}, {"conflicts", conflicts}, {"fileConflicts", fileConflicts}, {"warnings", world.warnings} };
	}

	std::vector<VanityEvents::Changes> VanityChangesOn(int64_t at) {
		auto events = Database::Get()->GetScheduledEvents();
		std::erase_if(events, [at](const Event& event) { return !IsOn(event, at); });
		VanityEvents::SortForMerge(events);
		std::vector<VanityEvents::Changes> changes;
		for (const auto& event : events) {
			for (auto& change : EventParts::VanityChanges(event.name, event.parts)) changes.push_back(std::move(change));
		}
		return changes;
	}

	nlohmann::json VanityUses(int64_t now) {
		nlohmann::json uses{ {"files", nlohmann::json::object()}, {"removes", nlohmann::json::object()}, {"events", nlohmann::json::array()} };
		auto events = Database::Get()->GetScheduledEvents();
		VanityEvents::SortForMerge(events);
		for (const auto& event : events) {
			const bool on = IsOn(event, now);
			const auto ref = [&](const std::string& use) { return nlohmann::json{ {"id", event.id}, {"name", event.name}, {"on", on}, {"use", use} }; };
			const auto changes = EventParts::VanityChanges(event.name, event.parts);
			if (!changes.empty()) {
				const auto [nextStart, nextEnd] = NextTimes(event, now);
				uses["events"].push_back({ {"id", event.id}, {"name", event.name}, {"on", on}, {"priority", event.priority}, {"modeName", GameLabels::Name(ModeOf(event))},
					{"nextStart", nextStart ? nlohmann::json(*nextStart) : nlohmann::json(nullptr)}, {"nextEnd", nextEnd ? nlohmann::json(*nextEnd) : nlohmann::json(nullptr)} });
			}
			for (const auto& change : changes) {
				if (!change.file.empty()) uses["files"][change.file].push_back(ref("overlay"));
				std::string error;
				for (const auto& [file, switchOn] : VanityEvents::ParseFileSwitches(change.fileSwitches, error).value_or(VanityEvents::FileSwitches{})) {
					uses["files"][file].push_back(ref(switchOn ? "on" : "off"));
				}
				for (const auto& name : VanityEvents::SplitNames(change.removals)) uses["removes"][name].push_back(ref("remove"));
			}
		}
		return uses;
	}
}
