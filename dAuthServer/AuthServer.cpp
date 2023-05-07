#include <iostream>
#include <string>
#include <ctime>
#include <chrono>
#include <thread>

//DLU Includes:
#include "dCommonVars.h"
#include "dServer.h"
#include "dLogger.h"
#include "Database.h"
#include "dConfig.h"
#include "Diagnostics.h"
#include "BinaryPathFinder.h"

//RakNet includes:
#include "RakNetDefines.h"
#include <MessageIdentifiers.h>

//Auth includes:
#include "AuthPackets.h"
#include "eConnectionType.h"
#include "eServerMessageType.h"
#include "eAuthMessageType.h"

#include "Game.h"
namespace Game {
	dLogger* logger = nullptr;
	dServer* server = nullptr;
	dConfig* config = nullptr;
	bool shouldShutdown = false;
}

dLogger* SetupLogger();
void HandlePacket(Packet* packet);

int main(int argc, char** argv) {
	constexpr uint32_t authFramerate = mediumFramerate;
	constexpr uint32_t authFrameDelta = mediumFrameDelta;
	Diagnostics::SetProcessName("Auth");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	//Create all the objects we need to run our service:
	Game::logger = SetupLogger();
	if (!Game::logger) return EXIT_FAILURE;

	//Read our config:
	Game::config = new dConfig((BinaryPathFinder::GetBinaryDir() / "authconfig.ini").string());
	Game::logger->SetLogToConsole(Game::config->GetValue("log_to_console") != "0");
	Game::logger->SetLogDebugStatements(Game::config->GetValue("log_debug_statements") == "1");

	Game::logger->Log("AuthServer", "Starting Auth server...");
	Game::logger->Log("AuthServer", "Version: %i.%i", PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR);
	Game::logger->Log("AuthServer", "Compiled on: %s", __TIMESTAMP__);

	//Connect to the MySQL Database
	std::string mysql_host = Game::config->GetValue("mysql_host");
	std::string mysql_database = Game::config->GetValue("mysql_database");
	std::string mysql_username = Game::config->GetValue("mysql_username");
	std::string mysql_password = Game::config->GetValue("mysql_password");

	try {
		Database::Connect(mysql_host, mysql_database, mysql_username, mysql_password);
	} catch (sql::SQLException& ex) {
		Game::logger->Log("AuthServer", "Got an error while connecting to the database: %s", ex.what());
		Database::Destroy("AuthServer");
		delete Game::server;
		delete Game::logger;
		return EXIT_FAILURE;
	}

	//Find out the master's IP:
	std::string masterIP;
	uint32_t masterPort = 1500;
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT ip, port FROM servers WHERE name='master';");
	auto res = stmt->executeQuery();
	while (res->next()) {
		masterIP = res->getString(1).c_str();
		masterPort = res->getInt(2);
	}

	delete res;
	delete stmt;

	//It's safe to pass 'localhost' here, as the IP is only used as the external IP.
	uint32_t maxClients = 50;
	uint32_t ourPort = 1001; //LU client is hardcoded to use this for auth port, so I'm making it the default.
	if (Game::config->GetValue("max_clients") != "") maxClients = std::stoi(Game::config->GetValue("max_clients"));
	if (Game::config->GetValue("port") != "") ourPort = std::atoi(Game::config->GetValue("port").c_str());

	Game::server = new dServer(Game::config->GetValue("external_ip"), ourPort, 0, maxClients, false, true, Game::logger, masterIP, masterPort, ServerType::Auth, Game::config, &Game::shouldShutdown);

	//Run it until server gets a kill message from Master:
	auto t = std::chrono::high_resolution_clock::now();
	Packet* packet = nullptr;
	constexpr uint32_t logFlushTime = 30 * authFramerate; // 30 seconds in frames
	constexpr uint32_t sqlPingTime = 10 * 60 * authFramerate; // 10 minutes in frames
	uint32_t framesSinceLastFlush = 0;
	uint32_t framesSinceMasterDisconnect = 0;
	uint32_t framesSinceLastSQLPing = 0;

	while (!Game::shouldShutdown) {
		//Check if we're still connected to master:
		if (!Game::server->GetIsConnectedToMaster()) {
			framesSinceMasterDisconnect++;

			if (framesSinceMasterDisconnect >= authFramerate)
				break; //Exit our loop, shut down.
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
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT ip, port FROM servers WHERE name='master';");
			auto res = stmt->executeQuery();
			while (res->next()) {
				masterIP = res->getString(1).c_str();
				masterPort = res->getInt(2);
			}

			delete res;
			delete stmt;

			framesSinceLastSQLPing = 0;
		} else framesSinceLastSQLPing++;

		//Sleep our thread since auth can afford to.
		t += std::chrono::milliseconds(authFrameDelta); //Auth can run at a lower "fps"
		std::this_thread::sleep_until(t);
	}

	//Delete our objects here:
	Database::Destroy("AuthServer");
	delete Game::server;
	delete Game::logger;
	delete Game::config;

	return EXIT_SUCCESS;
}

dLogger* SetupLogger() {
	std::string logPath = (BinaryPathFinder::GetBinaryDir() / ("logs/AuthServer_" + std::to_string(time(nullptr)) + ".log")).string();
	bool logToConsole = false;
	bool logDebugStatements = false;
#ifdef _DEBUG
	logToConsole = true;
	logDebugStatements = true;
#endif

	return new dLogger(logPath, logToConsole, logDebugStatements);
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
