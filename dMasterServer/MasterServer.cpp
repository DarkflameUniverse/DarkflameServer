#include "master/PlayerAction.h"
#include "master/DashboardMessages.h"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <set>
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
#include "ConfigSync.h"
#include "Diagnostics.h"
#include "dCommonVars.h"
#include "dConfig.h"
#include "Logger.h"
#include "dServer.h"
#include "AssetManager.h"
#include "BinaryPathFinder.h"
#include "ServiceType.h"
#include "MessageType/Master.h"

//RakNet includes:
#include "RakNetDefines.h"

//Packet includes:

#include "AuthPackets.h"
#include "Game.h"
#include "InstanceManager.h"
#include "MigrationCoordinator.h"
#include "MasterPackets.h"
#include "FdbToSqlite.h"
#include "BitStreamUtils.h"
#include "Start.h"
#include "Server.h"
#include "CDZoneTableTable.h"
#include "eGameMasterLevel.h"
#include "StringifiedEnum.h"
#include "PacketDispatcher.h"
#include "master/DataChanged.h"
#include "master/MessageCapture.h"
#include "master/InstanceMigration.h"
#include "master/ServerTraffic.h"

#ifdef DARKFLAME_PLATFORM_UNIX

#include <sys/types.h>
#include <sys/wait.h>

#endif

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

namespace {
	std::string g_ServiceName;
}

bool shutdownSequenceStarted = false;
int ShutdownSequence(int32_t signal = -1);
int32_t FinalizeShutdown(int32_t signal = -1);
void HandlePacket(Packet* packet);
std::map<uint32_t, std::string> activeSessions;
SystemAddress authServerMasterPeerSysAddr;
SystemAddress chatServerMasterPeerSysAddr;
SystemAddress dashboardServerMasterPeerSysAddr;
SystemAddress ugcServerMasterPeerSysAddr;
// The UGC server's process id from its last start (0: not started), for the dashboard
uint32_t ugcServerPid = 0;

namespace {
	// Every server for the dashboard: auth, chat, the UGC server and the worlds, including those launched but not
	// connected yet (starting) and those shutting down
	MasterPackets::ServerListResponse BuildServerList() {
		using eState = MasterPackets::ServerListResponse::eState;
		MasterPackets::ServerListResponse response;
		response.authOnline = authServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS ? 1 : 0;
		response.chatOnline = chatServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS ? 1 : 0;
		response.ugcEnabled = Game::config->GetValue("enable_ugc_server") == "1" ? 1 : 0;
		response.ugcOnline = ugcServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS ? 1 : 0;
		response.ugcPid = ugcServerPid;
		if (!Game::im) return response;
		for (const auto& inst : Game::im->GetInstances()) {
			if (!inst || inst->GetShutdownComplete()) continue;
			auto& entry = response.instances.emplace_back();
			entry.mapID = inst->GetMapID();
			entry.instanceID = inst->GetInstanceID();
			entry.cloneID = inst->GetCloneID();
			entry.players = static_cast<uint32_t>(inst->GetCurrentClientCount());
			entry.ip = LUString(inst->GetIP());
			entry.port = inst->GetPort();
			entry.isPrivate = inst->GetIsPrivate() ? 1 : 0;
			entry.state = inst->GetIsShuttingDown() ? eState::STOPPING : inst->GetIsReady() ? eState::READY : eState::STARTING;
		}
		return response;
	}

	// Tells the dashboard at once when a world is launched, connects or goes away (it also asks every 30 seconds)
	void PushServerListToDashboard() {
		if (dashboardServerMasterPeerSysAddr == UNASSIGNED_SYSTEM_ADDRESS) return;
		MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, BuildServerList());
	}

	// Dashboard player actions waiting for world servers to answer
	struct PendingPlayerAction {
		ePlayerAction action{};
		std::set<SystemAddress> waitingOn;
		uint32_t affected{};
		std::chrono::steady_clock::time_point deadline;
	};
	constexpr auto PLAYER_ACTION_TIMEOUT = std::chrono::seconds(3);
	std::map<uint32_t, PendingPlayerAction> g_PendingPlayerActions;

	void FinishPlayerAction(uint32_t requestId, bool timedOut) {
		const auto it = g_PendingPlayerActions.find(requestId);
		if (it == g_PendingPlayerActions.end()) return;

		if (dashboardServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
			PlayerActionResult result;
			result.requestId = requestId;
			result.action = it->second.action;
			result.affected = it->second.affected;
			result.timedOut = timedOut;

			MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, result);
		}
		g_PendingPlayerActions.erase(it);
	}

	void CheckPlayerActionTimeouts() {
		const auto now = std::chrono::steady_clock::now();
		std::vector<uint32_t> expired;
		for (const auto& [id, pending] : g_PendingPlayerActions) {
			if (now >= pending.deadline) expired.push_back(id);
		}
		for (const auto id : expired) FinishPlayerAction(id, true);
	}
}

int GenerateBCryptPassword(const std::string& password, const int workFactor, char salt[BCRYPT_HASHSIZE], char hash[BCRYPT_HASHSIZE]) {
	int32_t bcryptState = ::bcrypt_gensalt(workFactor, salt);
	assert(bcryptState == 0);
	bcryptState = ::bcrypt_hashpw(password.c_str(), salt, hash);
	assert(bcryptState == 0);
	return 0;
}

int main(int argc, char** argv) {
	constexpr uint32_t masterFramerate = mediumFramerate;
	constexpr uint32_t masterFrameDelta = mediumFrameDelta;
	const auto curTimeStr = std::to_string(time(nullptr));
	g_ServiceName = "MasterServer_" + curTimeStr;
	Diagnostics::SetProcessName(g_ServiceName);
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

#if defined(_WIN32) && defined(MARIADB_PLUGIN_DIR_OVERRIDE)
	_putenv_s("MARIADB_PLUGIN_DIR", MARIADB_PLUGIN_DIR_OVERRIDE);
#endif

	//Triggers the shutdown sequence at application exit
	std::atexit([]() { ShutdownSequence(); });
	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);

	Game::config = new dConfig("masterconfig.ini");

	//Create all the objects we need to run our service:
	Server::SetupLogger(g_ServiceName, "MasterServer");
	if (!Game::logger) return EXIT_FAILURE;
	Game::config->LogSettings();

	auto folders = { "navmeshes", "migrations", "vanity" };

	for (const auto folder : folders) {
		if (!std::filesystem::exists(BinaryPathFinder::GetBinaryDir() / folder)) {
			std::string msg = "The (" +
				std::string(folder) +
				") folder was not copied to the binary directory. Please copy the (" +
				std::string(folder) +
				") folder from your download to the binary directory or re-run cmake.";
			LOG("%s", msg.c_str());
			// toss an error box up for windows users running the download
#ifdef DARKFLAME_PLATFORM_WIN32
			MessageBoxA(nullptr, msg.c_str(), "Missing Folder", MB_OK | MB_ICONERROR);
#endif
			return EXIT_FAILURE;
		}
	}

	if (!dConfig::Exists("authconfig.ini")) LOG("Could not find authconfig.ini, using default settings");
	if (!dConfig::Exists("chatconfig.ini")) LOG("Could not find chatconfig.ini, using default settings");
	if (!dConfig::Exists("masterconfig.ini")) LOG("Could not find masterconfig.ini, using default settings");
	if (!dConfig::Exists("sharedconfig.ini")) LOG("Could not find sharedconfig.ini, using default settings");
	if (!dConfig::Exists("worldconfig.ini")) LOG("Could not find worldconfig.ini, using default settings");


	const auto clientNetVersionString = Game::config->GetValue("client_net_version");
	const uint32_t clientNetVersion = GeneralUtils::TryParse<uint32_t>(clientNetVersionString).value_or(171022);

	LOG("Using net version %i", clientNetVersion);

	LOG("Starting Master server...");
	LOG("Version: %s", PROJECT_VERSION);
	LOG("Compiled on: %s", __TIMESTAMP__);

	//Connect to the MySQL Database
	try {
		Database::Connect();
	} catch (std::exception& ex) {
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
	// Settings edited on the dashboard (server_config table) are layered over the files from here on
	Game::config->SetDatabaseSync(ConfigSync::Sync);
	Database::Get()->Commit();
	const auto resServerPath = BinaryPathFinder::GetBinaryDir() / "resServer";
	std::filesystem::create_directories(resServerPath);
	const bool cdServerExists = std::filesystem::exists(resServerPath / "CDServer.sqlite");
	const bool oldCDServerExists = std::filesystem::exists(Game::assetManager->GetResPath() / "CDServer.sqlite");
	const bool fdbExists = std::filesystem::exists(Game::assetManager->GetResPath() / "cdclient.fdb");
	const bool resServerPathExists = std::filesystem::is_directory(resServerPath);

	if (!resServerPathExists) {
		LOG("%s does not exist, creating it.", (resServerPath).c_str());
		if (!std::filesystem::create_directories(resServerPath)) {
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

	// Check for the --migrations-only flag
	if ((argc > 1 &&
		(strcmp(argv[1], "--migrations-only") == 0 || strcmp(argv[1], "-m") == 0))) {
		LOG("Migrations only flag detected.  Exiting.");
		return EXIT_SUCCESS;
	}

	//If the first command line argument is -a or --account then make the user
	//input a username and password, with the password being hidden.
	bool createAccount = Database::Get()->GetAccountCount() == 0 && Game::config->GetValue("skip_account_creation") != "1";
	if (createAccount) {
		LOG("No accounts exist in the database.  Please create an account.");
	}
	if ((argc > 1 &&
		(strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "--account") == 0)) || createAccount) {
		std::string username;
		std::string password;

		std::cout << "Enter a username: ";
		std::cin >> username;

		const auto checkIsAdmin = []() {
			std::string admin;
			std::cout << "What level of privilege should this account have? Please enter a number between 0 (Player) and 9 (Admin) inclusive. No entry will default to 0." << std::endl;
			std::cin >> admin;
			return admin;
			};

		auto accountId = Database::Get()->GetAccountInfo(username);
		if (accountId && accountId->id != 0) {
			LOG("Account with name \"%s\" already exists", username.c_str());
			std::cout << "Do you want to change the password of that account? [y/n]?";
			std::string prompt = "";
			std::cin >> prompt;
			if (prompt == "y" || prompt == "yes") {
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
				int res = GenerateBCryptPassword(password, 12, salt, hash);
				assert(res == 0);

				Database::Get()->UpdateAccountPassword(accountId->id, std::string(hash, BCRYPT_HASHSIZE));

				LOG("Account \"%s\" password updated successfully!", username.c_str());
			} else {
				LOG("Account \"%s\" was not updated.", username.c_str());
			}

			std::cout << "Update admin privileges? [y/n]? ";
			std::string admin;
			std::cin >> admin;
			bool updateAdmin = admin == "y" || admin == "yes";
			if (updateAdmin) {
				auto gmLevel = GeneralUtils::TryParse<int32_t>(checkIsAdmin()).value_or(0);
				if (gmLevel > 9 || gmLevel < 0) {
					LOG("Invalid admin level.  Defaulting to 0");
					gmLevel = 0;
				}
				Database::Get()->UpdateAccountGmLevel(accountId->id, static_cast<eGameMasterLevel>(gmLevel));
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
		int res = GenerateBCryptPassword(password, 12, salt, hash);
		assert(res == 0);

		//Create account
		try {
			// Accounts created from the command line are server operators
			Database::Get()->InsertNewAccount(username, std::string(hash, BCRYPT_HASHSIZE), eGameMasterLevel::OPERATOR);
		} catch (std::exception& e) {
			LOG("A SQL error occurred!:\n %s", e.what());
			return EXIT_FAILURE;
		}

		LOG("Account created successfully!");

		accountId = Database::Get()->GetAccountInfo(username);
		if (accountId) {
			auto gmLevel = GeneralUtils::TryParse<int32_t>(checkIsAdmin()).value_or(0);
			if (gmLevel > 9 || gmLevel < 0) {
				LOG("Invalid admin level.  Defaulting to 0");
				gmLevel = 0;
			}
			Database::Get()->UpdateAccountGmLevel(accountId->id, static_cast<eGameMasterLevel>(gmLevel));
		}

		return EXIT_SUCCESS;
	}

	Game::randomEngine = std::mt19937(time(0));
	uint32_t maxClients = Game::config->GetValue("max_clients", 999);
	uint32_t ourPort = Game::config->GetValue("master_server_port", 2000);
	std::string ourIP = Game::config->GetValue("external_ip", "localhost");

	char salt[BCRYPT_HASHSIZE];
	char hash[BCRYPT_HASHSIZE];
	const auto& cfgPassword = Game::config->GetValue<std::string>("master_password", "3.25DARKFLAME1");
	int res = GenerateBCryptPassword(cfgPassword, 13, salt, hash);
	assert(res == 0);

	Game::server = new dServer(ourIP, ourPort, 0, maxClients, true, false, Game::logger, "", 0, ServiceType::MASTER, Game::config, &Game::lastSignal, hash);
	// Master has no master to send its traffic report to: it goes straight to the dashboard
	Game::server->SetTrafficSink([](ServerTraffic& report) {
		if (dashboardServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, report);
	});

	std::string master_server_ip = "localhost";
	const auto masterServerIPString = Game::config->GetValue("master_ip");
	if (!masterServerIPString.empty()) master_server_ip = masterServerIPString;

	if (master_server_ip == "") master_server_ip = Game::server->GetIP();
	IServers::MasterInfo info;
	info.ip = master_server_ip;
	info.port = Game::server->GetPort();
	info.password = hash;

	Database::Get()->SetMasterInfo(info);

	//Create additional objects here:
	Game::im = new InstanceManager(Game::server->GetIP());
	Game::im->SetOnInstancesChanged(PushServerListToDashboard);

	//Get CDClient initial information
	try {
		CDClientManager::LoadValuesFromDatabase();
	} catch (CppSQLite3Exception& e) {
		LOG("Failed to initialize CDServer SQLite Database");
		LOG("May be caused by corrupted file: %s", (Game::assetManager->GetResPath() / "CDServer.sqlite").string().c_str());
		LOG("Error: %s", e.errorMessage());
		LOG("Error Code: %i", e.errorCode());
		return EXIT_FAILURE;
	}

	// Instance migration progress goes to every world, where the GM who asked for it hears about it
	MigrationCoordinator::SetReporter([](const MigrationStatus& status) {
		for (const auto& instance : Game::im->GetInstances()) {
			if (instance && instance->GetIsReady() && !instance->GetShutdownComplete()) MasterPackets::SendTo(instance->GetSysAddr(), status);
		}
	});
	Game::im->LoadZoneLimits();

	//Depending on the config, start up servers:
	if (Game::config->GetValue("prestart_servers") != "0") {
		StartChatServer();

		// The worlds started with master (prestart_worlds, zone ids; missing or empty: character select and Venture Explorer)
		for (auto part : GeneralUtils::SplitString(Game::config->GetValue("prestart_worlds", "0,1000"), ',')) {
			std::erase_if(part, [](const char c) { return std::isspace(static_cast<unsigned char>(c)); });
			if (part.empty()) continue;
			const auto zoneId = GeneralUtils::TryParse<LWOMAPID>(part);
			if (!zoneId) {
				LOG("prestart_worlds: '%s' isn't a zone id; skipped", part.c_str());
				continue;
			}
			Game::im->GetInstance(*zoneId, false, 0);
		}
		StartAuthServer();
	}

	// Start web dashboard if enabled
	if (Game::config->GetValue("enable_dashboard") == "1") {
		StartDashboardServer();
	}

	// The UGC server makes and serves player models' meshes and icons (docs/UgcServer.md)
	if (Game::config->GetValue("enable_ugc_server") == "1") {
		ugcServerPid = StartUgcServer();
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
	constexpr uint32_t spareCheckTime = 10 * masterFramerate;
	uint32_t framesSinceSpareCheck = 0;

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

		MigrationCoordinator::Update();
		CheckPlayerActionTimeouts();

		// Spare instances for busy zones (zone_limits), checked every few seconds
		if (framesSinceSpareCheck >= spareCheckTime) {
			Game::im->KeepSpareInstances();
			framesSinceSpareCheck = 0;
		} else framesSinceSpareCheck++;

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

		const auto& instances = Game::im->GetInstances();

		for (const auto& instance : instances) {
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
		for (const auto& instance : instances) {
			if (instance == nullptr) {
				break;
			}

			if (instance->GetShutdownComplete()) {
				MigrationCoordinator::OnInstanceGone(*instance);
				Game::im->RemoveInstance(instance);
			}
		}

#ifdef DARKFLAME_PLATFORM_UNIX
		// kill off dead zombie instances
		int status{};
		waitpid(static_cast<pid_t>(-1), &status, WNOHANG);
#endif

		t += std::chrono::milliseconds(masterFrameDelta);
		std::this_thread::sleep_until(t);
	}
	return ShutdownSequence(EXIT_SUCCESS);
}

namespace {
	using namespace MasterPackets;

	void OnRequestZoneTransfer(const RequestZoneTransfer& request, const SystemAddress& sysAddr) {
		LOG("Received zone transfer req");
		const uint64_t requestID = request.requestID;
		const uint8_t mythranShift = request.mythranShift;
		const uint32_t zoneID = request.zoneID;
		const uint32_t zoneClone = request.cloneID;
		// The login stamps travelling with the request (see Stamps.h); master adds its steps
		Stamps stamps = request.stamps;
		if (!stamps.empty()) stamps.Add(eStamps::PASSPORT_AUTH_WORLD_PACKET_RECEIVED, zoneID);
		if (shutdownSequenceStarted) {
			LOG("Shutdown sequence has been started.  Not creating a new zone.");
			return;
		}
		const auto& in = Game::im->GetInstance(zoneID, false, zoneClone);

		for (const auto& instance : Game::im->GetInstances()) {
			LOG("Instance: %i/%i/%i -> %i %s", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance == in, instance->GetSysAddr().ToString());
		}

		if (in && !in->GetIsReady()) //Instance not ready, make a pending request
		{
			if (!stamps.empty()) stamps.Add(eStamps::PASSPORT_AUTH_IM_LOGIN_QUEUED, in->GetInstanceID());
			in->GetPendingRequests().push_back({ requestID, static_cast<bool>(mythranShift), sysAddr, stamps });
			LOG("Server not ready, adding pending request %llu %i %i", requestID, zoneID, zoneClone);
			return;
		}

		//Instance is ready, transfer
		LOG("Responding to transfer request %llu for zone %i %i", requestID, zoneID, zoneClone);
		Game::im->RequestAffirmation(in, { requestID, static_cast<bool>(mythranShift), sysAddr, stamps });
	}

	//This is here because otherwise we'd have to include IM in
	//non-master servers. This packet allows us to add World
	//servers back if master crashed
	void OnServerInfo(const ServerInfo& info, const SystemAddress& sysAddr) {
		const uint32_t theirPort = info.port;
		const uint32_t theirZoneID = info.zoneID;
		const uint32_t theirInstanceID = info.instanceID;
		const ServiceType theirServerType = info.serverType;
		const LUString& theirIP = info.ip;

		switch (theirServerType) {
		case ServiceType::WORLD:
			if (!Game::im->IsPortInUse(theirPort)) {
				auto in = std::make_unique<Instance>(theirIP.string, theirPort, theirZoneID, theirInstanceID, 0, 12, 12);
				in->SetSysAddr(sysAddr);
				Game::im->AddInstance(in);
			} else {
				const auto& instance = Game::im->FindInstanceWithPrivate(theirZoneID, static_cast<LWOINSTANCEID>(theirInstanceID));
				if (instance) {
					instance->SetSysAddr(sysAddr);
				}
			}
			break;
		case ServiceType::CHAT:
			chatServerMasterPeerSysAddr = sysAddr;
			break;
		case ServiceType::AUTH:
			authServerMasterPeerSysAddr = sysAddr;
			break;
		case ServiceType::DASHBOARD:
			dashboardServerMasterPeerSysAddr = sysAddr;
			break;
		case ServiceType::UGC:
			ugcServerMasterPeerSysAddr = sysAddr;
			break;
		default:
			break;
		}

		if (theirServerType != ServiceType::DASHBOARD && dashboardServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
			MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, info);
		}

		LOG("Received %s server info, instance: %i port: %i", StringifiedEnum::ToString(theirServerType).data(), theirInstanceID, theirPort);
	}

	void OnSetSessionKey(const SetSessionKey& request, const SystemAddress& sysAddr) {
		const uint32_t sessionKey = request.sessionKey;
		const LUString& username = request.username;

		for (auto it : activeSessions) {
			if (it.second == username.string) {
				activeSessions.erase(it.first);

				NewSessionAlert alert;
				alert.sessionKey = sessionKey;
				alert.username = username;
				alert.Broadcast();

				break;
			}
		}

		activeSessions.insert(std::make_pair(sessionKey, username.string));
		LOG("Got sessionKey %i for user %s", sessionKey, username.string.c_str());
	}

	void OnRequestSessionKey(const RequestSessionKey& request, const SystemAddress& sysAddr) {
		const LUWString& username = request.username;
		LOG("Requesting session key for %s", username.GetAsString().c_str());
		for (auto key : activeSessions) {
			if (key.second == username.GetAsString()) {
				SessionKeyResponse response;
				response.sessionKey = key.first;
				response.username = username;
				MasterPackets::SendTo(sysAddr, response);
				break;
			}
		}
	}

	void OnPlayerAdded(const PlayerAdded& added, const SystemAddress& sysAddr) {
		const auto& instance =
			Game::im->FindInstanceWithPrivate(added.zoneID, added.instanceID);
		if (instance) {
			instance->AddPlayer(Player());
		} else {
			LOG("Instance missing? What?");
		}

		if (dashboardServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
			MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, added);
		}
	}

	void OnPlayerRemoved(const PlayerRemoved& removed, const SystemAddress& sysAddr) {
		const auto& instance =
			Game::im->FindInstance(removed.zoneID, removed.instanceID);
		if (instance) {
			instance->RemovePlayer(Player());
		}

		if (dashboardServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
			MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, removed);
		}
	}

	void OnCreatePrivateZone(const CreatePrivateZone& request, const SystemAddress& sysAddr) {
		// Passwords were cut to 50 characters when read
		const auto& newInst = Game::im->CreatePrivateInstance(request.zoneID, request.cloneID, request.password.c_str());
		LOG("Creating private zone %i/%i/%i", newInst->GetMapID(), newInst->GetCloneID(), newInst->GetInstanceID());
	}

	void OnRequestPrivateZone(const RequestPrivateZone& request, const SystemAddress& sysAddr) {
		const uint64_t requestID = request.requestID;
		const uint8_t mythranShift = request.mythranShift;

		const auto& instance = Game::im->FindPrivateInstance(request.password.c_str());

		LOG("Join private zone: %llu %d %p", requestID, mythranShift, instance.get());

		if (instance == nullptr) {
			return;
		}

		const auto& zone = instance->GetZoneID();

		RequestZoneTransferResponse response;
		response.requestID = requestID;
		response.mythranShift = static_cast<bool>(mythranShift);
		response.zoneID = zone.GetMapID();
		response.zoneInstance = instance->GetInstanceID();
		response.zoneClone = zone.GetCloneID();
		response.serverPort = static_cast<uint16_t>(instance->GetPort());
		response.serverIP = LUString(instance->GetIP(), 255);
		MasterPackets::SendTo(sysAddr, response);
	}

	void OnWorldReady(const WorldReady& ready, const SystemAddress& sysAddr) {
		const LWOMAPID zoneID = ready.zoneID;
		const LWOINSTANCEID instanceID = ready.instanceID;

		LOG("Got world ready %i %i", zoneID, instanceID);

		const auto& instance = Game::im->FindInstanceWithPrivate(zoneID, instanceID);

		if (instance == nullptr) {
			LOG("Failed to find zone to ready");
			return;
		}

		LOG("Ready zone %i", zoneID);
		Game::im->ReadyInstance(instance);

		if (dashboardServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
			WorldReadyInfo info;
			info.zoneID = zoneID;
			info.instanceID = instanceID;
			info.cloneID = instance->GetCloneID();
			info.ip = LUString(instance->GetIP());
			info.port = instance->GetPort();
			info.isPrivate = instance->GetIsPrivate() ? 1 : 0;
			MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, info);
		}
	}

	void OnPrepZone(const PrepZone& request, const SystemAddress& sysAddr) {
		const int32_t zoneID = request.zoneID;
		if (shutdownSequenceStarted) {
			LOG("Shutdown sequence has been started.  Not prepping a new zone.");
		} else {
			LOG("Prepping zone %i", zoneID);
			Game::im->GetInstance(zoneID, false, 0);
		}
	}

	void OnAffirmTransferResponse(const AffirmTransferResponse& response, const SystemAddress& sysAddr) {
		const uint64_t requestID = response.requestID;

		LOG("Got affirmation of transfer %llu", requestID);

		const auto& instance = Game::im->GetInstanceBySysAddr(sysAddr);

		if (instance == nullptr)
			return;

		Game::im->AffirmTransfer(instance, requestID);
		LOG("Affirmation complete %llu", requestID);
	}

	void OnShutdownResponse(const ShutdownResponse& response, const SystemAddress& sysAddr) {
		const auto& instance = Game::im->GetInstanceBySysAddr(sysAddr);
		LOG("Got shutdown response from %s", sysAddr.ToString());
		if (instance == nullptr) {
			return;
		}

		LOG("Got shutdown response from zone %i clone %i instance %i port %i", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance->GetPort());
		instance->SetIsShuttingDown(true);
		PushServerListToDashboard();
	}

	void OnShutdownUniverse(const ShutdownUniverse& request, const SystemAddress& sysAddr) {
		LOG("Received shutdown universe command, shutting down in 10 minutes.");
		Game::universeShutdownRequested = true;
	}

	void OnInstanceMigrate(const InstanceMigrationRequest& request, const SystemAddress& sysAddr) {
		// Only servers connected to master can send this (a world, for a GM's /replaceinstance or /mergeinstance)
		if (shutdownSequenceStarted) {
			LOG("Shutdown sequence has been started. Not starting instance migration %u.", request.requestId);
			return;
		}
		MigrationCoordinator::Start(request);
	}

	void OnPlayerAction(const PlayerActionRequest& request, const SystemAddress& sysAddr) {
		// Only the dashboard may ask worlds to act on players
		if (sysAddr != dashboardServerMasterPeerSysAddr) {
			LOG("Ignoring player action from a server that is not the dashboard");
			return;
		}

		PendingPlayerAction pending;
		pending.action = request.action;
		pending.deadline = std::chrono::steady_clock::now() + PLAYER_ACTION_TIMEOUT;

		for (const auto& instance : Game::im->GetInstances()) {
			if (!instance || !instance->GetIsReady() || instance->GetIsShuttingDown()) continue;
			MasterPackets::SendTo(instance->GetSysAddr(), request);
			pending.waitingOn.insert(instance->GetSysAddr());
		}
		// The chat server checks web chat with the same filter: it reloads too, but doesn't answer
		if (request.action == ePlayerAction::RELOAD_CHAT_FILTER && chatServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
			MasterPackets::SendTo(chatServerMasterPeerSysAddr, request);
		}

		LOG("Dashboard player action %i (request %u) sent to %zu world(s)", static_cast<int>(request.action), request.requestId, pending.waitingOn.size());
		const bool noWorlds = pending.waitingOn.empty();
		g_PendingPlayerActions[request.requestId] = std::move(pending);
		if (noWorlds) FinishPlayerAction(request.requestId, false);
	}

	void OnPlayerActionResult(const PlayerActionResult& result, const SystemAddress& sysAddr) {
		const auto it = g_PendingPlayerActions.find(result.requestId);
		if (it == g_PendingPlayerActions.end()) return;
		it->second.affected += result.affected;
		it->second.waitingOn.erase(sysAddr);
		if (it->second.waitingOn.empty()) FinishPlayerAction(result.requestId, false);
	}

	// World -> dashboard messages master passes on (only from worlds, only when a dashboard is connected)
	template<typename Msg>
	void ForwardWorldToDashboard(const Msg& msg, const SystemAddress& sysAddr) {
		if (dashboardServerMasterPeerSysAddr == UNASSIGNED_SYSTEM_ADDRESS || !Game::im->GetInstanceBySysAddr(sysAddr)) return;
		MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, msg);
	}

	// Every server's traffic report goes on to the dashboard (the dashboard keeps its own)
	void OnServerTraffic(const ServerTraffic& report, const SystemAddress& sysAddr) {
		if (dashboardServerMasterPeerSysAddr == UNASSIGNED_SYSTEM_ADDRESS || sysAddr == dashboardServerMasterPeerSysAddr) return;
		MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, report);
	}

	void OnAnnounce(const Announcement& announcement, const SystemAddress& sysAddr) {
		if (sysAddr != dashboardServerMasterPeerSysAddr) {
			LOG("Ignoring announcement from a server that is not the dashboard");
			return;
		}
		uint32_t worlds = 0;
		for (const auto& instance : Game::im->GetInstances()) {
			if (!instance || !instance->GetIsReady() || instance->GetIsShuttingDown() || !announcement.ShownIn(instance->GetMapID())) continue;
			MasterPackets::SendTo(instance->GetSysAddr(), announcement);
			worlds++;
		}
		LOG("Dashboard announcement sent to %u world(s)", worlds);
	}

	void OnConfigReload(const ConfigReload& reload, const SystemAddress& sysAddr) {
		if (sysAddr != dashboardServerMasterPeerSysAddr) {
			LOG("Ignoring config reload from a server that is not the dashboard");
			return;
		}
		LOG("Reloading settings (changed on the dashboard)");
		Game::config->ReloadConfig();
		Game::im->LoadZoneLimits();
		// Everyone else: auth, chat, UGC and every world
		for (const auto& peer : { authServerMasterPeerSysAddr, chatServerMasterPeerSysAddr, ugcServerMasterPeerSysAddr }) {
			if (peer != UNASSIGNED_SYSTEM_ADDRESS) MasterPackets::SendTo(peer, reload);
		}
		for (const auto& instance : Game::im->GetInstances()) {
			if (instance && instance->GetIsReady()) MasterPackets::SendTo(instance->GetSysAddr(), reload);
		}
	}

	void OnInstanceShutdown(const InstanceShutdown& request, const SystemAddress& sysAddr) {
		if (sysAddr != dashboardServerMasterPeerSysAddr) {
			LOG("Ignoring instance shutdown from a server that is not the dashboard");
			return;
		}
		const uint32_t zoneId = request.zoneID, instanceId = request.instanceID;
		const auto& instance = Game::im->FindInstanceWithPrivate(static_cast<LWOMAPID>(zoneId), static_cast<LWOINSTANCEID>(instanceId));
		if (!instance) {
			LOG("Dashboard asked to shut down zone %u instance %u, which isn't running", zoneId, instanceId);
			return;
		}
		LOG("Shutting down zone %u instance %u (from the dashboard)", zoneId, instanceId);
		instance->Shutdown();
	}

	void OnDashboardShutdown(const DashboardShutdown& request, const SystemAddress& sysAddr) {
		if (sysAddr != dashboardServerMasterPeerSysAddr) {
			LOG("Ignoring shutdown request from a server that is not the dashboard");
			return;
		}
		LOG("Shutdown requested from the dashboard (scheduled restart)");
		Game::lastSignal = -1;
	}

	void OnMessageCaptureControl(const MessageCaptureControl& control, const SystemAddress& sysAddr) {
		// Only the dashboard starts message captures; every world gets it, and the one with the player acts on it
		if (sysAddr != dashboardServerMasterPeerSysAddr) {
			LOG("Ignoring a message capture request from a server that is not the dashboard");
			return;
		}
		for (const auto& instance : Game::im->GetInstances()) {
			if (instance && instance->GetIsReady() && !instance->GetIsShuttingDown()) MasterPackets::SendTo(instance->GetSysAddr(), control);
		}
	}

	void OnRequestServerList(const RequestServerList& request, const SystemAddress& sysAddr) {
		LOG("Dashboard requested server list");

		const auto response = BuildServerList();
		MasterPackets::SendTo(sysAddr, response);
	}

	const PacketDispatcher<MessageType::Master>& MasterHandlers() {
		static const auto handlers = [] {
			PacketDispatcher<MessageType::Master> handlers;
			using MessageType::Master;
			handlers.On<RequestZoneTransfer>(Master::REQUEST_ZONE_TRANSFER, OnRequestZoneTransfer);
			handlers.On<ServerInfo>(Master::SERVER_INFO, OnServerInfo);
			handlers.On<SetSessionKey>(Master::SET_SESSION_KEY, OnSetSessionKey);
			handlers.On<RequestSessionKey>(Master::REQUEST_SESSION_KEY, OnRequestSessionKey);
			handlers.On<PlayerAdded>(Master::PLAYER_ADDED, OnPlayerAdded);
			handlers.On<PlayerRemoved>(Master::PLAYER_REMOVED, OnPlayerRemoved);
			handlers.On<CreatePrivateZone>(Master::CREATE_PRIVATE_ZONE, OnCreatePrivateZone);
			handlers.On<RequestPrivateZone>(Master::REQUEST_PRIVATE_ZONE, OnRequestPrivateZone);
			handlers.On<WorldReady>(Master::WORLD_READY, OnWorldReady);
			handlers.On<PrepZone>(Master::PREP_ZONE, OnPrepZone);
			handlers.On<AffirmTransferResponse>(Master::AFFIRM_TRANSFER_RESPONSE, OnAffirmTransferResponse);
			handlers.On<ShutdownResponse>(Master::SHUTDOWN_RESPONSE, OnShutdownResponse);
			handlers.On<ShutdownUniverse>(Master::SHUTDOWN_UNIVERSE, OnShutdownUniverse);
			handlers.On<InstanceMigrationRequest>(Master::INSTANCE_MIGRATE, OnInstanceMigrate);
			handlers.On<MigrationStatus>(Master::MIGRATE_STATUS, [](const MigrationStatus& status, const SystemAddress& sysAddr) { MigrationCoordinator::HandleStatus(sysAddr, status); });
			handlers.On<CarriedPlayerState>(Master::MIGRATE_PLAYER_STATE, [](const CarriedPlayerState& state, const SystemAddress& sysAddr) { MigrationCoordinator::HandleCarriedState(sysAddr, state); });
			handlers.On<PlayerActionRequest>(Master::PLAYER_ACTION, OnPlayerAction);
			handlers.On<PlayerActionResult>(Master::PLAYER_ACTION_RESULT, OnPlayerActionResult);
			handlers.On<PlayerPositions>(Master::PLAYER_POSITIONS, ForwardWorldToDashboard<PlayerPositions>);
			handlers.On<Announcement>(Master::ANNOUNCE, OnAnnounce);
			handlers.On<ConfigReload>(Master::CONFIG_RELOAD, OnConfigReload);
			handlers.On<InstanceShutdown>(Master::INSTANCE_SHUTDOWN, OnInstanceShutdown);
			handlers.On<DashboardShutdown>(Master::DASHBOARD_SHUTDOWN, OnDashboardShutdown);
			// Only world servers report game writes; pass them on unchanged
			handlers.On<DataChanged>(Master::DATA_CHANGED, ForwardWorldToDashboard<DataChanged>);
			handlers.On<MessageCaptureControl>(Master::MESSAGE_CAPTURE_CONTROL, OnMessageCaptureControl);
			handlers.On<MessageCaptureData>(Master::MESSAGE_CAPTURE_DATA, ForwardWorldToDashboard<MessageCaptureData>);
			handlers.On<RequestServerList>(Master::REQUEST_SERVER_LIST, OnRequestServerList);
			handlers.On<ServerTraffic>(Master::SERVER_TRAFFIC, OnServerTraffic);
			return handlers;
		}();
		return handlers;
	}
}

void HandlePacket(Packet* packet) {
	if (packet->length < 1) return;
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		const bool intentional = packet->data[0] == ID_DISCONNECTION_NOTIFICATION;
		LOG("A server has %s", intentional ? "disconnected" : "lost the connection");

		const auto& instance =
			Game::im->GetInstanceBySysAddr(packet->systemAddress);
		if (instance) {
			LOG("Actually disconnected from zone %i clone %i instance %i port %i", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance->GetPort());

			if (dashboardServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS &&
				packet->systemAddress != dashboardServerMasterPeerSysAddr) {
				MasterPackets::WorldShutDown shutDown;
				shutDown.zoneID = instance->GetMapID();
				shutDown.instanceID = instance->GetInstanceID();
				MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, shutDown);
			}

			MigrationCoordinator::OnInstanceGone(*instance);
			Game::im->RemoveInstance(instance);
		}

		if (packet->systemAddress == chatServerMasterPeerSysAddr) {
			chatServerMasterPeerSysAddr = UNASSIGNED_SYSTEM_ADDRESS;

			if (dashboardServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
				MasterPackets::ServerInfo offline;
				offline.serverType = ServiceType::CHAT;
				offline.ip = LUString("offline");
				MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, offline);
			}

			StartChatServer();
		}

		if (packet->systemAddress == authServerMasterPeerSysAddr) {
			authServerMasterPeerSysAddr = UNASSIGNED_SYSTEM_ADDRESS;

			if (dashboardServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
				MasterPackets::ServerInfo offline;
				offline.serverType = ServiceType::AUTH;
				offline.ip = LUString("offline");
				MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, offline);
			}

			StartAuthServer();
		}

		if (packet->systemAddress == dashboardServerMasterPeerSysAddr) {
			dashboardServerMasterPeerSysAddr = UNASSIGNED_SYSTEM_ADDRESS;
			StartDashboardServer();
		}

		if (packet->systemAddress == ugcServerMasterPeerSysAddr) {
			ugcServerMasterPeerSysAddr = UNASSIGNED_SYSTEM_ADDRESS;

			if (dashboardServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
				MasterPackets::ServerInfo offline;
				offline.serverType = ServiceType::UGC;
				offline.ip = LUString("offline");
				MasterPackets::SendTo(dashboardServerMasterPeerSysAddr, offline);
			}

			ugcServerPid = StartUgcServer();
		}
	}

	if (packet->length < 4) return;

	if (!MasterHandlers().Dispatch(packet, ServiceType::MASTER)) {
		RakNet::BitStream inStream(packet->data, packet->length, false);
		LUBitStream header;
		if (header.ReadHeader(inStream) && header.connectionType == ServiceType::MASTER) {
			LOG("Unknown master packet ID from server: %i", header.internalPacketID);
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
		MasterPackets::Shutdown().Broadcast();
		LOG("Triggered master shutdown");
	}

	// A server might not be finished spinning up yet, remove all of those here.
	// prune the unready ones before looping over all of them
	Game::im->PruneUnreadyInstances();
	for (const auto& instance : Game::im->GetInstances()) {
		if (!instance) continue;

		instance->SetIsShuttingDown(true);
	}

	LOG("Attempting to shutdown instances, max 10 seconds...");

	auto t = std::chrono::high_resolution_clock::now();
	uint32_t framesSinceShutdownStart = 0;
	constexpr uint32_t maxShutdownTime = 10 * mediumFramerate;
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

		for (const auto& instance : Game::im->GetInstances()) {
			if (instance == nullptr) {
				continue;
			}

			if (!instance->GetShutdownComplete()) {
				allInstancesShutdown = false;
			}
		}

		if (allInstancesShutdown && \
			authServerMasterPeerSysAddr == UNASSIGNED_SYSTEM_ADDRESS && \
			chatServerMasterPeerSysAddr == UNASSIGNED_SYSTEM_ADDRESS && \
			ugcServerMasterPeerSysAddr == UNASSIGNED_SYSTEM_ADDRESS && \
			dashboardServerMasterPeerSysAddr == UNASSIGNED_SYSTEM_ADDRESS) {
			LOG("Finished shutting down MasterServer!");
			break;
		}

		t += std::chrono::milliseconds(mediumFrameDelta);
		std::this_thread::sleep_until(t);

		framesSinceShutdownStart++;

		if (framesSinceShutdownStart == maxShutdownTime) {
			LOG("Finished shutting down by timeout!");
			// log what we were waiting on: worlds, chat, auth, dashboard, etc
			if (authServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
				LOG("Auth server did not shutdown in time");
			}
			if (chatServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
				LOG("Chat server did not shutdown in time");
			}
			if (dashboardServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
				LOG("Dashboard server did not shutdown in time");
			}
			if (ugcServerMasterPeerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
				LOG("UGC server did not shutdown in time");
			}
			for (const auto& instance : Game::im->GetInstances()) {
				if (instance == nullptr) {
					continue;
				}

				if (!instance->GetShutdownComplete()) {
					LOG("Instance zone %i clone %i instance %i port %i did not shutdown in time", instance->GetMapID(), instance->GetCloneID(), instance->GetInstanceID(), instance->GetPort());
				}
			}

			break;
		}
	}

	return FinalizeShutdown(signal);
}

int32_t FinalizeShutdown(int32_t signal) {
	//Delete our objects here:
	Database::Destroy(g_ServiceName);
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
