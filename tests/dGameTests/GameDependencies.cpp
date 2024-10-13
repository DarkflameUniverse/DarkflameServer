#include "GameDependencies.h"

#include "TransportPeerInterface.h"

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dZoneManager* zoneManager = nullptr;
	dChatFilter* chatFilter = nullptr;
	dConfig* config = nullptr;
	std::mt19937 randomEngine;
	TransportPeerInterface* chatServer = nullptr;
	AssetManager* assetManager = nullptr;
	SystemAddress chatSysAddr;
	EntityManager* entityManager = nullptr;
	std::string projectVersion;
}
