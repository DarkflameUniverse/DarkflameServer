#include <iostream>
#include <string>
#include <ctime>
#include <csignal>
#include <chrono>
#include <thread>

//DLU Includes:
#include "dCommonVars.h"
#include "dServer.h"
#include "Logger.h"
#include "Database.h"
#include "dConfig.h"
#include "Diagnostics.h"
#include "BinaryPathFinder.h"

//RakNet includes:
#include "RakNetDefines.h"
#include "MessageIdentifiers.h"

//Auth includes:
#include "AuthPackets.h"
#include "eConnectionType.h"
#include "eServerMessageType.h"
#include "eAuthMessageType.h"

#include "Game.h"
#include "Server.h"


namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dConfig* config = nullptr;
	Game::signal_t lastSignal = 0;
	std::mt19937 randomEngine;
}

void HandlePacket(Packet* packet);

int main(int argc, char** argv) {
	constexpr uint32_t authFramerate = mediumFramerate;
	constexpr uint32_t authFrameDelta = mediumFrameDelta;
	Diagnostics::SetProcessName("Auth");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);

	Game::config = new dConfig("authconfig.ini");

	//Create all the objects we need to run our service:
	Server::SetupLogger("AuthServer");
	if (!Game::logger) return EXIT_FAILURE;

	LOG("Starting Auth server...");
	LOG("Version: %s", PROJECT_VERSION);
	LOG("Compiled on: %s", __TIMESTAMP__);

	try {
		Database::Connect();
	} catch (sql::SQLException& ex) {
		LOG("Got an error while connecting to the database: %s", ex.what());
		Database::Destroy("AuthServer");
		delete Game::server;
		delete Game::logger;
		return EXIT_FAILURE;
	}

	//Find out the master's IP:
	std::string masterIP;
	uint32_t masterPort = 1500;

	auto masterInfo = Database::Get()->GetMasterInfo();
	if (masterInfo) {
		masterIP = masterInfo->ip;
		masterPort = masterInfo->port;
	}
	LOG("Master is at %s:%d", masterIP.c_str(), masterPort);

	Game::randomEngine = std::mt19937(time(0));

	//It's safe to pass 'localhost' here, as the IP is only used as the external IP.
	std::string ourIP = "localhost";
	const uint32_t maxClients = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("max_clients")).value_or(999);

	//LU client is hardcoded to use this for auth port, so I'm making it the default.
	const uint32_t ourPort = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("auth_server_port")).value_or(1001);
	const auto externalIPString = Game::config->GetValue("external_ip");
	if (!externalIPString.empty()) ourIP = externalIPString;

	Game::server = new dServer(ourIP, ourPort, 0, maxClients, false, true, Game::logger, masterIP, masterPort, ServerType::Auth, Game::config, &Game::lastSignal);

	//Run it until server gets a kill message from Master:
	auto t = std::chrono::high_resolution_clock::now();
	Packet* packet = nullptr;
	constexpr uint32_t logFlushTime = 30 * authFramerate; // 30 seconds in frames
	constexpr uint32_t sqlPingTime = 10 * 60 * authFramerate; // 10 minutes in frames
	uint32_t framesSinceLastFlush = 0;
	uint32_t framesSinceMasterDisconnect = 0;
	uint32_t framesSinceLastSQLPing = 0;

	AuthPackets::LoadClaimCodes();
	
	Game::logger->Flush(); // once immediately before main loop
	while (!Game::ShouldShutdown()) {
		//Check if we're still connected to master:
		if (!Game::server->GetIsConnectedToMaster()) {
			framesSinceMasterDisconnect++;

			if (framesSinceMasterDisconnect >= authFramerate) {
				LOG("No connection to master!");
				break; //Exit our loop, shut down.
			}
		} else framesSinceMasterDisconnect = 0;

		//In world we'd update our other systems here.

		//Check for packets here:
		Game::server->ReceiveFromMaster(); //ReceiveFromMaster also handles the master packets if needed.
		packet = Game::server->Receive();
		if (packet) {
			HandlePacket(packet);
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

		//Sleep our thread since auth can afford to.
		t += std::chrono::milliseconds(authFrameDelta); //Auth can run at a lower "fps"
		std::this_thread::sleep_until(t);
	}

	LOG("Exited Main Loop! (signal %d)", Game::lastSignal);
	//Delete our objects here:
	Database::Destroy("AuthServer");
	delete Game::server;
	delete Game::logger;
	delete Game::config;

	return EXIT_SUCCESS;
}

void HandlePacket(Packet* packet) {
	if (packet->length < 4) return;

	if (packet->data[0] == ID_USER_PACKET_ENUM) {
		if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::SERVER) {
			if (static_cast<eServerMessageType>(packet->data[3]) == eServerMessageType::VERSION_CONFIRM) {
				AuthPackets::HandleHandshake(Game::server, packet);
			}
		} else if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::AUTH) {
			if (static_cast<eAuthMessageType>(packet->data[3]) == eAuthMessageType::LOGIN_REQUEST) {
				AuthPackets::HandleLoginRequest(Game::server, packet);
			}
		}
	}
}
