#include "Performance.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <optional>

#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "MasterPackets.h"
#include "PerfHistory.h"
#include "Permissions.h"
#include "RouteUtils.h"
#include "ServiceType.h"
#include "Traffic.h"
#include "TrafficHistory.h"
#include "TrafficStats.h"
#include "Web.h"
#include "dServer.h"
#include "eHTTPMethod.h"
#include "eReplicaComponentType.h"
#include "magic_enum.hpp"
#include "master/Profiling.h"

using namespace RouteUtils;

namespace {
	constexpr const char* VIEW = "health_view";
	constexpr const char* RUN = "profiling_run";
	constexpr int64_t ONLINE_SECONDS = 20;
	constexpr uint32_t MAX_SECONDS = Profiler::Recorder::MAX_SESSION_MS / 1000;
	constexpr size_t SESSIONS_KEPT = 20;
	constexpr auto ANSWER_SLACK = std::chrono::seconds(20); // a session that hasn't answered this long after its end failed

	PerfHistory g_History;
	std::chrono::steady_clock::time_point g_NextForget{};

	struct Session {
		uint32_t id{};
		std::string server;
		std::string by;
		int64_t started{};
		uint32_t seconds{};
		std::string status; // requested, running, done, failed
		std::string error;
		std::optional<Profiler::Profile> profile;
		std::chrono::steady_clock::time_point deadline;
	};
	std::deque<Session> g_Sessions; // oldest first
	uint32_t g_NextId = static_cast<uint32_t>(TrafficStats::Now() & 0xFFFFF) << 8;

	// Scope names for people: packets and components by name
	std::string NodeLabel(const Profiler::Node& node) {
		if (node.name == Profiler::PACKET) {
			const auto names = Traffic::MessageNames(node.arg);
			const auto gameMessage = names.value("game_message", "");
			return "Packet " + (gameMessage.empty() ? names.value("service", "") + " " + names.value("packet", "") : gameMessage);
		}
		if (node.name == Profiler::COMPONENT) {
			const auto name = magic_enum::enum_name(static_cast<eReplicaComponentType>(node.arg));
			return "Component " + (name.empty() ? std::to_string(node.arg) : std::string(name));
		}
		return Profiler::DefaultLabel(node);
	}

	Session* FindSession(uint32_t id) {
		for (auto& session : g_Sessions) if (session.id == id) return &session;
		return nullptr;
	}

	nlohmann::json SessionJson(const Session& session, bool withProfile) {
		nlohmann::json out = { {"id", session.id}, {"server", session.server}, {"server_label", Traffic::Label(session.server)}, {"by", session.by},
			{"started", session.started}, {"seconds", session.seconds}, {"status", session.status}, {"error", session.error} };
		if (session.profile) {
			out["frames"] = session.profile->frames;
			out["duration_ms"] = session.profile->durationMs;
			out["total_ms"] = static_cast<double>(session.profile->totalUs) / 1000.0;
			if (withProfile) out["profile"] = PerfHistory::ProfileJson(*session.profile, NodeLabel);
		}
		return out;
	}

	void Send(const ProfileRequest& request) {
		// The dashboard profiles itself; everything else goes through master
		if (request.serverType == ServiceType::DASHBOARD) {
			if (Game::server) Game::server->HandleProfileRequest(request);
			return;
		}
		MasterPackets::SendToMaster(request);
	}

	nlohmann::json Overview(const std::string& requested, const std::string& range, int64_t now) {
		auto servers = g_History.ServersJson(now, 300, ONLINE_SECONDS, Traffic::Label);
		std::string key = requested;
		if (key.empty() || !g_History.Servers().contains(key)) {
			// The busiest server online
			key.clear();
			for (const auto& row : servers) {
				if (row["online"].get<bool>()) { key = row["key"].get<std::string>(); break; }
			}
		}
		const int64_t span = range == "1h" ? 3600 : 300, step = range == "1h" ? 10 : 1;
		const int64_t to = now - (now % step), from = to - span;
		nlohmann::json out = { {"range", range == "1h" ? "1h" : "5m"}, {"servers", servers}, {"server", key} };
		if (!key.empty()) {
			out["server_label"] = Traffic::Label(key);
			out["series"] = g_History.Series(key, from, to, step);
			out["worst"] = g_History.Worst(key, now - PerfHistory::RECENT_SECONDS, 10, NodeLabel);
			out["messages"] = g_History.Messages(key, now - 300, 15, Traffic::MessageNames);
		}
		return out;
	}
}

namespace Performance {
	void Ingest(const std::string& serverKey, const Profiler::Report& frames) {
		g_History.Ingest(serverKey, frames, TrafficStats::Now());
	}

	void IngestProfile(const ProfileResult& result) {
		auto* session = FindSession(result.sessionId);
		if (!session) return;
		switch (result.status) {
		case eProfileStatus::STARTED:
			if (session->status == "requested") session->status = "running";
			break;
		case eProfileStatus::DONE:
			session->status = "done";
			session->profile = result.profile;
			LOG("Profiling session %u of %s finished: %u frames", session->id, session->server.c_str(), result.profile.frames);
			break;
		case eProfileStatus::FAILED:
			session->status = "failed";
			session->error = result.error;
			break;
		}
	}

	void Update() {
		const auto now = std::chrono::steady_clock::now();
		if (now >= g_NextForget) {
			g_NextForget = now + std::chrono::seconds(15);
			g_History.Forget(TrafficStats::Now());
		}
		for (auto& session : g_Sessions) {
			if ((session.status == "requested" || session.status == "running") && now >= session.deadline) {
				session.status = "failed";
				session.error = "The server didn't answer";
			}
		}
	}

	void RegisterRoutes() {
		// The dashboard's own results stay here
		if (Game::server) Game::server->SetProfileSink([](ProfileResult& result) { IngestProfile(result); });

		Route(eHTTPMethod::GET, "/api/diagnostics/performance", Perm(VIEW),
			"Main loop frame timing. Query: ?server=world:1200:3 (default: the busiest server online) and ?range=5m|1h. Every server's frames per second, average, p95 and longest frame and how busy its loop was over 5 minutes; for the server: frame times and each phase's milliseconds per second, its longest frames of the last 10 minutes with their scopes, and the packet types that took longest to handle",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto server = QueryValue(context.queryString, "server");
				if (server.size() > 64) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "server is too long");
				auto out = Overview(server, QueryValue(context.queryString, "range"), TrafficStats::Now());
				out["can_profile"] = Can(context, RUN);
				JsonSuccess(reply, out);
			});

		Route(eHTTPMethod::GET, "/api/diagnostics/performance/slow", Perm(VIEW),
			"The last 50 slow frames of every server (over slow_frame_ms), newest first, with their phases and heaviest scopes. Query: ?server=<key> for one server",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto server = QueryValue(context.queryString, "server");
				JsonSuccess(reply, { {"frames", g_History.SlowJson(server, NodeLabel, Traffic::Label)} });
			});

		Route(eHTTPMethod::GET, "/api/diagnostics/performance/profiles", Perm(VIEW),
			"The last profiling sessions, newest first (without their trees)",
			[](HTTPReply& reply, const HTTPContext& context) {
				nlohmann::json sessions = nlohmann::json::array();
				for (auto it = g_Sessions.rbegin(); it != g_Sessions.rend(); ++it) sessions.push_back(SessionJson(*it, false));
				JsonSuccess(reply, { {"sessions", sessions}, {"can_profile", Can(context, RUN)}, {"max_seconds", MAX_SECONDS} });
			});

		Route(eHTTPMethod::GET, "/api/diagnostics/performance/profiles/:id", Perm(VIEW),
			"One profiling session with its merged scope tree (nodes in pre-order with depth, total and own microseconds) and its folded stacks",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<uint32_t>(context.path, 4);
				const auto* session = id ? FindSession(*id) : nullptr;
				if (!session) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such session");
				JsonSuccess(reply, { {"session", SessionJson(*session, true)} });
			});

		Route(eHTTPMethod::POST, "/api/diagnostics/performance/profiles", Perm(RUN),
			"Start a profiling session: {server: \"world:1200:3\", seconds: 1-60}. The server merges its main loop's scopes for that long; poll the session for the result",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body || !body->is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto server = body->value("server", "");
				uint16_t type{};
				uint32_t zoneId{}, instanceId{};
				if (!PerfHistory::ParseKey(server, type, zoneId, instanceId)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown server");
				const auto seconds = body->contains("seconds") && (*body)["seconds"].is_number_integer() ? (*body)["seconds"].get<int64_t>() : 10;
				if (seconds < 1 || seconds > MAX_SECONDS) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "seconds must be 1 to " + std::to_string(MAX_SECONDS));
				for (const auto& session : g_Sessions) {
					if (session.server == server && (session.status == "requested" || session.status == "running")) {
						return JsonError(reply, eHTTPStatusCode::CONFLICT, "A session of that server is running");
					}
				}
				Session session;
				session.id = ++g_NextId;
				session.server = server;
				session.by = context.authenticatedUser;
				session.started = TrafficStats::Now();
				session.seconds = static_cast<uint32_t>(seconds);
				session.status = "requested";
				session.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds) + ANSWER_SLACK;
				g_Sessions.push_back(session);
				if (g_Sessions.size() > SESSIONS_KEPT) g_Sessions.pop_front();

				ProfileRequest request;
				request.sessionId = session.id;
				request.serverType = static_cast<ServiceType>(type);
				request.zoneId = zoneId;
				request.instanceId = instanceId;
				request.durationMs = static_cast<uint32_t>(seconds) * 1000;
				Send(request);
				Audit(context, "profile_server", "Profiled " + Traffic::Label(server) + " for " + std::to_string(seconds) + " s");
				JsonSuccess(reply, { {"session", SessionJson(session, false)} });
			});

		Route(eHTTPMethod::POST, "/api/diagnostics/performance/profiles/:id/stop", Perm(RUN),
			"End a running profiling session early; the server sends what it has",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<uint32_t>(context.path, 4);
				const auto* session = id ? FindSession(*id) : nullptr;
				if (!session) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such session");
				if (session->status != "requested" && session->status != "running") return JsonError(reply, eHTTPStatusCode::CONFLICT, "The session isn't running");
				ProfileRequest request;
				uint16_t type{};
				PerfHistory::ParseKey(session->server, type, request.zoneId, request.instanceId);
				request.serverType = static_cast<ServiceType>(type);
				request.sessionId = session->id;
				request.stop = true;
				Send(request);
				JsonSuccess(reply);
			});
	}
}
