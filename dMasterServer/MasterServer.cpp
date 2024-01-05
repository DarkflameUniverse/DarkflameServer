#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <fstream>

#include <bcrypt/BCrypt.hpp>

#include <csignal>

//DLU Includes:
#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "Database.h"
#include "MigrationRunner.h"
#include "Diagnostics.h"
#include "dCommonVars.h"
#include "dConfig.h"
#include "Logger.h"
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
#include "Start.h"

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	InstanceManager* im = nullptr;
	dConfig* config = nullptr;
	AssetManager* assetManager = nullptr;
	Game::signal_t lastSignal = 0;
	bool universeShutdownRequested = false;
	std::mt19937 randomEngine;
} //namespace Game

bool shutdownSequenceStarted = false;
int ShutdownSequence(int32_t signal = -1);
int32_t FinalizeShutdown(int32_t signal = -1);
Logger* SetupLogger();
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
	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);

	//Create all the objects we need to run our service:
	Game::logger = SetupLogger();
	if (!Game::logger) return EXIT_FAILURE;

	if (!dConfig::Exists("authconfig.ini")) LOG("Could not find authconfig.ini, using default settings");
	if (!dConfig::Exists("chatconfig.ini")) LOG("Could not find chatconfig.ini, using default settings");
	if (!dConfig::Exists("masterconfig.ini")) LOG("Could not find masterconfig.ini, using default settings");
	if (!dConfig::Exists("sharedconfig.ini")) LOG("Could not find sharedconfig.ini, using default settings");
	if (!dConfig::Exists("worldconfig.ini")) LOG("Could not find worldconfig.ini, using default settings");

	Game::config = new dConfig("masterconfig.ini");
	Game::logger->SetLogToConsole(Game::config->GetValue("log_to_console") != "0");
	Game::logger->SetLogDebugStatements(Game::config->GetValue("log_debug_statements") == "1");

	uint32_t clientNetVersion = 171022;
	const auto clientNetVersionString = Game::config->GetValue("client_net_version");
	if (!clientNetVersionString.empty()) GeneralUtils::TryParse(clientNetVersionString, clientNetVersion);

	LOG("Using net version %i", clientNetVersion);

	LOG("Starting Master server...");
	LOG("Version: %s", PROJECT_VERSION);
	LOG("Compiled on: %s", __TIMESTAMP__);

	//Connect to the MySQL Database
	try {
		Database::Connect();
	} catch (sql::SQLException& ex) {
		LOG("Got an error while connecting to the database: %s", ex.what());
		LOG("Migrations not run");
		return EXIT_FAILURE;
	}

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
		LOG("Is the provided client_location in Windows Onedrive? If so, remove it from Onedrive.");
		return EXIT_FAILURE;
	}

	MigrationRunner::RunMigrations();
	const auto resServerPath = BinaryPathFinder::GetBinaryDir() / "resServer";
	const bool cdServerExists = std::filesystem::exists(resServerPath / "CDServer.sqlite");
	const bool oldCDServerExists = std::filesystem::exists(Game::assetManager->GetResPath() / "CDServer.sqlite");
	const bool fdbExists = std::filesystem::exists(Game::assetManager->GetResPath() / "cdclient.fdb");
	const bool resServerPathExists = std::filesystem::is_directory(resServerPath);

	if (!resServerPathExists) {
		LOG("%s does not exist, creating it.", (resServerPath).c_str());
		if(!std::filesystem::create_directories(resServerPath)){
			LOG("Failed to create %s", (resServerPath).string().c_str());
			return EXIT_FAILURE;
		}
	}

	if (!cdServerExists) {
		if (oldCDServerExists) {
			// If the file doesn't exist in the new CDServer location, copy it there.  We copy because we may not have write permissions from the previous directory.
			LOG("CDServer.sqlite is not located at resServer, but is located at res path.  Copying file...");
			std::filesystem::copy_file(Game::assetManager->GetResPath() / "CDServer.sqlite", resServerPath / "CDServer.sqlite");
		} else {
			LOG("%s could not be found in resServer or res. Looking for %s to convert to sqlite.",
				(resServerPath / "CDServer.sqlite").string().c_str(),
				(Game::assetManager->GetResPath() / "cdclient.fdb").string().c_str());

			auto cdclientStream = Game::assetManager->GetFile("cdclient.fdb");
			if (!cdclientStream) {
				LOG("Failed to load %s", (Game::assetManager->GetResPath() / "cdclient.fdb").string().c_str());
				throw std::runtime_error("Aborting initialization due to missing cdclient.fdb.");
			}

			LOG("Found %s.  Converting to SQLite", (Game::assetManager->GetResPath() / "cdclient.fdb").string().c_str());
			Game::logger->Flush();

			if (FdbToSqlite::Convert(resServerPath.string()).ConvertDatabase(cdclientStream) == false) {
				LOG("Failed to convert fdb to sqlite.");
				return EXIT_FAILURE;
			}
		}
	}

	//Connect to CDClient
	try {
		CDClientDatabase::Connect((BinaryPathFinder::GetBinaryDir() / "resServer" / "CDServer.sqlite").string());
	} catch (CppSQLite3Exception& e) {
		LOG("Unable to connect to CDServer SQLite Database");
		LOG("Error: %s", e.errorMessage());
		LOG("Error Code: %i", e.errorCode());
		return EXIT_FAILURE;
	}

	// Run migrations should any need to be run.
	MigrationRunner::RunSQLiteMigrations();

	//Get CDClient initial information
	try {
		CDClientManager::Instance();
	} catch (CppSQLite3Exception& e) {
		LOG("Failed to initialize CDServer SQLite Database");
		LOG("May be caused by corrupted file: %s", (Game::assetManager->GetResPath() / "CDServer.sqlite").string().c_str());
		LOG("Error: %s", e.errorMessage());
		LOG("Error Code: %i", e.errorCode());
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

		auto accountId = Database::Get()->GetAccountInfo(username);
		if (accountId) {
			LOG("Account with name \"%s\" already exists", username.c_str());
			std::cout << "Do you want to change the password of that account? [y/n]?";
			std::string prompt = "";
			std::cin >> prompt;
			if (prompt == "y" || prompt == "yes") {
				if (accountId->id == 0) return EXIT_FAILURE;

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

				Database::Get()->UpdateAccountPassword(accountId->id, std::string(hash, BCRYPT_HASHSIZE));

				LOG("Account \"%s\" password updated successfully!", username.c_str());
			} else {
				LOG("Account \"%s\" was not updated.", username.c_str());
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
			Database::Get()->InsertNewAccount(username, std::string(hash, BCRYPT_HASHSIZE));
		} catch(sql::SQLException& e) {
			LOG("A SQL error occurred!:\n %s",  e.what());
			return EXIT_FAILURE;
		}

		LOG("Account created successfully!");
		return EXIT_SUCCESS;
	}

	Game::randomEngine = std::mt19937(time(0));
	uint32_t maxClients = 999;
	uint32_t ourPort = 2000;
	std::string ourIP = "localhost";
	const auto maxClientsString = Game::config->GetValue("max_clients");
	if (!maxClientsString.empty()) maxClients = std::stoi(maxClientsString);
	const auto masterServerPortString = Game::config->GetValue("master_server_port");
	if (!masterServerPortString.empty()) ourPort = std::atoi(masterServerPortString.c_str());
	const auto externalIPString = Game::config->GetValue("external_ip");
	if (!externalIPString.empty()) ourIP = externalIPString;

	Game::server = new dServer(ourIP, ourPort, 0, maxClients, true, false, Game::logger, "", 0, ServerType::Master, Game::config, &Game::lastSignal);

	std::string master_server_ip = "localhost";
	const auto masterServerIPString = Game::config->GetValue("master_ip");
	if (!masterServerIPString.empty()) master_server_ip = masterServerIPString;

	if (master_server_ip == "") master_server_ip = Game::server->GetIP();

	Database::Get()->SetMasterIp(master_server_ip, Game::server->GetPort());

	//Create additional objects here:
	ObjectIDManager::Instance()->Initialize(Game::logger);
	Game::im = new InstanceManager(Game::logger, Game::server->GetIP());

	//Depending on the config, start up servers:
	if (Game::config->GetValue("prestart_servers") != "0") {
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

	Game::logger->Flush();
	while (!Game::ShouldShutdown()) {
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
			//Find out the master's IP for absolutely no reason:
			std::string masterIP;
			uint32_t masterPort;
			auto masterInfo = Database::Get()->GetMasterInfo();
			if (masterInfo) {
				masterIP = masterInfo->ip;
				masterPort = masterInfo->port;
			}

			framesSinceLastSQLPing = 0;
		} else
			framesSinceLastSQLPing++;

		//10m shutdown for universe kill command
		if (Game::universeShutdownRequested) {
			if (framesSinceKillUniverseCommand >= shutdownUniverseTime) {
				//Break main loop and exit
				Game::lastSignal = -1;
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
	return ShutdownSequence(EXIT_SUCCESS);
}

Logger* SetupLogger() {
	std::string logPath =
		(BinaryPathFinder::GetBinaryDir() / ("logs/MasterServer_" + std::to_string(time(nullptr)) + ".log")).string();
	bool logToConsole = false;
	bool logDebugStatements = false;
#ifdef _DEBUG
	logToConsole = true;
	logDebugStatements = true;
#endif

	return new Logger(logPath, logToConsole, logDebugStatements);
}

void HandlePacket(Packet* packet) {
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION) {
		LOG("A server has disconnected");

		//Since this disconnection is intentional, we'll just delete it as
		//we'll start a new one anyway if needed:
		Instance* instance =
			Game::im->GetInstanceBySysAddr(packet->systemAddress);
		if (instance) {
			LOG("Actually disconnected from zone %i clone %i instance %i port %i", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance->GetPort());
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
		LOG("A server has lost the connection");

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
			LOG("A persistent ID req");
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);
			uint64_t requestID = 0;
			inStream.Read(requestID);

			uint32_t objID = ObjectIDManager::Instance()->GeneratePersistentID();
			MasterPackets::SendPersistentIDResponse(Game::server, packet->systemAddress, requestID, objID);
			break;
		}

		case eMasterMessageType::REQUEST_ZONE_TRANSFER: {
			LOG("Received zone transfer req");
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
				LOG("Shutdown sequence has been started.  Not creating a new zone.");
				break;
			}
			Instance* in = Game::im->GetInstance(zoneID, false, zoneClone);

			for (auto* instance : Game::im->GetInstances()) {
				LOG("Instance: %i/%i/%i -> %i", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance == in);
			}

			if (in && !in->GetIsReady()) //Instance not ready, make a pending request
			{
				in->GetPendingRequests().push_back({ requestID, static_cast<bool>(mythranShift), packet->systemAddress });
				LOG("Server not ready, adding pending request %llu %i %i", requestID, zoneID, zoneClone);
				break;
			}

			//Instance is ready, transfer
			LOG("Responding to transfer request %llu for zone %i %i", requestID, zoneID, zoneClone);
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

			if (theirServerType == ServerType::World) {
				if (!Game::im->IsPortInUse(theirPort)) {
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

			LOG("Received server info, instance: %i port: %i", theirInstanceID, theirPort);

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
			LOG("Got sessionKey %i for user %s", sessionKey, username.c_str());
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

			LOG("Join private zone: %llu %d %s %p", requestID, mythranShift, password.c_str(), instance);

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

			LOG("Got world ready %i %i", zoneID, instanceID);

			auto* instance = Game::im->FindInstance(zoneID, instanceID);

			if (instance == nullptr) {
				LOG("Failed to find zone to ready");
				return;
			}

			LOG("Ready zone %i", zoneID);
			Game::im->ReadyInstance(instance);
			break;
		}

		case eMasterMessageType::PREP_ZONE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			int32_t zoneID;
			inStream.Read(zoneID);
			if (shutdownSequenceStarted) {
				LOG("Shutdown sequence has been started.  Not prepping a new zone.");
				break;
			} else {
				LOG("Prepping zone %i", zoneID);
				Game::im->GetInstance(zoneID, false, 0);
			}
			break;
		}

		case eMasterMessageType::AFFIRM_TRANSFER_RESPONSE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			uint64_t requestID;

			inStream.Read(requestID);

			LOG("Got affirmation of transfer %llu", requestID);

			auto* instance = Game::im->GetInstanceBySysAddr(packet->systemAddress);

			if (instance == nullptr)
				return;

			Game::im->AffirmTransfer(instance, requestID);
			LOG("Affirmation complete %llu", requestID);
			break;
		}

		case eMasterMessageType::SHUTDOWN_RESPONSE: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);

			auto* instance = Game::im->GetInstanceBySysAddr(packet->systemAddress);

			if (instance == nullptr) {
				return;
			}

			LOG("Got shutdown response from zone %i clone %i instance %i port %i", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance->GetPort());
			instance->SetIsShuttingDown(true);
			break;
		}

		case eMasterMessageType::SHUTDOWN_UNIVERSE: {
			LOG("Received shutdown universe command, shutting down in 10 minutes.");
			Game::universeShutdownRequested = true;
			break;
		}

		default:
			LOG("Unknown master packet ID from server: %i", packet->data[3]);
		}
	}
}

int ShutdownSequence(int32_t signal) {
	if (!Game::logger) return -1;
	LOG("Recieved Signal %d", signal);
	if (shutdownSequenceStarted) {
		LOG("Duplicate Shutdown Sequence");
		return -1;
	}

	if (!Game::im) {
		FinalizeShutdown(EXIT_FAILURE);
	}

	Game::im->SetIsShuttingDown(true);
	shutdownSequenceStarted = true;
	Game::lastSignal = -1;

	{
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::SHUTDOWN);
		Game::server->Send(&bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
		LOG("Triggered master shutdown");
	}

	auto* objIdManager = ObjectIDManager::TryInstance();
	if (objIdManager) {
		objIdManager->SaveToDatabase();
		LOG("Saved ObjectIDTracker to DB");
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

	LOG("Attempting to shutdown instances, max 60 seconds...");

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
			LOG("Finished shutting down MasterServer!");
			break;
		}

		t += std::chrono::milliseconds(mediumFrameDelta);
		std::this_thread::sleep_until(t);

		framesSinceShutdownStart++;

		if (framesSinceShutdownStart == maxShutdownTime) {
			LOG("Finished shutting down by timeout!");
			break;
		}
	}

	return FinalizeShutdown(signal);
}

int32_t FinalizeShutdown(int32_t signal) {
	//Delete our objects here:
	Database::Destroy("MasterServer");
	if (Game::config) delete Game::config;
	Game::config = nullptr;
	if (Game::im) delete Game::im;
	Game::im = nullptr;
	if (Game::server) delete Game::server;
	Game::server = nullptr;
	if (Game::logger) delete Game::logger;
	Game::logger = nullptr;

	if (signal != EXIT_SUCCESS) exit(signal);
	return signal;
}
