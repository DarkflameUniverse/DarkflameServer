#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <csignal>
#include <memory>

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "Database.h"
#include "dConfig.h"
#include "Logger.h"
#include "dServer.h"
#include "AssetManager.h"
#include "BinaryPathFinder.h"
#include "ServiceType.h"
#include "MessageType/Master.h"
#include "Game.h"
#include "BitStreamUtils.h"
#include "Diagnostics.h"
#include "Web.h"
#include "Server.h"

#include "ServerState.h"
#include "APIRoutes.h"
#include "StaticRoutes.h"
#include "DashboardRoutes.h"
#include "WSRoutes.h"
#include "AuthRoutes.h"
#include "AuthMiddleware.h"

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dConfig* config = nullptr;
	Game::signal_t lastSignal = 0;
	std::mt19937 randomEngine;
}

// Define global server state
namespace ServerState {
	ServerStatus g_AuthStatus{};
	ServerStatus g_ChatStatus{};
	std::vector<WorldInstanceInfo> g_WorldInstances{};
	std::mutex g_StatusMutex{};
}

namespace {
	dServer* g_Server = nullptr;
	bool g_RequestedServerList = false;
}

int main(int argc, char** argv) {
	Diagnostics::SetProduceMemoryDump(true);
	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);

	uint32_t maxClients = 999;
	uint32_t ourPort = 2006;
	std::string ourIP = "127.0.0.1";

	// Read config
	Game::config = new dConfig("dashboardconfig.ini");

	// Setup logger
	Server::SetupLogger("DashboardServer");
	if (!Game::logger) return EXIT_FAILURE;
	Game::config->LogSettings();

	LOG("Starting Dashboard Server");

	// Load settings
	if (Game::config->GetValue("max_clients") != "") 
		maxClients = std::stoi(Game::config->GetValue("max_clients"));
	
	if (Game::config->GetValue("port") != "") 
		ourPort = std::atoi(Game::config->GetValue("port").c_str());
	
	if (Game::config->GetValue("listen_ip") != "") 
		ourIP = Game::config->GetValue("listen_ip");

	// Connect to CDClient database
	try {
		const std::string cdclientPath = BinaryPathFinder::GetBinaryDir() / "resServer/CDServer.sqlite";
		CDClientDatabase::Connect(cdclientPath);
	} catch (std::exception& ex) {
		LOG("Failed to connect to CDClient database: %s", ex.what());
		return EXIT_FAILURE;
	}

	// Connect to the database
	try {
		Database::Connect();
	} catch (std::exception& ex) {
		LOG("Failed to connect to the database: %s", ex.what());
		return EXIT_FAILURE;
	}

	// Get master info from database
	std::string masterIP = "localhost";
	uint32_t masterPort = 1000;
	std::string masterPassword;
	auto masterInfo = Database::Get()->GetMasterInfo();
	if (masterInfo) {
		masterIP = masterInfo->ip;
		masterPort = masterInfo->port;
		masterPassword = masterInfo->password;
	}

	// Setup network server for communicating with Master
	g_Server = new dServer(
		masterIP,
		ourPort,
		0,
		maxClients,
		false,
		false,
		Game::logger,
		masterIP,
		masterPort,
		ServiceType::DASHBOARD, // Connect as dashboard to master
		Game::config,
		&Game::lastSignal,
		masterPassword
	);

	// Initialize web server
	if (!Game::web.Startup(ourIP, ourPort)) {
		LOG("Failed to start web server on %s:%d", ourIP.c_str(), ourPort);
		return EXIT_FAILURE;
	}

	// Register global middleware
	Game::web.AddGlobalMiddleware(std::make_shared<AuthMiddleware>());

	// Register routes in order: API, Static, Auth, WebSocket, Dashboard (dashboard MUST be last)
	RegisterAPIRoutes();
	RegisterStaticRoutes();
	RegisterAuthRoutes();
	RegisterWSRoutes();
	RegisterDashboardRoutes(); // Must be last - catches all unmatched routes

	LOG("Dashboard Server started successfully on %s:%d", ourIP.c_str(), ourPort);
	LOG("Connected to Master Server at %s:%d", masterIP.c_str(), masterPort);

	// Main loop
	auto lastTime = std::chrono::high_resolution_clock::now();
	auto lastBroadcast = lastTime;
	auto currentTime = lastTime;
	constexpr float deltaTime = 1.0f / 60.0f; // 60 FPS
	constexpr float broadcastInterval = 2000.0f; // Broadcast every 2 seconds

	while (!Game::ShouldShutdown()) {
		currentTime = std::chrono::high_resolution_clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count();
		const auto elapsedSinceBroadcast = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastBroadcast).count();

		if (elapsed >= 1000.0f / 60.0f) {
			// // Handle master server packets
			// Packet* packet = g_Server->ReceiveFromMaster();
			// if (packet) {
			// 	RakNet::BitStream bitStream(packet->data, packet->length, false);
			// 	PacketHandler::HandlePacket(bitStream, packet->systemAddress);
			// 	g_Server->DeallocateMasterPacket(packet);
			// }

			// // Handle RakNet protocol packets from connected servers
			// packet = g_Server->Receive();
			// while (packet) {
			// 	RakNet::BitStream bitStream(packet->data, packet->length, false);
			// 	PacketHandler::HandlePacket(bitStream, packet->systemAddress);
			// 	g_Server->DeallocatePacket(packet);
			// 	packet = g_Server->Receive();
			// }

			// Handle web requests
			Game::web.ReceiveRequests();

			// Broadcast dashboard updates periodically
			if (elapsedSinceBroadcast >= broadcastInterval) {
				BroadcastDashboardUpdate();
				lastBroadcast = currentTime;
			}

			lastTime = currentTime;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// Cleanup
	Database::Destroy("DashboardServer");
	delete g_Server;
	g_Server = nullptr;
	delete Game::logger;
	Game::logger = nullptr;
	delete Game::config;
	Game::config = nullptr;

	return EXIT_SUCCESS;
}


