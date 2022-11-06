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

//RakNet includes:
#include "RakNetDefines.h"

//Auth includes:
#include "AuthPackets.h"
#include "dMessageIdentifiers.h"

#include "Game.h"
namespace Game {
	dLogger* logger;
	dServer* server;
	dConfig* config;
}

dLogger* SetupLogger();
void HandlePacket(Packet* packet);

int main(int argc, char** argv) {
	Diagnostics::SetProcessName("Auth");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	//Create all the objects we need to run our service:
	Game::logger = SetupLogger();
	if (!Game::logger) return 0;
	Game::logger->Log("AuthServer", "Starting Auth server...");
	Game::logger->Log("AuthServer", "Version: %i.%i", PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR);
	Game::logger->Log("AuthServer", "Compiled on: %s", __TIMESTAMP__);

	//Read our config:
	dConfig config("authconfig.ini");
	Game::config = &config;
	Game::logger->SetLogToConsole(bool(std::stoi(config.GetValue("log_to_console"))));
	Game::logger->SetLogDebugStatements(config.GetValue("log_debug_statements") == "1");

	//Connect to the MySQL Database
	std::string mysql_host = config.GetValue("mysql_host");
	std::string mysql_database = config.GetValue("mysql_database");
	std::string mysql_username = config.GetValue("mysql_username");
	std::string mysql_password = config.GetValue("mysql_password");

	try {
		Database::Connect(mysql_host, mysql_database, mysql_username, mysql_password);
	} catch (sql::SQLException& ex) {
		Game::logger->Log("AuthServer", "Got an error while connecting to the database: %s", ex.what());
		Database::Destroy("AuthServer");
		delete Game::server;
		delete Game::logger;
		return 0;
	}

	//Find out the master's IP:
	std::string masterIP;
	int masterPort = 1500;
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT ip, port FROM servers WHERE name='master';");
	auto res = stmt->executeQuery();
	while (res->next()) {
		masterIP = res->getString(1).c_str();
		masterPort = res->getInt(2);
	}

	delete res;
	delete stmt;

	//It's safe to pass 'localhost' here, as the IP is only used as the external IP.
	int maxClients = 50;
	int ourPort = 1001; //LU client is hardcoded to use this for auth port, so I'm making it the default.
	if (config.GetValue("max_clients") != "") maxClients = std::stoi(config.GetValue("max_clients"));
	if (config.GetValue("port") != "") ourPort = std::atoi(config.GetValue("port").c_str());

	Game::server = new dServer(config.GetValue("external_ip"), ourPort, 0, maxClients, false, true, Game::logger, masterIP, masterPort, ServerType::Auth);

	//Run it until server gets a kill message from Master:
	auto t = std::chrono::high_resolution_clock::now();
	Packet* packet = nullptr;
	int framesSinceLastFlush = 0;
	int framesSinceMasterDisconnect = 0;
	int framesSinceLastSQLPing = 0;

	while (true) {
		//Check if we're still connected to master:
		if (!Game::server->GetIsConnectedToMaster()) {
			framesSinceMasterDisconnect++;

			if (framesSinceMasterDisconnect >= 30)
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
		if (framesSinceLastFlush >= 900) {
			Game::logger->Flush();
			framesSinceLastFlush = 0;
		} else framesSinceLastFlush++;

		//Every 10 min we ping our sql server to keep it alive hopefully:
		if (framesSinceLastSQLPing >= 40000) {
			//Find out the master's IP for absolutely no reason:
			std::string masterIP;
			int masterPort;
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
		t += std::chrono::milliseconds(mediumFramerate); //Auth can run at a lower "fps"
		std::this_thread::sleep_until(t);
	}

	//Delete our objects here:
	Database::Destroy("AuthServer");
	delete Game::server;
	delete Game::logger;

	exit(EXIT_SUCCESS);
	return EXIT_SUCCESS;
}

dLogger* SetupLogger() {
	std::string logPath = "./logs/AuthServer_" + std::to_string(time(nullptr)) + ".log";
	bool logToConsole = false;
	bool logDebugStatements = false;
#ifdef _DEBUG
	logToConsole = true;
	logDebugStatements = true;
#endif

	return new dLogger(logPath, logToConsole, logDebugStatements);
}

void HandlePacket(Packet* packet) {
	if (packet->data[0] == ID_USER_PACKET_ENUM) {
		if (packet->data[1] == SERVER) {
			if (packet->data[3] == MSG_SERVER_VERSION_CONFIRM) {
				AuthPackets::HandleHandshake(Game::server, packet);
			}
		} else if (packet->data[1] == AUTH) {
			if (packet->data[3] == MSG_AUTH_LOGIN_REQUEST) {
				AuthPackets::HandleLoginRequest(Game::server, packet);
			}
		}
	}
}
