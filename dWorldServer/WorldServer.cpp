#include <iostream>
#include <string>
#include <ctime>
#include <chrono>
#include <thread>

#include "MD5.h"

//DLU Includes:
#include "dCommonVars.h"
#include "dServer.h"
#include "Logger.h"
#include "Database.h"
#include "dConfig.h"
#include "dpWorld.h"
#include "dZoneManager.h"
#include "Metrics.hpp"
#include "PerformanceManager.h"
#include "Diagnostics.h"
#include "BinaryPathFinder.h"

//RakNet includes:
#include "RakNetDefines.h"
#include "RakNetworkFactory.h"
#include "RakString.h"

//World includes:
#include <csignal>

#include "AuthPackets.h"
#include "PacketUtils.h"
#include "BitStreamUtils.h"
#include "WorldPackets.h"
#include "UserManager.h"
#include "CDClientManager.h"
#include "CDClientDatabase.h"
#include "GeneralUtils.h"
#include "ObjectIDManager.h"
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
#include "Player.h"
#include "PropertyManagementComponent.h"
#include "AssetManager.h"
#include "LevelProgressionComponent.h"
#include "eBlueprintSaveResponseType.h"
#include "Amf3.h"
#include "NiPoint3.h"
#include "eServerDisconnectIdentifiers.h"
#include "eObjectBits.h"
#include "eConnectionType.h"
#include "eServerMessageType.h"
#include "eChatInternalMessageType.h"
#include "eWorldMessageType.h"
#include "eMasterMessageType.h"
#include "eGameMessageType.h"
#include "ZCompression.h"
#include "EntityManager.h"
#include "CheatDetection.h"

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dpWorld* physicsWorld = nullptr;
	dChatFilter* chatFilter = nullptr;
	dConfig* config = nullptr;
	AssetManager* assetManager = nullptr;
	RakPeerInterface* chatServer = nullptr;
	std::mt19937 randomEngine;
	SystemAddress chatSysAddr;
	bool shouldShutdown = false;
	EntityManager* entityManager = nullptr;
	dZoneManager* zoneManager = nullptr;
} // namespace Game

bool chatDisabled = false;
bool chatConnected = false;
bool worldShutdownSequenceComplete = false;
void WorldShutdownSequence();
void WorldShutdownProcess(uint32_t zoneId);
void FinalizeShutdown();
void SendShutdownMessageToMaster();

Logger* SetupLogger(uint32_t zoneID, uint32_t instanceID);
void HandlePacketChat(Packet* packet);
void HandlePacket(Packet* packet);

struct tempSessionInfo {
	SystemAddress sysAddr;
	std::string hash;
};

std::map<std::string, tempSessionInfo> m_PendingUsers;
uint32_t instanceID = 0;
uint32_t g_CloneID = 0;
std::string databaseChecksum = "";

int main(int argc, char** argv) {
	Diagnostics::SetProcessName("World");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	// Triggers the shutdown sequence at application exit
	std::atexit(WorldShutdownSequence);

	signal(SIGINT, [](int) { WorldShutdownSequence(); });
	signal(SIGTERM, [](int) { WorldShutdownSequence(); });

	uint32_t zoneID = 1000;
	uint32_t cloneID = 0;
	uint32_t maxClients = 8;
	uint32_t ourPort = 2007;

	//Check our arguments:
	for (int32_t i = 0; i < argc; ++i) {
		std::string argument(argv[i]);

		if (argument == "-zone") zoneID = atoi(argv[i + 1]);
		if (argument == "-instance") instanceID = atoi(argv[i + 1]);
		if (argument == "-clone") cloneID = atoi(argv[i + 1]);
		if (argument == "-maxclients") maxClients = atoi(argv[i + 1]);
		if (argument == "-port") ourPort = atoi(argv[i + 1]);
	}

	//Create all the objects we need to run our service:
	Game::logger = SetupLogger(zoneID, instanceID);
	if (!Game::logger) return EXIT_FAILURE;

	//Read our config:
	Game::config = new dConfig((BinaryPathFinder::GetBinaryDir() / "worldconfig.ini").string());
	Game::logger->SetLogToConsole(Game::config->GetValue("log_to_console") != "0");
	Game::logger->SetLogDebugStatements(Game::config->GetValue("log_debug_statements") == "1");

	LOG("Starting World server...");
	LOG("Version: %i.%i", PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR);
	LOG("Compiled on: %s", __TIMESTAMP__);

	if (Game::config->GetValue("disable_chat") == "1") chatDisabled = true;

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

		return EXIT_FAILURE;
	}

	// Connect to CDClient
	try {
		CDClientDatabase::Connect((BinaryPathFinder::GetBinaryDir() / "resServer" / "CDServer.sqlite").string());
	} catch (CppSQLite3Exception& e) {
		LOG("Unable to connect to CDServer SQLite Database");
		LOG("Error: %s", e.errorMessage());
		LOG("Error Code: %i", e.errorCode());
		return EXIT_FAILURE;
	}

	CDClientManager::Instance();

	Diagnostics::SetProduceMemoryDump(Game::config->GetValue("generate_dump") == "1");

	if (!Game::config->GetValue("dump_folder").empty()) {
		Diagnostics::SetOutDirectory(Game::config->GetValue("dump_folder"));
	}

	//Connect to the MySQL Database:
	try {
		Database::Connect();
	} catch (sql::SQLException& ex) {
		LOG("Got an error while connecting to the database: %s", ex.what());
		return EXIT_FAILURE;
	}

	//Find out the master's IP:
	std::string masterIP = "localhost";
	uint32_t masterPort = 1000;
	auto masterInfo = Database::Get()->GetMasterInfo();

	if (masterInfo) {
		masterIP = masterInfo->ip;
		masterPort = masterInfo->port;
	}

	ObjectIDManager::Instance()->Initialize();
	UserManager::Instance()->Initialize();
	Game::chatFilter = new dChatFilter(Game::assetManager->GetResPath().string() + "/chatplus_en_us", bool(std::stoi(Game::config->GetValue("dont_generate_dcf"))));

	Game::server = new dServer(masterIP, ourPort, instanceID, maxClients, false, true, Game::logger, masterIP, masterPort, ServerType::World, Game::config, &Game::shouldShutdown, zoneID);

	//Connect to the chat server:
	uint32_t chatPort = 1501;
	if (Game::config->GetValue("chat_server_port") != "") chatPort = std::atoi(Game::config->GetValue("chat_server_port").c_str());

	auto chatSock = SocketDescriptor(uint16_t(ourPort + 2), 0);
	Game::chatServer = RakNetworkFactory::GetRakPeerInterface();
	Game::chatServer->Startup(1, 30, &chatSock, 1);
	Game::chatServer->Connect(masterIP.c_str(), chatPort, "3.25 ND1", 8);

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

	PerformanceManager::SelectProfile(zoneID);

	Game::entityManager = new EntityManager();
	Game::zoneManager = new dZoneManager();
	//Load our level:
	if (zoneID != 0) {
		dpWorld::Instance().Initialize(zoneID);
		Game::zoneManager->Initialize(LWOZONEID(zoneID, instanceID, cloneID));
		g_CloneID = cloneID;

	} else {
		Game::entityManager->Initialize();
	}

	// pre calculate the FDB checksum
	if (Game::config->GetValue("check_fdb") == "1") {
		std::ifstream fileStream;

		static const std::vector<std::string> aliases = {
			"CDServers.fdb",
			"cdserver.fdb",
			"CDClient.fdb",
			"cdclient.fdb",
		};

		for (const auto& file : aliases) {
			fileStream.open(Game::assetManager->GetResPath() / file, std::ios::binary | std::ios::in);
			if (fileStream.is_open()) {
				break;
			}
		}

		const int32_t bufferSize = 1024;
		MD5* md5 = new MD5();

		char fileStreamBuffer[1024] = {};

		while (!fileStream.eof()) {
			memset(fileStreamBuffer, 0, bufferSize);
			fileStream.read(fileStreamBuffer, bufferSize);
			md5->update(fileStreamBuffer, fileStream.gcount());
		}

		fileStream.close();

		const char* nullTerminateBuffer = "\0";
		md5->update(nullTerminateBuffer, 1); // null terminate the data
		md5->finalize();
		databaseChecksum = md5->hexdigest();

		delete md5;

		LOG("FDB Checksum calculated as: %s", databaseChecksum.c_str());
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
			float_t ratioBeforeToAfter = (float)currentFrameDelta / (float)newFrameDelta;
			currentFrameDelta = newFrameDelta;
			currentFramerate = MS_TO_FRAMES(newFrameDelta);
			LOG_DEBUG("Framerate for zone/instance/clone %i/%i/%i is now %i", zoneID, instanceID, cloneID, currentFramerate);
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
			LOG("We're running behind, dT: %f > %f (framerate %i)", deltaTime, currentFrameDelta, currentFramerate);
		}

		//Check if we're still connected to master:
		if (!Game::server->GetIsConnectedToMaster()) {
			framesSinceMasterDisconnect++;

			if (framesSinceMasterDisconnect >= noMasterConnectionTimeout && !Game::shouldShutdown) {
				LOG("Game loop running but no connection to master for %d frames, shutting down", noMasterConnectionTimeout);
				Game::shouldShutdown = true;
			}
		} else framesSinceMasterDisconnect = 0;

		// Check if we're still connected to chat:
		if (!chatConnected) {
			framesSinceChatDisconnect++;

			if (framesSinceChatDisconnect >= chatReconnectionTime) {
				framesSinceChatDisconnect = 0;

				Game::chatServer->Connect(masterIP.c_str(), chatPort, "3.25 ND1", 8);
			}
		} else framesSinceChatDisconnect = 0;

		//In world we'd update our other systems here.

		if (zoneID != 0 && deltaTime > 0.0f) {
			Metrics::StartMeasurement(MetricVariable::Physics);
			dpWorld::Instance().StepWorld(deltaTime);
			Metrics::EndMeasurement(MetricVariable::Physics);

			Metrics::StartMeasurement(MetricVariable::UpdateEntities);
			Game::entityManager->UpdateEntities(deltaTime);
			Metrics::EndMeasurement(MetricVariable::UpdateEntities);

			Metrics::StartMeasurement(MetricVariable::Ghosting);
			if (std::chrono::duration<float>(currentTime - ghostingLastTime).count() >= 1.0f) {
				Game::entityManager->UpdateGhosting();
				ghostingLastTime = currentTime;
			}
			Metrics::EndMeasurement(MetricVariable::Ghosting);

			Metrics::StartMeasurement(MetricVariable::UpdateSpawners);
			Game::zoneManager->Update(deltaTime);
			Metrics::EndMeasurement(MetricVariable::UpdateSpawners);
		}

		Metrics::StartMeasurement(MetricVariable::PacketHandling);

		//Check for packets here:
		packet = Game::server->ReceiveFromMaster();
		if (packet) { //We can get messages not handle-able by the dServer class, so handle them if we returned anything.
			HandlePacket(packet);
			Game::server->DeallocateMasterPacket(packet);
		}

		//Handle our chat packets:
		packet = Game::chatServer->Receive();
		if (packet) {
			HandlePacketChat(packet);
			Game::chatServer->DeallocatePacket(packet);
		}

		//Handle world-specific packets:
		float timeSpent = 0.0f;

		UserManager::Instance()->DeletePendingRemovals();

		auto t1 = std::chrono::high_resolution_clock::now();
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
				Game::shouldShutdown = true;
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
			// Some delay is required here or else we crash the client?

			framesSinceMasterStatus++;

			if (framesSinceMasterStatus >= 200) {
				LOG("Finished loading world with zone (%i), ready up!", Game::server->GetZoneID());

				MasterPackets::SendWorldReady(Game::server, Game::server->GetZoneID(), Game::server->GetInstanceID());

				ready = true;
			}
		}

		if (Game::shouldShutdown && !worldShutdownSequenceComplete) {
			WorldShutdownProcess(zoneID);
			break;
		}

		Metrics::AddMeasurement(MetricVariable::CPUTime, (1e6 * (1000.0 * (std::clock() - metricCPUTimeStart))) / CLOCKS_PER_SEC);
		Metrics::EndMeasurement(MetricVariable::Frame);
	}
	FinalizeShutdown();
	return EXIT_SUCCESS;
}

Logger* SetupLogger(uint32_t zoneID, uint32_t instanceID) {
	std::string logPath = (BinaryPathFinder::GetBinaryDir() / ("logs/WorldServer_" + std::to_string(zoneID) + "_" + std::to_string(instanceID) + "_" + std::to_string(time(nullptr)) + ".log")).string();
	bool logToConsole = false;
	bool logDebugStatements = false;
#ifdef _DEBUG
	logToConsole = true;
	logDebugStatements = true;
#endif

	return new Logger(logPath, logToConsole, logDebugStatements);
}

void HandlePacketChat(Packet* packet) {
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		LOG("Lost our connection to chat, zone(%i), instance(%i)", Game::server->GetZoneID(), Game::server->GetInstanceID());

		chatConnected = false;
	}

	if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED) {
		LOG("Established connection to chat, zone(%i), instance (%i)", Game::server->GetZoneID(), Game::server->GetInstanceID());
		Game::chatSysAddr = packet->systemAddress;

		chatConnected = true;
	}

	if (packet->data[0] == ID_USER_PACKET_ENUM) {
		if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::CHAT_INTERNAL) {
			switch (static_cast<eChatInternalMessageType>(packet->data[3])) {
			case eChatInternalMessageType::ROUTE_TO_PLAYER: {
				CINSTREAM_SKIP_HEADER;
				LWOOBJID playerID;
				inStream.Read(playerID);

				auto player = Game::entityManager->GetEntity(playerID);
				if (!player) return;

				auto sysAddr = player->GetSystemAddress();

				//Write our stream outwards:
				CBITSTREAM;
				for (BitSize_t i = 0; i < inStream.GetNumberOfBytesUsed(); i++) {
					bitStream.Write(packet->data[i + 16]); //16 bytes == header + playerID to skip
				}

				SEND_PACKET; //send routed packet to player

				break;
			}

			case eChatInternalMessageType::ANNOUNCEMENT: {
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

			case eChatInternalMessageType::MUTE_UPDATE: {
				CINSTREAM_SKIP_HEADER;
				LWOOBJID playerId;
				time_t expire = 0;
				inStream.Read(playerId);
				inStream.Read(expire);

				auto* entity = Game::entityManager->GetEntity(playerId);

				if (entity != nullptr) {
					entity->GetParentUser()->SetMuteExpire(expire);

					entity->GetCharacter()->SendMuteNotice();
				}

				break;
			}

			case eChatInternalMessageType::TEAM_UPDATE: {
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
				LOG("Updating team (%llu), (%i), (%i)", teamID, lootOption, memberCount);
				for (char i = 0; i < memberCount; i++) {
					LWOOBJID member = LWOOBJID_EMPTY;
					inStream.Read(member);
					members.push_back(member);

					LOG("Updating team member (%llu)", member);
				}

				TeamManager::Instance()->UpdateTeam(teamID, lootOption, members);

				break;
			}

			default:
				LOG("Received an unknown chat internal: %i", int(packet->data[3]));
			}
		}
	}
}

void HandlePacket(Packet* packet) {
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		auto user = UserManager::Instance()->GetUser(packet->systemAddress);
		if (!user) return;

		auto c = user->GetLastUsedChar();
		if (!c) {
			UserManager::Instance()->DeleteUser(packet->systemAddress);
			return;
		}

		auto* entity = Game::entityManager->GetEntity(c->GetObjectID());

		if (!entity) {
			entity = Player::GetPlayer(packet->systemAddress);
		}

		if (entity) {
			auto* skillComponent = entity->GetComponent<SkillComponent>();

			if (skillComponent != nullptr) {
				skillComponent->Reset();
			}

			entity->GetCharacter()->SaveXMLToDatabase();

			LOG("Deleting player %llu", entity->GetObjectID());

			Game::entityManager->DestroyEntity(entity);
		}

		{
			CBITSTREAM;
			BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::PLAYER_REMOVED_NOTIFICATION);
			bitStream.Write(user->GetLoggedInChar());
			Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
		}

		UserManager::Instance()->DeleteUser(packet->systemAddress);

		if (PropertyManagementComponent::Instance() != nullptr) {
			PropertyManagementComponent::Instance()->Save();
		}

		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::PLAYER_REMOVED);
		bitStream.Write((LWOMAPID)Game::server->GetZoneID());
		bitStream.Write((LWOINSTANCEID)instanceID);
		Game::server->SendToMaster(&bitStream);
	}

	if (packet->data[0] != ID_USER_PACKET_ENUM || packet->length < 4) return;
	if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::SERVER) {
		if (static_cast<eServerMessageType>(packet->data[3]) == eServerMessageType::VERSION_CONFIRM) {
			AuthPackets::HandleHandshake(Game::server, packet);
		}
	}

	if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::MASTER) {
		switch (static_cast<eMasterMessageType>(packet->data[3])) {
		case eMasterMessageType::REQUEST_PERSISTENT_ID_RESPONSE: {
			uint64_t requestID = PacketUtils::ReadU64(8, packet);
			uint32_t objectID = PacketUtils::ReadU32(16, packet);
			ObjectIDManager::Instance()->HandleRequestPersistentIDResponse(requestID, objectID);
			break;
		}

		case eMasterMessageType::REQUEST_ZONE_TRANSFER_RESPONSE: {
			uint64_t requestID = PacketUtils::ReadU64(8, packet);
			ZoneInstanceManager::Instance()->HandleRequestZoneTransferResponse(requestID, packet);
			break;
		}

		case eMasterMessageType::SESSION_KEY_RESPONSE: {
			//Read our session key and to which user it belongs:
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);
			uint32_t sessionKey = 0;
			std::string username;

			inStream.Read(sessionKey);
			username = PacketUtils::ReadString(12, packet, false);

			//Find them:
			auto it = m_PendingUsers.find(username);
			if (it == m_PendingUsers.end()) return;

			//Convert our key:
			std::string userHash = std::to_string(sessionKey);
			userHash = md5(userHash);

			//Verify it:
			if (userHash != it->second.hash) {
				LOG("SOMEONE IS TRYING TO HACK? SESSION KEY MISMATCH: ours: %s != master: %s", userHash.c_str(), it->second.hash.c_str());
				Game::server->Disconnect(it->second.sysAddr, eServerDisconnectIdentifiers::INVALID_SESSION_KEY);
				return;
			} else {
				LOG("User %s authenticated with correct key.", username.c_str());

				UserManager::Instance()->DeleteUser(packet->systemAddress);

				//Create our user and send them in:
				UserManager::Instance()->CreateUser(it->second.sysAddr, username, userHash);

				auto zone = Game::zoneManager->GetZone();
				if (zone) {
					float x = 0.0f;
					float y = 0.0f;
					float z = 0.0f;

					if (zone->GetZoneID().GetMapID() == 1100) {
						auto pos = zone->GetSpawnPos();
						x = pos.x;
						y = pos.y;
						z = pos.z;
					}

					WorldPackets::SendLoadStaticZone(it->second.sysAddr, x, y, z, zone->GetChecksum());
				}

				if (Game::server->GetZoneID() == 0) {
					//Since doing this reroute breaks the client's request, we have to call this manually.
					UserManager::Instance()->RequestCharacterList(it->second.sysAddr);
				}

				m_PendingUsers.erase(username);

				//Notify master:
				{
					CBITSTREAM;
					BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::PLAYER_ADDED);
					bitStream.Write((LWOMAPID)Game::server->GetZoneID());
					bitStream.Write((LWOINSTANCEID)instanceID);
					Game::server->SendToMaster(&bitStream);
				}
			}

			break;
		}
		case eMasterMessageType::AFFIRM_TRANSFER_REQUEST: {
			const uint64_t requestID = PacketUtils::ReadU64(8, packet);

			LOG("Got affirmation request of transfer %llu", requestID);

			CBITSTREAM;

			BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::AFFIRM_TRANSFER_RESPONSE);
			bitStream.Write(requestID);
			Game::server->SendToMaster(&bitStream);

			break;
		}

		case eMasterMessageType::SHUTDOWN: {
			Game::shouldShutdown = true;
			LOG("Got shutdown request from master, zone (%i), instance (%i)", Game::server->GetZoneID(), Game::server->GetInstanceID());
			break;
		}

		case eMasterMessageType::NEW_SESSION_ALERT: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			uint64_t header = inStream.Read(header);
			uint32_t sessionKey = inStream.Read(sessionKey);

			std::string username;

			uint32_t len;
			inStream.Read(len);

			for (uint32_t i = 0; i < len; i++) {
				char character; inStream.Read<char>(character);
				username += character;
			}

			//Find them:
			User* user = UserManager::Instance()->GetUser(username.c_str());
			if (!user) {
				LOG("Got new session alert for user %s, but they're not logged in.", username.c_str());
				return;
			}

			//Check the key:
			if (sessionKey != std::atoi(user->GetSessionKey().c_str())) {
				LOG("Got new session alert for user %s, but the session key is invalid.", username.c_str());
				Game::server->Disconnect(user->GetSystemAddress(), eServerDisconnectIdentifiers::INVALID_SESSION_KEY);
				return;
			}
			break;
		}

		default:
			LOG("Unknown packet ID from master %i", int(packet->data[3]));
		}

		return;
	}

	if (static_cast<eConnectionType>(packet->data[1]) != eConnectionType::WORLD) return;

	switch (static_cast<eWorldMessageType>(packet->data[3])) {
	case eWorldMessageType::VALIDATION: {
		std::string username = PacketUtils::ReadString(0x08, packet, true);
		std::string sessionKey = PacketUtils::ReadString(74, packet, true);
		std::string clientDatabaseChecksum = PacketUtils::ReadString(packet->length - 33, packet, false);

		// sometimes client puts a null terminator at the end of the checksum and sometimes doesn't, weird
		clientDatabaseChecksum = clientDatabaseChecksum.substr(0, 32);

		// If the check is turned on, validate the client's database checksum.
		if (Game::config->GetValue("check_fdb") == "1" && !databaseChecksum.empty()) {
			uint32_t gmLevel = 0;
			auto* stmt = Database::Get()->CreatePreppedStmt("SELECT gm_level FROM accounts WHERE name=? LIMIT 1;");
			stmt->setString(1, username.c_str());

			auto* res = stmt->executeQuery();
			while (res->next()) {
				gmLevel = res->getInt(1);
			}

			delete stmt;
			delete res;

			// Developers may skip this check
			if (gmLevel < 8 && clientDatabaseChecksum != databaseChecksum) {
				LOG("Client's database checksum does not match the server's, aborting connection.");
				Game::server->Disconnect(packet->systemAddress, eServerDisconnectIdentifiers::WRONG_GAME_VERSION);
				return;
			}
		}

		//Request the session info from Master:
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::REQUEST_SESSION_KEY);
		bitStream.Write(LUString(username, 64));
		Game::server->SendToMaster(&bitStream);

		//Insert info into our pending list
		tempSessionInfo info;
		info.sysAddr = SystemAddress(packet->systemAddress);
		info.hash = sessionKey;
		m_PendingUsers.insert(std::make_pair(username, info));

		break;
	}

	case eWorldMessageType::CHARACTER_LIST_REQUEST: {
		//We need to delete the entity first, otherwise the char list could delete it while it exists in the world!
		if (Game::server->GetZoneID() != 0) {
			auto user = UserManager::Instance()->GetUser(packet->systemAddress);
			if (!user) return;
			Game::entityManager->DestroyEntity(user->GetLastUsedChar()->GetEntity());
		}

		//This loops prevents users who aren't authenticated to double-request the char list, which
		//would make the login screen freeze sometimes.
		if (m_PendingUsers.size() > 0) {
			for (auto it : m_PendingUsers) {
				if (it.second.sysAddr == packet->systemAddress) {
					return;
				}
			}
		}

		UserManager::Instance()->RequestCharacterList(packet->systemAddress);
		break;
	}

	case eWorldMessageType::GAME_MSG: {
		RakNet::BitStream bitStream(packet->data, packet->length, false);

		uint64_t header;
		LWOOBJID objectID;
		eGameMessageType messageID;

		bitStream.Read(header);
		bitStream.Read(objectID);
		bitStream.Read(messageID);

		RakNet::BitStream dataStream;
		bitStream.Read(dataStream, bitStream.GetNumberOfUnreadBits());

		auto isSender = CheatDetection::VerifyLwoobjidIsSender(
			objectID,
			packet->systemAddress,
			CheckType::Entity,
			"Sending GM with a sending player that does not match their own. GM ID: %i",
			static_cast<int32_t>(messageID)
			);

		if (isSender) GameMessageHandler::HandleMessage(&dataStream, packet->systemAddress, objectID, messageID);
		break;
	}

	case eWorldMessageType::CHARACTER_CREATE_REQUEST: {
		UserManager::Instance()->CreateCharacter(packet->systemAddress, packet);
		break;
	}

	case eWorldMessageType::LOGIN_REQUEST: {
		RakNet::BitStream inStream(packet->data, packet->length, false);
		uint64_t header = inStream.Read(header);

		LWOOBJID playerID = 0;
		inStream.Read(playerID);

		bool valid = CheatDetection::VerifyLwoobjidIsSender(
			playerID,
			packet->systemAddress,
			CheckType::User,
			"Sending login request with a sending player that does not match their own. Player ID: %llu",
			playerID
			);

		if (!valid) return;

		GeneralUtils::ClearBit(playerID, eObjectBits::CHARACTER);
		GeneralUtils::ClearBit(playerID, eObjectBits::PERSISTENT);

		auto user = UserManager::Instance()->GetUser(packet->systemAddress);

		if (user) {
			auto lastCharacter = user->GetLoggedInChar();
			// This means we swapped characters and we need to remove the previous player from the container.
			if (static_cast<uint32_t>(lastCharacter) != playerID) {
				CBITSTREAM;
				BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::PLAYER_REMOVED_NOTIFICATION);
				bitStream.Write(lastCharacter);
				Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
			}
		}

		UserManager::Instance()->LoginCharacter(packet->systemAddress, static_cast<uint32_t>(playerID));
		break;
	}

	case eWorldMessageType::CHARACTER_DELETE_REQUEST: {
		UserManager::Instance()->DeleteCharacter(packet->systemAddress, packet);
		UserManager::Instance()->RequestCharacterList(packet->systemAddress);
		break;
	}

	case eWorldMessageType::CHARACTER_RENAME_REQUEST: {
		UserManager::Instance()->RenameCharacter(packet->systemAddress, packet);
		break;
	}

	case eWorldMessageType::LEVEL_LOAD_COMPLETE: {
		LOG("Received level load complete from user.");
		User* user = UserManager::Instance()->GetUser(packet->systemAddress);
		if (user) {
			Character* c = user->GetLastUsedChar();
			if (c != nullptr) {
				std::u16string username = GeneralUtils::ASCIIToUTF16(c->GetName());
				Game::server->GetReplicaManager()->AddParticipant(packet->systemAddress);

				EntityInfo info{};
				info.lot = 1;
				Entity* player = Game::entityManager->CreateEntity(info, UserManager::Instance()->GetUser(packet->systemAddress));

				WorldPackets::SendCreateCharacter(packet->systemAddress, player, c->GetXMLData(), username, c->GetGMLevel());
				WorldPackets::SendServerState(packet->systemAddress);

				const auto respawnPoint = player->GetCharacter()->GetRespawnPoint(Game::zoneManager->GetZone()->GetWorldID());

				Game::entityManager->ConstructEntity(player, UNASSIGNED_SYSTEM_ADDRESS, true);

				if (respawnPoint != NiPoint3::ZERO) {
					GameMessages::SendPlayerReachedRespawnCheckpoint(player, respawnPoint, NiQuaternion::IDENTITY);
				}

				Game::entityManager->ConstructAllEntities(packet->systemAddress);

				auto* characterComponent = player->GetComponent<CharacterComponent>();
				if (characterComponent) {
					player->GetComponent<CharacterComponent>()->RocketUnEquip(player);
				}

				// Do charxml fixes here
				auto* levelComponent = player->GetComponent<LevelProgressionComponent>();
				if (!levelComponent) return;

				auto version = levelComponent->GetCharacterVersion();
				switch(version) {
					case eCharacterVersion::RELEASE:
						// TODO: Implement, super low priority
					case eCharacterVersion::LIVE:
						LOG("Updating Character Flags");
						c->SetRetroactiveFlags();
						levelComponent->SetCharacterVersion(eCharacterVersion::PLAYER_FACTION_FLAGS);
					case eCharacterVersion::PLAYER_FACTION_FLAGS:
						LOG("Updating Vault Size");
						player->RetroactiveVaultSize();
						levelComponent->SetCharacterVersion(eCharacterVersion::VAULT_SIZE);
					case eCharacterVersion::VAULT_SIZE:
						LOG("Updaing Speedbase");
						levelComponent->SetRetroactiveBaseSpeed();
						levelComponent->SetCharacterVersion(eCharacterVersion::UP_TO_DATE);
					case eCharacterVersion::UP_TO_DATE:
						break;
				}

				player->GetCharacter()->SetTargetScene("");

				// Fix the destroyable component
				auto* destroyableComponent = player->GetComponent<DestroyableComponent>();

				if (destroyableComponent != nullptr) {
					destroyableComponent->FixStats();
				}

				//Tell the player to generate BBB models, if any:
				if (g_CloneID != 0) {
					const auto& worldId = Game::zoneManager->GetZone()->GetZoneID();

					const auto zoneId = Game::server->GetZoneID();
					const auto cloneId = g_CloneID;

					auto query = CDClientDatabase::CreatePreppedStmt(
						"SELECT id FROM PropertyTemplate WHERE mapID = ?;");
					query.bind(1, (int)zoneId);

					auto result = query.execQuery();

					if (result.eof() || result.fieldIsNull(0)) {
						LOG("No property templates found for zone %d, not sending BBB", zoneId);
						goto noBBB;
					}

					//Check for BBB models:
					auto stmt = Database::Get()->CreatePreppedStmt("SELECT ugc_id FROM properties_contents WHERE lot=14 AND property_id=?");

					int32_t templateId = result.getIntField(0);

					result.finalize();

					auto* propertyLookup = Database::Get()->CreatePreppedStmt("SELECT * FROM properties WHERE template_id = ? AND clone_id = ?;");

					propertyLookup->setInt(1, templateId);
					propertyLookup->setInt64(2, g_CloneID);

					auto* propertyEntry = propertyLookup->executeQuery();
					uint64_t propertyId = 0;

					if (propertyEntry->next()) {
						propertyId = propertyEntry->getUInt64(1);
					}

					delete propertyLookup;

					stmt->setUInt64(1, propertyId);
					auto res = stmt->executeQuery();
					while (res->next()) {
						LOG("Getting lxfml ugcID: %u", res->getUInt(1));

						//Get lxfml:
						auto stmtL = Database::Get()->CreatePreppedStmt("SELECT lxfml from ugc where id=?");
						stmtL->setUInt(1, res->getUInt(1));

						auto lxres = stmtL->executeQuery();

						while (lxres->next()) {
							auto lxfml = lxres->getBlob(1);

							lxfml->seekg(0, std::ios::end);
							size_t lxfmlSize = lxfml->tellg();
							lxfml->seekg(0);

							//Send message:
							{
								LWOOBJID blueprintID = res->getUInt(1);
								GeneralUtils::SetBit(blueprintID, eObjectBits::CHARACTER);
								GeneralUtils::SetBit(blueprintID, eObjectBits::PERSISTENT);

								CBITSTREAM;
								BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::BLUEPRINT_SAVE_RESPONSE);
								bitStream.Write<LWOOBJID>(LWOOBJID_EMPTY); //always zero so that a check on the client passes
								bitStream.Write(eBlueprintSaveResponseType::EverythingWorked);
								bitStream.Write<uint32_t>(1);
								bitStream.Write(blueprintID);

								bitStream.Write<uint32_t>(lxfmlSize);

								for (size_t i = 0; i < lxfmlSize; ++i)
									bitStream.Write<uint8_t>(lxfml->get());

								SystemAddress sysAddr = packet->systemAddress;
								SEND_PACKET;
								PacketUtils::SavePacket("lxfml packet " + std::to_string(res->getUInt(1)) + ".bin", (char*)bitStream.GetData(), bitStream.GetNumberOfBytesUsed());
							}
						}

						delete stmtL;
						delete lxres;
					}

					delete stmt;
					delete res;
				}

			noBBB:

				// Tell the client it's done loading:
				GameMessages::SendInvalidZoneTransferList(player, packet->systemAddress, GeneralUtils::ASCIIToUTF16(Game::config->GetValue("source")), u"", false, false);
				GameMessages::SendServerDoneLoadingAllObjects(player, packet->systemAddress);

				//Send the player it's mail count:
				//update: this might not be needed so im going to try disabling this here.
				//Mail::HandleNotificationRequest(packet->systemAddress, player->GetObjectID());

				//Notify chat that a player has loaded:
				{
					const auto& playerName = player->GetCharacter()->GetName();
					//RakNet::RakString playerName(player->GetCharacter()->GetName().c_str());

					CBITSTREAM;
					BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::PLAYER_ADDED_NOTIFICATION);
					bitStream.Write(player->GetObjectID());
					bitStream.Write<uint32_t>(playerName.size());
					for (size_t i = 0; i < playerName.size(); i++) {
						bitStream.Write(playerName[i]);
					}

					auto zone = Game::zoneManager->GetZone()->GetZoneID();
					bitStream.Write(zone.GetMapID());
					bitStream.Write(zone.GetInstanceID());
					bitStream.Write(zone.GetCloneID());
					bitStream.Write(player->GetParentUser()->GetMuteExpire());

					Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
				}
			} else {
				LOG("Couldn't find character to log in with for user %s (%i)!", user->GetUsername().c_str(), user->GetAccountID());
				Game::server->Disconnect(packet->systemAddress, eServerDisconnectIdentifiers::CHARACTER_NOT_FOUND);
			}
		} else {
			LOG("Couldn't get user for level load complete!");
		}
		break;
	}

	case eWorldMessageType::POSITION_UPDATE: {
		ClientPackets::HandleClientPositionUpdate(packet->systemAddress, packet);
		break;
	}

	case eWorldMessageType::MAIL: {
		RakNet::BitStream bitStream(packet->data, packet->length, false);
		// FIXME: Change this to the macro to skip the header...
		LWOOBJID space;
		bitStream.Read(space);
		Mail::HandleMailStuff(&bitStream, packet->systemAddress, UserManager::Instance()->GetUser(packet->systemAddress)->GetLastUsedChar()->GetEntity());
		break;
	}

	case eWorldMessageType::ROUTE_PACKET: {
		//Yeet to chat
		CINSTREAM_SKIP_HEADER;
		uint32_t size = 0;
		inStream.Read(size);

		if (size > 20000) {
			LOG("Tried to route a packet with a read size > 20000, so likely a false packet.");
			return;
		}

		CBITSTREAM;

		BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, packet->data[14]);

		//We need to insert the player's objectID so the chat server can find who originated this request:
		LWOOBJID objectID = 0;
		auto user = UserManager::Instance()->GetUser(packet->systemAddress);
		if (user) {
			objectID = user->GetLastUsedChar()->GetObjectID();
		}

		bitStream.Write(objectID);

		//Now write the rest of the data:
		auto data = inStream.GetData();
		for (uint32_t i = 0; i < size; ++i) {
			bitStream.Write(data[i + 23]);
		}

		Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, Game::chatSysAddr, false);
		break;
	}

	case eWorldMessageType::STRING_CHECK: {
		ClientPackets::HandleChatModerationRequest(packet->systemAddress, packet);
		break;
	}

	case eWorldMessageType::GENERAL_CHAT_MESSAGE: {
		if (chatDisabled) {
			ChatPackets::SendMessageFail(packet->systemAddress);
		} else {
			ClientPackets::HandleChatMessage(packet->systemAddress, packet);
		}

		break;
	}

	case eWorldMessageType::HANDLE_FUNNESS: {
		//This means the client is running slower or faster than it should.
		//Could be insane lag, but I'mma just YEET them as it's usually speedhacking.
		//This is updated to now count the amount of times we've been caught "speedhacking" to kick with a delay
		//This is hopefully going to fix the random disconnects people face sometimes.
		if (Game::config->GetValue("disable_anti_speedhack") == "1") {
			return;
		}

		User* user = UserManager::Instance()->GetUser(packet->systemAddress);
		if (user) {
			user->UserOutOfSync();
		} else {
			Game::server->Disconnect(packet->systemAddress, eServerDisconnectIdentifiers::KICK);
		}
		break;
	}

	default:
		LOG("Unknown world packet received: %i", int(packet->data[3]));
	}
}

void WorldShutdownProcess(uint32_t zoneId) {
	LOG("Saving map %i instance %i", zoneId, instanceID);
	for (auto i = 0; i < Game::server->GetReplicaManager()->GetParticipantCount(); ++i) {
		const auto& player = Game::server->GetReplicaManager()->GetParticipantAtIndex(i);

		auto* entity = Player::GetPlayer(player);
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

	LOG("ALL DATA HAS BEEN SAVED FOR ZONE %i INSTANCE %i!", zoneId, instanceID);

	while (Game::server->GetReplicaManager()->GetParticipantCount() > 0) {
		const auto& player = Game::server->GetReplicaManager()->GetParticipantAtIndex(0);

		Game::server->Disconnect(player, eServerDisconnectIdentifiers::SERVER_SHUTDOWN);
	}
	SendShutdownMessageToMaster();
}

void WorldShutdownSequence() {
	if (Game::shouldShutdown || worldShutdownSequenceComplete) {
		return;
	}

	Game::shouldShutdown = true;

	LOG("Zone (%i) instance (%i) shutting down outside of main loop!", Game::server->GetZoneID(), instanceID);
	WorldShutdownProcess(Game::server->GetZoneID());
	FinalizeShutdown();
}

void FinalizeShutdown() {
	LOG("Shutdown complete, zone (%i), instance (%i)", Game::server->GetZoneID(), instanceID);

	//Delete our objects here:
	Metrics::Clear();
	Database::Destroy("WorldServer");
	if (Game::chatFilter) delete Game::chatFilter;
	if (Game::zoneManager) delete Game::zoneManager;
	if (Game::server) delete Game::server;
	if (Game::config) delete Game::config;
	if (Game::entityManager) delete Game::entityManager;
	if (Game::logger) delete Game::logger;

	worldShutdownSequenceComplete = true;

	exit(EXIT_SUCCESS);
}

void SendShutdownMessageToMaster() {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::SHUTDOWN_RESPONSE);
	Game::server->SendToMaster(&bitStream);
}
