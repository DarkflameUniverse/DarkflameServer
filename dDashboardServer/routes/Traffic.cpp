#include "Traffic.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <random>

#include "Background.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "MessageIdentifiers.h"
#include "MetricsFormat.h"
#include "NetworkView.h"
#include "Performance.h"
#include "Permissions.h"
#include "RouteUtils.h"
#include "GameText.h"
#include "ServerState.h"
#include "ServiceType.h"
#include "TrafficHistory.h"
#include "TrafficStats.h"
#include "Web.h"
#include "Workers.h"
#include "dServer.h"
#include "eHTTPMethod.h"
#include "magic_enum.hpp"
#include "master/ServerTraffic.h"
#include "MessageType/Auth.h"
#include "MessageType/Chat.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "MessageType/Master.h"
#include "MessageType/Server.h"
#include "MessageType/World.h"

using namespace RouteUtils;

namespace {
	constexpr const char* PERMISSION = "health_view";
	constexpr const char* TOPIC = "traffic";
	constexpr const char* IPS_PERMISSION = "network_ips";
	constexpr auto PUSH_INTERVAL = std::chrono::seconds(2);
	constexpr auto WRITE_INTERVAL = std::chrono::seconds(15);
	constexpr int64_t ONLINE_SECONDS = 20; // a server that reported this recently is shown as up

	TrafficHistory g_History;
	bool g_Changed = false;
	std::chrono::steady_clock::time_point g_NextPush{};
	std::chrono::steady_clock::time_point g_NextWrite{};
	// Minutes the background writer couldn't take yet (it was still busy)
	std::vector<IServerTraffic::TrafficMinute> g_Unwritten;

	template<typename Enum>
	std::string EnumName(uint32_t value) {
		const auto name = magic_enum::enum_name(static_cast<Enum>(value));
		return name.empty() ? "#" + std::to_string(value) : std::string(name);
	}

	std::string RakNetName(uint32_t id) {
		switch (id) {
		case ID_REPLICA_MANAGER_CONSTRUCTION: return "REPLICA_CONSTRUCTION";
		case ID_REPLICA_MANAGER_SERIALIZE: return "REPLICA_SERIALIZE";
		case ID_REPLICA_MANAGER_DESTRUCTION: return "REPLICA_DESTRUCTION";
		case ID_NEW_INCOMING_CONNECTION: return "NEW_INCOMING_CONNECTION";
		case ID_CONNECTION_REQUEST_ACCEPTED: return "CONNECTION_REQUEST_ACCEPTED";
		case ID_DISCONNECTION_NOTIFICATION: return "DISCONNECTION_NOTIFICATION";
		case ID_CONNECTION_LOST: return "CONNECTION_LOST";
		default: return "RAKNET_" + std::to_string(id);
		}
	}

	// {service, packet, game message} names of a message key
	struct MessageNames {
		std::string service;
		std::string packet;
		std::string gameMessage; // empty unless a game message
	};

	MessageNames NamesOf(const TrafficStats::MessageKey& key) {
		if (key.service == TrafficStats::MessageKey::RAKNET) return { "RAKNET", RakNetName(key.packet), "" };
		MessageNames names;
		const auto service = static_cast<ServiceType>(key.service);
		const auto serviceName = magic_enum::enum_name(service);
		names.service = serviceName.empty() ? "SERVICE_" + std::to_string(key.service) : std::string(serviceName);
		switch (service) {
		case ServiceType::COMMON: names.packet = EnumName<MessageType::Server>(key.packet); break;
		case ServiceType::AUTH: names.packet = EnumName<MessageType::Auth>(key.packet); break;
		case ServiceType::CHAT: names.packet = EnumName<MessageType::Chat>(key.packet); break;
		case ServiceType::WORLD: names.packet = EnumName<MessageType::World>(key.packet); break;
		case ServiceType::CLIENT: names.packet = EnumName<MessageType::Client>(key.packet); break;
		case ServiceType::MASTER: names.packet = EnumName<MessageType::Master>(key.packet); break;
		default: names.packet = "#" + std::to_string(key.packet); break;
		}
		const bool gameMessage = (service == ServiceType::WORLD && key.packet == static_cast<uint32_t>(MessageType::World::GAME_MSG)) ||
			(service == ServiceType::CLIENT && key.packet == static_cast<uint32_t>(MessageType::Client::GAME_MSG));
		if (gameMessage) names.gameMessage = EnumName<MessageType::Game>(key.gameMessage);
		return names;
	}

	// "World 1100 Avant Gardens #3" for world:1100:3, else the key capitalised
	std::string LabelOf(const TrafficHistory::Server& server) {
		switch (static_cast<ServiceType>(server.type)) {
		case ServiceType::MASTER: return "Master";
		case ServiceType::AUTH: return "Auth";
		case ServiceType::CHAT: return "Chat";
		case ServiceType::DASHBOARD: return "Dashboard";
		case ServiceType::UGC: return "UGC";
		case ServiceType::WORLD: {
			// The zone's name from the client's locale, in the viewer's language (none for a zone it doesn't name)
			const auto& names = GameText::ZoneNames();
			const auto known = names.find(std::to_string(server.zoneId));
			const std::string zoneName = known != names.end() && known->is_string() ? known->get<std::string>() : "";
			return "World " + std::to_string(server.zoneId) + (zoneName.empty() ? "" : " " + zoneName) + " #" + std::to_string(server.instanceId);
		}
		default: return server.key;
		}
	}

	double Rate(uint64_t value, int64_t seconds) {
		return std::round(static_cast<double>(value) / static_cast<double>(std::max<int64_t>(seconds, 1)) * 100.0) / 100.0;
	}

	double Millis(uint64_t microseconds) {
		return std::round(static_cast<double>(microseconds) / 10.0) / 100.0;
	}

	// One server's chart lines: per-second rates of each bucket, latency in ms
	struct Lines {
		nlohmann::json packetsIn = nlohmann::json::array(), packetsOut = nlohmann::json::array(), bytesIn = nlohmann::json::array(), bytesOut = nlohmann::json::array();
		nlohmann::json http = nlohmann::json::array(), httpErrors = nlohmann::json::array(), p50 = nlohmann::json::array(), p95 = nlohmann::json::array(), p99 = nlohmann::json::array();
		bool anyHttp = false;

		void Add(uint64_t pin, uint64_t pout, uint64_t bin, uint64_t bout, uint64_t requests, uint64_t errors, double lat50, double lat95, double lat99, int64_t step) {
			packetsIn.push_back(Rate(pin, step));
			packetsOut.push_back(Rate(pout, step));
			bytesIn.push_back(Rate(bin, step));
			bytesOut.push_back(Rate(bout, step));
			http.push_back(Rate(requests, step));
			httpErrors.push_back(Rate(errors, step));
			p50.push_back(requests ? nlohmann::json(lat50) : nlohmann::json(nullptr));
			p95.push_back(requests ? nlohmann::json(lat95) : nlohmann::json(nullptr));
			p99.push_back(requests ? nlohmann::json(lat99) : nlohmann::json(nullptr));
			anyHttp = anyHttp || requests > 0;
		}

		nlohmann::json Json() const {
			nlohmann::json out = { {"packets_in", packetsIn}, {"packets_out", packetsOut}, {"bytes_in", bytesIn}, {"bytes_out", bytesOut} };
			if (anyHttp) {
				out["http"] = http;
				out["http_errors"] = httpErrors;
				out["p50"] = p50;
				out["p95"] = p95;
				out["p99"] = p99;
			}
			return out;
		}
	};

	nlohmann::json ServerInfo(const std::string& key, int64_t now) {
		const auto& servers = g_History.Servers();
		const auto it = servers.find(key);
		nlohmann::json info = { {"key", key}, {"label", key}, {"online", false} };
		if (it == servers.end()) {
			// Only in the database (e.g. a world that has since closed)
			if (key.starts_with("world:")) info["label"] = "World " + key.substr(6);
			return info;
		}
		const auto& server = it->second;
		info["label"] = LabelOf(server);
		info["type"] = std::string(magic_enum::enum_name(static_cast<ServiceType>(server.type)));
		info["online"] = now - server.lastSeen <= ONLINE_SECONDS;
		info["last_seen"] = server.lastSeen;
		info["link"] = { {"connections", server.link.connections}, {"ping_ms", server.link.averagePingMs}, {"resend_queue", server.link.resendQueue},
			{"resends", server.link.resends}, {"datagrams_sent", server.link.datagramsSent}, {"datagrams_received", server.link.datagramsReceived} };
		nlohmann::json gauges = nlohmann::json::object();
		for (const auto& [name, value] : server.gauges) gauges[name] = value;
		info["gauges"] = gauges;
		return info;
	}

	// The chart data of a range: live (5m) and 1h from memory, 24h and 7d from the database plus the minutes not written yet
	nlohmann::json Series(const std::string& range, int64_t now) {
		int64_t span = 300, step = 1;
		if (range == "1h") { span = 3600; step = 10; }
		else if (range == "24h") { span = 86400; step = 300; }
		else if (range == "7d") { span = 7 * 86400; step = 1800; }
		const int64_t to = now - (now % step);        // the current bucket isn't complete yet
		const int64_t from = to - span;
		const auto count = static_cast<size_t>(span / step);

		nlohmann::json times = nlohmann::json::array();
		for (size_t i = 0; i < count; i++) times.push_back(from + static_cast<int64_t>(i) * step);

		std::map<std::string, Lines> lines;
		if (step < 60) {
			for (const auto& [key, points] : g_History.Buckets(from, to, step)) {
				auto& line = lines[key];
				for (const auto& p : points) {
					const auto latency = p.Latency();
					line.Add(p.packetsIn, p.packetsOut, p.bytesIn, p.bytesOut, p.httpRequests, p.http4xx + p.http5xx,
						Millis(latency.Percentile(0.5)), Millis(latency.Percentile(0.95)), Millis(latency.Percentile(0.99)), step);
				}
			}
		} else {
			// Database rows per bucket and server, then the minutes still in memory
			std::map<std::string, std::vector<IServerTraffic::TrafficMinute>> buckets;
			const auto add = [&](const IServerTraffic::TrafficMinute& row) {
				if (row.time < from || row.time >= to + step) return;
				auto& list = buckets[row.server];
				if (list.empty()) {
					list.resize(count);
					for (size_t i = 0; i < count; i++) list[i].time = from + static_cast<int64_t>(i) * step;
				}
				const auto index = std::min(count - 1, static_cast<size_t>((row.time - from) / step));
				auto& b = list[index];
				b.packetsIn += row.packetsIn; b.packetsOut += row.packetsOut; b.bytesIn += row.bytesIn; b.bytesOut += row.bytesOut;
				b.httpRequests += row.httpRequests; b.http4xx += row.http4xx; b.http5xx += row.http5xx;
				// Several minutes in one bucket: the worst minute's percentiles
				b.latencyP50Us = std::max(b.latencyP50Us, row.latencyP50Us);
				b.latencyP95Us = std::max(b.latencyP95Us, row.latencyP95Us);
				b.latencyP99Us = std::max(b.latencyP99Us, row.latencyP99Us);
			};
			try {
				for (const auto& row : Database::Get()->GetTrafficMinutes(from, to + step, step)) add(row);
			} catch (const std::exception& ex) {
				LOG_DEBUG("Traffic: reading server_traffic failed: %s", ex.what());
			}
			for (const auto& row : g_Unwritten) add(row);
			for (const auto& [key, server] : g_History.Servers()) {
				for (const auto& minute : server.minutes) {
					const auto latency = minute.point.Latency();
					add({ .time = minute.point.time, .server = key, .packetsIn = minute.point.packetsIn, .packetsOut = minute.point.packetsOut,
						.bytesIn = minute.point.bytesIn, .bytesOut = minute.point.bytesOut, .httpRequests = minute.point.httpRequests,
						.http4xx = minute.point.http4xx, .http5xx = minute.point.http5xx,
						.latencyP50Us = static_cast<uint32_t>(latency.Percentile(0.5)), .latencyP95Us = static_cast<uint32_t>(latency.Percentile(0.95)),
						.latencyP99Us = static_cast<uint32_t>(latency.Percentile(0.99)) });
				}
			}
			for (const auto& [key, list] : buckets) {
				auto& line = lines[key];
				for (const auto& b : list) {
					line.Add(b.packetsIn, b.packetsOut, b.bytesIn, b.bytesOut, b.httpRequests, b.http4xx + b.http5xx,
						Millis(b.latencyP50Us), Millis(b.latencyP95Us), Millis(b.latencyP99Us), step);
				}
			}
		}

		nlohmann::json servers = nlohmann::json::array();
		for (const auto& [key, line] : lines) {
			auto info = ServerInfo(key, now);
			info["series"] = line.Json();
			servers.push_back(std::move(info));
		}
		// Servers that are up but had nothing in the range yet
		for (const auto& [key, server] : g_History.Servers()) {
			if (!lines.contains(key) && now - server.lastSeen <= ONLINE_SECONDS) servers.push_back(ServerInfo(key, now));
		}

		const int64_t since = step < 60 ? from : std::max<int64_t>(from, now - 86400);
		nlohmann::json messages = nlohmann::json::array();
		for (const auto& count : g_History.TopMessages("", since, 15)) {
			const auto names = NamesOf(count.key);
			messages.push_back({ {"direction", count.key.outbound ? "out" : "in"}, {"service", names.service}, {"packet", names.packet},
				{"game_message", names.gameMessage}, {"count", count.count}, {"bytes", count.bytes} });
		}
		nlohmann::json routes = nlohmann::json::array();
		for (const auto& [server, stats] : g_History.Routes(std::max<int64_t>(from, now - 3600))) {
			if (routes.size() >= 30) break;
			routes.push_back({ {"server", server}, {"route", stats.route}, {"count", stats.count}, {"status_4xx", stats.status[3]}, {"status_5xx", stats.status[4]},
				{"bytes", stats.bytesOut}, {"p50", Millis(stats.latency.Percentile(0.5))}, {"p95", Millis(stats.latency.Percentile(0.95))},
				{"p99", Millis(stats.latency.Percentile(0.99))} });
		}
		return { {"range", range}, {"from", from}, {"to", to}, {"step", step}, {"times", times}, {"servers", servers}, {"messages", messages},
			{"messages_since", since}, {"routes", routes} };
	}

	// What open pages get every few seconds (the Network page draws it; Diagnostics reloads on it)
	nlohmann::json Summary(int64_t now) {
		return NetworkView::Summary(g_History, now, ONLINE_SECONDS, LabelOf);
	}

	uint64_t AddressSalt() {
		static const uint64_t salt = [] {
			std::random_device device;
			return (static_cast<uint64_t>(device()) << 32) ^ device();
		}();
		return salt;
	}

	// One server for the Network page's detail panel: what ServerInfo says, its busiest message types each way over the
	// last 5 minutes, and its packets and bytes per second over the last 10 minutes (5 second steps)
	nlohmann::json NetworkServer(const std::string& key, int64_t now) {
		auto info = ServerInfo(key, now);
		nlohmann::json messages = nlohmann::json::array();
		for (const auto& count : g_History.TopMessages(key, now - 300, 8)) {
			const auto names = NamesOf(count.key);
			messages.push_back({ {"direction", count.key.outbound ? "out" : "in"}, {"service", names.service}, {"packet", names.packet},
				{"game_message", names.gameMessage}, {"count", count.count}, {"bytes", count.bytes} });
		}
		info["messages"] = messages;
		constexpr int64_t STEP = 5, SPAN = 600;
		const int64_t to = now - (now % STEP), from = to - SPAN;
		nlohmann::json times = nlohmann::json::array(), packetsIn = nlohmann::json::array(), packetsOut = nlohmann::json::array(),
			bytesIn = nlohmann::json::array(), bytesOut = nlohmann::json::array();
		const auto buckets = g_History.Buckets(from, to, STEP);
		const auto it = buckets.find(key);
		for (int64_t i = 0; i < SPAN / STEP; i++) {
			times.push_back(from + i * STEP);
			const auto* p = it != buckets.end() ? &it->second[static_cast<size_t>(i)] : nullptr;
			packetsIn.push_back(p ? Rate(p->packetsIn, STEP) : 0.0);
			packetsOut.push_back(p ? Rate(p->packetsOut, STEP) : 0.0);
			bytesIn.push_back(p ? Rate(p->bytesIn, STEP) : 0.0);
			bytesOut.push_back(p ? Rate(p->bytesOut, STEP) : 0.0);
		}
		info["series"] = { {"step", STEP}, {"times", times}, {"packets_in", packetsIn}, {"packets_out", packetsOut}, {"bytes_in", bytesIn}, {"bytes_out", bytesOut} };
		return info;
	}

	void Write() {
		auto rows = g_History.TakeFinishedMinutes(TrafficStats::Now());
		g_Unwritten.insert(g_Unwritten.end(), rows.begin(), rows.end());
		if (g_Unwritten.empty()) return;
		// A day of a busy universe at most, should the database be away for long
		if (g_Unwritten.size() > 100000) g_Unwritten.erase(g_Unwritten.begin(), g_Unwritten.begin() + static_cast<std::ptrdiff_t>(g_Unwritten.size() - 100000));
		auto batch = std::move(g_Unwritten);
		g_Unwritten.clear();
		const bool queued = Background::Run("traffic_minutes", [batch](GameDatabase& db) -> nlohmann::json {
			db.InsertTrafficMinutes(batch);
			return nlohmann::json::object();
		}, [](nlohmann::json, const std::string& error) {
			if (!error.empty()) LOG("Traffic: writing minutes failed: %s", error.c_str());
		});
		if (!queued) g_Unwritten = std::move(batch); // still writing the last batch: next time
	}
}

namespace Traffic {
	nlohmann::json Server(const std::string& key) {
		return ServerInfo(key, TrafficStats::Now());
	}

	void Ingest(const ServerTraffic& report) {
		g_History.Ingest(static_cast<uint16_t>(report.serverType), report.zoneId, report.instanceId, report.report, TrafficStats::Now());
		g_Changed = true;
		Performance::Ingest(TrafficHistory::KeyFor(static_cast<uint16_t>(report.serverType), report.zoneId, report.instanceId), report.frames);
	}

	std::string Label(const std::string& key) {
		const auto& servers = g_History.Servers();
		const auto it = servers.find(key);
		if (it != servers.end()) return LabelOf(it->second);
		if (key.starts_with("world:")) return "World " + key.substr(6);
		if (!key.empty()) return std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])))) + key.substr(1);
		return key;
	}

	nlohmann::json MessageNames(uint64_t packedKey) {
		const auto names = NamesOf(TrafficStats::MessageKey::Unpack(packedKey));
		return { {"service", names.service}, {"packet", names.packet}, {"game_message", names.gameMessage} };
	}

	void Update() {
		const auto now = std::chrono::steady_clock::now();
		if (now >= g_NextWrite) {
			g_NextWrite = now + WRITE_INTERVAL;
			Write();
			g_History.Forget(TrafficStats::Now());
		}
		if (g_Changed && now >= g_NextPush) {
			g_NextPush = now + PUSH_INTERVAL;
			g_Changed = false;
			auto summary = Summary(TrafficStats::Now());
			Web::SendWSMessage(TOPIC, summary);
		}
	}

	void AddMetrics(MetricsFormat::Writer& w) {
		using MetricsFormat::Labels;
		w.Declare("darkflame_net_packets_total", "Packets a server sent and received (LU packets and RakNet messages), since the dashboard started", "counter");
		w.Declare("darkflame_net_bytes_total", "Bytes of the packets a server sent and received, since the dashboard started", "counter");
		w.Declare("darkflame_net_messages_total", "Packets by type (the busiest types each server reports), since the dashboard started", "counter");
		w.Declare("darkflame_net_datagrams_total", "UDP datagrams (acknowledgements and resends included, as RakNet counts them), since the dashboard started", "counter");
		w.Declare("darkflame_net_resends_total", "Messages RakNet had to send again, since the dashboard started", "counter");
		w.Declare("darkflame_net_connections", "Open RakNet connections of a server", "gauge");
		w.Declare("darkflame_net_ping_milliseconds", "Average ping over a server's connections", "gauge");
		w.Declare("darkflame_http_requests_total", "HTTP requests answered, by route pattern and status class, since the dashboard started", "counter");
		w.Declare("darkflame_http_response_bytes_total", "HTTP response bodies sent, since the dashboard started", "counter");
		w.Declare("darkflame_http_request_duration_seconds", "HTTP request latency (deferred requests until their answer went out)", "histogram");
		const auto now = TrafficStats::Now();
		for (const auto& [key, server] : g_History.Servers()) {
			const auto& t = server.totals;
			const auto labels = [&key](std::initializer_list<std::pair<std::string, std::string>> more) {
				Labels out{ {"server", key} };
				out.insert(out.end(), more.begin(), more.end());
				return out;
			};
			w.Add("darkflame_net_packets_total", "", "counter", labels({ {"direction", "in"} }), static_cast<double>(t.packetsIn));
			w.Add("darkflame_net_packets_total", "", "counter", labels({ {"direction", "out"} }), static_cast<double>(t.packetsOut));
			w.Add("darkflame_net_bytes_total", "", "counter", labels({ {"direction", "in"} }), static_cast<double>(t.bytesIn));
			w.Add("darkflame_net_bytes_total", "", "counter", labels({ {"direction", "out"} }), static_cast<double>(t.bytesOut));
			w.Add("darkflame_net_datagrams_total", "", "counter", labels({ {"direction", "in"} }), static_cast<double>(t.datagramsReceived));
			w.Add("darkflame_net_datagrams_total", "", "counter", labels({ {"direction", "out"} }), static_cast<double>(t.datagramsSent));
			w.Add("darkflame_net_resends_total", "", "counter", labels({}), static_cast<double>(t.resends));
			for (const auto& [_, count] : t.messages) {
				const auto names = NamesOf(count.key);
				w.Add("darkflame_net_messages_total", "", "counter", labels({ {"direction", count.key.outbound ? "out" : "in"}, {"service", names.service},
					{"message", names.gameMessage.empty() ? names.packet : names.gameMessage} }), static_cast<double>(count.count));
			}
			if (now - server.lastSeen <= ONLINE_SECONDS) {
				w.Add("darkflame_net_connections", "", "gauge", labels({}), server.link.connections);
				w.Add("darkflame_net_ping_milliseconds", "", "gauge", labels({}), server.link.averagePingMs);
				for (const auto& [name, value] : server.gauges) {
					w.Add("darkflame_server_" + name, "Reported by a server with its traffic (see docs/Dashboard.md, Traffic diagnostics)", "gauge", labels({}), value);
				}
			}
			static constexpr const char* CLASSES[] = { "1xx", "2xx", "3xx", "4xx", "5xx" };
			for (const auto& [route, stats] : t.routes) {
				for (size_t i = 0; i < 5; i++) {
					if (stats.status[i]) w.Add("darkflame_http_requests_total", "", "counter", labels({ {"route", route}, {"status", CLASSES[i]} }), static_cast<double>(stats.status[i]));
				}
				w.Add("darkflame_http_response_bytes_total", "", "counter", labels({ {"route", route} }), static_cast<double>(stats.bytesOut));
				// Buckets at every doubling from 0.1 ms (every third histogram bucket), cumulative
				uint64_t cumulative = 0;
				const std::string family = "darkflame_http_request_duration_seconds";
				for (size_t i = 0; i + 1 < TrafficStats::Histogram::BUCKETS; i++) {
					cumulative += stats.latency.At(i);
					if (i % 3 != 0) continue;
					w.AddSample(family, family + "_bucket", "", "histogram", labels({ {"route", route}, {"le", MetricsFormat::Value(static_cast<double>(TrafficStats::Histogram::UpperBound(i)) / 1e6)} }), static_cast<double>(cumulative));
				}
				w.AddSample(family, family + "_bucket", "", "histogram", labels({ {"route", route}, {"le", "+Inf"} }), static_cast<double>(stats.latency.Count()));
				w.AddSample(family, family + "_sum", "", "histogram", labels({ {"route", route} }), static_cast<double>(stats.latency.Sum()) / 1e6);
				w.AddSample(family, family + "_count", "", "histogram", labels({ {"route", route} }), static_cast<double>(stats.latency.Count()));
			}
		}
	}

	void RegisterRoutes() {
		Game::web.RegisterWSSubscription(TOPIC, std::function<uint8_t()>([] { return Permissions::Level(PERMISSION); }), PERMISSION);

		// The dashboard's own report stays here; the worker pool is what its deferred requests wait for
		Game::server->SetTrafficSink([](ServerTraffic& report) { Ingest(report); });
		TrafficStats::Local().SetGauge("workers_busy", [] { return static_cast<double>(Workers::Pool().Active()); });
		TrafficStats::Local().SetGauge("workers_queued", [] { return static_cast<double>(Workers::Pool().Queued()); });
		TrafficStats::Local().SetGauge("workers_threads", [] { return static_cast<double>(Workers::Pool().Threads()); });

		Route(eHTTPMethod::GET, "/api/diagnostics/network", Perm(PERMISSION),
			"The Network page's live view: every reporting server's packets and bytes per second over its last report, its split by peer (clients, master, other servers; null for servers too old to report one), HTTP requests, RakNet link statistics and gauges",
			[](HTTPReply& reply, const HTTPContext&) {
				JsonSuccess(reply, Summary(TrafficStats::Now()));
			});

		Route(eHTTPMethod::GET, "/api/diagnostics/network/server", Perm(PERMISSION),
			"One server for the Network page. Query: ?key=world:1200:3 (or master, auth, chat, dashboard, ugc). Its link statistics and gauges, busiest message types over 5 minutes, packets and bytes per second over 10 minutes",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto key = QueryValue(context.queryString, "key");
				if (key.empty() || key.size() > 64) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "key is required");
				JsonSuccess(reply, NetworkServer(key, TrafficStats::Now()));
			});

		Route(eHTTPMethod::GET, "/api/diagnostics/network/connections", Perm(PERMISSION),
			"Every server's remote ends from its last report, grouped by address (game clients' RakNet connections, HTTP clients, server links), with rates, ping and the logged-in player. Addresses only with network_ips; otherwise a token per address",
			[](HTTPReply& reply, const HTTPContext& context) {
				JsonSuccess(reply, NetworkView::Connections(g_History, TrafficStats::Now(), ONLINE_SECONDS, Can(context, IPS_PERMISSION), AddressSalt(), LabelOf));
			});

		Route(eHTTPMethod::GET, "/api/diagnostics/traffic", Perm(PERMISSION),
			"Packets, bytes and HTTP requests per second of every server. Query: ?range=5m|1h|24h|7d. Series are per-second rates of each step, latency in ms",
			[](HTTPReply& reply, const HTTPContext& context) {
				auto range = QueryValue(context.queryString, "range");
				if (range != "1h" && range != "24h" && range != "7d") range = "5m";
				JsonSuccess(reply, Series(range, TrafficStats::Now()));
			});
	}
}
