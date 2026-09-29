#pragma once

#include <cstdint>
#include <functional>
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

	/**
	 * Every server that reported in the last `onlineSeconds`, by key: its packets and bytes per second in and out over
	 * its last WINDOW reported seconds, HTTP requests, RakNet link statistics, gauges, and the split of the packets by
	 * peer (clients, master, servers) with its HTTP requests from and to other servers, or split: null when its reports
	 * don't have one (an older server). Never holds addresses.
	 */
	nlohmann::json Summary(const TrafficHistory& history, int64_t now, int64_t onlineSeconds, const Labeler& label);

	/**
	 * The remote ends of every server (from each one's last report) grouped by address: game clients by their RakNet
	 * connections, HTTP clients of the dashboard and the UGC server, and the servers' own links. Rates per second.
	 * Without `showAddresses` each address is replaced by a token that stays the same for the same address and `salt`.
	 */
	nlohmann::json Connections(const TrafficHistory& history, int64_t now, int64_t onlineSeconds, bool showAddresses, uint64_t salt, const Labeler& label);

	// "peer-" and 8 hex digits: the same for the same address and salt, and says nothing about the address
	std::string MaskAddress(const std::string& address, uint64_t salt);

	// "clients", "master" or "servers"
	const char* PeerName(TrafficStats::Peer peer);
}
