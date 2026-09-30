#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>

#include "json.hpp"
#include "TrafficHistory.h"

/**
 * What the Network page (and the `traffic` WebSocket topic) is sent, built from TrafficHistory. Pure (no database,
 * network or clock), so it is unit tested; Traffic.cpp serves it.
 */
namespace NetworkView {
	constexpr int64_t WINDOW = 5; // seconds the live rates are over (one report)

	// A server's name for people ("World 1200 Nimbus Station #3")
	using Labeler = std::function<std::string(const TrafficHistory::Server& server)>;

	// Where a server listens and the machine it runs on, from master's server list
	struct Endpoint {
		uint32_t port{};
		std::string host; // the machine's address as master sees it (empty: unknown)
	};
	// By TrafficHistory key ("master", "auth", "world:1200:3")
	using Endpoints = std::map<std::string, Endpoint>;

	/**
	 * Every server that reported in the last `onlineSeconds`, by key: its packets and bytes per second in and out over
	 * its last WINDOW reported seconds, HTTP requests, RakNet link statistics, gauges, and the split of the packets by
	 * peer (clients, master, servers) with its HTTP requests from and to other servers, or split: null when its reports
	 * don't have one (an older server). With `endpoints`, its listening `port` and its machine as `host`, a token
	 * (MaskAddress with `salt`; null when not known): the summary goes to every viewer, so it never holds addresses.
	 */
	nlohmann::json Summary(const TrafficHistory& history, int64_t now, int64_t onlineSeconds, const Labeler& label,
		const Endpoints& endpoints = {}, uint64_t salt = 0);

	/**
	 * The remote ends of every server (from each one's last report): game clients by player (else by RakNet connection),
	 * HTTP clients of the dashboard and the UGC server by address and signed-in account (`user`, the dashboard account
	 * the requests were signed in as), and the servers' own links. Rates per second. Account and user names are always
	 * included; without `showAddresses` each address is replaced by a token that stays the same for the same address
	 * and `salt`, and ports are left out. With `showAddresses`, `hosts` maps each machine's token in the Summary to its
	 * address (empty otherwise).
	 */
	nlohmann::json Connections(const TrafficHistory& history, int64_t now, int64_t onlineSeconds, bool showAddresses, uint64_t salt, const Labeler& label,
		const Endpoints& endpoints = {});

	// "peer-" and 8 hex digits: the same for the same address and salt, and says nothing about the address
	std::string MaskAddress(const std::string& address, uint64_t salt);

	// "clients", "master" or "servers"
	const char* PeerName(TrafficStats::Peer peer);
}
