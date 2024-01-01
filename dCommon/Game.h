#pragma once

#include <string>
#include <random>
#include <csignal>

class dServer;
class Logger;
class InstanceManager;
class dChatFilter;
class dConfig;
class RakPeerInterface;
class AssetManager;
struct SystemAddress;
class EntityManager;
class dZoneManager;
class PlayerContainer;

namespace Game {
	using signal_t = volatile std::sig_atomic_t;
	extern Logger* logger;
	extern dServer* server;
	extern InstanceManager* im;
	extern dChatFilter* chatFilter;
	extern dConfig* config;
	extern std::mt19937 randomEngine;
	extern RakPeerInterface* chatServer;
	extern AssetManager* assetManager;
	extern SystemAddress chatSysAddr;
	extern signal_t lastSignal;
	extern EntityManager* entityManager;
	extern dZoneManager* zoneManager;
	extern PlayerContainer playerContainer;
	extern std::string projectVersion;

	inline bool ShouldShutdown() {
		return lastSignal != 0;
	}
}
