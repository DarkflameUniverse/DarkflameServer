#include "NetworkView.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

#include "ServiceType.h"
#include "magic_enum.hpp"

namespace {
	double Rate(uint64_t value, int64_t seconds) {
		return std::round(static_cast<double>(value) / static_cast<double>(std::max<int64_t>(seconds, 1)) * 100.0) / 100.0;
	}

	nlohmann::json Rates(uint64_t packetsIn, uint64_t packetsOut, uint64_t bytesIn, uint64_t bytesOut, int64_t seconds) {
		return { {"packets_in", Rate(packetsIn, seconds)}, {"packets_out", Rate(packetsOut, seconds)},
			{"bytes_in", Rate(bytesIn, seconds)}, {"bytes_out", Rate(bytesOut, seconds)} };
	}

	nlohmann::json Rates(const TrafficStats::PeerCounts& c, int64_t seconds) {
		return Rates(c.packetsIn, c.packetsOut, c.bytesIn, c.bytesOut, seconds);
	}

	nlohmann::json Rates(const TrafficStats::Connection& c, int64_t seconds) {
		return Rates(c.packetsIn, c.packetsOut, c.bytesIn, c.bytesOut, seconds);
	}
}

namespace NetworkView {
	const char* PeerName(TrafficStats::Peer peer) {
		switch (peer) {
		case TrafficStats::Peer::MASTER: return "master";
		case TrafficStats::Peer::SERVERS: return "servers";
		default: return "clients";
		}
	}

	std::string MaskAddress(const std::string& address, uint64_t salt) {
		// FNV-1a over the salt's bytes, then the address
		uint64_t hash = 14695981039346656037ull;
		const auto mix = [&hash](uint8_t byte) { hash = (hash ^ byte) * 1099511628211ull; };
		for (int i = 0; i < 8; i++) mix(static_cast<uint8_t>(salt >> (8 * i)));
		for (const char c : address) mix(static_cast<uint8_t>(c));
		char out[16]{};
		std::snprintf(out, sizeof(out), "peer-%08x", static_cast<uint32_t>(hash ^ (hash >> 32)));
		return out;
	}

	nlohmann::json Summary(const TrafficHistory& history, int64_t now, int64_t onlineSeconds, const Labeler& label) {
		nlohmann::json servers = nlohmann::json::object();
		for (const auto& [key, server] : history.Servers()) {
			if (now - server.lastSeen > onlineSeconds || server.seconds.empty()) continue;
			const auto last = server.seconds.back().time;
			const auto first = last - (WINDOW - 1);
			TrafficHistory::Point p;
			for (auto it = server.seconds.rbegin(); it != server.seconds.rend() && it->time >= first; ++it) p.Add(*it);

			nlohmann::json entry = Rates(p.packetsIn, p.packetsOut, p.bytesIn, p.bytesOut, WINDOW);
			entry["key"] = key;
			entry["label"] = label ? label(server) : key;
			entry["type"] = std::string(magic_enum::enum_name(static_cast<ServiceType>(server.type)));
			entry["zone"] = server.zoneId;
			entry["instance"] = server.instanceId;
			entry["http"] = Rate(p.httpRequests, WINDOW);
			entry["http_bytes_out"] = Rate(p.httpBytesOut, WINDOW);
			entry["link"] = { {"connections", server.link.connections}, {"ping_ms", server.link.averagePingMs}, {"resends", server.link.resends},
				{"resend_queue", server.link.resendQueue} };
			nlohmann::json gauges = nlohmann::json::object();
			for (const auto& [name, value] : server.gauges) gauges[name] = value;
			entry["gauges"] = gauges;

			TrafficHistory::Split split;
			if (server.peerSplit && history.SplitOf(key, first, last + 1, split)) {
				entry["split"] = {
					{"clients", Rates(split.peers[static_cast<size_t>(TrafficStats::Peer::CLIENTS)], WINDOW)},
					{"master", Rates(split.peers[static_cast<size_t>(TrafficStats::Peer::MASTER)], WINDOW)},
					{"servers", Rates(split.peers[static_cast<size_t>(TrafficStats::Peer::SERVERS)], WINDOW)},
					{"http_from_servers", Rate(split.httpFromServers, WINDOW)},
					{"http_from_servers_bytes_out", Rate(split.httpFromServersBytesOut, WINDOW)},
					{"http_out", Rate(split.httpOutRequests, WINDOW)},
					{"http_out_bytes_in", Rate(split.httpOutBytesIn, WINDOW)} };
			} else {
				entry["split"] = nullptr;
			}
			servers[key] = std::move(entry);
		}
		return { {"time", now}, {"window", WINDOW}, {"servers", servers} };
	}

	nlohmann::json Connections(const TrafficHistory& history, int64_t now, int64_t onlineSeconds, bool showAddresses, uint64_t salt, const Labeler& label) {
		struct Group {
			std::string address;
			std::string kind = "server";
			std::string account, character;
			uint32_t accountId{};
			uint64_t characterId{};
			uint64_t bytes{};
			nlohmann::json servers = nlohmann::json::array();
			TrafficStats::Connection total;
			int64_t seconds = 1;
		};
		const auto rank = [](const std::string& kind) { return kind == "game" ? 2 : kind == "web" ? 1 : 0; };
		std::map<std::string, Group> groups;
		nlohmann::json others = nlohmann::json::array();
		bool anyReported = false;
		for (const auto& [key, server] : history.Servers()) {
			if (now - server.lastSeen > onlineSeconds || !server.hasConnections) continue;
			anyReported = true;
			const auto name = label ? label(server) : key;
			const auto seconds = std::max<int64_t>(server.connectionsSeconds, 1);
			for (const auto& c : server.connections) {
				auto& group = groups[c.address];
				group.address = c.address;
				group.seconds = std::max(group.seconds, seconds);
				const std::string kind = c.peer != TrafficStats::Peer::CLIENTS ? "server" : c.http ? "web" : "game";
				if (rank(kind) > rank(group.kind)) group.kind = kind;
				if (c.accountId && !group.accountId) {
					group.accountId = c.accountId;
					group.account = c.account;
					group.characterId = c.characterId;
					group.character = c.character;
				}
				group.bytes += c.Bytes();
				group.total.Merge(c);
				auto entry = Rates(c, seconds);
				entry["server"] = key;
				entry["label"] = name;
				entry["peer"] = PeerName(c.peer);
				entry["http"] = c.http;
				if (!c.http) {
					entry["port"] = showAddresses ? nlohmann::json(c.port) : nlohmann::json(nullptr);
					entry["ping_ms"] = c.pingMs;
					entry["resends"] = c.resends;
				}
				group.servers.push_back(std::move(entry));
			}
			if (server.otherConnectionCount) {
				auto entry = Rates(server.otherConnections, seconds);
				entry["server"] = key;
				entry["label"] = name;
				entry["count"] = server.otherConnectionCount;
				others.push_back(std::move(entry));
			}
		}

		std::vector<Group*> sorted;
		for (auto& [_, group] : groups) sorted.push_back(&group);
		std::sort(sorted.begin(), sorted.end(), [](const Group* a, const Group* b) { return a->bytes != b->bytes ? a->bytes > b->bytes : a->address < b->address; });
		nlohmann::json list = nlohmann::json::array();
		for (const auto* group : sorted) {
			nlohmann::json entry = Rates(group->total, group->seconds);
			entry["address"] = showAddresses ? group->address : MaskAddress(group->address, salt);
			entry["kind"] = group->kind;
			entry["servers"] = group->servers;
			if (group->accountId) {
				entry["account_id"] = group->accountId;
				entry["account"] = group->account;
				if (group->characterId) {
					entry["character_id"] = std::to_string(group->characterId); // 64-bit: a string for JavaScript
					entry["character"] = group->character;
				}
			}
			list.push_back(std::move(entry));
		}
		return { {"time", now}, {"addresses_shown", showAddresses}, {"reported", anyReported}, {"peers", list}, {"others", others} };
	}
}
