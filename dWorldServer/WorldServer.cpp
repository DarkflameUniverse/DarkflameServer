#include "DashboardActions.h"
#include "ConfigSync.h"
#include "EconomyLedger.h"
#include "DashboardNotify.h"
#include "DashboardMessages.h"
#include "PlayerAction.h"
#include "MessageCapture.h"
#include "MessageInspector.h"
#include "LiveEvents.h"
#include <iostream>
#include <string>
#include <ctime>
#include <chrono>
#include <thread>
#include <functional>
#include <memory>

#include "MD5.h"

//DLU Includes:
#include "dCommonVars.h"
#include "dServer.h"
#include "Logger.h"
#include "Database.h"
#include "dConfig.h"
#include "dpWorld.h"
#include "dZoneManager.h"
#include "Metrics.h"
#include "PerformanceManager.h"
#include "Diagnostics.h"
#include "BinaryPathFinder.h"
#include "dPlatforms.h"

//RakNet includes:
#include "RakNetDefines.h"
#include "RakNetworkFactory.h"
#include "RakString.h"

//World includes:
#include <csignal>

#include "AuthPackets.h"
#include "CommonPackets.h"
#include "BitStreamUtils.h"
#include "WorldPackets.h"
#include "UserManager.h"
#include "CDClientManager.h"
#include "CDClientDatabase.h"
#include "GeneralUtils.h"
#include "ZoneInstanceManager.h"
#include "dChatFilter.h"
#include "ClientPackets.h"
#include "CharacterComponent.h"

#include "EntityManager.h"
#include "EntityInfo.h"
#include "User.h"
#include "Loot.h"
#include "Entity.h"
#include "Character.h"
#include "ChatPackets.h"
#include "GameMessageHandler.h"
#include "GameMessages.h"
#include "Mail.h"
#include "TeamManager.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"
#include "Game.h"
#include "MasterPackets.h"
#include "PropertyManagementComponent.h"
#include "AssetManager.h"
#include "LevelProgressionComponent.h"
#include "eBlueprintSaveResponseType.h"
#include "Amf3.h"
#include "NiPoint3.h"
#include "eServerDisconnectIdentifiers.h"
#include "eObjectBits.h"
#include "ServiceType.h"
#include "MessageType/Server.h"
#include "MessageType/Chat.h"
#include "MessageType/World.h"
#include "MessageType/Master.h"
#include "MessageType/Game.h"
#include "ZCompression.h"
#include "EntityManager.h"
#include "CheatDetection.h"
#include "eGameMasterLevel.h"
#include "StringifiedEnum.h"
#include "Server.h"
#include "PositionUpdate.h"
#include "PlayerManager.h"
#include "eLoginResponse.h"
#include "MissionComponent.h"
#include "SlashCommandHandler.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "eFunnessTypes.h"
#include "WorldMigration.h"

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dChatFilter* chatFilter = nullptr;
	dConfig* config = nullptr;
	AssetManager* assetManager = nullptr;
	RakPeerInterface* chatServer = nullptr;
	std::mt19937 randomEngine;
	SystemAddress chatSysAddr;
	Game::signal_t lastSignal = 0;
	EntityManager* entityManager = nullptr;
	dZoneManager* zoneManager = nullptr;
	std::string projectVersion = PROJECT_VERSION;
} // namespace Game

namespace {
	std::string g_ServiceName;
}

namespace {
	struct TempSessionInfo {
		SystemAddress sysAddr;
		std::string hash;
	};

	std::map<std::string, TempSessionInfo> g_PendingUsers;
	uint32_t g_InstanceID = 0;
	uint32_t g_CloneID = 0;
	std::string g_DatabaseChecksum = "";

	bool g_ChatDisabled = false;
	bool g_ChatConnected = false;
	bool g_WorldShutdownSequenceComplete = false;
}; // namespace anonymous

void WorldShutdownSequence();
void WorldShutdownProcess(uint32_t zoneId);
void FinalizeShutdown();
void SendShutdownMessageToMaster();

void HandlePacketChat(Packet* packet);
void HandleMasterPacket(Packet* packet);
void HandlePacket(Packet* packet);
void CleanupDisconnectedUser(const SystemAddress& sysAddr);
void LoadPlayer(const SystemAddress& sysAddr);

int main(int argc, char** argv) {
	const auto curTimeStr = std::to_string(time(nullptr));
	g_ServiceName = "WorldServer";
	// Set this once here before we parse a bunch of options in case we crash early
	Diagnostics::SetProcessName(g_ServiceName);
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	// Triggers the shutdown sequence at application exit
	std::atexit(WorldShutdownSequence);

	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);

	uint32_t zoneID = 1000;
	uint32_t cloneID = 0;
	uint32_t maxClients = 8;
	uint32_t ourPort = 2007;

	//Check our arguments:
	for (int32_t i = 0; (i + 1) < argc; i++) {
		std::string argument(argv[i]);
		const auto valOptional = GeneralUtils::TryParse<uint32_t>(argv[i + 1]);
		if (!valOptional) {
			continue;
		}

		if (argument == "-zone") zoneID = valOptional.value_or(1000);
		else if (argument == "-instance") g_InstanceID = valOptional.value_or(0);
		else if (argument == "-clone") cloneID = valOptional.value_or(0);
		else if (argument == "-maxclients") maxClients = valOptional.value_or(8);
		else if (argument == "-port") ourPort = valOptional.value_or(2007);
	}

	Game::config = new dConfig("worldconfig.ini");

	//Create all the objects we need to run our service:
	const auto zoneStr = std::to_string(zoneID);
	const auto cloneStr = std::to_string(cloneID);
	const auto instanceStr = std::to_string(g_InstanceID);
	g_ServiceName += "_" + zoneStr + "_" + cloneStr + "_" + instanceStr + "_" + curTimeStr;
	// Here we re-set the process name since it'll have more info now
	Diagnostics::SetProcessName(g_ServiceName);
	Server::SetupLogger(g_ServiceName, "WorldServer/" + zoneStr + "/" + cloneStr + "/");
	if (!Game::logger) return EXIT_FAILURE;
	Game::config->LogSettings();

	LOG("Starting World server...");
	LOG("Version: %s", Game::projectVersion.c_str());
	LOG("Compiled on: %s", __TIMESTAMP__);

	g_ChatDisabled = Game::config->GetValue("disable_chat") == "1";

	try {
		std::string clientPathStr = Game::config->GetValue("client_location");
		if (clientPathStr.empty()) clientPathStr = "./res";
		std::filesystem::path clientPath = std::filesystem::path(clientPathStr);
		if (clientPath.is_relative()) {
			clientPath = BinaryPathFinder::GetBinaryDir() / clientPath;
		}
		Game::assetManager = new AssetManager(clientPath);
	} catch (const std::exception& ex) {
		LOG("Got an error while setting up assets: %s", ex.what());

		return EXIT_FAILURE;
	}

	// Connect to CDClient
	try {
		CDClientDatabase::Connect((BinaryPathFinder::GetBinaryDir() / "resServer" / "CDServer.sqlite").string());
	} catch (const CppSQLite3Exception& e) {
		LOG("Unable to connect to CDServer SQLite Database");
		LOG("Error: %s", e.errorMessage());
		LOG("Error Code: %i", e.errorCode());
		return EXIT_FAILURE;
	} catch (const std::exception& e) {
		LOG("Caught generic exception %s when connecting to CDClient", e.what());
		return EXIT_FAILURE;
	}

	CDClientManager::LoadValuesFromDatabase();

	Diagnostics::SetProduceMemoryDump(Game::config->GetValue("generate_dump") == "1");

	if (!Game::config->GetValue("dump_folder").empty()) {
		Diagnostics::SetOutDirectory(Game::config->GetValue("dump_folder"));
	}

	//Connect to the MySQL Database:
	try {
		Database::Connect();
	} catch (const std::exception& ex) {
		LOG("Got an error while connecting to the database: %s", ex.what());
		return EXIT_FAILURE;
	}

	// Settings edited on the dashboard (server_config table) are layered over the files from here on
	Game::config->SetDatabaseSync(ConfigSync::Sync);

	//Find out the master's IP:
	std::string masterIP = "localhost";
	uint32_t masterPort = 1000;
	std::string masterPassword;
	const auto masterInfo = Database::Get()->GetMasterInfo();

	if (masterInfo) {
		masterIP = masterInfo->ip;
		masterPort = masterInfo->port;
		masterPassword = masterInfo->password;
	}

	UserManager::Instance()->Initialize();

	const bool dontGenerateDCF = GeneralUtils::TryParse<bool>(Game::config->GetValue("dont_generate_dcf")).value_or(false);
	Game::chatFilter = new dChatFilter(Game::assetManager->GetResPath().string() + "/chatplus_en_us", dontGenerateDCF);

	Game::server = new dServer(masterIP,
		ourPort,
		g_InstanceID,
		maxClients,
		false /* Is internal */,
		true /* Use encryption */,
		Game::logger,
		masterIP,
		masterPort,
		ServiceType::WORLD,
		Game::config,
		&Game::lastSignal,
		masterPassword,
		zoneID);
	WorldMigration::SetCleanupHandler(CleanupDisconnectedUser);
	DashboardActions::SetLogoutHandler(CleanupDisconnectedUser);

	//Connect to the chat server:
	uint32_t chatPort = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("chat_server_port")).value_or(1501);

	auto chatSock = SocketDescriptor(static_cast<uint16_t>(ourPort + 2), NULL);
	Game::chatServer = RakNetworkFactory::GetRakPeerInterface();
	Game::chatServer->Startup(1, 30, &chatSock, 1);
	Game::chatServer->Connect(masterIP.c_str(), chatPort, NET_PASSWORD_EXTERNAL, strnlen(NET_PASSWORD_EXTERNAL, sizeof(NET_PASSWORD_EXTERNAL)));

	//Set up other things:
	Game::randomEngine = std::mt19937(time(0));

	//Run it until server gets a kill message from Master:
	auto lastTime = std::chrono::high_resolution_clock::now();
	auto t = std::chrono::high_resolution_clock::now();

	Packet* packet = nullptr;
	uint32_t framesSinceLastFlush = 0;
	uint32_t framesSinceMasterDisconnect = 0;
	uint32_t framesSinceChatDisconnect = 0;
	uint32_t framesSinceLastUsersSave = 0;
	uint32_t framesSinceLastSQLPing = 0;
	uint32_t framesSinceLastUser = 0;

	const float maxPacketProcessingTime = 1.5f; //0.015f;
	const uint32_t maxPacketsToProcess = 1024;

	bool ready = false;
	uint32_t framesSinceMasterStatus = 0;
	uint32_t framesSinceShutdownSequence = 0;
	uint32_t currentFramerate = highFramerate;

	uint32_t ghostingStepCount = 0;
	auto ghostingLastTime = std::chrono::high_resolution_clock::now();
	auto economyLastFlush = ghostingLastTime;

	PerformanceManager::SelectProfile(zoneID);

	Game::entityManager = new EntityManager();
	Game::zoneManager = new dZoneManager();
	//Load our level:
	if (zoneID != 0) {
		dpWorld::Initialize(zoneID);
		Game::zoneManager->Initialize(LWOZONEID(zoneID, g_InstanceID, cloneID));
		g_CloneID = cloneID;
	} else {
		Game::entityManager->Initialize();
	}

	// pre calculate the FDB checksum
	if (Game::config->GetValue("check_fdb") == "1") {
		auto cdclient = Game::assetManager->GetFile("cdclient.fdb");
		if (cdclient) {

			const int32_t bufferSize = 1024;
			MD5 md5;

			char fileStreamBuffer[bufferSize] = {};

			while (!cdclient.eof()) {
				memset(fileStreamBuffer, 0, bufferSize);
				cdclient.read(fileStreamBuffer, bufferSize);
				md5.update(fileStreamBuffer, cdclient.gcount());
			}

			const char* nullTerminateBuffer = "\0";
			md5.update(nullTerminateBuffer, 1); // null terminate the data
			md5.finalize();
			g_DatabaseChecksum = md5.hexdigest();

			LOG("FDB Checksum calculated as: %s", g_DatabaseChecksum.c_str());
		}
		if (g_DatabaseChecksum.empty()) {
			LOG("check_fdb is on but no fdb file found.");
			return EXIT_FAILURE;
		}
	}

	uint32_t currentFrameDelta = highFrameDelta;
	// These values are adjust them selves to the current framerate should it update.
	uint32_t logFlushTime = 15 * currentFramerate; // 15 seconds in frames
	uint32_t shutdownTimeout = 10 * 60 * currentFramerate; // 10 minutes in frames
	uint32_t noMasterConnectionTimeout = 5 * currentFramerate; // 5 seconds in frames
	uint32_t chatReconnectionTime = 30 * currentFramerate; // 30 seconds in frames
	uint32_t saveTime = 10 * 60 * currentFramerate; // 10 minutes in frames
	uint32_t sqlPingTime = 10 * 60 * currentFramerate; // 10 minutes in frames
	uint32_t emptyShutdownTime = (cloneID == 0 ? 30 : 5) * 60 * currentFramerate; // 30 minutes for main worlds, 5 for all others.

	// Register slash commands if not in zone 0
	if (zoneID != 0) {
		SlashCommandHandler::Startup();
		// So the dashboard can list the commands and change their levels
		SlashCommandHandler::ReportCommands();
	}

	Game::logger->Flush(); // once immediately before the main loop
	while (true) {
		Metrics::StartMeasurement(MetricVariable::Frame);
		Metrics::StartMeasurement(MetricVariable::GameLoop);

		std::clock_t metricCPUTimeStart = std::clock();

		const auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
		lastTime = currentTime;

		const auto occupied = UserManager::Instance()->GetUserCount() != 0;

		uint32_t newFrameDelta = currentFrameDelta;
		if (!ready) {
			newFrameDelta = highFrameDelta;
		} else {
			newFrameDelta = PerformanceManager::GetServerFrameDelta();
		}

		// Update to the new framerate and scale all timings to said new framerate
		if (newFrameDelta != currentFrameDelta) {
			float_t ratioBeforeToAfter = static_cast<float>(currentFrameDelta) / static_cast<float>(newFrameDelta);
			currentFrameDelta = newFrameDelta;
			currentFramerate = MS_TO_FRAMES(newFrameDelta);
			LOG_DEBUG("Framerate for zone/instance/clone %i/%i/%i is now %i", zoneID, g_InstanceID, cloneID, currentFramerate);
			logFlushTime = 15 * currentFramerate; // 15 seconds in frames
			framesSinceLastFlush *= ratioBeforeToAfter;
			shutdownTimeout = 10 * 60 * currentFramerate; // 10 minutes in frames
			framesSinceLastUser *= ratioBeforeToAfter;
			noMasterConnectionTimeout = 5 * currentFramerate; // 5 seconds in frames
			framesSinceMasterDisconnect *= ratioBeforeToAfter;
			chatReconnectionTime = 30 * currentFramerate; // 30 seconds in frames
			framesSinceChatDisconnect *= ratioBeforeToAfter;
			saveTime = 10 * 60 * currentFramerate; // 10 minutes in frames
			framesSinceLastUsersSave *= ratioBeforeToAfter;
			sqlPingTime = 10 * 60 * currentFramerate; // 10 minutes in frames
			framesSinceLastSQLPing *= ratioBeforeToAfter;
			emptyShutdownTime = (cloneID == 0 ? 30 : 5) * 60 * currentFramerate; // 30 minutes for main worlds, 5 for all others.
			framesSinceLastUser *= ratioBeforeToAfter;
		}

		//Warning if we ran slow
		if (deltaTime > currentFrameDelta) {
			LOG("We're running behind, dT: %f > %i (framerate %i)", deltaTime, currentFrameDelta, currentFramerate);
		}

		//Check if we're still connected to master:
		if (!Game::server->GetIsConnectedToMaster()) {
			framesSinceMasterDisconnect++;

			if (framesSinceMasterDisconnect >= noMasterConnectionTimeout && !Game::ShouldShutdown()) {
				LOG("Game loop running but no connection to master for %d frames, shutting down", noMasterConnectionTimeout);
				Game::lastSignal = -1;
			}
		} else framesSinceMasterDisconnect = 0;

		// Check if we're still connected to chat:
		if (!g_ChatConnected) {
			framesSinceChatDisconnect++;

			if (framesSinceChatDisconnect >= chatReconnectionTime) {
				framesSinceChatDisconnect = 0;

				Game::chatServer->Connect(masterIP.c_str(), chatPort, NET_PASSWORD_EXTERNAL, strnlen(NET_PASSWORD_EXTERNAL, sizeof(NET_PASSWORD_EXTERNAL)));
			}
		} else framesSinceChatDisconnect = 0;

		//In world we'd update our other systems here.

		if (zoneID != 0 && deltaTime > 0.0f) {
			Metrics::StartMeasurement(MetricVariable::UpdateEntities);
			Game::entityManager->UpdateEntities(deltaTime);
			Metrics::EndMeasurement(MetricVariable::UpdateEntities);

			Metrics::StartMeasurement(MetricVariable::Physics);
			dpWorld::StepWorld(deltaTime);
			Metrics::EndMeasurement(MetricVariable::Physics);

			Metrics::StartMeasurement(MetricVariable::Ghosting);
			if (std::chrono::duration<float>(currentTime - ghostingLastTime).count() >= 1.0f) {
				Game::entityManager->UpdateGhosting();
				ghostingLastTime = currentTime;
			}
			Metrics::EndMeasurement(MetricVariable::Ghosting);

			// Often, so drops, coins and kills show on the Economy page within seconds; each write is one small batch
			if (currentTime - economyLastFlush >= std::chrono::seconds(5)) {
				EconomyLedger::Flush();
				economyLastFlush = currentTime;
			}
			DashboardNotify::Flush();
			DashboardNotify::SendPlayerPositions(g_InstanceID);
			MessageInspector::Update();
			LiveEvents::Update();

			Metrics::StartMeasurement(MetricVariable::UpdateSpawners);
			Game::zoneManager->Update(deltaTime);
			Metrics::EndMeasurement(MetricVariable::UpdateSpawners);

			WorldMigration::Update(deltaTime);
		}

		Metrics::StartMeasurement(MetricVariable::PacketHandling);

		//Check for packets here:
		packet = Game::server->ReceiveFromMaster();
		while (packet) { //We can get messages not handle-able by the dServer class, so handle them if we returned anything.
			HandleMasterPacket(packet);
			Game::server->DeallocateMasterPacket(packet);
			packet = Game::server->ReceiveFromMaster();
		}

		//Handle our chat packets:
		packet = Game::chatServer->Receive();
		while (packet) {
			HandlePacketChat(packet);
			Game::chatServer->DeallocatePacket(packet);
			packet = Game::chatServer->Receive();
		}

		//Handle world-specific packets:
		float timeSpent = 0.0f;

		UserManager::Instance()->DeletePendingRemovals();

		for (uint32_t curPacket = 0; curPacket < maxPacketsToProcess && timeSpent < maxPacketProcessingTime; curPacket++) {
			packet = Game::server->Receive();
			if (packet) {
				auto t1 = std::chrono::high_resolution_clock::now();
				HandlePacket(packet);
				auto t2 = std::chrono::high_resolution_clock::now();

				timeSpent += std::chrono::duration_cast<std::chrono::duration<float>>(t2 - t1).count();
				Game::server->DeallocatePacket(packet);
				packet = nullptr;
			} else {
				break;
			}
		}

		Metrics::EndMeasurement(MetricVariable::PacketHandling);

		Metrics::StartMeasurement(MetricVariable::UpdateReplica);

		//Update our replica objects:
		Game::server->UpdateReplica();

		Metrics::EndMeasurement(MetricVariable::UpdateReplica);

		//Push our log every 15s:
		if (framesSinceLastFlush >= logFlushTime) {
			Game::logger->Flush();
			framesSinceLastFlush = 0;
		} else framesSinceLastFlush++;

		if (zoneID != 0 && !occupied) {
			framesSinceLastUser++;

			//If we haven't had any players for a while, time out and shut down:
			if (framesSinceLastUser >= emptyShutdownTime) {
				Game::lastSignal = -1;
			}
		} else {
			framesSinceLastUser = 0;
		}

		//Save all connected users every 10 minutes:
		if (framesSinceLastUsersSave >= saveTime && zoneID != 0) {
			UserManager::Instance()->SaveAllActiveCharacters();
			framesSinceLastUsersSave = 0;

			if (PropertyManagementComponent::Instance() != nullptr) {
				PropertyManagementComponent::Instance()->Save();
			}
		} else framesSinceLastUsersSave++;

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

		Metrics::EndMeasurement(MetricVariable::GameLoop);

		Metrics::StartMeasurement(MetricVariable::Sleep);

		t += std::chrono::milliseconds(currentFrameDelta);
		std::this_thread::sleep_until(t);

		Metrics::EndMeasurement(MetricVariable::Sleep);

		if (!ready && Game::server->GetIsConnectedToMaster()) {
			LOG("Finished loading world with zone (%i), ready up!", Game::server->GetZoneID());

			MasterPackets::SendWorldReady(Game::server, Game::server->GetZoneID(), Game::server->GetInstanceID());

			ready = true;
		}

		if (Game::ShouldShutdown() && !g_WorldShutdownSequenceComplete) {
			WorldShutdownProcess(zoneID);
			break;
		}

		Metrics::AddMeasurement(MetricVariable::CPUTime, (1e6 * (1000.0 * (std::clock() - metricCPUTimeStart))) / CLOCKS_PER_SEC);
		Metrics::EndMeasurement(MetricVariable::Frame);
	}
	FinalizeShutdown();
	return EXIT_SUCCESS;
}

void HandlePacketChat(Packet* packet) {
	if (packet->length < 1) return;
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		LOG("Lost our connection to chat, zone(%i), instance(%i)", Game::server->GetZoneID(), Game::server->GetInstanceID());

		g_ChatConnected = false;
	}

	if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED) {
		LOG("Established connection to chat, zone(%i), instance (%i)", Game::server->GetZoneID(), Game::server->GetInstanceID());
		Game::chatSysAddr = packet->systemAddress;

		g_ChatConnected = true;
	}

	if (packet->data[0] == ID_USER_PACKET_ENUM && packet->length >= 4) {
		if (static_cast<ServiceType>(packet->data[1]) == ServiceType::CHAT) {
			switch (static_cast<MessageType::Chat>(packet->data[3])) {
			case MessageType::Chat::WORLD_ROUTE_PACKET: {
				CINSTREAM_SKIP_HEADER;
				LWOOBJID playerID;
				inStream.Read(playerID);

				auto player = Game::entityManager->GetEntity(playerID);
				if (!player) return;

				auto sysAddr = player->GetSystemAddress();

				//Write our stream outwards:
				CBITSTREAM;
				unsigned char data;
				while (inStream.Read(data)) {
					bitStream.Write(data);
				}

				SEND_PACKET; //send routed packet to player
				break;
			}

			case MessageType::Chat::GM_ANNOUNCE: {
				CINSTREAM_SKIP_HEADER;

				std::string title;
				std::string msg;

				uint32_t len;
				inStream.Read<uint32_t>(len);
				for (uint32_t i = 0; len > i; i++) {
					char character;
					inStream.Read<char>(character);
					title += character;
				}

				len = 0;
				inStream.Read<uint32_t>(len);
				for (uint32_t i = 0; len > i; i++) {
					char character;
					inStream.Read<char>(character);
					msg += character;
				}

				//Send to our clients:
				AMFArrayValue args;

				args.Insert("title", title);
				args.Insert("message", msg);

				GameMessages::SendUIMessageServerToAllClients("ToggleAnnounce", args);

				break;
			}

			case MessageType::Chat::GM_MUTE: {
				CINSTREAM_SKIP_HEADER;
				LWOOBJID playerId;
				time_t expire = 0;
				inStream.Read(playerId);
				inStream.Read(expire);

				auto* entity = Game::entityManager->GetEntity(playerId);
				auto* character = entity != nullptr ? entity->GetCharacter() : nullptr;
				auto* user = character != nullptr ? character->GetParentUser() : nullptr;
				if (user) {
					user->SetMuteExpire(expire);

					entity->GetCharacter()->SendMuteNotice();
				}

				break;
			}

			case MessageType::Chat::TEAM_GET_STATUS: {
				CINSTREAM_SKIP_HEADER;

				LWOOBJID teamID = 0;
				char lootOption = 0;
				char memberCount = 0;
				std::vector<LWOOBJID> members;

				inStream.Read(teamID);
				bool deleteTeam = inStream.ReadBit();

				if (deleteTeam) {
					TeamManager::Instance()->DeleteTeam(teamID);

					LOG("Deleting team (%llu)", teamID);

					break;
				}

				inStream.Read(lootOption);
				inStream.Read(memberCount);
				LOG("Updating team ID:(%llu), Loot:(%i), #Members:(%i)", teamID, lootOption, memberCount);
				for (char i = 0; i < memberCount; i++) {
					LWOOBJID member = LWOOBJID_EMPTY;
					inStream.Read(member);
					members.push_back(member);

					LOG("Added member (%llu) to the team", member);
				}

				TeamManager::Instance()->UpdateTeam(teamID, lootOption, members);

				break;
			}
			default:
				LOG("Received an unknown chat: %s", StringifiedEnum::ToString(static_cast<MessageType::Chat>(packet->data[3])).data());
			}
		}
	}
}

void HandleMasterPacket(Packet* packet) {
	if (packet->length < 2) return;
	if (static_cast<ServiceType>(packet->data[1]) != ServiceType::MASTER || packet->length < 4) return;
	switch (static_cast<MessageType::Master>(packet->data[3])) {

	case MessageType::Master::SESSION_KEY_RESPONSE: {
		//Read our session key and to which user it belongs:
		CINSTREAM_SKIP_HEADER;
		uint32_t sessionKey = 0;
		inStream.Read(sessionKey);
		LUWString username;
		inStream.Read(username);

		//Find them:
		auto it = g_PendingUsers.find(username.GetAsString());
		if (it == g_PendingUsers.end()) return;

		//Convert our key:
		std::string userHash = std::to_string(sessionKey);
		userHash = md5(userHash);

		//Verify it:
		if (userHash != it->second.hash) {
			LOG("SOMEONE IS TRYING TO HACK? SESSION KEY MISMATCH: ours: %s != master: %s", userHash.c_str(), it->second.hash.c_str());
			Game::server->Disconnect(it->second.sysAddr, eServerDisconnectIdentifiers::INVALID_SESSION_KEY);
			return;
		} else {
			LOG("User %s authenticated with correct key.", username.GetAsString().c_str());

			UserManager::Instance()->DeleteUser(packet->systemAddress);

			//Create our user and send them in:
			UserManager::Instance()->CreateUser(it->second.sysAddr, username.GetAsString(), userHash);

			// Moved here by an experimental seamless migration: the client still has the zone loaded
			const auto sysAddr = it->second.sysAddr;
			auto* newUser = UserManager::Instance()->GetUser(sysAddr);
			const auto* lastCharacter = newUser ? newUser->GetLastUsedChar() : nullptr;
			const auto seamlessCharacter = lastCharacter && WorldMigration::ArrivesSeamlessly(lastCharacter->GetObjectID()) ? lastCharacter->GetObjectID() : LWOOBJID_EMPTY;

			if (Game::zoneManager->HasZone() && seamlessCharacter == LWOOBJID_EMPTY) {
				float x = 0.0f;
				float y = 0.0f;
				float z = 0.0f;

				auto zone = Game::zoneManager->GetZone();
				if (zone->GetZoneID().GetMapID() == 1100) {
					auto pos = zone->GetSpawnPos();
					x = pos.x;
					y = pos.y;
					z = pos.z;
				}

				ClientPackets::LoadStaticZone loadZone;
				const auto zoneID = Game::zoneManager->GetZoneID();
				loadZone.mapID = zoneID.GetMapID();
				loadZone.instanceID = zoneID.GetInstanceID();
				loadZone.cloneID = 0; // DLU has always sent 0 here, whatever the clone
				loadZone.mapChecksum = zone->GetChecksum();
				loadZone.playerPosition = NiPoint3(x, y, z);
				loadZone.Send(it->second.sysAddr);
			}

			if (Game::server->GetZoneID() == 0) {
				//Since doing this reroute breaks the client's request, we have to call this manually.
				UserManager::Instance()->RequestCharacterList(it->second.sysAddr);
			}

			g_PendingUsers.erase(username.GetAsString());

			//Notify master:
			{
				CBITSTREAM;
				BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::PLAYER_ADDED);
				bitStream.Write<LWOMAPID>(Game::server->GetZoneID());
				bitStream.Write<LWOINSTANCEID>(g_InstanceID);
				Game::server->SendToMaster(bitStream);
			}

			if (seamlessCharacter != LWOOBJID_EMPTY) {
				LoadPlayer(sysAddr);
				WorldMigration::OnSeamlessArrival(Game::entityManager->GetEntity(seamlessCharacter));
			}
		}

		break;
	}
	case MessageType::Master::AFFIRM_TRANSFER_REQUEST: {
		CINSTREAM_SKIP_HEADER;
		uint64_t requestID;
		inStream.Read(requestID);
		LOG("Got affirmation request of transfer %llu", requestID);

		CBITSTREAM;

		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::AFFIRM_TRANSFER_RESPONSE);
		bitStream.Write(requestID);
		Game::server->SendToMaster(bitStream);

		break;
	}

	case MessageType::Master::SHUTDOWN: {
		Game::lastSignal = -1;
		LOG("Got shutdown request from master, zone (%i), instance (%i)", Game::server->GetZoneID(), Game::server->GetInstanceID());
		break;
	}

	case MessageType::Master::MIGRATE_PLAYERS: {
		CINSTREAM_SKIP_HEADER;
		MigratePlayersOrder order;
		if (order.Deserialize(inStream)) WorldMigration::HandleOrder(order);
		break;
	}

	case MessageType::Master::MIGRATE_PLAYER_STATE: {
		CINSTREAM_SKIP_HEADER;
		CarriedPlayerState state;
		if (state.Deserialize(inStream)) WorldMigration::StoreCarriedState(state);
		break;
	}

	case MessageType::Master::MIGRATE_STATUS: {
		CINSTREAM_SKIP_HEADER;
		MigrationStatus status;
		if (status.Deserialize(inStream)) WorldMigration::HandleStatus(status);
		break;
	}

	case MessageType::Master::PLAYER_ACTION: {
		CINSTREAM_SKIP_HEADER;
		PlayerActionRequest request;
		if (!request.Deserialize(inStream)) break;

		PlayerActionResult result;
		result.requestId = request.requestId;
		result.action = request.action;
		result.affected = DashboardActions::Apply(request);

		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::PLAYER_ACTION_RESULT);
		result.Serialize(bitStream);
		Game::server->SendToMaster(bitStream);
		break;
	}

	case MessageType::Master::MESSAGE_CAPTURE_CONTROL: {
		CINSTREAM_SKIP_HEADER;
		MessageCaptureControl control;
		if (control.Deserialize(inStream)) MessageInspector::Control(control);
		break;
	}

	case MessageType::Master::ANNOUNCE: {
		CINSTREAM_SKIP_HEADER;
		Announcement announcement;
		if (announcement.Deserialize(inStream)) DashboardNotify::Announce(announcement.title, announcement.message);
		break;
	}

	case MessageType::Master::NEW_SESSION_ALERT: {
		CINSTREAM_SKIP_HEADER;
		uint32_t sessionKey{};
		inStream.Read(sessionKey);

		LUString username;
		inStream.Read(username);
		LOG("Got new session alert for user %s", username.string.c_str());
		//Find them:
		User* user = UserManager::Instance()->GetUser(username.string.c_str());
		if (!user) {
			LOG("But they're not logged in?");
			return;
		}

		//Check the key:
		if (sessionKey != std::atoi(user->GetSessionKey().c_str())) {
			LOG("But the session key is invalid!", username.string.c_str());
			Game::server->Disconnect(user->GetSystemAddress(), eServerDisconnectIdentifiers::INVALID_SESSION_KEY);
			return;
		}
		break;
	}
	default:
		LOG("Unknown packet ID from master %i", int(packet->data[3]));
	}
}

// Creates the player's entity and sends the client everything in the world: after the client loaded the zone
// (LEVEL_LOAD_COMPLETE), or at once when it kept its scene (an experimental seamless migration)
void LoadPlayer(const SystemAddress& sysAddr) {
	User* user = UserManager::Instance()->GetUser(sysAddr);
	if (user) {
		Character* c = user->GetLastUsedChar();
		if (c != nullptr) {
			if (Game::entityManager->GetEntity(c->GetObjectID())) return;
			std::u16string username = GeneralUtils::ASCIIToUTF16(c->GetName());
			Game::server->GetReplicaManager()->AddParticipant(sysAddr);

			EntityInfo info{};
			info.lot = 1;
			Entity* player = Game::entityManager->CreateEntity(info, UserManager::Instance()->GetUser(sysAddr));

			auto* characterComponent = player->GetComponent<CharacterComponent>();
			if (!characterComponent) return;

			// Do charxml fixes here
			auto* levelComponent = player->GetComponent<LevelProgressionComponent>();
			auto* const inventoryComponent = player->GetComponent<InventoryComponent>();
			auto* const missionComponent = player->GetComponent<MissionComponent>();
			if (!levelComponent || !missionComponent || !inventoryComponent) return;

			auto version = levelComponent->GetCharacterVersion();
			LOG("Updating character from version %s", StringifiedEnum::ToString(version).data());
			if (version < eCharacterVersion::UP_TO_DATE) {
				switch (version) {
				case eCharacterVersion::RELEASE:
					// TODO: Implement, super low priority
					[[fallthrough]];
				case eCharacterVersion::LIVE:
					LOG("Updating Character Flags");
					c->SetRetroactiveFlags();
					levelComponent->SetCharacterVersion(eCharacterVersion::PLAYER_FACTION_FLAGS);
					[[fallthrough]];
				case eCharacterVersion::PLAYER_FACTION_FLAGS:
					LOG("Updating Vault Size");
					player->RetroactiveVaultSize();
					levelComponent->SetCharacterVersion(eCharacterVersion::VAULT_SIZE);
					[[fallthrough]];
				case eCharacterVersion::VAULT_SIZE:
					LOG("Updaing Speedbase");
					levelComponent->SetRetroactiveBaseSpeed();
					levelComponent->SetCharacterVersion(eCharacterVersion::SPEED_BASE);
					[[fallthrough]];
				case eCharacterVersion::SPEED_BASE: {
					LOG("Removing lots from NJ Jay missions bugged at foss");
					// https://explorer.lu/missions/1789
					const auto* mission = missionComponent->GetMission(1789);
					if (mission && mission->IsComplete()) {
						inventoryComponent->RemoveItem(14474, 1, eInventoryType::ITEMS);
						inventoryComponent->RemoveItem(14474, 1, eInventoryType::VAULT_ITEMS);
					}
					// https://explorer.lu/missions/1927
					mission = missionComponent->GetMission(1927);
					if (mission && mission->IsComplete()) {
						inventoryComponent->RemoveItem(14493, 1, eInventoryType::ITEMS);
						inventoryComponent->RemoveItem(14493, 1, eInventoryType::VAULT_ITEMS);
					}
					levelComponent->SetCharacterVersion(eCharacterVersion::NJ_JAYMISSIONS);
					[[fallthrough]];
				}
				case eCharacterVersion::NJ_JAYMISSIONS: {
					LOG("Fixing Nexus Force Explorer missions");
					auto missions = { 502 /* Pet Cove */, 593/* Nimbus Station */, 938/* Avant Gardens */, 284/* Gnarled Forest */, 754/* Forbidden Valley */ };
					bool complete = true;
					for (auto missionID : missions) {
						auto* mission = missionComponent->GetMission(missionID);
						if (!mission || !mission->IsComplete()) {
							complete = false;
						}
					}

					if (complete) missionComponent->CompleteMission(937 /* Nexus Force explorer */);
					levelComponent->SetCharacterVersion(eCharacterVersion::NEXUS_FORCE_EXPLORER);
					[[fallthrough]];
				}
				case eCharacterVersion::NEXUS_FORCE_EXPLORER: {
					LOG("Fixing pet IDs");

					// First copy the original ids
					const auto pets = inventoryComponent->GetPetsMut();

					// Then clear the pets so we can re-add them with the updated IDs
					auto& invPets = inventoryComponent->GetPetsMut();
					invPets.clear();
					for (auto& [id, databasePet] : pets) {
						const auto originalID = id;
						const auto newId = GeneralUtils::ClearBit(id, 32); // Persistent bit that didn't exist
						LOG("New ID %llu", newId);
						auto* item = inventoryComponent->FindItemBySubKey(originalID);
						if (item) {
							LOG("item subkey %llu", item->GetSubKey());
							item->SetSubKey(newId);
							invPets[newId] = databasePet;
						}
					}
					levelComponent->SetCharacterVersion(eCharacterVersion::PET_IDS);
					[[fallthrough]];
				}
				case eCharacterVersion::PET_IDS: {
					LOG("Regenerating item ids");
					inventoryComponent->RegenerateItemIDs();
					levelComponent->SetCharacterVersion(eCharacterVersion::INVENTORY_PERSISTENT_IDS);
					[[fallthrough]];
				}
				case eCharacterVersion::INVENTORY_PERSISTENT_IDS: {
					LOG("Fixing racing meta missions");
					missionComponent->FixRacingMetaMissions();
					levelComponent->SetCharacterVersion(eCharacterVersion::UP_TO_DATE);
					[[fallthrough]];
				}
				case eCharacterVersion::UP_TO_DATE:
					break;
				}
			}

			// Update the characters xml to ensure the update above is not only saved, but so the client picks up on the changes.
			c->SaveXMLToDatabase();

			// Fix the destroyable component
			auto* destroyableComponent = player->GetComponent<DestroyableComponent>();

			if (destroyableComponent != nullptr) {
				destroyableComponent->FixStats();
			}

			ClientPackets::CreateCharacter createCharacter;
			createCharacter.objectID = player->GetObjectID();
			createCharacter.xmlData = c->GetXMLData();
			createCharacter.name = username;
			createCharacter.gmLevel = c->GetGMLevel();
			createCharacter.chatMode = static_cast<int32_t>(c->GetGMLevel());
			createCharacter.reputation = characterComponent->GetReputation();
			createCharacter.propertyCloneID = c->GetPropertyCloneID();
			createCharacter.Send(sysAddr);
			LOG("Sent CreateCharacter for ID: %llu", player->GetObjectID());
			ClientPackets::ServerStates serverStates;
			serverStates.Send(sysAddr);

			const auto respawnPoint = player->GetCharacter()->GetRespawnPoint(Game::zoneManager->GetZone()->GetWorldID());

			Game::entityManager->ConstructEntity(player, UNASSIGNED_SYSTEM_ADDRESS);

			if (respawnPoint != NiPoint3Constant::ZERO) {
				GameMessages::SendPlayerReachedRespawnCheckpoint(player, respawnPoint, QuatUtils::IDENTITY);
			}

			Game::entityManager->ConstructAllEntities(sysAddr);

			characterComponent->RocketUnEquip(player);

			player->GetCharacter()->SetTargetScene("");

			//Tell the player to generate BBB models, if any:
			if (g_CloneID != 0) {
				const auto& worldId = Game::zoneManager->GetZone()->GetZoneID();

				const auto zoneId = worldId.GetMapID();
				const auto cloneId = g_CloneID;

				//Check for BBB models:
				auto propertyInfo = Database::Get()->GetPropertyInfo(zoneId, cloneId);

				LWOOBJID propertyId = LWOOBJID_EMPTY;
				if (propertyInfo) propertyId = propertyInfo->id;
				else {
					LOG("Couldn't find property ID for zone %i, clone %i", zoneId, cloneId);
					goto noBBB;
				}

				// Workaround for not having a UGC server to get model LXFML onto the client so it
				// can generate the physics and nif for the object.

				auto bbbModels = Database::Get()->GetUgcModels(propertyId);
				if (bbbModels.empty()) {
					LOG("No BBB models found for property %llu", propertyId);
					goto noBBB;
				}

				ClientPackets::BlueprintSaveResponse response;
				response.localId = LWOOBJID_EMPTY; //always zero so that a check on the client passes
				response.reasonCode = eBlueprintSaveResponseType::EverythingWorked;
				for (auto& bbbModel : bbbModels) {
					LOG("Getting lxfml ugcID: %llu", bbbModel.id);

					bbbModel.lxfmlData.seekg(0, std::ios::end);
					size_t lxfmlSize = bbbModel.lxfmlData.tellg();
					bbbModel.lxfmlData.seekg(0);

					// write data
					auto& model = response.models.emplace_back();
					model.blueprintId = bbbModel.id;
					model.data = bbbModel.lxfmlData.str().substr(0, lxfmlSize);
				}
				response.Send(sysAddr);
			}

		noBBB:

			// Tell the client it's done loading:
			GameMessages::SendInvalidZoneTransferList(player, sysAddr, GeneralUtils::ASCIIToUTF16(Game::config->GetValue("source")), u"", false, false);
			GameMessages::SendServerDoneLoadingAllObjects(player, sysAddr);

			//Send the player it's mail count:
			//update: this might not be needed so im going to try disabling this here.
			//Mail::HandleNotificationRequest(sysAddr, player->GetObjectID());

			//Notify chat that a player has loaded:
			auto* character = player->GetCharacter();
			auto* user = character != nullptr ? character->GetParentUser() : nullptr;
			if (user) {
				const auto& playerName = character->GetName();

				CBITSTREAM;
				BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::LOGIN_SESSION_NOTIFY);
				bitStream.Write(player->GetObjectID());
				bitStream.Write<uint32_t>(playerName.size());
				for (size_t i = 0; i < playerName.size(); i++) {
					bitStream.Write(playerName[i]);
				}

				auto zone = Game::zoneManager->GetZone()->GetZoneID();
				bitStream.Write(zone.GetMapID());
				bitStream.Write(zone.GetInstanceID());
				bitStream.Write(zone.GetCloneID());
				bitStream.Write(user->GetMuteExpire());
				bitStream.Write(player->GetGMLevel());

				Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
			}
		} else {
			LOG("Couldn't find character to log in with for user %s (%i)!", user->GetUsername().c_str(), user->GetAccountID());
			Game::server->Disconnect(sysAddr, eServerDisconnectIdentifiers::CHARACTER_NOT_FOUND);
		}
	} else {
		LOG("Couldn't get user for level load complete!");
	}
}

// Save and remove a user whose connection closed (or who is being dropped). Safe to call twice: the second call finds
// no user and does nothing.
void CleanupDisconnectedUser(const SystemAddress& sysAddr) {
	// Players moved to another instance were saved when they were sent; saving again could overwrite what the
	// other instance has saved since
	const bool savedByMigration = WorldMigration::IsLeaving(sysAddr);
	WorldMigration::OnDisconnected(sysAddr);

	auto user = UserManager::Instance()->GetUser(sysAddr);
	if (!user) return;

	auto c = user->GetLastUsedChar();
	if (!c) {
		UserManager::Instance()->DeleteUser(sysAddr);
		return;
	}

	auto* entity = Game::entityManager->GetEntity(c->GetObjectID());

	if (!entity) {
		entity = PlayerManager::GetPlayer(sysAddr);
	}

	if (entity) {
		auto* skillComponent = entity->GetComponent<SkillComponent>();

		if (skillComponent != nullptr) {
			skillComponent->Reset();
		}

		if (!savedByMigration) entity->GetCharacter()->SaveXMLToDatabase();

		LOG("Deleting player %llu", entity->GetObjectID());

		Game::entityManager->DestroyEntity(entity);
	}

	{
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::UNEXPECTED_DISCONNECT);
		bitStream.Write(user->GetLoggedInChar());
		Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
	}

	UserManager::Instance()->DeleteUser(sysAddr);

	if (PropertyManagementComponent::Instance() != nullptr) {
		PropertyManagementComponent::Instance()->Save();
	}

	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::PLAYER_REMOVED);
	bitStream.Write<LWOMAPID>(Game::server->GetZoneID());
	bitStream.Write<LWOINSTANCEID>(g_InstanceID);
	Game::server->SendToMaster(bitStream);
}

// The world server's handlers for what clients send (ServiceType::WORLD). WorldPackets has the structs; each one is
// handled by overriding Handle here, where the server's state is.
namespace {
	struct ValidationPacket final : public WorldPackets::Validation {
		void Handle() override {
			const auto& clientDatabaseChecksum = fdbChecksum;

			// If the check is turned on, validate the client's database checksum.
			if (Game::config->GetValue("check_fdb") == "1" && !g_DatabaseChecksum.empty()) {
				auto accountInfo = Database::Get()->GetAccountInfo(username.GetAsString());
				if (!accountInfo) {
					LOG("Client's account does not exist in the database, aborting connection.");
					Game::server->Disconnect(sysAddr, eServerDisconnectIdentifiers::CHARACTER_NOT_FOUND);
					return;
				}

				// Developers may skip this check
				if (clientDatabaseChecksum.string != g_DatabaseChecksum) {

					if (accountInfo->maxGmLevel < eGameMasterLevel::DEVELOPER) {
						LOG("Client's database checksum does not match the server's, aborting connection.");
						Stamps stamps;
						stamps.Add(eStamps::PASSPORT_AUTH_ERROR, 1);

						// Using the LoginResponse here since the UI is still in the login screen state
						// and we have a way to send a message about the client mismatch.
						AuthPackets::SendLoginResponse(
							Game::server, sysAddr, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH,
							Game::config->GetValue("cdclient_mismatch_message"), "", 0, "", stamps);
						return;
					} else {
						AMFArrayValue args;

						args.Insert("title", Game::config->GetValue("cdclient_mismatch_title"));
						args.Insert("message", Game::config->GetValue("cdclient_mismatch_message"));

						GameMessages::SendUIMessageServerToSingleClient("ToggleAnnounce", args, sysAddr);
						LOG("Account (%s) with GmLevel (%s) does not have a matching FDB, but is a developer and will skip this check."
							, username.GetAsString().c_str(), StringifiedEnum::ToString(accountInfo->maxGmLevel).data());
					}
				}
			}

			//Request the session info from Master:
			CBITSTREAM;
			BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::REQUEST_SESSION_KEY);
			bitStream.Write(username);
			Game::server->SendToMaster(bitStream);

			//Insert info into our pending list
			TempSessionInfo info;
			info.sysAddr = sysAddr;
			info.hash = sessionKey.GetAsString();
			g_PendingUsers[username.GetAsString()] = info;
		}
	};

	struct CharacterListRequestPacket final : public WorldPackets::CharacterListRequest {
		void Handle() override {
			//We need to delete the entity first, otherwise the char list could delete it while it exists in the world!
			if (Game::server->GetZoneID() != 0) {
				auto user = UserManager::Instance()->GetUser(sysAddr);
				if (!user || !user->GetLastUsedChar()) return;
				Game::entityManager->DestroyEntity(user->GetLastUsedChar()->GetEntity());
			}

			//This loops prevents users who aren't authenticated to double-request the char list, which
			//would make the login screen freeze sometimes.
			if (g_PendingUsers.size() > 0) {
				for (const auto& it : g_PendingUsers) {
					if (it.second.sysAddr == sysAddr) {
						return;
					}
				}
			}

			UserManager::Instance()->RequestCharacterList(sysAddr);
		}
	};

	struct GameMessagePacket final : public WorldPackets::GameMessage {
		void Handle() override {
			auto isSender = CheatDetection::VerifyLwoobjidIsSender(
				objectID,
				sysAddr,
				CheckType::Entity,
				"Sending GM with a sending player that does not match their own. GM ID: %i",
				static_cast<int32_t>(messageID)
			);

			if (isSender) GameMessageHandler::HandleMessage(data, sysAddr, objectID, messageID);
		}
	};

	struct CharacterCreateRequestPacket final : public WorldPackets::CharacterCreateRequest {
		void Handle() override {
			UserManager::Instance()->CreateCharacter(sysAddr, *this);
		}
	};

	struct CharacterLoginRequestPacket final : public WorldPackets::CharacterLoginRequest {
		void Handle() override {
			LOG("User is requesting to login with character %llu", playerID);
			bool valid = CheatDetection::VerifyLwoobjidIsSender(
				playerID,
				sysAddr,
				CheckType::User,
				"Sending login request with a sending player that does not match their own. Player ID: %llu",
				playerID
			);
			LOG("Login request for player %llu is %s", playerID, valid ? "valid" : "invalid");
			if (!valid) return;

			auto user = UserManager::Instance()->GetUser(sysAddr);

			if (user) {
				auto lastCharacter = user->GetLoggedInChar();
				// This means we swapped characters and we need to remove the previous player from the container.
				if (lastCharacter != playerID) {
					CBITSTREAM;
					BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::UNEXPECTED_DISCONNECT);
					bitStream.Write(lastCharacter);
					Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
				}
			}

			UserManager::Instance()->LoginCharacter(sysAddr, playerID);
		}
	};

	struct CharacterDeleteRequestPacket final : public WorldPackets::CharacterDeleteRequest {
		void Handle() override {
			UserManager::Instance()->DeleteCharacter(sysAddr, *this);
		}
	};

	struct CharacterRenameRequestPacket final : public WorldPackets::CharacterRenameRequest {
		void Handle() override {
			UserManager::Instance()->RenameCharacter(sysAddr, *this);
		}
	};

	struct LevelLoadCompletePacket final : public WorldPackets::LevelLoadComplete {
		void Handle() override {
			LOG("Received level load complete from user.");
			LoadPlayer(sysAddr);
		}
	};

	struct PositionUpdatePacket final : public WorldPackets::PositionUpdate {
		void Handle() override {
			auto positionUpdate = update;

			User* user = UserManager::Instance()->GetUser(sysAddr);
			if (!user) {
				LOG("Unable to get user to parse position update");
				return;
			}

			if (const auto* const lastChar = user->GetLastUsedChar()) {
				if (auto* const entity = Game::entityManager->GetEntity(lastChar->GetObjectID())) {
					entity->ProcessPositionUpdate(positionUpdate);
				}
			}
		}
	};

	struct MailPacket final : public WorldPackets::MailPacket {
		void Handle() override {
			if (auto* const user = UserManager::Instance()->GetUser(sysAddr)) {
				if (auto* const lastChar = user->GetLastUsedChar()) {
					if (auto* const entity = lastChar->GetEntity()) {
						Mail::HandleMail(data, sysAddr, entity);
					}
				}
			}
		}
	};

	struct RoutePacket final : public WorldPackets::RoutePacket {
		void Handle() override {
			//Yeet to chat
			if (size > 20000) {
				LOG("Tried to route a packet with a read size > 20000, so likely a false packet.");
				return;
			}

			//We need to insert the player's objectID so the chat server can find who originated this request:
			LWOOBJID objectID = 0;
			auto user = UserManager::Instance()->GetUser(sysAddr);
			if (user) {
				const auto* const lastChar = user->GetLastUsedChar();
				if (lastChar) objectID = lastChar->GetObjectID();
			}

			const auto routed = ToChat(objectID);
			RakNet::BitStream bitStream;
			routed.WritePacket(bitStream);
			Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, Game::chatSysAddr, false);
		}
	};

	struct StringCheckPacket final : public WorldPackets::StringCheck {
		void Handle() override {
			const auto receiver = GetNarrowReceiver();
			const auto message = GetNarrowMessage();

			// TODO: Find a good home for the logic in this case.
			User* user = UserManager::Instance()->GetUser(sysAddr);
			if (!user) {
				LOG("Unable to get user to parse chat moderation request");
				return;
			}

			auto* entity = PlayerManager::GetPlayer(sysAddr);

			if (entity == nullptr) {
				LOG("Unable to get player to parse chat moderation request");
				return;
			}

			// Check if the player has restricted chat access
			auto* character = entity->GetCharacter();

			if (character->HasPermission(ePermissionMap::RestrictedChatAccess)) {
				// Send a message to the player
				ChatPackets::SendSystemMessage(
					sysAddr,
					u"This character has restricted chat access."
				);

				return;
			}

			bool isBestFriend = false;

			if (chatLevel == 1) {
				// Private chat
				LWOOBJID idOfReceiver = LWOOBJID_EMPTY;

				{
					auto characterIdFetch = Database::Get()->GetCharacterInfo(receiver);

					if (characterIdFetch) {
						idOfReceiver = characterIdFetch->id;
					}
				}
				const auto& bffMap = user->GetIsBestFriendMap();
				if (bffMap.find(receiver) == bffMap.end() && idOfReceiver != LWOOBJID_EMPTY) {
					auto bffInfo = Database::Get()->GetBestFriendStatus(entity->GetObjectID(), idOfReceiver);

					if (bffInfo) {
						isBestFriend = bffInfo->bestFriendStatus == 3;
					}

					if (isBestFriend) {
						user->UpdateBestFriendValue(receiver, true);
					}
				} else if (bffMap.find(receiver) != bffMap.end()) {
					isBestFriend = true;
				}
			}

			const auto segments = Game::chatFilter->IsSentenceOkay(message, entity->GetGMLevel(), !(isBestFriend && chatLevel == 1));

			bool bAllClean = segments.empty();

			if (user->GetIsMuted()) {
				bAllClean = false;
			}

			user->SetLastChatMessageApproved(bAllClean);

			ClientPackets::ChatModerationString response;
			response.requestAccepted = segments.empty(); // What DLU has always sent, even when bAllClean is false
			response.requestID = requestID;
			response.receiver = LUWString(receiver, 42);
			response.rejectedSegments = segments;
			response.Send(sysAddr);
		}
	};

	struct GeneralChatMessagePacket final : public WorldPackets::GeneralChatMessage {
		void Handle() override {
			if (g_ChatDisabled) {
				ChatPackets::SendMessageFail(sysAddr);
			} else {
				// TODO: Find a good home for the logic in this case.
				User* user = UserManager::Instance()->GetUser(sysAddr);
				if (!user) {
					LOG("Unable to get user to parse chat message");
					return;
				}

				const auto* const lastChar = user->GetLastUsedChar();
				if (!lastChar) {
					LOG("No last used character for chat message %i", user->GetAccountID());
					return;
				}

				if (user->GetIsMuted()) {
					lastChar->SendMuteNotice();
					return;
				}
				std::string playerName = lastChar->GetName();
				bool isMythran = lastChar->GetGMLevel() > eGameMasterLevel::CIVILIAN;
				bool isOk = Game::chatFilter->IsSentenceOkay(GeneralUtils::UTF16ToWTF8(message), lastChar->GetGMLevel()).empty();
				LOG_DEBUG("Msg: %s was approved previously? %i", GeneralUtils::UTF16ToWTF8(message).c_str(), user->GetLastChatMessageApproved());
				// Kept for moderation and chat bridges, including what the filter stopped (log_chat=0 turns it off)
				if (Game::config->GetValue("log_chat") != "0") {
					IChatLog::ChatMessage entry;
					entry.time = static_cast<int64_t>(std::time(nullptr));
					entry.channel = "zone";
					entry.senderId = user->GetLoggedInChar();
					entry.senderName = lastChar->GetName();
					entry.accountId = user->GetAccountID();
					entry.zoneId = Game::server->GetZoneID();
					entry.instanceId = static_cast<uint32_t>(Game::server->GetInstanceID());
					entry.cloneId = Game::zoneManager->GetZoneID().GetCloneID();
					entry.message = GeneralUtils::UTF16ToWTF8(message);
					entry.blocked = !isOk;
					try {
						Database::Get()->InsertChatMessage(entry);
					} catch (const std::exception& ex) {
						LOG("Couldn't log a chat message: %s", ex.what());
					}
				}
				if (!isOk) return;
				if (!isOk && !isMythran) return;

				std::string sMessage = GeneralUtils::UTF16ToWTF8(message);
				LOG("%s: %s", playerName.c_str(), sMessage.c_str());
				ChatPackets::SendChatMessage(sysAddr, chatChannel, playerName, user->GetLoggedInChar(), isMythran, message);
				if (PropertyManagementComponent::Instance()) PropertyManagementComponent::Instance()->OnChatMessageReceived(sMessage);
			}
		}
	};

	struct HandleFunnessPacket final : public WorldPackets::HandleFunness {
		void Handle() override {
			//This means the client is running slower or faster than it should.
			//Could be insane lag, but I'mma just YEET them as it's usually speedhacking.
			//This is updated to now count the amount of times we've been caught "speedhacking" to kick with a delay
			//This is hopefully going to fix the random disconnects people face sometimes.

			CaughtFunness funness;
			funness.cheatInfo = this->cheatInfo;
			funness.cheatType = this->cheatType;
			const auto [cheatType, cheatInfo] = funness;
			LOG_DEBUG("Received cheat type %s with info %f", StringifiedEnum::ToString(cheatType).data(), cheatInfo);
			const auto disabledCheats = Game::config->GetValue("disable_anti_speedhack") == "1";

			User* user = UserManager::Instance()->GetUser(sysAddr);
			if (!user) {
				Game::server->Disconnect(sysAddr, eServerDisconnectIdentifiers::KICK);
				return;
			}

			const auto* const character = user->GetLastUsedChar();
			if (character) {
				const auto dcUser = [](const User& user, const SystemAddress& sysAddr) {
					if (user.GetMaxGMLevel() < eGameMasterLevel::DEVELOPER) {
						Game::server->Disconnect(sysAddr, eServerDisconnectIdentifiers::KICK);
					}
					};
				if (cheatType == eFunnessTypes::DebuggerActive) {
					LOG("Player %s was detected to be using a debugger (funness thread was %f seconds longer than expected deviation).", character->GetName().c_str(), cheatInfo);
					// Immediately kick unless they are a gm
					if (!disabledCheats) dcUser(*user, sysAddr);
				} else if (cheatType == eFunnessTypes::FdbFailedChecksum) {
					LOG("Player %s has failed the fdb checksum after passing it to sign into this server, highly likely cheating.", character->GetName().c_str());
					// Immedately kick unless they are a gm
					if (!disabledCheats) dcUser(*user, sysAddr);
				} else if (cheatType == eFunnessTypes::RacingBoostTimeTooLong) {
					if (cheatInfo == 0.0f) LOG("Player %s has enabled a speedboost for far too long (> 3.501 seconds).", character->GetName().c_str());
					else if (cheatInfo == 1.0f) LOG("Unknown racing boost/speed variable was tampered with.");
					else if (cheatInfo == 2.0f) LOG("Player vehicle top speed was tampered with.");
					if (!disabledCheats) dcUser(*user, sysAddr);
				} else if (cheatType == eFunnessTypes::SomeRacingManipCheat) {
					if (cheatInfo >= 0.0f && cheatInfo <= 6.0f) LOG("Cheat RNG value A does not match what it should be %f.", cheatInfo);
					else if (cheatInfo >= 7.0f && cheatInfo <= 10.0f) LOG("Cheat RNG value B does not match what it should be %f.", cheatInfo);
				} else if (cheatType == eFunnessTypes::Unknown_9) {
					LOG("Racing cheat 9 detected with value %f.", cheatInfo);
				} else if (cheatType == eFunnessTypes::Unknown_10) {
					LOG("Racing cheat 10 detected with value %f.", cheatInfo);
				} else if (cheatType == eFunnessTypes::CharacterPosLength) {
					LOG("Detected pos length of %f which is greater than the expected value 1.0f, not normal!", cheatInfo);
				} else if (cheatType == eFunnessTypes::CharacterVelLength) {
					LOG("Detected vel length of %f which is greater than the expected value 1.0f, not normal!", cheatInfo);
				} else if (cheatType == eFunnessTypes::CharacterGravityScale) {
					LOG("Detected gravity scale difference of %f which is greater than the expected value of 0.0f, not normal!", cheatInfo);
				} else if (cheatType == eFunnessTypes::CharacterRunMultiplier) {
					LOG("Detected run multiplier difference of %f which is greater than the expected value 0.0f, not normal!", cheatInfo);
				}

				if (!disabledCheats && user->GetMaxGMLevel() < eGameMasterLevel::DEVELOPER) user->UserOutOfSync(funness);
			}
		}
	};

	struct UIHelpTop5Packet final : public WorldPackets::UIHelpTop5 {
		void Handle() override {
			// TODO: Handle different languages in a nice way
			// 0: en_US
			// 1: pl_US
			// 2: de_DE
			// 3: en_GB

			// TODO: Find a good home for the logic in this case.
			auto* user = UserManager::Instance()->GetUser(sysAddr);
			if (!user) return;
			auto* character = user->GetLastUsedChar();
			if (!character) return;
			auto* entity = character->GetEntity();
			if (!entity) return;

			AMFArrayValue data;
			// Summaries
			data.Insert("Summary0", Game::config->GetValue("help_0_summary"));
			data.Insert("Summary1", Game::config->GetValue("help_1_summary"));
			data.Insert("Summary2", Game::config->GetValue("help_2_summary"));
			data.Insert("Summary3", Game::config->GetValue("help_3_summary"));
			data.Insert("Summary4", Game::config->GetValue("help_4_summary"));

			// Descriptions
			data.Insert("Description0", Game::config->GetValue("help_0_description"));
			data.Insert("Description1", Game::config->GetValue("help_1_description"));
			data.Insert("Description2", Game::config->GetValue("help_2_description"));
			data.Insert("Description3", Game::config->GetValue("help_3_description"));
			data.Insert("Description4", Game::config->GetValue("help_4_description"));

			GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, "UIHelpTop5", data);
		}
	};

	template<typename T>
	std::unique_ptr<WorldPackets::WorldLUBitStream> Create() { return std::make_unique<T>(); }

	const std::map<MessageType::World, std::function<std::unique_ptr<WorldPackets::WorldLUBitStream>()>> g_WorldHandlers = {
		{ MessageType::World::VALIDATION, Create<ValidationPacket> },
		{ MessageType::World::CHARACTER_LIST_REQUEST, Create<CharacterListRequestPacket> },
		{ MessageType::World::GAME_MSG, Create<GameMessagePacket> },
		{ MessageType::World::CHARACTER_CREATE_REQUEST, Create<CharacterCreateRequestPacket> },
		{ MessageType::World::LOGIN_REQUEST, Create<CharacterLoginRequestPacket> },
		{ MessageType::World::CHARACTER_DELETE_REQUEST, Create<CharacterDeleteRequestPacket> },
		{ MessageType::World::CHARACTER_RENAME_REQUEST, Create<CharacterRenameRequestPacket> },
		{ MessageType::World::LEVEL_LOAD_COMPLETE, Create<LevelLoadCompletePacket> },
		{ MessageType::World::POSITION_UPDATE, Create<PositionUpdatePacket> },
		{ MessageType::World::MAIL, Create<MailPacket> },
		{ MessageType::World::ROUTE_PACKET, Create<RoutePacket> },
		{ MessageType::World::STRING_CHECK, Create<StringCheckPacket> },
		{ MessageType::World::GENERAL_CHAT_MESSAGE, Create<GeneralChatMessagePacket> },
		{ MessageType::World::HANDLE_FUNNESS, Create<HandleFunnessPacket> },
		{ MessageType::World::UI_HELP_TOP_5, Create<UIHelpTop5Packet> },
	};
}

void HandlePacket(Packet* packet) {
	if (packet->length < 1) return;
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		CleanupDisconnectedUser(packet->systemAddress);
	}

	// Sent to another instance and locked: nothing more from them counts here
	if (WorldMigration::IsLeaving(packet->systemAddress)) return;

	if (packet->data[0] != ID_USER_PACKET_ENUM || packet->length < 4) return;

	CINSTREAM;
	LUBitStream luBitStream;
	if (!luBitStream.ReadHeader(inStream)) return;

	if (luBitStream.connectionType == ServiceType::COMMON) {
		CommonPackets::Handle(inStream, packet->systemAddress, luBitStream.internalPacketID);
	}

	if (luBitStream.connectionType != ServiceType::WORLD) return;
	const auto messageId = static_cast<MessageType::World>(luBitStream.internalPacketID);
	LOG_DEBUG("Got world packet %s", StringifiedEnum::ToString(messageId).data());

	const auto handler = g_WorldHandlers.find(messageId);
	if (handler == g_WorldHandlers.end()) {
		const std::string_view messageIdString = StringifiedEnum::ToString(messageId);
		LOG("Unknown world packet received: %4i, %s", messageId, messageIdString.data());
		return;
	}

	auto request = handler->second();
	request->sysAddr = packet->systemAddress;
	if (!request->Deserialize(inStream)) {
		LOG("Failed to read world packet %s from %s", StringifiedEnum::ToString(messageId).data(), packet->systemAddress.ToString());
		return;
	}
	request->Handle();
}

void WorldShutdownProcess(uint32_t zoneId) {
	LOG("Saving map %i instance %i", zoneId, g_InstanceID);
	for (auto i = 0; i < Game::server->GetReplicaManager()->GetParticipantCount(); ++i) {
		const auto& player = Game::server->GetReplicaManager()->GetParticipantAtIndex(i);

		auto* entity = PlayerManager::GetPlayer(player);
		LOG("Saving data!");
		if (entity != nullptr && entity->GetCharacter() != nullptr) {
			auto* skillComponent = entity->GetComponent<SkillComponent>();

			if (skillComponent != nullptr) {
				skillComponent->Reset();
			}
			LOG("Saving character %s...", entity->GetCharacter()->GetName().c_str());
			entity->GetCharacter()->SaveXMLToDatabase();
			LOG("Character data for %s was saved!", entity->GetCharacter()->GetName().c_str());
		}
	}

	if (PropertyManagementComponent::Instance() != nullptr) {
		LOG("Saving ALL property data for zone %i clone %i!", zoneId, PropertyManagementComponent::Instance()->GetCloneId());
		PropertyManagementComponent::Instance()->Save();
		LOG("ALL property data saved for zone %i clone %i!", zoneId, PropertyManagementComponent::Instance()->GetCloneId());
	}

	LOG("ALL DATA HAS BEEN SAVED FOR ZONE %i INSTANCE %i!", zoneId, g_InstanceID);

	while (Game::server->GetReplicaManager()->GetParticipantCount() > 0) {
		const auto& player = Game::server->GetReplicaManager()->GetParticipantAtIndex(0);

		Game::server->Disconnect(player, eServerDisconnectIdentifiers::SERVER_SHUTDOWN);
	}
	LiveEvents::Shutdown();
	EconomyLedger::Flush();
	DashboardNotify::Flush(true);
	SendShutdownMessageToMaster();
}

void WorldShutdownSequence() {
	bool shouldShutdown = Game::ShouldShutdown() || g_WorldShutdownSequenceComplete;
	Game::lastSignal = -1;
#ifndef DARKFLAME_PLATFORM_WIN32
	if (shouldShutdown)
#endif
	{
		return;
	}

	if (!Game::logger) return;

	LOG("Zone (%i) instance (%i) shutting down outside of main loop!", Game::server->GetZoneID(), g_InstanceID);
	WorldShutdownProcess(Game::server->GetZoneID());
	FinalizeShutdown();
}

void FinalizeShutdown() {
	LOG("Shutdown complete, zone (%i), instance (%i)", Game::server->GetZoneID(), g_InstanceID);

	//Delete our objects here:
	dpWorld::Shutdown();
	Database::Destroy(g_ServiceName);
	if (Game::chatFilter) delete Game::chatFilter;
	Game::chatFilter = nullptr;
	if (Game::zoneManager) delete Game::zoneManager;
	Game::zoneManager = nullptr;
	if (Game::server) delete Game::server;
	Game::server = nullptr;
	if (Game::config) delete Game::config;
	Game::config = nullptr;
	if (Game::entityManager) delete Game::entityManager;
	Game::entityManager = nullptr;
	if (Game::logger) delete Game::logger;
	Game::logger = nullptr;

	g_WorldShutdownSequenceComplete = true;

	exit(EXIT_SUCCESS);
}

void SendShutdownMessageToMaster() {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::SHUTDOWN_RESPONSE);
	Game::server->SendToMaster(bitStream);
}
