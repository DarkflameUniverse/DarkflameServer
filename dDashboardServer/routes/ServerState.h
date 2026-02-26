#pragma once

#include <chrono>
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
};

namespace ServerState {
	extern ServerStatus g_AuthStatus;
	extern ServerStatus g_ChatStatus;
	extern std::vector<WorldInstanceInfo> g_WorldInstances;

	// Helper function to get all server state as JSON
	inline nlohmann::json GetServerStateJson() {
		nlohmann::json data;
		data["auth"]["online"] = g_AuthStatus.online;
		data["auth"]["players"] = g_AuthStatus.players;
		data["chat"]["online"] = g_ChatStatus.online;
		data["chat"]["players"] = g_ChatStatus.players;

		data["worlds"] = nlohmann::json::array();
		for (const auto& world : g_WorldInstances) {
			data["worlds"].push_back({
				{"mapID", world.mapID},
				{"instanceID", world.instanceID},
				{"cloneID", world.cloneID},
				{"players", world.players},
				{"isPrivate", world.isPrivate}
			});
		}

		return data;
	}
}
