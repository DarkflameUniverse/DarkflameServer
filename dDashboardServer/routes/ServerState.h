#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <vector>
#include <string>
#include <cstdint>
#include "json.hpp"

struct ServerStatus {
	bool online{false};
	uint32_t players{0};
	std::string version{};
	std::chrono::steady_clock::time_point lastSeen{};
};

struct WorldInstanceInfo {
	uint32_t mapID{0};
	uint32_t instanceID{0};
	uint32_t cloneID{0};
	uint32_t players{0};
	std::string ip{};
	uint32_t port{0};
	bool isPrivate{false};
	std::string zoneName{};
	// For property instances (cloneID != 0), looked up once when the world is added
	std::string propertyId{};
	std::string propertyName{};
	std::string ownerId{};
	std::string ownerName{};
};

namespace ServerState {
	extern ServerStatus g_AuthStatus;
	extern ServerStatus g_ChatStatus;
	extern std::vector<WorldInstanceInfo> g_WorldInstances;
	extern std::mutex g_StatusMutex;

	inline nlohmann::json GetServerStateJson() {
		std::lock_guard lock(g_StatusMutex);

		nlohmann::json data;
		data["auth"]["online"] = g_AuthStatus.online;
		data["auth"]["players"] = g_AuthStatus.players;
		data["chat"]["online"] = g_ChatStatus.online;
		data["chat"]["players"] = g_ChatStatus.players;

		uint32_t totalOnlinePlayers = 0;
		data["worlds"] = nlohmann::json::array();
		for (const auto& world : g_WorldInstances) {
			totalOnlinePlayers += world.players;
			data["worlds"].push_back({
				{"mapID", world.mapID},
				{"instanceID", world.instanceID},
				{"cloneID", world.cloneID},
				{"players", world.players},
				{"isPrivate", world.isPrivate},
				{"zoneName", world.zoneName}
			});
		}

		data["stats"]["onlinePlayers"] = totalOnlinePlayers;
		data["stats"]["worlds"] = g_WorldInstances.size();
		return data;
	}

	/**
	 * The state as anyone signed in may see it (without players_view): no clone IDs or private flags, and property
	 * instances merged into one row per zone ({properties: n}), so nobody can tell who is on which property.
	 */
	inline nlohmann::json PlayerSafe(nlohmann::json state) {
		nlohmann::json worlds = nlohmann::json::array();
		std::map<uint32_t, size_t> propertyRows; // mapID -> index in worlds
		for (const auto& world : state.value("worlds", nlohmann::json::array())) {
			const auto mapID = world.value("mapID", 0u);
			if (world.value("cloneID", 0u) == 0) {
				worlds.push_back({ {"mapID", mapID}, {"instanceID", world.value("instanceID", 0u)}, {"players", world.value("players", 0u)},
					{"zoneName", world.value("zoneName", "")} });
				continue;
			}
			const auto it = propertyRows.find(mapID);
			if (it == propertyRows.end()) {
				propertyRows[mapID] = worlds.size();
				worlds.push_back({ {"mapID", mapID}, {"instanceID", 0}, {"players", world.value("players", 0u)},
					{"zoneName", world.value("zoneName", "")}, {"properties", 1} });
			} else {
				auto& row = worlds[it->second];
				row["players"] = row.value("players", 0u) + world.value("players", 0u);
				row["properties"] = row.value("properties", 0u) + 1;
			}
		}
		state["worlds"] = std::move(worlds);
		return state;
	}
}
