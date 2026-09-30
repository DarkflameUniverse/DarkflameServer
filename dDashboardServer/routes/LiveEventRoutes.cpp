#include "LiveEventRoutes.h"
#include "GameText.h"
#include "LiveWorld.h"
#include "DashboardRoutes.h"
#include "ClientAssets.h"
#include "PlayerActions.h"
#include "ServerState.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <map>
#include <set>

#include "RouteUtils.h"
#include "WSRoutes.h"
#include "Alerts.h"
#include "CDClientDatabase.h"
#include "Database.h"
#include "Logger.h"
#include "LiveOpsRules.h"
#include "eHTTPMethod.h"
#include "eReplicaComponentType.h"

using namespace RouteUtils;
using LiveOpsRules::eEventType;

namespace {
	using Event = ILiveOps::LiveEvent;
	using eState = ILiveOps::eLiveEventState;
	constexpr auto CHECK_INTERVAL = std::chrono::seconds(3);
	constexpr size_t MAX_TITLE = 100;
	constexpr size_t MAX_MESSAGE = 300;

	std::chrono::steady_clock::time_point g_NextCheck{};

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	// Objects.type values each picker offers, so staff pick from what the client has rather than typing LOTs
	const std::vector<std::string>& ObjectTypes(const std::string& kind) {
		static const std::vector<std::string> treasure{ "Smashables", "Powerup", "Coin", "Environmental", "Structure", "Interactives", "Exhibits" };
		static const std::vector<std::string> enemy{ "Enemies", "Enemy" };
		static const std::vector<std::string> item{ "Loot" };
		if (kind == "enemy") return enemy;
		if (kind == "item") return item;
		return treasure;
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

	std::string TypeLabel(eEventType type) {
		switch (type) {
		case eEventType::TREASURE_HUNT: return "Treasure hunt";
		case eEventType::BONUS: return "Bonus";
		case eEventType::INVASION: return "Invasion";
		case eEventType::CELEBRATION: return "Celebration";
		}
		return "Event";
	}

	std::string Describe(const Event& event) {
		return "#" + std::to_string(event.id) + " " + event.type + " \"" + event.title + "\" in " + ZoneList(event.zones) +
			(event.instanceId >= 0 ? " (instance " + std::to_string(event.instanceId) + ")" : "");
	}

	// Does the CDClient have this object, drawn in the world (a render component), and of one of these types?
	bool ObjectOfTypes(LOT lot, const std::vector<std::string>& types) {
		auto stmt = CDClientDatabase::CreatePreppedStmt(
			"SELECT o.type FROM Objects o WHERE o.id = ? AND EXISTS (SELECT 1 FROM ComponentsRegistry c WHERE c.id = o.id AND c.component_type = ?);");
		stmt.bind(1, static_cast<int>(lot));
		stmt.bind(2, static_cast<int>(eReplicaComponentType::RENDER));
		auto result = stmt.execQuery();
		if (result.eof()) return false;
		const std::string type = result.getStringField(0, "");
		return std::find(types.begin(), types.end(), type) != types.end();
	}

	bool EffectExists(int32_t effectId, const std::string& effectType) {
		auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT 1 FROM BehaviorEffect WHERE effectID = ? AND effectType = ? LIMIT 1;");
		stmt.bind(1, effectId);
		stmt.bind(2, effectType.c_str());
		return !stmt.execQuery().eof();
	}

	float Multiplier(const nlohmann::json& body, const char* key) {
		const auto& value = body.contains(key) ? body[key] : nlohmann::json(1.0);
		return value.is_number() ? value.get<float>() : 0.0f;
	}

	template<typename T>
	std::optional<T> Number(const nlohmann::json& body, const char* key, T fallback, T min, T max) {
		if (!body.contains(key) || body[key].is_null()) return fallback;
		if (!body[key].is_number()) return std::nullopt;
		const auto value = body[key].get<double>();
		if (value < static_cast<double>(min) || value > static_cast<double>(max)) return std::nullopt;
		return static_cast<T>(value);
	}

	/**
	 * The event's settings from a request, checked and with only what the world reads. error says what is wrong.
	 */
	std::optional<nlohmann::json> ReadConfig(eEventType type, const nlohmann::json& body, std::string& error) {
		nlohmann::json config = nlohmann::json::object();
		const auto fail = [&error](const std::string& message) { error = message; return std::nullopt; };
		switch (type) {
		case eEventType::TREASURE_HUNT: {
			const auto lot = Number<int32_t>(body, "lot", 0, 1, INT32_MAX);
			if (!lot || !ObjectOfTypes(*lot, ObjectTypes("treasure"))) return fail("Pick the treasure from the list");
			const auto count = Number<uint32_t>(body, "count", 10, 1, LiveOpsRules::MAX_TREASURES);
			if (!count) return fail("Treasures: 1 to " + std::to_string(LiveOpsRules::MAX_TREASURES));
			const auto coins = Number<int64_t>(body, "coins", 0, 0, 100000);
			if (!coins) return fail("Coins per find: 0 to 100000");
			const auto radius = Number<float>(body, "radius", 4.0f, 2.0f, 15.0f);
			if (!radius) return fail("Pickup distance: 2 to 15");
			const auto itemLot = Number<int32_t>(body, "itemLot", 0, 0, INT32_MAX);
			if (!itemLot || (*itemLot > 0 && !ObjectOfTypes(*itemLot, ObjectTypes("item")))) return fail("Pick the item reward from the list");
			const auto itemCount = Number<uint32_t>(body, "itemCount", 1, 1, 999);
			if (!itemCount) return fail("Item count: 1 to 999");
			config = { {"lot", *lot}, {"count", *count}, {"coins", *coins}, {"radius", *radius}, {"itemLot", *itemLot}, {"itemCount", *itemCount} };
			if (const auto effect = Number<int32_t>(body, "foundEffectId", 0, 0, INT32_MAX); effect && *effect > 0) {
				const auto effectType = body.value("foundEffectType", std::string{});
				if (!EffectExists(*effect, effectType)) return fail("Pick the find effect from the list");
				config["foundEffectId"] = *effect;
				config["foundEffectType"] = effectType;
			}
			break;
		}
		case eEventType::BONUS: {
			const LiveOpsRules::Multipliers multipliers{ Multiplier(body, "coins"), Multiplier(body, "uscore"), Multiplier(body, "lootChance") };
			for (const auto value : { multipliers.coins, multipliers.uscore, multipliers.lootChance }) {
				if (!(value >= 1.0f && value <= LiveOpsRules::MAX_MULTIPLIER)) return fail("Multipliers go from 1 to " + std::to_string(static_cast<int>(LiveOpsRules::MAX_MULTIPLIER)));
			}
			if (!multipliers.Any()) return fail("Raise at least one multiplier above 1");
			config = { {"coins", multipliers.coins}, {"uscore", multipliers.uscore}, {"lootChance", multipliers.lootChance} };
			break;
		}
		case eEventType::INVASION: {
			nlohmann::json lots = nlohmann::json::array();
			if (body.contains("lots") && body["lots"].is_array()) {
				for (const auto& lot : body["lots"]) {
					if (!lot.is_number_integer() || !ObjectOfTypes(lot.get<int32_t>(), ObjectTypes("enemy"))) return fail("Pick the enemies from the list");
					if (std::find(lots.begin(), lots.end(), lot) == lots.end()) lots.push_back(lot);
				}
			}
			if (lots.empty() || lots.size() > 5) return fail("Pick 1 to 5 kinds of enemy");
			const auto waves = Number<uint32_t>(body, "waves", 3, 1, LiveOpsRules::MAX_WAVES);
			const auto perWave = Number<uint32_t>(body, "perWave", 5, 1, LiveOpsRules::MAX_PER_WAVE);
			const auto interval = Number<int64_t>(body, "interval", 90, 15, 3600);
			const auto radius = Number<float>(body, "radius", 25.0f, 5.0f, 80.0f);
			if (!waves || !perWave || !interval || !radius) return fail("Waves 1-20, enemies per wave 1-30, 15-3600 seconds apart, radius 5-80");
			const auto center = body.value("center", std::string{ "spawn" });
			if (center != "spawn" && center != "players") return fail("Attack the spawn point or the players");
			config = { {"lots", lots}, {"waves", *waves}, {"perWave", *perWave}, {"interval", *interval}, {"radius", *radius}, {"center", center} };
			break;
		}
		case eEventType::CELEBRATION: {
			const auto effect = Number<int32_t>(body, "effectId", 0, 1, INT32_MAX);
			const auto effectType = body.value("effectType", std::string{});
			if (!effect || !EffectExists(*effect, effectType)) return fail("Pick the effect from the list");
			const auto interval = Number<int64_t>(body, "interval", 10, 3, 600);
			if (!interval) return fail("Every 3 to 600 seconds");
			config = { {"effectId", *effect}, {"effectType", effectType}, {"interval", *interval} };
			break;
		}
		}
		return config;
	}

	// Summary of the instances' reports: {instances, players, found, treasures, kills, spawned, ...}
	nlohmann::json Totals(const std::vector<ILiveOps::LiveEventInstance>& instances) {
		nlohmann::json totals{ {"instances", 0}, {"open", 0} };
		for (const auto& instance : instances) {
			const auto status = nlohmann::json::parse(instance.status, nullptr, false);
			if (!status.is_object()) continue;
			totals["instances"] = totals["instances"].get<int>() + 1;
			if (!status.value("closed", false)) totals["open"] = totals["open"].get<int>() + 1;
			for (const auto& [key, value] : status.items()) {
				if (!value.is_number_integer() || key == "players") continue;
				totals[key] = totals.value(key, int64_t{ 0 }) + value.get<int64_t>();
			}
		}
		return totals;
	}

	nlohmann::json EventJson(const Event& event, const std::vector<ILiveOps::LiveEventInstance>& instances, bool detail) {
		const auto type = LiveOpsRules::ParseType(event.type);
		nlohmann::json list = nlohmann::json::array();
		for (const auto& instance : instances) {
			if (instance.eventId != event.id) continue;
			auto status = nlohmann::json::parse(instance.status, nullptr, false);
			list.push_back({ {"zone", instance.zoneId}, {"instance", instance.instanceId}, {"status", status.is_object() ? status : nlohmann::json::object()},
				{"updatedAt", instance.updatedAt} });
		}
		std::vector<ILiveOps::LiveEventInstance> own;
		for (const auto& instance : instances) if (instance.eventId == event.id) own.push_back(instance);
		nlohmann::json scores = nlohmann::json::array();
		if (detail) {
			for (const auto& score : Database::Get()->GetLiveEventScores(event.id, 10)) {
				scores.push_back({ {"characterId", std::to_string(score.characterId)}, {"name", score.name}, {"amount", score.amount} });
			}
		}
		auto config = nlohmann::json::parse(event.config, nullptr, false);
		return { {"id", event.id}, {"type", event.type}, {"typeLabel", type ? TypeLabel(*type) : event.type}, {"title", event.title}, {"message", event.message},
			{"zones", event.zones}, {"zoneNames", ZoneList(event.zones)}, {"instanceId", event.instanceId}, {"config", config.is_object() ? config : nlohmann::json::object()},
			{"startsAt", event.startsAt}, {"endsAt", event.endsAt}, {"state", event.state == eState::ACTIVE ? "running" : event.state == eState::ENDED ? "ended" : "cancelled"},
			{"endedAt", event.endedAt}, {"endReason", event.endReason}, {"createdBy", event.createdBy}, {"endedBy", event.endedBy},
			{"instances", list}, {"totals", Totals(own)}, {"scores", scores} };
	}

	// What happened, for the end announcement: "3 of 10 treasures found. Most found: Bob (2)."
	std::string Summary(const Event& event) {
		const auto type = LiveOpsRules::ParseType(event.type);
		const auto totals = Totals(Database::Get()->GetLiveEventInstances({ event.id }));
		const auto scores = Database::Get()->GetLiveEventScores(event.id, 3);
		std::string text;
		if (type == eEventType::TREASURE_HUNT) text = std::to_string(totals.value("found", 0)) + " of " + std::to_string(totals.value("treasures", 0)) + " treasures found.";
		if (type == eEventType::INVASION) text = std::to_string(totals.value("kills", 0)) + " invaders smashed.";
		if (!scores.empty() && !scores.front().name.empty()) {
			text += (text.empty() ? "" : " ") + std::string(type == eEventType::INVASION ? "Top defenders: " : "Top finders: ");
			for (size_t i = 0; i < scores.size(); i++) text += (i ? ", " : "") + scores[i].name + " (" + std::to_string(scores[i].amount) + ")";
			text += ".";
		}
		return text;
	}

	void Finish(const Event& event, eState state, const HTTPContext& actor, const std::string& reason) {
		if (!Database::Get()->EndLiveEvent(event.id, state, Now(), actor.authenticatedUser, reason)) return;
		const auto summary = Summary(event);
		// Treasure hunts and invasions are announced over by each world when it stops them ("The treasure hunt is
		// over: 3 of 10 treasures were found here.", dGame/dUtilities/LiveEvents.cpp), with its own final count;
		// announcing here as well said it twice
		const auto type = LiveOpsRules::ParseType(event.type);
		const bool worldsAnnounce = type == eEventType::TREASURE_HUNT || type == eEventType::INVASION;
		if (!worldsAnnounce) {
			LiveWorld::Announce(event.title.empty() ? "Event over" : event.title, "The event is over. " + (summary.empty() ? std::string("Thanks for playing!") : summary), event.zones);
		}
		Audit(actor, "end_live_event", Describe(event) + ": " + reason + (summary.empty() ? "" : " " + summary));
		Alerts::Emit("server", "Live event ended", event.title + " (" + event.type + "): " + reason + ". " + summary, {}, "/live_events");
		BroadcastTableChanged("live_events", std::to_string(event.id));
		// Worlds end it on their own at its end time; an early end needs them told
		LiveEventRoutes::ReloadWorlds();
	}

	// Worlds running a zone now, for the page: which zones would pick an event up straight away
	nlohmann::json RunningWorlds();

	/**
	 * What an event is from a request: {type, title, message, zones, instance, config}, checked the way the worlds read
	 * it. False and error when something is wrong.
	 */
	bool ReadEvent(const nlohmann::json& body, Event& event, std::string& error) {
		const auto type = LiveOpsRules::ParseType(body.value("type", std::string{}));
		if (!type) { error = "Pick a kind of live event"; return false; }
		event.type = std::string(LiveOpsRules::TypeName(*type));
		event.title = body.value("title", std::string{});
		event.message = body.value("message", std::string{});
		if (event.title.empty() || event.title.size() > MAX_TITLE) { error = "The title is needed (up to 100 characters)"; return false; }
		if (event.message.size() > MAX_MESSAGE) { error = "The message is up to 300 characters"; return false; }
		event.zones.clear();
		if (body.contains("zones") && body["zones"].is_array()) {
			const auto& known = GameText::ZoneNames();
			for (const auto& zone : body["zones"]) {
				if (!zone.is_number_unsigned() || zone.get<uint32_t>() == 0 || !known.contains(std::to_string(zone.get<uint32_t>()))) { error = "Pick zones from the list"; return false; }
				if (std::find(event.zones.begin(), event.zones.end(), zone.get<uint32_t>()) == event.zones.end()) event.zones.push_back(zone.get<uint32_t>());
			}
		}
		if (LiveOpsRules::NeedsZone(*type) && event.zones.empty()) { error = "Pick the zones to run it in"; return false; }
		const auto instance = body.contains("instance") && body["instance"].is_number_integer() ? body["instance"].get<int64_t>() : -1;
		if (instance < -1 || instance > INT32_MAX) { error = "Invalid instance"; return false; }
		event.instanceId = static_cast<int32_t>(instance);
		const auto config = ReadConfig(*type, body.value("config", nlohmann::json::object()), error);
		if (!config) return false;
		event.config = config->dump();
		return true;
	}

	// Store, announce and tell the worlds; the message for staff
	std::string Launch(Event& event, const HTTPContext& actor) {
		const auto type = LiveOpsRules::ParseType(event.type);
		const auto minutes = (event.endsAt - event.startsAt + 59) / 60;
		event.createdBy = actor.authenticatedUser;
		event.id = Database::Get()->InsertLiveEvent(event);

		LiveEventRoutes::ReloadWorlds();
		const auto label = type ? TypeLabel(*type) : event.type;
		const bool announced = LiveWorld::Announce(event.title, event.message.empty() ? label + " for the next " + std::to_string(minutes) + " minutes!" : event.message, event.zones);
		Audit(actor, "start_live_event", Describe(event) + " for " + std::to_string(minutes) + " minutes: " + event.config);
		Alerts::Emit("server", "Live event started", event.title + " (" + event.type + ") in " + ZoneList(event.zones) + " for " + std::to_string(minutes) + " minutes", {}, "/live_events");
		BroadcastTableChanged("live_events", std::to_string(event.id));

		// Worlds of the zones running now pick it up at once; others when one starts
		size_t running = 0;
		for (const auto& world : RunningWorlds()) {
			if (event.instanceId >= 0 && world["instance"].get<int64_t>() != event.instanceId) continue; // only that instance runs it
			if (event.zones.empty() || std::find(event.zones.begin(), event.zones.end(), world["zone"].get<uint32_t>()) != event.zones.end()) running++;
		}
		return std::string(announced ? "Started" : "Started, but master isn't connected so it wasn't announced") +
			(running == 0 ? ". No world of those zones is running yet; it starts in each one that opens before it ends." : " in " + std::to_string(running) + " running world(s).");
	}

	nlohmann::json RunningWorlds() {
		nlohmann::json worlds = nlohmann::json::array();
		std::lock_guard lock(ServerState::g_StatusMutex);
		for (const auto& world : ServerState::g_WorldInstances) {
			if (world.mapID == 0 || world.cloneID != 0) continue;
			worlds.push_back({ {"zone", world.mapID}, {"instance", world.instanceID}, {"players", world.players} });
		}
		return worlds;
	}
}

namespace LiveEventRoutes {
	std::optional<nlohmann::json> CheckPart(const nlohmann::json& body, std::string& error) {
		if (!body.is_object()) { error = "The live event is an object"; return std::nullopt; }
		Event event;
		if (!ReadEvent(body, event, error)) return std::nullopt;
		return nlohmann::json{ {"type", event.type}, {"title", event.title}, {"message", event.message}, {"zones", event.zones}, {"instance", event.instanceId},
			{"config", nlohmann::json::parse(event.config)} };
	}

	std::optional<uint64_t> StartPart(const nlohmann::json& body, int64_t endsAt, const HTTPContext& actor, std::string& error, std::string& message) {
		Event event;
		if (!ReadEvent(body, event, error)) return std::nullopt;
		event.startsAt = Now();
		event.endsAt = std::min(endsAt, event.startsAt + LiveOpsRules::MAX_DURATION);
		if (event.endsAt - event.startsAt < LiveOpsRules::MIN_DURATION) { error = "Less than a minute is left of the event"; return std::nullopt; }
		message = Launch(event, actor);
		return event.id;
	}

	bool EndPart(uint64_t id, const HTTPContext& actor, const std::string& reason) {
		const auto event = Database::Get()->GetLiveEvent(id);
		if (!event || event->state != eState::ACTIVE) return false;
		Finish(*event, eState::CANCELLED, actor, reason);
		return true;
	}

	bool Running(uint64_t id) {
		const auto event = Database::Get()->GetLiveEvent(id);
		return event && event->state == eState::ACTIVE;
	}

	void ReloadWorlds() {
		PlayerActionRequest request;
		request.action = ePlayerAction::RELOAD_LIVE_OPS;
		PlayerActions::Request(request, 0, [](const PlayerActionResult& result) {
			return PlayerActions::Outcome{ true, "Reloaded in " + std::to_string(result.affected) + " world(s)" };
		});
	}

	void Update() {
		const auto steady = std::chrono::steady_clock::now();
		if (steady < g_NextCheck) return;
		g_NextCheck = steady + CHECK_INTERVAL;
		try {
			const auto now = Now();
			for (const auto& event : Database::Get()->GetLiveEvents(true, 200)) {
				if (now >= event.endsAt) Finish(event, eState::ENDED, SystemContext(), "Time is up");
			}
		} catch (const std::exception& ex) {
			LOG("Live events check failed: %s", ex.what());
		}
	}

	nlohmann::json PublicJson() {
		nlohmann::json events = nlohmann::json::array();
		const auto now = Now();
		for (const auto& event : Database::Get()->GetLiveEvents(true, 20)) {
			if (!LiveOpsRules::InWindow(event.startsAt, event.endsAt, now)) continue;
			const auto type = LiveOpsRules::ParseType(event.type);
			events.push_back({ {"title", event.title.empty() ? (type ? TypeLabel(*type) : event.type) : event.title}, {"type", event.type},
				{"where", ZoneList(event.zones)}, {"ends_at", event.endsAt} });
		}
		return events;
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/live_events", Perm("live_events_manage"), "Live events page",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "live_events.jinja2", "live_events"); });

		Route(eHTTPMethod::GET, "/api/live-events", Perm("live_events_manage"),
			"Running and recent live events with each world instance's progress and the top players, the zones and the worlds running now",
			[](HTTPReply& reply, const HTTPContext&) {
				const auto events = Database::Get()->GetLiveEvents(false, 40);
				std::vector<uint64_t> ids;
				for (const auto& event : events) ids.push_back(event.id);
				const auto instances = Database::Get()->GetLiveEventInstances(ids);
				nlohmann::json rows = nlohmann::json::array();
				for (const auto& event : events) rows.push_back(EventJson(event, instances, event.state == eState::ACTIVE || rows.size() < 10));
				JsonSuccess(reply, { {"events", rows}, {"zones", ZoneOptions()}, {"worlds", RunningWorlds()}, {"now", Now()},
					{"limits", { {"title", MAX_TITLE}, {"message", MAX_MESSAGE}, {"minMinutes", LiveOpsRules::MIN_DURATION / 60}, {"maxMinutes", LiveOpsRules::MAX_DURATION / 60},
						{"treasures", LiveOpsRules::MAX_TREASURES}, {"waves", LiveOpsRules::MAX_WAVES}, {"perWave", LiveOpsRules::MAX_PER_WAVE},
						{"multiplier", LiveOpsRules::MAX_MULTIPLIER} }} });
			});

		Route(eHTTPMethod::GET, "/api/live-events/objects", Perm("live_events_manage"),
			"Objects from the CDClient to pick for an event. Query: kind=treasure|enemy|item, q (name or LOT)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto kind = QueryValue(context.queryString, "kind");
				const auto query = QueryValue(context.queryString, "q");
				const auto& types = ObjectTypes(kind);
				std::string placeholders;
				for (size_t i = 0; i < types.size(); i++) placeholders += i ? ", ?" : "?";
				// IN, not a correlated EXISTS: ComponentsRegistry has no index, and the EXISTS form took seconds with the
				// bundled SQLite for the kinds with thousands of objects
				auto stmt = CDClientDatabase::CreatePreppedStmt(
					"SELECT o.id, o.name, o.displayName, o.type FROM Objects o WHERE o.type IN (" + placeholders + ") "
					"AND o.id IN (SELECT c.id FROM ComponentsRegistry c WHERE c.component_type = ?) "
					"AND (? = '' OR o.name LIKE '%' || ? || '%' OR o.displayName LIKE '%' || ? || '%' OR o.id = ?) ORDER BY o.name LIMIT 60;");
				int index = 1;
				for (const auto& type : types) stmt.bind(index++, type.c_str());
				stmt.bind(index++, static_cast<int>(eReplicaComponentType::RENDER));
				stmt.bind(index++, query.c_str());
				stmt.bind(index++, query.c_str());
				stmt.bind(index++, query.c_str());
				stmt.bind(index++, GeneralUtils::TryParse<int>(query).value_or(-1));
				auto result = stmt.execQuery();
				nlohmann::json objects = nlohmann::json::array();
				for (; !result.eof(); result.nextRow()) {
					const std::string displayName = result.getStringField("displayName", "");
					objects.push_back({ {"lot", result.getIntField("id")}, {"name", displayName.empty() ? result.getStringField("name", "") : displayName},
						{"type", result.getStringField("type", "")} });
				}
				JsonSuccess(reply, { {"objects", objects}, {"types", types} });
			});

		Route(eHTTPMethod::GET, "/api/live-events/effects", Perm("live_events_manage"),
			"Effects from the CDClient's BehaviorEffect table to play in a celebration. Query: q (name, type or ID)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto query = QueryValue(context.queryString, "q");
				auto stmt = CDClientDatabase::CreatePreppedStmt(
					"SELECT effectID, effectType, MIN(effectName) AS effectName FROM BehaviorEffect WHERE effectType IS NOT NULL AND effectType != '' "
					"AND (effectName LIKE '%' || ? || '%' OR effectType LIKE '%' || ? || '%' OR effectID = ?) GROUP BY effectID, effectType ORDER BY effectID LIMIT 60;");
				stmt.bind(1, query.c_str());
				stmt.bind(2, query.c_str());
				stmt.bind(3, GeneralUtils::TryParse<int>(query).value_or(-1));
				auto result = stmt.execQuery();
				nlohmann::json effects = nlohmann::json::array();
				for (; !result.eof(); result.nextRow()) {
					effects.push_back({ {"id", result.getIntField("effectID")}, {"type", result.getStringField("effectType", "")}, {"name", result.getStringField("effectName", "")} });
				}
				JsonSuccess(reply, { {"effects", effects} });
			});

		Route(eHTTPMethod::POST, "/api/live-events", Perm("live_events_manage"),
			"Start a live event now. Body: {type: treasure_hunt|bonus|invasion|celebration, title, message, zones: [zone IDs], instance (-1: every instance), "
			"minutes, config: {...per type}}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body || !body->is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				Event event;
				std::string error;
				if (!ReadEvent(*body, event, error)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
				const auto minutes = body->value("minutes", int64_t{ 0 });
				if (minutes * 60 < LiveOpsRules::MIN_DURATION || minutes * 60 > LiveOpsRules::MAX_DURATION) {
					return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "It can run from 1 minute to 7 days");
				}
				event.startsAt = Now();
				event.endsAt = event.startsAt + minutes * 60;
				const auto message = Launch(event, context);
				JsonSuccess(reply, { {"id", event.id}, {"message", message} });
			});

		Route(eHTTPMethod::POST, "/api/live-events/:id/end", Perm("live_events_manage"), "End a running live event now; worlds remove what it spawned",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<uint64_t>(context.path, 2);
				const auto event = id ? Database::Get()->GetLiveEvent(*id) : std::nullopt;
				if (!event) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Event not found");
				if (event->state != eState::ACTIVE) return JsonError(reply, eHTTPStatusCode::CONFLICT, "It has already ended");
				Finish(*event, eState::CANCELLED, context, "Ended early by " + context.authenticatedUser);
				JsonSuccess(reply, { {"message", "Ended"} });
			});
	}
}
