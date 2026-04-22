#ifndef PROJECT_VERSION
#define PROJECT_VERSION "dev"
#endif
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

//DLU Includes:
#include "dCommonVars.h"
#include "dServer.h"
#include "Logger.h"
#include "Database.h"
#include "dConfig.h"
#include "Diagnostics.h"
#include "AssetManager.h"
#include "BinaryPathFinder.h"
#include "ServiceType.h"
#include "StringifiedEnum.h"

#include "Game.h"
#include "Server.h"

//RakNet includes:
#include "RakNetDefines.h"
#include "MessageIdentifiers.h"

#include "MessageType/Server.h"

#include "DashboardWeb.h"
#include "DashboardShared.h"

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dConfig* config = nullptr;
	AssetManager* assetManager = nullptr;
	Game::signal_t lastSignal = 0;
	std::mt19937 randomEngine;
}

// Forward declaration
void HandlePacket(Packet* packet);

int main(int argc, char** argv) {
	constexpr uint32_t dashboardFramerate = mediumFramerate;
	constexpr uint32_t dashboardFrameDelta = mediumFrameDelta;
	Diagnostics::SetProcessName("Dashboard");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);
	
	Game::config = new dConfig("dashboardconfig.ini");

	//Create all the objects we need to run our service:
	Server::SetupLogger("DashboardServer");
	if (!Game::logger) return EXIT_FAILURE;
	Game::config->LogSettings();

	//Read our config:

	LOG("Starting Dashboard server...");
	LOG("Version: %s", PROJECT_VERSION);
	LOG("Compiled on: %s", __TIMESTAMP__);

	try {
		std::string clientPathStr = Game::config->GetValue("client_location");
		if (clientPathStr.empty()) clientPathStr = "./res";
		std::filesystem::path clientPath = std::filesystem::path(clientPathStr);
		if (clientPath.is_relative()) {
			clientPath = BinaryPathFinder::GetBinaryDir() / clientPath;
		}

		Game::assetManager = new AssetManager(clientPath);
	} catch (std::runtime_error& ex) {
		LOG("Got an error while setting up assets: %s", ex.what());
		delete Game::logger;
		delete Game::config;
		return EXIT_FAILURE;
	}

	//Connect to the Database
	try {
		Database::Connect();
	} catch (std::exception& ex) {
		LOG("Got an error while connecting to the database: %s", ex.what());
		Database::Destroy("DashboardServer");
		delete Game::logger;
		delete Game::config;
		return EXIT_FAILURE;
	}

	// Setup and start the Crow web server (runs in its own thread)
	const uint32_t web_server_port = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("web_server_port")).value_or(8080);
	DashboardWeb::Initialize(web_server_port);

	//Find out the master's IP:
	std::string masterIP;
	uint32_t masterPort = 1000;
	std::string masterPassword;
	auto masterInfo = Database::Get()->GetMasterInfo();
	if (masterInfo) {
		masterIP = masterInfo->ip;
		masterPort = masterInfo->port;
		masterPassword = masterInfo->password;
	}

	//It's safe to pass 'localhost' here, as the IP is only used as the external IP.
	std::string ourIP = "localhost";
	const uint32_t maxClients = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("max_clients")).value_or(999);
	const uint32_t ourPort = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("dashboard_server_port")).value_or(2006);
	const auto externalIPString = Game::config->GetValue("external_ip");
	if (!externalIPString.empty()) ourIP = externalIPString;

	Game::server = new dServer(ourIP, ourPort, 0, maxClients, false, true, Game::logger, masterIP, masterPort, ServiceType::COMMON, Game::config, &Game::lastSignal, masterPassword);

	// Update shared state with master server info
	DashboardShared::g_Stats.SetMasterInfo(masterIP, masterPort);

	Game::randomEngine = std::mt19937(time(0));

	//Run it until server gets a kill message from Master:
	auto t = std::chrono::high_resolution_clock::now();
	Packet* packet = nullptr;
	constexpr uint32_t logFlushTime = 30 * dashboardFramerate; // 30 seconds in frames
	constexpr uint32_t sqlPingTime = 10 * 60 * dashboardFramerate; // 10 minutes in frames
	uint32_t framesSinceLastFlush = 0;
	uint32_t framesSinceMasterDisconnect = 0;
	uint32_t framesSinceLastSQLPing = 0;

	auto lastTime = std::chrono::high_resolution_clock::now();
	auto startTime = lastTime; // Track server start time for uptime

	Game::logger->Flush(); // once immediately before main loop
	while (!Game::ShouldShutdown()) {
		// Check if we're still connected to master:
		if (!Game::server->GetIsConnectedToMaster()) {
			framesSinceMasterDisconnect++;

			if (framesSinceMasterDisconnect >= dashboardFramerate)
				break; //Exit our loop, shut down.
			
			DashboardShared::SetMasterConnected(false);
		} else {
			framesSinceMasterDisconnect = 0;
			DashboardShared::SetMasterConnected(true);
		}

		const auto currentTime = std::chrono::high_resolution_clock::now();
		const float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
		lastTime = currentTime;

		// Check for packets from master:
		Game::server->ReceiveFromMaster();
		
		// Process queued packet sends from Crow threads
		if (DashboardShared::g_PacketQueue.HasPending()) {
			auto pendingPackets = DashboardShared::g_PacketQueue.DequeueAll();
			for (const auto& request : pendingPackets) {
				// Create BitStream from queued data
				RakNet::BitStream bitStream(const_cast<unsigned char*>(request.data.data()), request.data.size(), false);
				
				// Send via RakNet (safe - we're in the RakNet thread)
				Game::server->Send(bitStream, request.target, request.broadcast);
				DashboardShared::OnPacketSent();
				
				LOG("Sent queued packet from web request (%zu bytes)", request.data.size());
			}
		}
		
		// Check for RakNet packets:
		packet = Game::server->Receive();
		if (packet) {
			HandlePacket(packet);
			DashboardShared::OnPacketReceived(); // Update shared stats
			Game::server->DeallocatePacket(packet);
			packet = nullptr;
		}

		//Push our log every 30s:
		if (framesSinceLastFlush >= logFlushTime) {
			Game::logger->Flush();
			framesSinceLastFlush = 0;
		} else framesSinceLastFlush++;

		//Every 10 min we ping our sql server to keep it alive hopefully:
		if (framesSinceLastSQLPing >= sqlPingTime) {
			//Find out the master's IP for absolutely no reason:
			std::string masterIP;
			uint32_t masterPort;

			auto masterInfo = Database::Get()->GetMasterInfo();
			if (masterInfo) {
				masterIP = masterInfo->ip;
				masterPort = masterInfo->port;
			}

			framesSinceLastSQLPing = 0;
		} else framesSinceLastSQLPing++;

		//Sleep our thread since dashboard can afford to.
		t += std::chrono::milliseconds(dashboardFrameDelta);
		std::this_thread::sleep_until(t);
	}

	// Stop the Crow web server
	DashboardWeb::Stop();

	//Delete our objects here:
	Database::Destroy("DashboardServer");
	delete Game::server;
	delete Game::logger;
	delete Game::config;

	return EXIT_SUCCESS;
}

void HandlePacket(Packet* packet) {
	if (packet->length < 4) return;

	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		LOG("A client has disconnected");
		DashboardShared::OnClientDisconnected();
		return;
	}

	if (packet->data[0] == ID_NEW_INCOMING_CONNECTION) {
		LOG("New incoming connection from %s", packet->systemAddress.ToString());
		DashboardShared::OnClientConnected();
		return;
	}

	if (packet->data[0] != ID_USER_PACKET_ENUM) return;

	// Handle server packets
	if (static_cast<ServiceType>(packet->data[1]) == ServiceType::COMMON) {
		if (static_cast<MessageType::Server>(packet->data[3]) == MessageType::Server::VERSION_CONFIRM) {
			LOG("Version confirmation received from client");
			DashboardShared::OnPacketReceived("VERSION_CONFIRM");
		}
	}

	// Add more packet handling as needed
	// This is where you would handle custom dashboard-specific packets
	// All packet handling can safely update DashboardShared state
}
