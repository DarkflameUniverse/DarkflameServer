#pragma once

#include <random>

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

namespace Game {
	extern Logger* logger;
	extern dServer* server;
	extern InstanceManager* im;
	extern dChatFilter* chatFilter;
	extern dConfig* config;
	extern std::mt19937 randomEngine;
	extern RakPeerInterface* chatServer;
	extern AssetManager* assetManager;
	extern SystemAddress chatSysAddr;
	extern bool shouldShutdown;
	extern EntityManager* entityManager;
	extern dZoneManager* zoneManager;
}
