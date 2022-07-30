#pragma once

#include <random>

class dServer;
class dLogger;
class InstanceManager;
class dpWorld;
class dChatFilter;
class dConfig;
class dLocale;
class RakPeerInterface;
struct SystemAddress;

namespace Game {
	extern dLogger* logger;
	extern dServer* server;
	extern InstanceManager* im;
	extern dpWorld* physicsWorld;
	extern dChatFilter* chatFilter;
	extern dConfig* config;
	extern dLocale* locale;
	extern std::mt19937 randomEngine;
	extern RakPeerInterface* chatServer;
	extern SystemAddress chatSysAddr;
}
