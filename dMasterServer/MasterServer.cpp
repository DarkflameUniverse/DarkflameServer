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
#include "AssetManager.h"
#include "BinaryPathFinder.h"
#include "eConnectionType.h"
#include "eMasterMessageType.h"

//RakNet includes:
#include "RakNetDefines.h"

//Packet includes:

#include "AuthPackets.h"
#include "Game.h"
#include "InstanceManager.h"
#include "MasterPackets.h"
#include "ObjectIDManager.h"
#include "PacketUtils.h"
#include "FdbToSqlite.h"
#include "BitStreamUtils.h"

namespace Game {
	dLogger* logger = nullptr;
	dServer* server = nullptr;
	InstanceManager* im = nullptr;
	dConfig* config = nullptr;
	AssetManager* assetManager = nullptr;
	bool shouldShutdown = false;
	std::mt19937 randomEngine;
} //namespace Game

bool shutdownSequenceStarted = false;
void ShutdownSequence(int32_t signal = -1);
int32_t FinalizeShutdown(int32_t signal = -1);
dLogger* SetupLogger();
void StartAuthServer();
void StartChatServer();
void HandlePacket(Packet* packet);
std::map<uint32_t, std::string> activeSessions;
SystemAddress authServerMasterPeerSysAddr;
SystemAddress chatServerMasterPeerSysAddr;

int main(int argc, char** argv) {
	constexpr uint32_t masterFramerate = mediumFramerate;
	constexpr uint32_t masterFrameDelta = mediumFrameDelta;
	Diagnostics::SetProcessName("Master");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

#if defined(_WIN32) && defined(MARIADB_PLUGIN_DIR_OVERRIDE)
	_putenv_s("MARIADB_PLUGIN_DIR", MARIADB_PLUGIN_DIR_OVERRIDE);
#endif

	//Triggers the shutdown sequence at application exit
	std::atexit([]() { ShutdownSequence(); });
	signal(SIGINT, [](int32_t signal) { ShutdownSequence(EXIT_FAILURE); });
	signal(SIGTERM, [](int32_t signal) { ShutdownSequence(EXIT_FAILURE); });

	//Create all the objects we need to run our service:
	Game::logger = SetupLogger();
	if (!Game::logger) return EXIT_FAILURE;

	if (!std::filesystem::exists(BinaryPathFinder::GetBinaryDir() / "authconfig.ini")) {
		Game::logger->Log("MasterServer", "Couldnt find authconfig.ini");
		return EXIT_FAILURE;
	}

	if (!std::filesystem::exists(BinaryPathFinder::GetBinaryDir() / "chatconfig.ini")) {
		Game::logger->Log("MasterServer", "Couldnt find chatconfig.ini");
		return EXIT_FAILURE;
	}

	if (!std::filesystem::exists(BinaryPathFinder::GetBinaryDir() / "masterconfig.ini")) {
		Game::logger->Log("MasterServer", "Couldnt find masterconfig.ini");
		return EXIT_FAILURE;
	}

	if (!std::filesystem::exists(BinaryPathFinder::GetBinaryDir() / "sharedconfig.ini")) {
		Game::logger->Log("MasterServer", "Couldnt find sharedconfig.ini");
		return EXIT_FAILURE;
	}

	if (!std::filesystem::exists(BinaryPathFinder::GetBinaryDir() / "worldconfig.ini")) {
		Game::logger->Log("MasterServer", "Couldnt find worldconfig.ini");
		return EXIT_FAILURE;
	}

	Game::config = new dConfig((BinaryPathFinder::GetBinaryDir() / "masterconfig.ini").string());
	Game::logger->SetLogToConsole(Game::config->GetValue("log_to_console") != "0");
	Game::logger->SetLogDebugStatements(Game::config->GetValue("log_debug_statements") == "1");

	uint32_t clientNetVersion = 0;
	if (!GeneralUtils::TryParse(Game::config->GetValue("client_net_version"), clientNetVersion)) {
		Game::logger->Log("MasterServer", "Failed to parse (%s) as net version. Cannot start server as no clients could connect.",Game::config->GetValue("client_net_version").c_str());
		Game::logger->Log("MasterServer", "As of version 1.1.1, client_net_version is required to be defined in sharedconfig.ini as opposed to in CMakeVariables.txt as NET_VERSION.");
		Game::logger->Log("MasterServer", "Rerun cmake to ensure all config values exist. If client_net_version already exists in sharedconfig.ini, please ensure it is a valid number.");
		Game::logger->Log("MasterServer", "like 171022");
		return EXIT_FAILURE;
	}

	Game::logger->Log("MasterServer", "Using net version %s", Game::config->GetValue("client_net_version").c_str());

	Game::logger->Log("MasterServer", "Starting Master server...");
	Game::logger->Log("MasterServer", "Version: %i.%i", PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR);
	Game::logger->Log("MasterServer", "Compiled on: %s", __TIMESTAMP__);

	Database::Connect(Game::config);

	try {
		std::string clientPathStr = Game::config->GetValue("client_location");
		if (clientPathStr.empty()) clientPathStr = "./res";
		std::filesystem::path clientPath = std::filesystem::path(clientPathStr);
		if (clientPath.is_relative()) {
			clientPath = BinaryPathFinder::GetBinaryDir() / clientPath;
		}

		Game::assetManager = new AssetManager(clientPath);
	} catch (std::runtime_error& ex) {
		Game::logger->Log("MasterServer", "Got an error while setting up assets: %s", ex.what());

		return EXIT_FAILURE;
	}

	MigrationRunner::RunMigrations();

	const bool cdServerExists = std::filesystem::exists(BinaryPathFinder::GetBinaryDir() / "resServer" / "CDServer.sqlite");
	const bool oldCDServerExists = std::filesystem::exists(Game::assetManager->GetResPath() / "CDServer.sqlite");
	const bool fdbExists = std::filesystem::exists(Game::assetManager->GetResPath() / "cdclient.fdb");

	if (!cdServerExists) {
		if (oldCDServerExists) {
			// If the file doesn't exist in the new CDServer location, copy it there.  We copy because we may not have write permissions from the previous directory.
			Game::logger->Log("MasterServer", "CDServer.sqlite is not located at resServer, but is located at res path.  Copying file...");
			std::filesystem::copy_file(Game::assetManager->GetResPath() / "CDServer.sqlite", BinaryPathFinder::GetBinaryDir() / "resServer" / "CDServer.sqlite");
		} else {
			Game::logger->Log("MasterServer",
				"%s could not be found in resServer or res. Looking for %s to convert to sqlite.",
				(BinaryPathFinder::GetBinaryDir() / "resServer" / "CDServer.sqlite").c_str(),
				(Game::assetManager->GetResPath() / "cdclient.fdb").c_str());

			AssetMemoryBuffer cdClientBuffer = Game::assetManager->GetFileAsBuffer("cdclient.fdb");
			if (!cdClientBuffer.m_Success) {
				Game::logger->Log("MasterServer", "Failed to load %s", (Game::assetManager->GetResPath() / "cdclient.fdb").c_str());
				throw std::runtime_error("Aborting initialization due to missing cdclient.fdb.");
			}

			Game::logger->Log("MasterServer", "Found %s.  Converting to SQLite", (Game::assetManager->GetResPath() / "cdclient.fdb").c_str());
			Game::logger->Flush();

			if (FdbToSqlite::Convert((BinaryPathFinder::GetBinaryDir() / "resServer").string()).ConvertDatabase(cdClientBuffer) == false) {
				Game::logger->Log("MasterServer", "Failed to convert fdb to sqlite.");
				return EXIT_FAILURE;
			}
			cdClientBuffer.close();
		}
	}

	//Connect to CDClient
	try {
		CDClientDatabase::Connect((BinaryPathFinder::GetBinaryDir() / "resServer" / "CDServer.sqlite").string());
	} catch (CppSQLite3Exception& e) {
		Game::logger->Log("WorldServer", "Unable to connect to CDServer SQLite Database");
		Game::logger->Log("WorldServer", "Error: %s", e.errorMessage());
		Game::logger->Log("WorldServer", "Error Code: %i", e.errorCode());
		return EXIT_FAILURE;
	}

	// Run migrations should any need to be run.
	MigrationRunner::RunSQLiteMigrations();

	//Get CDClient initial information
	try {
		CDClientManager::Instance();
	} catch (CppSQLite3Exception& e) {
		Game::logger->Log("WorldServer", "Failed to initialize CDServer SQLite Database");
		Game::logger->Log("WorldServer", "May be caused by corrupted file: %s", (Game::assetManager->GetResPath() / "CDServer.sqlite").string().c_str());
		Game::logger->Log("WorldServer", "Error: %s", e.errorMessage());
		Game::logger->Log("WorldServer", "Error Code: %i", e.errorCode());
		return EXIT_FAILURE;
	}

	//If the first command line argument is -a or --account then make the user
	//input a username and password, with the password being hidden.
	if (argc > 1 &&
		(strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "--account") == 0)) {
		std::string username;
		std::string password;

		std::cout << "Enter a username: ";
		std::cin >> username;

		std::unique_ptr<sql::PreparedStatement> userLookupStatement(Database::CreatePreppedStmt("SELECT id FROM accounts WHERE name=? LIMIT 1;"));
		userLookupStatement->setString(1, username.c_str());
		std::unique_ptr<sql::ResultSet> res(userLookupStatement->executeQuery());
		if (res->rowsCount() > 0) {
			Game::logger->Log("MasterServer", "Account with name \"%s\" already exists", username.c_str());
			std::cout << "Do you want to change the password of that account? [y/n]?";
			std::string prompt = "";
			std::cin >> prompt;
			if (prompt == "y" || prompt == "yes"){
				uint32_t accountId = 0;
				res->next();
				accountId = res->getUInt(1);
				if (accountId == 0) return EXIT_FAILURE;

				//Read the password from the console without echoing it.
				#ifdef __linux__
						//This function is obsolete, but it only meant to be used by the
						//sysadmin to create their first account.
						password = getpass("Enter a password: ");
				#else
						std::cout << "Enter a password: ";
						std::cin >> password;
				#endif

				// Regenerate hash based on new password
				char salt[BCRYPT_HASHSIZE];
				char hash[BCRYPT_HASHSIZE];
				int32_t bcryptState = ::bcrypt_gensalt(12, salt);
				assert(bcryptState == 0);
				bcryptState = ::bcrypt_hashpw(password.c_str(), salt, hash);
				assert(bcryptState == 0);

				std::unique_ptr<sql::PreparedStatement> userUpdateStatement(Database::CreatePreppedStmt("UPDATE accounts SET password = ? WHERE id = ?;"));
				userUpdateStatement->setString(1, std::string(hash, BCRYPT_HASHSIZE).c_str());
				userUpdateStatement->setUInt(2, accountId);
				userUpdateStatement->execute();

				Game::logger->Log("MasterServer", "Account \"%s\" password updated successfully!", username.c_str());
			} else {
				Game::logger->Log("MasterServer", "Account \"%s\" was not updated.", username.c_str());
			}
			return EXIT_SUCCESS;
		}

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
		try {
			std::unique_ptr<sql::PreparedStatement> statement(Database::CreatePreppedStmt("INSERT INTO accounts (name, password, gm_level) VALUES (?, ?, ?);"));
			statement->setString(1, username.c_str());
			statement->setString(2, std::string(hash, BCRYPT_HASHSIZE).c_str());
			statement->setInt(3, 9);
			statement->execute();
		} catch(sql::SQLException& e) {
			Game::logger->Log("MasterServer", "A SQL error occurred!:\n %s",  e.what());
			return EXIT_FAILURE;
		}

		Game::logger->Log("MasterServer", "Account created successfully!");
		return EXIT_SUCCESS;
	}

	Game::randomEngine = std::mt19937(time(0));
	uint32_t maxClients = 999;
	uint32_t ourPort = 1000;
	if (Game::config->GetValue("max_clients") != "") maxClients = std::stoi(Game::config->GetValue("max_clients"));
	if (Game::config->GetValue("port") != "") ourPort = std::stoi(Game::config->GetValue("port"));

	Game::server = new dServer(Game::config->GetValue("external_ip"), ourPort, 0, maxClients, true, false, Game::logger, "", 0, ServerType::Master, Game::config, &Game::shouldShutdown);

	//Query for the database for a server labeled "master"
	auto masterServerSock = Database::Connection->GetMasterServerIP();

	auto master_server_ip = Game::config->GetValue("master_ip");

	if (master_server_ip == "") {
		master_server_ip = Game::server->GetIP();
	}

	//If we found a server, update it's IP and port to the current one.
	if (masterServerSock.port != 0) {
		Database::Connection->SetServerIpAndPortByName("master", master_server_ip, Game::server->GetPort());
	} else {
		// If we didn't find a server, create one.
		Database::Connection->CreateServer("master", master_server_ip, Game::server->GetPort(), 0, 171023);
	}

	//Create additional objects here:
	ObjectIDManager::Instance()->Initialize(Game::logger);
	Game::im = new InstanceManager(Game::logger, Game::server->GetIP());

	//Depending on the config, start up servers:
	if (Game::config->GetValue("prestart_servers") != "" && Game::config->GetValue("prestart_servers") == "1") {
		StartChatServer();

		Game::im->GetInstance(0, false, 0);
		Game::im->GetInstance(1000, false, 0);
		StartAuthServer();
	}

	auto t = std::chrono::high_resolution_clock::now();
	Packet* packet = nullptr;
	constexpr uint32_t logFlushTime = 15 * masterFramerate;
	constexpr uint32_t sqlPingTime = 10 * 60 * masterFramerate;
	constexpr uint32_t shutdownUniverseTime = 10 * 60 * masterFramerate;
	constexpr uint32_t instanceReadyTimeout = 30 * masterFramerate;
	uint32_t framesSinceLastFlush = 0;
	uint32_t framesSinceLastSQLPing = 0;
	uint32_t framesSinceKillUniverseCommand = 0;

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
		if (framesSinceLastFlush >= logFlushTime) {
			Game::logger->Flush();
			framesSinceLastFlush = 0;
		} else
			framesSinceLastFlush++;

		//Every 10 min we ping our sql server to keep it alive hopefully:
		if (framesSinceLastSQLPing >= sqlPingTime) {
			Database::Connection->GetMasterServerIP();

			framesSinceLastSQLPing = 0;
		} else
			framesSinceLastSQLPing++;

		//10m shutdown for universe kill command
		if (Game::shouldShutdown) {
			if (framesSinceKillUniverseCommand >= shutdownUniverseTime) {
				//Break main loop and exit
				break;
			} else
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
			} else {
				affirmTimeout = 0;
			}

			instance->SetAffirmationTimeout(affirmTimeout);

			if (affirmTimeout == instanceReadyTimeout) {
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

		t += std::chrono::milliseconds(masterFrameDelta);
		std::this_thread::sleep_until(t);
	}
	return FinalizeShutdown(EXIT_SUCCESS);
}

dLogger* SetupLogger() {
	std::string logPath =
		(BinaryPathFinder::GetBinaryDir() / ("logs/MasterServer_" + std::to_string(time(nullptr)) + ".log")).string();
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
		Game::logger->Log("MasterServer", "A server has disconnected");

		//Since this disconnection is intentional, we'll just delete it as
		//we'll start a new one anyway if needed:
		Instance* instance =
			Game::im->GetInstanceBySysAddr(packet->systemAddress);
		if (instance) {
			Game::logger->Log("MasterServer", "Actually disconnected from zone %i clone %i instance %i port %i", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance->GetPort());
			Game::im->RemoveInstance(instance); //Delete the old
		}

		if (packet->systemAddress == chatServerMasterPeerSysAddr) {
			chatServerMasterPeerSysAddr = UNASSIGNED_SYSTEM_ADDRESS;
			StartChatServer();
		}

		if (packet->systemAddress == authServerMasterPeerSysAddr) {
			authServerMasterPeerSysAddr = UNASSIGNED_SYSTEM_ADDRESS;
			StartAuthServer();
		}
	}

	if (packet->data[0] == ID_CONNECTION_LOST) {
		Game::logger->Log("MasterServer", "A server has lost the connection");

		Instance* instance =
			Game::im->GetInstanceBySysAddr(packet->systemAddress);
		if (instance) {
			LWOZONEID zoneID = instance->GetZoneID(); //Get the zoneID so we can recreate a server
			Game::im->RemoveInstance(instance); //Delete the old
		}

		if (packet->systemAddress == chatServerMasterPeerSysAddr) {
			chatServerMasterPeerSysAddr = UNASSIGNED_SYSTEM_ADDRESS;
			StartChatServer();
		}

		if (packet->systemAddress == authServerMasterPeerSysAddr) {
			authServerMasterPeerSysAddr = UNASSIGNED_SYSTEM_ADDRESS;
			StartAuthServer();
		}
	}

	if (packet->length < 4) return;

	if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::MASTER) {
		switch (static_cast<eMasterMessageType>(packet->data[3])) {
		case eMasterMessageType::REQUEST_PERSISTENT_ID: {
			Game::logger->Log("MasterServer", "A persistent ID req");
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);
			uint64_t requestID = 0;
			inStream.Read(requestID);

			uint32_t objID = ObjectIDManager::Instance()->GeneratePersistentID();
			MasterPackets::SendPersistentIDResponse(Game::server, packet->systemAddress, requestID, objID);
			break;
		}

		case eMasterMessageType::REQUEST_ZONE_TRANSFER: {
			Game::logger->Log("MasterServer", "Received zone transfer req");
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
			if (shutdownSequenceStarted) {
				Game::logger->Log("MasterServer", "Shutdown sequence has been started.  Not creating a new zone.");
				break;
			}
			Instance* in = Game::im->GetInstance(zoneID, false, zoneClone);

			for (auto* instance : Game::im->GetInstances()) {
				Game::logger->Log("MasterServer", "Instance: %i/%i/%i -> %i", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance == in);
			}

			if (in && !in->GetIsReady()) //Instance not ready, make a pending request
			{
				in->GetPendingRequests().push_back({ requestID, static_cast<bool>(mythranShift), packet->systemAddress });
				Game::logger->Log("MasterServer", "Server not ready, adding pending request %llu %i %i", requestID, zoneID, zoneClone);
				break;
			}

			//Instance is ready, transfer
			Game::logger->Log("MasterServer", "Responding to transfer request %llu for zone %i %i", requestID, zoneID, zoneClone);
			Game::im->RequestAffirmation(in, { requestID, static_cast<bool>(mythranShift), packet->systemAddress });
			break;
		}

		case eMasterMessageType::SERVER_INFO: {
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
			} else {
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

			if (theirServerType == ServerType::Auth) {
				SystemAddress copy;
				copy.binaryAddress = packet->systemAddress.binaryAddress;
				copy.port = packet->systemAddress.port;

				authServerMasterPeerSysAddr = copy;
			}

			Game::logger->Log("MasterServer", "Received server info, instance: %i port: %i", theirInstanceID, theirPort);

			break;
		}

		case eMasterMessageType::SET_SESSION_KEY: {
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
					BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::NEW_SESSION_ALERT);
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
			Game::logger->Log("MasterServer", "Got sessionKey %i for user %s", sessionKey, username.c_str());
			break;
		}

		case eMasterMessageType::REQUEST_SESSION_KEY: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);
			std::string username = PacketUtils::ReadString(8, packet, false);

			for (auto key : activeSessions) {
				if (key.second == username) {
					CBITSTREAM;
					BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::SESSION_KEY_RESPONSE);
					bitStream.Write(key.first);
					bitStream.Write(LUString(key.second, 64));
					Game::server->Send(&bitStream, packet->systemAddress, false);
					break;
				}
			}
			break;
		}

		case eMasterMessageType::PLAYER_ADDED: {
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
			} else {
				printf("Instance missing? What?");
			}
			break;
		}

		case eMasterMessageType::PLAYER_REMOVED: {
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

		case eMasterMessageType::CREATE_PRIVATE_ZONE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			uint32_t mapId;
			LWOCLONEID cloneId;
			std::string password;

			inStream.Read(mapId);
			inStream.Read(cloneId);

			uint32_t len;
			inStream.Read<uint32_t>(len);
			for (uint32_t i = 0; len > i; i++) {
				char character;
				inStream.Read<char>(character);
				password += character;
			}

			Game::im->CreatePrivateInstance(mapId, cloneId, password.c_str());

			break;
		}

		case eMasterMessageType::REQUEST_PRIVATE_ZONE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			uint64_t requestID = 0;
			uint8_t mythranShift = false;

			std::string password;

			inStream.Read(requestID);
			inStream.Read(mythranShift);

			uint32_t len;
			inStream.Read<uint32_t>(len);

			for (uint32_t i = 0; i < len; i++) {
				char character; inStream.Read<char>(character);
				password += character;
			}

			auto* instance = Game::im->FindPrivateInstance(password.c_str());

			Game::logger->Log("MasterServer", "Join private zone: %llu %d %s %p", requestID, mythranShift, password.c_str(), instance);

			if (instance == nullptr) {
				return;
			}

			const auto& zone = instance->GetZoneID();

			MasterPackets::SendZoneTransferResponse(Game::server, packet->systemAddress, requestID, (bool)mythranShift, zone.GetMapID(), instance->GetInstanceID(), zone.GetCloneID(), instance->GetIP(), instance->GetPort());

			break;
		}

		case eMasterMessageType::WORLD_READY: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			LWOMAPID zoneID;
			LWOINSTANCEID instanceID;

			inStream.Read(zoneID);
			inStream.Read(instanceID);

			Game::logger->Log("MasterServer", "Got world ready %i %i", zoneID, instanceID);

			auto* instance = Game::im->FindInstance(zoneID, instanceID);

			if (instance == nullptr) {
				Game::logger->Log("MasterServer", "Failed to find zone to ready");
				return;
			}

			Game::logger->Log("MasterServer", "Ready zone %i", zoneID);
			Game::im->ReadyInstance(instance);
			break;
		}

		case eMasterMessageType::PREP_ZONE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			int32_t zoneID;
			inStream.Read(zoneID);
			if (shutdownSequenceStarted) {
				Game::logger->Log("MasterServer", "Shutdown sequence has been started.  Not prepping a new zone.");
				break;
			} else {
				Game::logger->Log("MasterServer", "Prepping zone %i", zoneID);
				Game::im->GetInstance(zoneID, false, 0);
			}
			break;
		}

		case eMasterMessageType::AFFIRM_TRANSFER_RESPONSE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			uint64_t requestID;

			inStream.Read(requestID);

			Game::logger->Log("MasterServer", "Got affirmation of transfer %llu", requestID);

			auto* instance = Game::im->GetInstanceBySysAddr(packet->systemAddress);

			if (instance == nullptr)
				return;

			Game::im->AffirmTransfer(instance, requestID);
			Game::logger->Log("MasterServer", "Affirmation complete %llu", requestID);
			break;
		}

		case eMasterMessageType::SHUTDOWN_RESPONSE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			auto* instance = Game::im->GetInstanceBySysAddr(packet->systemAddress);

			if (instance == nullptr) {
				return;
			}

			Game::logger->Log("MasterServer", "Got shutdown response from zone %i clone %i instance %i port %i", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance->GetPort());
			instance->SetIsShuttingDown(true);
			break;
		}

		case eMasterMessageType::SHUTDOWN_UNIVERSE: {
			Game::logger->Log("MasterServer", "Received shutdown universe command, shutting down in 10 minutes.");
			Game::shouldShutdown = true;
			break;
		}

		default:
			Game::logger->Log("MasterServer", "Unknown master packet ID from server: %i", packet->data[3]);
		}
	}
}

void StartChatServer() {
	if (Game::shouldShutdown) {
		Game::logger->Log("MasterServer", "Currently shutting down.  Chat will not be restarted.");
		return;
	}
#ifdef __APPLE__
	//macOS doesn't need sudo to run on ports < 1024
	auto result = system(((BinaryPathFinder::GetBinaryDir() / "ChatServer").string() + "&").c_str());
#elif _WIN32
	auto result = system(("start " + (BinaryPathFinder::GetBinaryDir() / "ChatServer.exe").string()).c_str());
#else
	if (std::atoi(Game::config->GetValue("use_sudo_chat").c_str())) {
		auto result = system(("sudo " + (BinaryPathFinder::GetBinaryDir() / "ChatServer").string() + "&").c_str());
	} else {
		auto result = system(((BinaryPathFinder::GetBinaryDir() / "ChatServer").string() + "&").c_str());
}
#endif
}

void StartAuthServer() {
	if (Game::shouldShutdown) {
		Game::logger->Log("MasterServer", "Currently shutting down.  Auth will not be restarted.");
		return;
	}
#ifdef __APPLE__
	auto result = system(((BinaryPathFinder::GetBinaryDir() / "AuthServer").string() + "&").c_str());
#elif _WIN32
	auto result = system(("start " + (BinaryPathFinder::GetBinaryDir() / "AuthServer.exe").string()).c_str());
#else
	if (std::atoi(Game::config->GetValue("use_sudo_auth").c_str())) {
		auto result = system(("sudo " + (BinaryPathFinder::GetBinaryDir() / "AuthServer").string() + "&").c_str());
	} else {
		auto result = system(((BinaryPathFinder::GetBinaryDir() / "AuthServer").string() + "&").c_str());
}
#endif
}

void ShutdownSequence(int32_t signal) {
	if (shutdownSequenceStarted) {
		return;
	}

	if (!Game::im) {
		FinalizeShutdown(EXIT_FAILURE);
	}

	Game::im->SetIsShuttingDown(true);
	shutdownSequenceStarted = true;
	Game::shouldShutdown = true;

	{
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::SHUTDOWN);
		Game::server->Send(&bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
		Game::logger->Log("MasterServer", "Triggered master shutdown");
	}

	auto* objIdManager = ObjectIDManager::TryInstance();
	if (objIdManager) {
		objIdManager->SaveToDatabase();
		Game::logger->Log("MasterServer", "Saved ObjectIDTracker to DB");
	}

	// A server might not be finished spinning up yet, remove all of those here.
	for (auto* instance : Game::im->GetInstances()) {
		if (!instance->GetIsReady()) {
			Game::im->RemoveInstance(instance);
		}
	}

	for (auto* instance : Game::im->GetInstances()) {
		instance->SetIsShuttingDown(true);
	}

	Game::logger->Log("MasterServer", "Attempting to shutdown instances, max 60 seconds...");

	auto t = std::chrono::high_resolution_clock::now();
	uint32_t framesSinceShutdownStart = 0;
	constexpr uint32_t maxShutdownTime = 60 * mediumFramerate;
	bool allInstancesShutdown = false;
	Packet* packet = nullptr;
	while (true) {
		packet = Game::server->Receive();
		if (packet) {
			HandlePacket(packet);
			Game::server->DeallocatePacket(packet);
			packet = nullptr;
		}

		allInstancesShutdown = true;

		for (auto* instance : Game::im->GetInstances()) {
			if (instance == nullptr) {
				continue;
			}

			if (!instance->GetShutdownComplete()) {
				allInstancesShutdown = false;
			}
		}

		if (allInstancesShutdown && authServerMasterPeerSysAddr == UNASSIGNED_SYSTEM_ADDRESS && chatServerMasterPeerSysAddr == UNASSIGNED_SYSTEM_ADDRESS) {
			Game::logger->Log("MasterServer", "Finished shutting down MasterServer!");
			break;
		}

		t += std::chrono::milliseconds(mediumFrameDelta);
		std::this_thread::sleep_until(t);

		framesSinceShutdownStart++;

		if (framesSinceShutdownStart == maxShutdownTime) {
			Game::logger->Log("MasterServer", "Finished shutting down by timeout!");
			break;
		}
	}

	FinalizeShutdown(signal);
}

int32_t FinalizeShutdown(int32_t signal) {
	// Delete our objects here
	Database::Destroy();
	if (Game::config) delete Game::config;
	if (Game::im) delete Game::im;
	if (Game::server) delete Game::server;
	if (Game::logger) delete Game::logger;

	if (signal != EXIT_SUCCESS) exit(signal);
	return signal;
}
