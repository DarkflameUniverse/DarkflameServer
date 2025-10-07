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

#include "DashboardWeb.h"

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dConfig* config = nullptr;
	AssetManager* assetManager = nullptr;
	Game::signal_t lastSignal = 0;
	std::mt19937 randomEngine;
}

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

	// setup the chat api web server
	const uint32_t web_server_port = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("web_server_port")).value_or(80);
	if (!Game::web.Startup("localhost", web_server_port)) {
		// if we want the web server and it fails to start, exit
		LOG("Failed to start web server, shutting down.");
		Database::Destroy("DashboardServer");
		delete Game::logger;
		delete Game::config;
		return EXIT_FAILURE;
	}

	DashboardWeb::RegisterRoutes();

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

	Game::logger->Flush(); // once immediately before main loop
	while (!Game::ShouldShutdown()) {
		//Check if we're still connected to master:
		if (!Game::server->GetIsConnectedToMaster()) {
			framesSinceMasterDisconnect++;

			if (framesSinceMasterDisconnect >= dashboardFramerate)
				break; //Exit our loop, shut down.
		} else framesSinceMasterDisconnect = 0;

		const auto currentTime = std::chrono::high_resolution_clock::now();
		const float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
		lastTime = currentTime;

		// Check and handle web requests:
		Game::web.ReceiveRequests();

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

		//Sleep our thread since auth can afford to.
		t += std::chrono::milliseconds(dashboardFrameDelta); //Chat can run at a lower "fps"
		std::this_thread::sleep_until(t);
	}

	//Delete our objects here:
	Database::Destroy("DashboardServer");
	delete Game::server;
	delete Game::logger;
	delete Game::config;

	return EXIT_SUCCESS;
}
