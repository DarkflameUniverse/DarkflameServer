#pragma once

#include <chrono>
#include <mutex>
#include <vector>
#include <string>
#include <cstdint>

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
	extern std::mutex g_StatusMutex;
}
