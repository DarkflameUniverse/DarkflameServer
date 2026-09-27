#include "InstanceLoad.h"
#include "MasterPackets.h"
#include "InstanceLimits.h"
#include "DashboardRoutes.h"
#include "ServerState.h"

#include <chrono>
#include <ctime>
#include <map>

#include "RouteUtils.h"
#include "WSRoutes.h"
#include "BitStreamUtils.h"
#include "CDClientDatabase.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "MessageType/Master.h"
#include "ServiceType.h"
#include "dServer.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	constexpr auto SAMPLE_INTERVAL = std::chrono::minutes(1);
	std::chrono::steady_clock::time_point g_NextSample{};

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	// The client's caps per zone (ZoneTable population_soft_cap / population_hard_cap)
	const std::map<uint32_t, InstanceLimits::Caps>& ClientCaps() {
		static std::optional<std::map<uint32_t, InstanceLimits::Caps>> cache;
		if (cache) return *cache;
		cache.emplace();
		try {
			auto result = CDClientDatabase::ExecuteQuery("SELECT zoneID, population_soft_cap, population_hard_cap FROM ZoneTable;");
			for (; !result.eof(); result.nextRow()) {
				(*cache)[static_cast<uint32_t>(result.getIntField(0, 0))] = { static_cast<uint32_t>(result.getIntField(1, 8)), static_cast<uint32_t>(result.getIntField(2, 12)) };
			}
		} catch (const std::exception& ex) {
			LOG("Could not read the zone caps: %s", ex.what());
		}
		return *cache;
	}

	// As InstanceManager: zones missing from the ZoneTable get 8 and 12
	InstanceLimits::Caps ClientCapsOf(uint32_t zone) {
		const auto it = ClientCaps().find(zone);
		return it == ClientCaps().end() ? InstanceLimits::Caps{ 8, 12 } : it->second;
	}

	std::string ZoneName(uint32_t zone) {
		const auto& names = ZoneNames();
		const auto key = std::to_string(zone);
		return names.contains(key) ? names[key].get<std::string>() : "Zone " + key;
	}

	nlohmann::json OptionalJson(const std::optional<uint32_t>& value) {
		return value ? nlohmann::json(*value) : nlohmann::json(nullptr);
	}

	// Master reloads its settings and zone limits on CONFIG_RELOAD (and passes it on to every server)
	bool TellMaster() {
		if (!Game::server || !Game::server->GetIsConnectedToMaster()) return false;
		MasterPackets::SendToMaster(MasterPackets::ConfigReload());
		return true;
	}

	std::optional<uint32_t> ReadCap(const nlohmann::json& body, const char* key, bool& ok) {
		if (!body.contains(key) || body[key].is_null()) return std::nullopt;
		if (!body[key].is_number_unsigned()) { ok = false; return std::nullopt; }
		return body[key].get<uint32_t>();
	}
}

namespace InstanceLoad {
	void Update() {
		const auto now = std::chrono::steady_clock::now();
		// Like the health sample: the first waits a minute, for the worlds to report in
		if (g_NextSample == std::chrono::steady_clock::time_point{}) g_NextSample = now + SAMPLE_INTERVAL;
		if (now < g_NextSample) return;
		g_NextSample = now + SAMPLE_INTERVAL;
		std::vector<IServerHealth::InstanceSample> samples;
		{
			std::lock_guard lock(ServerState::g_StatusMutex);
			const auto time = Now();
			for (const auto& world : ServerState::g_WorldInstances) samples.push_back({ time, world.mapID, world.instanceID, world.cloneID, world.players });
		}
		if (samples.empty()) return;
		try {
			Database::Get()->InsertInstanceSamples(samples);
		} catch (const std::exception& ex) {
			LOG_DEBUG("Could not record instance samples: %s", ex.what());
		}
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/instances", Perm("health_view"), "Instance load page: players per instance over time and per-zone limits",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "instances.jinja2", "instances"); });

		Route(eHTTPMethod::GET, "/api/instances/load", Perm("health_view"),
			"Players per world instance over time. Query: ?range=24h|7d|30d, &zone= for one zone's instances. Without a zone: each zone's busiest instance, "
			"most instances at once and most players at once",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto range = QueryValue(context.queryString, "range");
				const auto zone = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "zone")).value_or(0);
				const int64_t span = range == "30d" ? 30 * 86400 : range == "7d" ? 7 * 86400 : 86400;
				const int64_t bucket = range == "30d" ? 4 * 3600 : range == "7d" ? 3600 : 300;
				const auto now = Now();
				const auto samples = Database::Get()->GetInstanceSamples(now - span, now, bucket, zone);
				nlohmann::json json{ {"from", now - span}, {"to", now}, {"bucket", bucket} };
				if (zone) {
					const auto caps = ClientCapsOf(zone);
					nlohmann::json points = nlohmann::json::array();
					for (const auto& s : samples) points.push_back({ {"time", s.time}, {"instance", s.instanceId}, {"clone", s.cloneId}, {"players", s.players} });
					json["zone"] = { {"id", zone}, {"name", ZoneName(zone)} };
					json["samples"] = points;
					json["clientCaps"] = { {"soft", caps.soft}, {"hard", caps.hard} };
				} else {
					struct Summary { uint32_t busiest{}; uint32_t instances{}; uint32_t players{}; };
					std::map<uint32_t, Summary> zones;
					std::map<std::pair<int64_t, uint32_t>, std::pair<uint32_t, uint32_t>> perBucket; // (time, zone) -> (instances, players)
					for (const auto& s : samples) {
						auto& summary = zones[s.zoneId];
						summary.busiest = std::max(summary.busiest, s.players);
						auto& bucketTotals = perBucket[{ s.time, s.zoneId }];
						bucketTotals.first++;
						bucketTotals.second += s.players;
					}
					for (const auto& [key, totals] : perBucket) {
						auto& summary = zones[key.second];
						summary.instances = std::max(summary.instances, totals.first);
						summary.players = std::max(summary.players, totals.second);
					}
					nlohmann::json list = nlohmann::json::array();
					for (const auto& [id, summary] : zones) {
						list.push_back({ {"id", id}, {"name", ZoneName(id)}, {"busiest", summary.busiest}, {"instances", summary.instances}, {"players", summary.players} });
					}
					json["zones"] = list;
				}
				JsonSuccess(reply, json);
			});

		Route(eHTTPMethod::GET, "/api/instances/limits", Perm("health_view"),
			"Per-zone player caps: the client's (ZoneTable), any set here, what new instances get, and spare instances to keep running; plus the running worlds",
			[](HTTPReply& reply, const HTTPContext&) {
				std::map<uint32_t, IServerOperations::ZoneLimit> limits;
				for (auto& limit : Database::Get()->GetZoneLimits()) limits[limit.zoneId] = std::move(limit);
				nlohmann::json zones = nlohmann::json::array();
				for (const auto& [id, caps] : ClientCaps()) {
					if (id == 0) continue;
					const auto it = limits.find(id);
					const auto* limit = it == limits.end() ? nullptr : &it->second;
					const auto effective = InstanceLimits::Effective(limit ? limit->softCap : std::nullopt, limit ? limit->hardCap : std::nullopt, caps);
					zones.push_back({ {"id", id}, {"name", ZoneName(id)}, {"clientSoft", caps.soft}, {"clientHard", caps.hard},
						{"softCap", limit ? OptionalJson(limit->softCap) : nullptr}, {"hardCap", limit ? OptionalJson(limit->hardCap) : nullptr},
						{"spare", limit ? limit->spareInstances : 0}, {"soft", effective.soft}, {"hard", effective.hard},
						{"updatedAt", limit ? limit->updatedAt : 0}, {"updatedBy", limit ? limit->updatedBy : ""} });
				}
				nlohmann::json worlds = nlohmann::json::array();
				{
					std::lock_guard lock(ServerState::g_StatusMutex);
					for (const auto& w : ServerState::g_WorldInstances) {
						worlds.push_back({ {"zone", w.mapID}, {"instance", w.instanceID}, {"clone", w.cloneID}, {"players", w.players}, {"isPrivate", w.isPrivate} });
					}
				}
				JsonSuccess(reply, { {"zones", zones}, {"worlds", worlds}, {"maxCap", InstanceLimits::MAX_CAP}, {"maxSpare", InstanceLimits::MAX_SPARE} });
			});

		Route(eHTTPMethod::POST, "/api/instances/limits", Perm("instances_manage"),
			"Set a zone's player caps and spare instances. Body: {zone, softCap, hardCap (null: the client's), spare}. Master applies them at once",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto zone = body->value("zone", 0u);
				if (!ClientCaps().contains(zone)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick a zone from the list");
				bool ok = true;
				IServerOperations::ZoneLimit limit;
				limit.zoneId = zone;
				limit.softCap = ReadCap(*body, "softCap", ok);
				limit.hardCap = ReadCap(*body, "hardCap", ok);
				limit.spareInstances = body->value("spare", 0u);
				if (!ok) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Caps are whole numbers, or empty for the client's");
				const auto client = ClientCapsOf(zone);
				if (const auto error = InstanceLimits::Validate(zone, limit.softCap, limit.hardCap, limit.spareInstances, client)) {
					return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
				}
				limit.updatedAt = Now();
				limit.updatedBy = context.authenticatedUser;
				const bool reset = !limit.softCap && !limit.hardCap && limit.spareInstances == 0;
				if (reset) Database::Get()->DeleteZoneLimit(zone);
				else Database::Get()->SetZoneLimit(limit);
				const auto effective = InstanceLimits::Effective(limit.softCap, limit.hardCap, client);
				const auto text = [](const std::optional<uint32_t>& value) { return value ? std::to_string(*value) : std::string("client's"); };
				Audit(context, "set_zone_limits", ZoneName(zone) + " (" + std::to_string(zone) + "): " + (reset ? std::string("back to the client's caps, no spare instances") :
					"soft cap " + text(limit.softCap) + ", hard cap " + text(limit.hardCap) + ", " + std::to_string(limit.spareInstances) + " spare instance(s)"));
				BroadcastTableChanged("zone_limits", std::to_string(zone));
				const bool told = TellMaster();
				JsonSuccess(reply, { {"message", told ? "Saved. New instances take " + std::to_string(effective.soft) + "/" + std::to_string(effective.hard) + " players."
					: "Saved. Master isn't connected; it applies them when it next starts."} });
			});
	}
}
