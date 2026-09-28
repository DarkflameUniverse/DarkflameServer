#include "LiveUpdateRoutes.h"

#include <chrono>
#include <ctime>

#include "Alerts.h"
#include "DashboardRoutes.h"
#include "Game.h"
#include "Logger.h"
#include "MasterPackets.h"
#include "Permissions.h"
#include "RouteUtils.h"
#include "Web.h"
#include "dServer.h"
#include "eHTTPMethod.h"
#include "master/LiveUpdate.h"

using namespace RouteUtils;

namespace {
	constexpr const char* TOPIC = "live_update";
	constexpr const char* PERMISSION = "server_live_update";

	LiveUpdateStatus g_Status;
	bool g_Asked = false;           // asked master for the status since connecting
	bool g_WasConnected = false;
	LiveUpdate::ePhase g_AlertedPhase = LiveUpdate::ePhase::IDLE;
	uint32_t g_AlertedUpdate = 0;

	nlohmann::json ToJson(const LiveUpdateStatus& status) {
		using namespace LiveUpdate;
		const auto& zones = ZoneNames();
		nlohmann::json units = nlohmann::json::array();
		size_t done = 0, failed = 0;
		for (const auto& unit : status.units) {
			if (IsFinished(unit.state)) done++;
			if (unit.state == eUnitState::FAILED) failed++;
			nlohmann::json row{ {"kind", KindName(unit.kind)}, {"state", StateName(unit.state)}, {"message", unit.message} };
			if (unit.kind == eUnitKind::WORLD) {
				const auto zone = std::to_string(unit.zoneId);
				row["zone"] = unit.zoneId;
				row["zoneName"] = unit.zoneId == 0 ? "Character Select" : zones.contains(zone) ? zones[zone].get<std::string>() : "Zone " + zone;
				row["instance"] = unit.instanceId;
				row["clone"] = unit.cloneId;
				row["replacement"] = unit.replacement;
				row["players"] = unit.players;
				row["moved"] = unit.moved;
				row["isPrivate"] = unit.isPrivate != 0;
			}
			units.push_back(std::move(row));
		}
		return {
			{"updateId", status.updateId}, {"phase", PhaseName(status.phase)}, {"running", !IsFinished(status.phase) && status.phase != ePhase::IDLE},
			{"startedAt", status.startedAt}, {"finishedAt", status.finishedAt}, {"by", status.by}, {"message", status.message},
			{"done", done}, {"failed", failed}, {"total", status.units.size()}, {"units", units}
		};
	}

	bool SendRequest(LiveUpdate::eAction action, int32_t warnSeconds, const std::string& by) {
		if (!Game::server || !Game::server->GetIsConnectedToMaster()) return false;
		LiveUpdateRequest request;
		request.action = action;
		request.warnSeconds = warnSeconds;
		request.requestedBy = by.substr(0, LiveUpdateRequest::MAX_BY);
		MasterPackets::SendToMaster(request);
		return true;
	}
}

namespace LiveUpdateRoutes {
	void HandleStatus(const LiveUpdateStatus& status) {
		g_Status = status;
		auto json = ToJson(status);
		Game::web.SendWSMessage(TOPIC, json);
		// Webhooks hear about the start and the end
		using LiveUpdate::ePhase;
		if (status.updateId != 0 && (status.updateId != g_AlertedUpdate || status.phase != g_AlertedPhase)) {
			const bool started = status.phase == ePhase::RUNNING && status.updateId != g_AlertedUpdate;
			const bool finished = LiveUpdate::IsFinished(status.phase);
			if (started) Alerts::Emit("server", "Live update started", "The servers are being moved onto the new build.", { { "By", status.by } }, "/#liveUpdate");
			// Not for an old one a restarted dashboard hears about
			const bool recent = status.finishedAt == 0 || std::time(nullptr) - status.finishedAt < 600;
			if (finished && g_AlertedPhase != status.phase && recent) {
				Alerts::Emit("server", std::string("Live update ") + LiveUpdate::PhaseName(status.phase), status.message, { { "By", status.by } }, "/#liveUpdate");
			}
			g_AlertedUpdate = status.updateId;
			g_AlertedPhase = status.phase;
		}
	}

	void Update() {
		const bool connected = Game::server && Game::server->GetIsConnectedToMaster();
		if (connected && !g_WasConnected) g_Asked = false;
		g_WasConnected = connected;
		if (!connected || g_Asked) return;
		g_Asked = SendRequest(LiveUpdate::eAction::STATUS, LiveUpdateRequest::DEFAULT_WARN, "dashboard");
	}

	nlohmann::json StatusJson() {
		return ToJson(g_Status);
	}

	void RegisterRoutes() {
		Game::web.RegisterWSSubscription(TOPIC, std::function<uint8_t()>([] { return Permissions::Level(PERMISSION); }), PERMISSION);

		Route(eHTTPMethod::GET, "/api/server/live_update", Perm(PERMISSION),
			"The last live update (or the running one): phase, who started it, and every server and world instance with its state (also pushed on the live_update socket topic)",
			[](HTTPReply& reply, const HTTPContext&) {
				JsonSuccess(reply, { {"liveUpdate", StatusJson()} });
			});

		Route(eHTTPMethod::POST, "/api/server/live_update", Perm(PERMISSION),
			"Start a live update onto the binaries on disk now: UGC, auth and chat restart, every world instance is replaced and its players moved, the dashboard restarts last. Body: {warnSeconds (0-300, optional)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				int32_t warn = LiveUpdateRequest::DEFAULT_WARN;
				if (body && body->contains("warnSeconds") && !(*body)["warnSeconds"].is_null()) {
					warn = body->value("warnSeconds", -1);
					if (warn < 0 || warn > InstanceMigrationRequest::MAX_WARN_SECONDS) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Warn players 0 to 300 seconds before they are moved");
				}
				if (!SendRequest(LiveUpdate::eAction::START, warn, context.authenticatedUser)) {
					return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Not connected to the master server");
				}
				Audit(context, "live_update", warn >= 0 ? "Started a live update (players warned " + std::to_string(warn) + " s before they are moved)" : "Started a live update");
				JsonSuccess(reply, { {"message", "Asked master to start a live update"} });
			});

		Route(eHTTPMethod::POST, "/api/server/live_update/cancel", Perm(PERMISSION),
			"Cancel the running live update: nothing new is started, what is under way finishes",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!SendRequest(LiveUpdate::eAction::CANCEL, LiveUpdateRequest::DEFAULT_WARN, context.authenticatedUser)) {
					return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Not connected to the master server");
				}
				Audit(context, "live_update_cancel", "Cancelled the live update");
				JsonSuccess(reply, { {"message", "Asked master to cancel the live update"} });
			});
	}
}
