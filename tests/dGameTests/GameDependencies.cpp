#include "GameDependencies.h"

namespace Game {
	dServer* server;
	dZoneManager* zoneManager;
	dChatFilter* chatFilter;
	dConfig* config;
	std::mt19937 randomEngine;
	RakPeerInterface* chatServer;
	AssetManager* assetManager;
	SystemAddress chatSysAddr;
}
