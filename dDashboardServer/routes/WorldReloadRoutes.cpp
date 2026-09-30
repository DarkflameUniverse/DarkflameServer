#include "WorldReloadRoutes.h"

#include "FdbSnapshot.h"
#include "Game.h"
#include "GameText.h"
#include "MasterPackets.h"
#include "Permissions.h"
#include "RouteUtils.h"
#include "Web.h"
#include "dServer.h"
#include "eHTTPMethod.h"
#include "master/WorldFiles.h"

using namespace RouteUtils;

namespace {
	constexpr const char* TOPIC = "world_files";
	constexpr const char* PERMISSION = "world_reload";

	WorldFilesStatus g_Status;
	bool g_Received = false;

	nlohmann::json ToJson(const WorldFilesStatus& status) {
		nlohmann::json zones = nlohmann::json::array();
		for (const auto& zone : status.zones) {
			nlohmann::json files = nlohmann::json::array();
			bool changed = false;
			for (const auto& file : zone.files) {
				changed = changed || file.changed;
				files.push_back({ {"kind", ZoneFileLog::KindName(file.disk.kind)}, {"path", file.disk.path}, {"packed", file.disk.packed},
					{"size", file.disk.size}, {"hash", FdbSnapshot::HashText(file.disk.hash)}, {"hashed", file.hashed},
					{"missing", file.missing}, {"changed", file.changed} });
			}
			nlohmann::json instances = nlohmann::json::array();
			for (const auto& instance : zone.instances) {
				instances.push_back({ {"instance", instance.instanceId}, {"clone", instance.cloneId}, {"players", instance.players},
					{"stale", instance.stale}, {"reloading", instance.reloading}, {"outdated", instance.outdated} });
			}
			zones.push_back({ {"zone", zone.zoneId}, {"zoneName", GameText::ZoneName(zone.zoneId)}, {"changed", changed},
				{"files", files}, {"instances", instances}, {"message", zone.message} });
		}
		return { {"received", g_Received}, {"watching", status.watching}, {"watchSeconds", status.watchSeconds},
			{"seamless", status.seamless}, {"zones", zones} };
	}
}

namespace WorldReloadRoutes {
	void HandleStatus(const WorldFilesStatus& status) {
		g_Status = status;
		g_Received = true;
		auto json = ToJson(status);
		Game::web.SendWSMessage(TOPIC, json);
	}

	nlohmann::json StatusJson() {
		return ToJson(g_Status);
	}

	void RegisterRoutes() {
		Game::web.RegisterWSSubscription(TOPIC, std::function<uint8_t()>([] { return Permissions::Level(PERMISSION); }), PERMISSION);

		Route(eHTTPMethod::GET, "/api/worlds/files", Perm(PERMISSION),
			"Each running zone's data files (.luz, .lvl, .lutriggers, terrain, navmesh) as its worlds loaded them, whether they changed on disk since, "
			"and which instances loaded an older version (also pushed on the world_files socket topic)",
			[](HTTPReply& reply, const HTTPContext&) {
				JsonSuccess(reply, { {"worldFiles", StatusJson()} });
			});

		Route(eHTTPMethod::POST, "/api/worlds/reload", Perm(PERMISSION),
			"Replace every instance of a zone with a new one on the zone files on disk now: players are moved over (the Mythran shift, or the "
			"seamless mode when world_reload_seamless=1), empty instances are stopped. Body: {zone, warnSeconds (0-300, optional)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				WorldReloadRequest request;
				request.zoneId = body->value("zone", 0u);
				if (request.zoneId == 0) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick a zone");
				if (body->contains("warnSeconds") && !(*body)["warnSeconds"].is_null()) {
					const int32_t warn = body->value("warnSeconds", -1);
					if (warn < 0 || warn > InstanceMigrationRequest::MAX_WARN_SECONDS) {
						return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Warn players 0 to 300 seconds before they are moved");
					}
					request.warnSeconds = static_cast<uint16_t>(warn);
				}
				if (!Game::server || !Game::server->GetIsConnectedToMaster()) return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Not connected to the master server");
				request.requestedBy = context.authenticatedUser.substr(0, WorldFiles::MAX_BY);
				MasterPackets::SendToMaster(request);
				Audit(context, "world_reload", "Reloaded zone " + std::to_string(request.zoneId) + " (players warned " + std::to_string(request.warnSeconds) + " s before they are moved)");
				JsonSuccess(reply, { {"message", "Asked master to reload the zone"} });
			});
	}
}
