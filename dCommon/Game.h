#pragma once

#include <random>

class dServer;
class dLogger;
class InstanceManager;
class dpWorld;
class dChatFilter;
class dConfig;
class RakPeerInterface;
class AssetManager;
struct SystemAddress;

namespace Game {
	extern dLogger* logger;
	extern dServer* server;
	extern InstanceManager* im;
	extern dpWorld* physicsWorld;
	extern dChatFilter* chatFilter;
	extern dConfig* config;
	extern std::mt19937 randomEngine;
	extern RakPeerInterface* chatServer;
	extern AssetManager* assetManager;
	extern SystemAddress chatSysAddr;
}
