#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <fstream>

#ifdef _WIN32
#include <bcrypt/BCrypt.hpp>
#else
#include <bcrypt.h>
#endif

#include <csignal>

//DLU Includes:
#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "Database.h"
#include "MigrationRunner.h"
#include "Diagnostics.h"
#include "dCommonVars.h"
#include "dConfig.h"
#include "dLogger.h"
#include "dServer.h"

//RakNet includes:
#include "RakNetDefines.h"

//Packet includes:

#include "AuthPackets.h"
#include "Game.h"
#include "InstanceManager.h"
#include "MasterPackets.h"
#include "ObjectIDManager.h"
#include "PacketUtils.h"
#include "dMessageIdentifiers.h"

namespace Game {
	dLogger* logger;
	dServer* server;
	InstanceManager* im;
	dConfig* config;
} //namespace Game

bool shutdownSequenceStarted = false;
void ShutdownSequence();
int FinalizeShutdown();
dLogger* SetupLogger();
void StartAuthServer();
void StartChatServer();
void HandlePacket(Packet* packet);
std::map<uint32_t, std::string> activeSessions;
bool shouldShutdown = false;
SystemAddress chatServerMasterPeerSysAddr;

int main(int argc, char** argv) {
	Diagnostics::SetProcessName("Master");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

#if defined(_WIN32) && defined(MARIADB_PLUGIN_DIR_OVERRIDE)
	_putenv_s("MARIADB_PLUGIN_DIR", MARIADB_PLUGIN_DIR_OVERRIDE);
#endif

	//Triggers the shutdown sequence at application exit
	std::atexit(ShutdownSequence);
	signal(SIGINT, [](int) { ShutdownSequence(); });
	signal(SIGTERM, [](int) { ShutdownSequence(); });

	//Create all the objects we need to run our service:
	Game::logger = SetupLogger();
	if (!Game::logger) return EXIT_FAILURE;

	Game::logger->Log("MasterServer", "Starting Master server...\n");
	Game::logger->Log("MasterServer", "Version: %i.%i\n", PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR);
	Game::logger->Log("MasterServer", "Compiled on: %s\n", __TIMESTAMP__);

	//Read our config:
	dConfig config("masterconfig.ini");
	Game::config = &config;
	Game::logger->SetLogToConsole(bool(std::stoi(config.GetValue("log_to_console"))));
	Game::logger->SetLogDebugStatements(config.GetValue("log_debug_statements") == "1");
	
	if (argc > 1 && (strcmp(argv[1], "-m") == 0 || strcmp(argv[1], "--migrations") == 0)) {
		//Connect to the MySQL Database
		std::string mysql_host = config.GetValue("mysql_host");
		std::string mysql_database = config.GetValue("mysql_database");
		std::string mysql_username = config.GetValue("mysql_username");
		std::string mysql_password = config.GetValue("mysql_password");

		try {
			Database::Connect(mysql_host, mysql_database, mysql_username, mysql_password);
		} catch (sql::SQLException& ex) {
			Game::logger->Log("MasterServer", "Got an error while connecting to the database: %s\n", ex.what());
			Game::logger->Log("MigrationRunner", "Migrations not run\n");
			return EXIT_FAILURE;
		}

		MigrationRunner::RunMigrations();
		Game::logger->Log("MigrationRunner", "Finished running migrations\n");

		return EXIT_SUCCESS;
	}	
	else {

		//Check CDClient exists
		const std::string cdclient_path = "./res/CDServer.sqlite";
		std::ifstream cdclient_fd(cdclient_path);
		if (!cdclient_fd.good()) {
			Game::logger->Log("WorldServer", "%s could not be opened\n", cdclient_path.c_str());
			return EXIT_FAILURE;
		}
		cdclient_fd.close();

		//Connect to CDClient
		try {
			CDClientDatabase::Connect(cdclient_path);
		} catch (CppSQLite3Exception& e) {
			Game::logger->Log("WorldServer", "Unable to connect to CDServer SQLite Database\n");
			Game::logger->Log("WorldServer", "Error: %s\n", e.errorMessage());
			Game::logger->Log("WorldServer", "Error Code: %i\n", e.errorCode());
			return EXIT_FAILURE;
		}

		//Get CDClient initial information
		try {
			CDClientManager::Instance()->Initialize();
		} catch (CppSQLite3Exception& e) {
			Game::logger->Log("WorldServer", "Failed to initialize CDServer SQLite Database\n");
			Game::logger->Log("WorldServer", "May be caused by corrupted file: %s\n", cdclient_path.c_str());
			Game::logger->Log("WorldServer", "Error: %s\n", e.errorMessage());
			Game::logger->Log("WorldServer", "Error Code: %i\n", e.errorCode());
			return EXIT_FAILURE;
		}

		//Connect to the MySQL Database
		std::string mysql_host = config.GetValue("mysql_host");
		std::string mysql_database = config.GetValue("mysql_database");
		std::string mysql_username = config.GetValue("mysql_username");
		std::string mysql_password = config.GetValue("mysql_password");

		try {
			Database::Connect(mysql_host, mysql_database, mysql_username, mysql_password);
		} catch (sql::SQLException& ex) {
			Game::logger->Log("MasterServer", "Got an error while connecting to the database: %s\n", ex.what());
			return EXIT_FAILURE;
		}
	}


	//If the first command line argument is -a or --account then make the user
	//input a username and password, with the password being hidden.
	if (argc > 1 &&
		(strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "--account") == 0)) {
		std::string username;
		std::string password;

		std::cout << "Enter a username: ";
		std::cin >> username;

		//Read the password from the console without echoing it.
#ifdef __linux__
		//This function is obsolete, but it only meant to be used by the
		//sysadmin to create their first account.
		password = getpass("Enter a password: ");
#else
		std::cout << "Enter a password: ";
		std::cin >> password;
#endif

		//Generate new hash for bcrypt

		char salt[BCRYPT_HASHSIZE];
		char hash[BCRYPT_HASHSIZE];

		int32_t bcryptState = ::bcrypt_gensalt(12, salt);

		assert(bcryptState == 0);

		bcryptState = ::bcrypt_hashpw(password.c_str(), salt, hash);

		assert(bcryptState == 0);

		//Create account

		auto* statement = Database::CreatePreppedStmt("INSERT INTO accounts (name, password, ""gm_level) VALUES (?, ?, ?);");
		statement->setString(1, username);
		statement->setString(2, std::string(hash, BCRYPT_HASHSIZE).c_str());
		statement->setInt(3, 9);

		statement->execute();

		delete statement;

		std::cout << "Account created successfully!\n";

		Database::Destroy("MasterServer");
		delete Game::logger;

		return EXIT_SUCCESS;
	}

	int maxClients = 999;
	int ourPort = 1000;
	if (config.GetValue("max_clients") != "") maxClients = std::stoi(config.GetValue("max_clients"));
	if (config.GetValue("port") != "") ourPort = std::stoi(config.GetValue("port"));

	Game::server = new dServer(config.GetValue("external_ip"), ourPort, 0, maxClients, true, false, Game::logger, "", 0, ServerType::Master);

	//Query for the database for a server labeled "master"
	auto* masterLookupStatement = Database::CreatePreppedStmt("SELECT id FROM `servers` WHERE `name` = 'master'");
	auto* result = masterLookupStatement->executeQuery();

	auto master_server_ip = config.GetValue("master_ip");

	if (master_server_ip == "") {
		master_server_ip = Game::server->GetIP();
	}

	//If we found a server, update it's IP and port to the current one.
	if (result->next()) {
		auto* updateStatement = Database::CreatePreppedStmt("UPDATE `servers` SET `ip` = ?, `port` = ? WHERE `id` = ?");
		updateStatement->setString(1, master_server_ip);
		updateStatement->setInt(2, Game::server->GetPort());
		updateStatement->setInt(3, result->getInt("id"));
		updateStatement->execute();
		delete updateStatement;
	}
	else {
		//If we didn't find a server, create one.
		auto* insertStatement = Database::CreatePreppedStmt("INSERT INTO `servers` (`name`, `ip`, `port`, `state`, `version`) VALUES ('master', ?, ?, 0, 171023)");
		insertStatement->setString(1, master_server_ip);
		insertStatement->setInt(2, Game::server->GetPort());
		insertStatement->execute();
		delete insertStatement;
	}

	//Create additional objects here:
	ObjectIDManager::Instance()->Initialize(Game::logger);
	Game::im = new InstanceManager(Game::logger, Game::server->GetIP());

	//Depending on the config, start up servers:
	if (config.GetValue("prestart_servers") != "" && config.GetValue("prestart_servers") == "1") {
		StartChatServer();

		Game::im->GetInstance(0, false, 0)->SetIsReady(true);
		Game::im->GetInstance(1000, false, 0)->SetIsReady(true);

		StartAuthServer();
	}

	auto t = std::chrono::high_resolution_clock::now();
	Packet* packet = nullptr;
	int framesSinceLastFlush = 0;
	int framesSinceLastSQLPing = 0;
	int framesSinceKillUniverseCommand = 0;

	while (true) {
		//In world we'd update our other systems here.

		//Check for packets here:
		packet = Game::server->Receive();
		if (packet) {
			HandlePacket(packet);
			Game::server->DeallocatePacket(packet);
			packet = nullptr;
		}

		//Push our log every 15s:
		if (framesSinceLastFlush >= 900) {
			Game::logger->Flush();
			framesSinceLastFlush = 0;
		}
		else
			framesSinceLastFlush++;

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
		}
		else
			framesSinceLastSQLPing++;

		//10m shutdown for universe kill command
		if (shouldShutdown) {
			if (framesSinceKillUniverseCommand >= 40000) {
				//Break main loop and exit
				break;
			}
			else
				framesSinceKillUniverseCommand++;
		}

		const auto instances = Game::im->GetInstances();

		for (auto* instance : instances) {
			if (instance == nullptr) {
				break;
			}

			auto affirmTimeout = instance->GetAffirmationTimeout();

			if (!instance->GetPendingAffirmations().empty()) {
				affirmTimeout++;
			}
			else {
				affirmTimeout = 0;
			}

			instance->SetAffirmationTimeout(affirmTimeout);

			if (affirmTimeout == 1000) {
				instance->Shutdown();
				instance->SetIsShuttingDown(true);

				Game::im->RedirectPendingRequests(instance);
			}
		}

		//Remove dead instances
		for (auto* instance : instances) {
			if (instance == nullptr) {
				break;
			}

			if (instance->GetShutdownComplete()) {
				Game::im->RemoveInstance(instance);
			}
		}

		t += std::chrono::milliseconds(highFrameRate);
		std::this_thread::sleep_until(t);
	}
	FinalizeShutdown();
	exit(EXIT_SUCCESS);
	return EXIT_SUCCESS;
}

dLogger* SetupLogger() {
	std::string logPath =
		"./logs/MasterServer_" + std::to_string(time(nullptr)) + ".log";
	bool logToConsole = false;
	bool logDebugStatements = false;
#ifdef _DEBUG
	logToConsole = true;
	logDebugStatements = true;
#endif

	return new dLogger(logPath, logToConsole, logDebugStatements);
}

void HandlePacket(Packet* packet) {
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION) {
		Game::logger->Log("MasterServer", "A server has disconnected\n");

		//Since this disconnection is intentional, we'll just delete it as
		//we'll start a new one anyway if needed:
		Instance* instance =
			Game::im->GetInstanceBySysAddr(packet->systemAddress);
		if (instance) {
			Game::logger->Log("MasterServer", "Actually disconnected from zone %i clone %i instance %i port %i\n", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance->GetPort());
			Game::im->RemoveInstance(instance); //Delete the old
		}

		if (packet->systemAddress == chatServerMasterPeerSysAddr && !shouldShutdown) {
			StartChatServer();
		}
	}

	if (packet->data[0] == ID_CONNECTION_LOST) {
		Game::logger->Log("MasterServer", "A server has lost the connection\n");

		Instance* instance =
			Game::im->GetInstanceBySysAddr(packet->systemAddress);
		if (instance) {
			LWOZONEID zoneID = instance->GetZoneID(); //Get the zoneID so we can recreate a server
			Game::im->RemoveInstance(instance); //Delete the old
			//Game::im->GetInstance(zoneID.GetMapID(), false, 0); //Create the new
		}

		if (packet->systemAddress == chatServerMasterPeerSysAddr && !shouldShutdown) {
			StartChatServer();
		}
	}

	if (packet->data[1] == MASTER) {
		switch (packet->data[3]) {
		case MSG_MASTER_REQUEST_PERSISTENT_ID: {
			Game::logger->Log("MasterServer", "A persistent ID req\n");
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);
			uint64_t requestID = 0;
			inStream.Read(requestID);

			uint32_t objID = ObjectIDManager::Instance()->GeneratePersistentID();
			MasterPackets::SendPersistentIDResponse(Game::server, packet->systemAddress, requestID, objID);
			break;
		}

		case MSG_MASTER_REQUEST_ZONE_TRANSFER: {
			Game::logger->Log("MasterServer","Received zone transfer req\n");
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);
			uint64_t requestID = 0;
			uint8_t mythranShift = false;
			uint32_t zoneID = 0;
			uint32_t zoneClone = 0;

			inStream.Read(requestID);
			inStream.Read(mythranShift);
			inStream.Read(zoneID);
			inStream.Read(zoneClone);

			Instance* in = Game::im->GetInstance(zoneID, false, zoneClone);

			for (auto* instance : Game::im->GetInstances()) {
				Game::logger->Log("MasterServer", "Instance: %i/%i/%i -> %i\n",instance->GetMapID(), instance->GetCloneID(),instance->GetInstanceID(), instance == in);
			}

			if (!in->GetIsReady()) //Instance not ready, make a pending request
			{
				in->GetPendingRequests().push_back({ requestID, static_cast<bool>(mythranShift), packet->systemAddress });
				Game::logger->Log("MasterServer", "Server not ready, adding pending request %llu %i %i\n", requestID, zoneID, zoneClone);
				break;
			}

			//Instance is ready, transfer
			Game::logger->Log("MasterServer", "Responding to transfer request %llu for zone %i %i\n", requestID, zoneID, zoneClone);
			Game::im->RequestAffirmation(in, { requestID, static_cast<bool>(mythranShift), packet->systemAddress });
			break;
		}

		case MSG_MASTER_SERVER_INFO: {
			//MasterPackets::HandleServerInfo(packet);

			//This is here because otherwise we'd have to include IM in
			//non-master servers. This packet allows us to add World
			//servers back if master crashed
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			uint32_t theirPort = 0;
			uint32_t theirZoneID = 0;
			uint32_t theirInstanceID = 0;
			ServerType theirServerType;
			std::string theirIP = "";

			inStream.Read(theirPort);
			inStream.Read(theirZoneID);
			inStream.Read(theirInstanceID);
			inStream.Read(theirServerType);
			theirIP = PacketUtils::ReadString(24, packet, false); //24 is the current offset

			if (theirServerType == ServerType::World && !Game::im->IsPortInUse(theirPort)) {
				Instance* in = new Instance(theirIP, theirPort, theirZoneID, theirInstanceID, 0, 12, 12);

				SystemAddress copy;
				copy.binaryAddress = packet->systemAddress.binaryAddress;
				copy.port = packet->systemAddress.port;

				in->SetSysAddr(copy);
				Game::im->AddInstance(in);
			}
			else {
				auto instance = Game::im->FindInstance(
					theirZoneID, static_cast<uint16_t>(theirInstanceID));
				if (instance) {
					instance->SetSysAddr(packet->systemAddress);
				}
			}

			if (theirServerType == ServerType::Chat) {
				SystemAddress copy;
				copy.binaryAddress = packet->systemAddress.binaryAddress;
				copy.port = packet->systemAddress.port;

				chatServerMasterPeerSysAddr = copy;
			}

			Game::logger->Log("MasterServer", "Received server info, instance: %i port: %i\n", theirInstanceID, theirPort);

			break;
		}

		case MSG_MASTER_SET_SESSION_KEY: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);
			uint32_t sessionKey = 0;
			std::string username;

			inStream.Read(sessionKey);
			username = PacketUtils::ReadString(12, packet, false);

			for (auto it : activeSessions) {
				if (it.second == username) {
					activeSessions.erase(it.first);

					CBITSTREAM;
					PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_NEW_SESSION_ALERT);
					bitStream.Write(sessionKey);
					bitStream.Write<uint32_t>(username.size());
					for (auto character : username) {
						bitStream.Write(character);
					}
					SEND_PACKET_BROADCAST;

					break;
				}
			}

			activeSessions.insert(std::make_pair(sessionKey, username));
			Game::logger->Log("MasterServer", "Got sessionKey %i for user %s\n", sessionKey, username.c_str());
			break;
		}

		case MSG_MASTER_REQUEST_SESSION_KEY: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);
			std::string username = PacketUtils::ReadString(8, packet, false);

			for (auto key : activeSessions) {
				if (key.second == username) {
					CBITSTREAM;
					PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_SESSION_KEY_RESPONSE);
					bitStream.Write(key.first);
					PacketUtils::WriteString(bitStream, key.second, 64);
					Game::server->Send(&bitStream, packet->systemAddress, false);
					break;
				}
			}
			break;
		}

		case MSG_MASTER_PLAYER_ADDED: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			LWOMAPID theirZoneID = 0;
			LWOINSTANCEID theirInstanceID = 0;

			inStream.Read(theirZoneID);
			inStream.Read(theirInstanceID);

			auto instance =
				Game::im->FindInstance(theirZoneID, theirInstanceID);
			if (instance) {
				instance->AddPlayer(Player());
			}
			else {
				printf("Instance missing? What?\n");
			}
			break;
		}

		case MSG_MASTER_PLAYER_REMOVED: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			LWOMAPID theirZoneID = 0;
			LWOINSTANCEID theirInstanceID = 0;

			inStream.Read(theirZoneID);
			inStream.Read(theirInstanceID);

			auto instance =
				Game::im->FindInstance(theirZoneID, theirInstanceID);
			if (instance) {
				instance->RemovePlayer(Player());
			}
			break;
		}

		case MSG_MASTER_CREATE_PRIVATE_ZONE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			uint32_t mapId;
			LWOCLONEID cloneId;
			std::string password;

			inStream.Read(mapId);
			inStream.Read(cloneId);

			uint32_t len;
			inStream.Read<uint32_t>(len);
			for (int i = 0; len > i; i++) {
				char character;
				inStream.Read<char>(character);
				password += character;
			}

			Game::im->CreatePrivateInstance(mapId, cloneId, password.c_str());

			break;
		}

		case MSG_MASTER_REQUEST_PRIVATE_ZONE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			uint64_t requestID = 0;
			uint8_t mythranShift = false;

			std::string password;

			inStream.Read(requestID);
			inStream.Read(mythranShift);
			
			uint32_t len;
			inStream.Read<uint32_t>(len);

			for (int i = 0; i < len; i++) {
				char character; inStream.Read<char>(character);
				password += character;
			}

			auto* instance = Game::im->FindPrivateInstance(password.c_str());

			Game::logger->Log( "MasterServer", "Join private zone: %llu %d %s %p\n", requestID, mythranShift, password.c_str(), instance);

			if (instance == nullptr) {
				return;
			}

			const auto& zone = instance->GetZoneID();

			MasterPackets::SendZoneTransferResponse(Game::server, packet->systemAddress, requestID,(bool)mythranShift, zone.GetMapID(),instance->GetInstanceID(), zone.GetCloneID(),instance->GetIP(), instance->GetPort());

			break;
		}

		case MSG_MASTER_WORLD_READY: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			LWOMAPID zoneID;
			LWOINSTANCEID instanceID;

			inStream.Read(zoneID);
			inStream.Read(instanceID);

			Game::logger->Log("MasterServer", "Got world ready %i %i\n",zoneID, instanceID);

			auto* instance = Game::im->FindInstance(zoneID, instanceID);

			if (instance == nullptr) {
				Game::logger->Log("MasterServer","Failed to find zone to ready\n");
				return;
			}

			Game::logger->Log("MasterServer", "Ready zone %i\n", zoneID);
			Game::im->ReadyInstance(instance);
			break;
		}

		case MSG_MASTER_PREP_ZONE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			int zoneID;
			inStream.Read(zoneID);

			Game::logger->Log("MasterServer", "Prepping zone %i\n", zoneID);
			Game::im->GetInstance(zoneID, false, 0);
			break;
		}

		case MSG_MASTER_AFFIRM_TRANSFER_RESPONSE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			uint64_t requestID;

			inStream.Read(requestID);

			Game::logger->Log("MasterServer","Got affirmation of transfer %llu\n",requestID);

			auto* instance =Game::im->GetInstanceBySysAddr(packet->systemAddress);

			if (instance == nullptr)
				return;

			Game::im->AffirmTransfer(instance, requestID);
			Game::logger->Log("MasterServer", "Affirmation complete %llu\n",requestID);
			break;
		}

		case MSG_MASTER_SHUTDOWN_RESPONSE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			auto* instance = Game::im->GetInstanceBySysAddr(packet->systemAddress);

			if (instance == nullptr) {
				return;
			}

			Game::logger->Log("MasterServer", "Got shutdown response from zone %i clone %i instance %i port %i\n", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance->GetPort());
			instance->SetIsShuttingDown(true);
			break;
		}

		case MSG_MASTER_SHUTDOWN_UNIVERSE: {
			Game::logger->Log("MasterServer","Received shutdown universe command, shutting down in 10 minutes.\n");
			shouldShutdown = true;
			break;
		}

		default:
			Game::logger->Log("MasterServer","Unknown master packet ID from server: %i\n",packet->data[3]);
		}
	}
}

void StartChatServer() {
#ifdef __APPLE__
		//macOS doesn't need sudo to run on ports < 1024
		system("./ChatServer&");
#elif _WIN32
		system("start ./ChatServer.exe");
#else
		if (std::atoi(Game::config->GetValue("use_sudo_chat").c_str())) {
			system("sudo ./ChatServer&");
		}
		else {
			system("./ChatServer&");
		}
#endif
}

void StartAuthServer() {
#ifdef __APPLE__
		system("./AuthServer&");
#elif _WIN32
		system("start ./AuthServer.exe");
#else
		if (std::atoi(Game::config->GetValue("use_sudo_auth").c_str())) {
			system("sudo ./AuthServer&");
		}
		else {
			system("./AuthServer&");
		}
#endif
}

void ShutdownSequence() {
	if (shutdownSequenceStarted) {
		return;
	}

	shutdownSequenceStarted = true;

	if (Game::im) {
		for (auto* instance : Game::im->GetInstances()) {
			if (instance == nullptr) {
				continue;
			}

			instance->Shutdown();
		}
	}

	auto* objIdManager = ObjectIDManager::TryInstance();
	if (objIdManager != nullptr) {
		objIdManager->SaveToDatabase();
		Game::logger->Log("MasterServer", "Saved ObjectIDTracker to DB\n");
	}

	auto t = std::chrono::high_resolution_clock::now();
	auto ticks = 0;

	if (!Game::im) {
		exit(EXIT_SUCCESS);
	}

	Game::logger->Log("MasterServer", "Attempting to shutdown instances, max 60 seconds...\n");

	while (true) {

		auto packet = Game::server->Receive();
		if (packet) {
			HandlePacket(packet);
			Game::server->DeallocatePacket(packet);
			packet = nullptr;
		}
		
		auto done = true;

		for (auto* instance : Game::im->GetInstances()) {
			if (instance == nullptr) {
				continue;
			}

			if (!instance->GetShutdownComplete()) {
				done = false;
			}
		}

		if (done) {
			Game::logger->Log("MasterServer", "Finished shutting down MasterServer!\n");
			break;
		}

		t += std::chrono::milliseconds(highFrameRate);
		std::this_thread::sleep_until(t);

		ticks++;

		if (ticks == 600 * 6) {
			Game::logger->Log("MasterServer", "Finished shutting down by timeout!\n");
			break;
		}
	}

	FinalizeShutdown();
}

int FinalizeShutdown() {
	//Delete our objects here:
	Database::Destroy("MasterServer");
	delete Game::im;
	delete Game::server;
	delete Game::logger;

	exit(EXIT_SUCCESS);
	return EXIT_SUCCESS;
}
