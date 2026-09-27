#include <iostream>
#include <string>
#include <chrono>
#include <thread>

//DLU Includes:
#include "dCommonVars.h"
#include "ConfigSync.h"
#include "dServer.h"
#include "Logger.h"
#include "Database.h"
#include "dConfig.h"
#include "dChatFilter.h"
#include "PlayerAction.h"
#include "MessageType/Master.h"
#include "Diagnostics.h"
#include "AssetManager.h"
#include "BinaryPathFinder.h"
#include "ServiceType.h"
#include "PlayerContainer.h"
#include "ChatPacketHandler.h"
#include "MessageType/Chat.h"
#include "MessageType/World.h"
#include "ChatIgnoreList.h"
#include "StringifiedEnum.h"
#include "TeamContainer.h"
#include "PacketDispatcher.h"
#include "ChatPackets.h"

#include "Game.h"
#include "Server.h"

//RakNet includes:
#include "RakNetDefines.h"
#include "MessageIdentifiers.h"

#include "ChatWeb.h"

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dConfig* config = nullptr;
	dChatFilter* chatFilter = nullptr;
	AssetManager* assetManager = nullptr;
	Game::signal_t lastSignal = 0;
	std::mt19937 randomEngine;
	PlayerContainer playerContainer;
}

void HandlePacket(Packet* packet);
void HandleMasterPacket(Packet* packet);

int main(int argc, char** argv) {
	constexpr uint32_t chatFramerate = mediumFramerate;
	constexpr uint32_t chatFrameDelta = mediumFrameDelta;
	const auto curTimeStr = std::to_string(time(nullptr));
	const auto serviceName = "ChatServer_" + curTimeStr;
	Diagnostics::SetProcessName(serviceName);
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);

	Game::config = new dConfig("chatconfig.ini");

	//Create all the objects we need to run our service:
	Server::SetupLogger(serviceName, "ChatServer");
	if (!Game::logger) return EXIT_FAILURE;
	Game::config->LogSettings();

	//Read our config:

	LOG("Starting Chat server...");
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

	//Connect to the MySQL Database
	try {
		Database::Connect();
	} catch (std::exception& ex) {
		LOG("Got an error while connecting to the database: %s", ex.what());
		Database::Destroy(serviceName);
		delete Game::logger;
		delete Game::config;
		return EXIT_FAILURE;
	}

	// Settings edited on the dashboard (server_config table) are layered over the files from here on
	Game::config->SetDatabaseSync(ConfigSync::Sync);

	// setup the chat api web server
	const uint32_t web_server_port = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("web_server_port")).value_or(2005);
	if (Game::config->GetValue("web_server_enabled") == "1" && !Game::web.Startup("localhost", web_server_port)) {
		// if we want the web server and it fails to start, exit
		LOG("Failed to start web server, shutting down.");
		Database::Destroy(serviceName);
		delete Game::logger;
		delete Game::config;
		return EXIT_FAILURE;
	}

	if (Game::web.IsEnabled()) ChatWeb::RegisterRoutes();

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
	const uint32_t ourPort = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("chat_server_port")).value_or(2005);
	const auto externalIPString = Game::config->GetValue("external_ip");
	if (!externalIPString.empty()) ourIP = externalIPString;

	Game::server = new dServer(ourIP, ourPort, 0, maxClients, false, true, Game::logger, masterIP, masterPort, ServiceType::CHAT, Game::config, &Game::lastSignal, masterPassword);

	const bool dontGenerateDCF = GeneralUtils::TryParse<bool>(Game::config->GetValue("dont_generate_dcf")).value_or(false);
	Game::chatFilter = new dChatFilter(Game::assetManager->GetResPath().string() + "/chatplus_en_us", dontGenerateDCF);

	Game::randomEngine = std::mt19937(time(0));

	Game::playerContainer.Initialize();

	//Run it until server gets a kill message from Master:
	auto t = std::chrono::high_resolution_clock::now();
	Packet* packet = nullptr;
	constexpr uint32_t logFlushTime = 30 * chatFramerate; // 30 seconds in frames
	constexpr uint32_t sqlPingTime = 10 * 60 * chatFramerate; // 10 minutes in frames
	uint32_t framesSinceLastFlush = 0;
	uint32_t framesSinceMasterDisconnect = 0;
	uint32_t framesSinceLastSQLPing = 0;

	auto lastTime = std::chrono::high_resolution_clock::now();

	Game::logger->Flush(); // once immediately before main loop
	while (!Game::ShouldShutdown()) {
		//Check if we're still connected to master:
		if (!Game::server->GetIsConnectedToMaster()) {
			framesSinceMasterDisconnect++;

			if (framesSinceMasterDisconnect >= chatFramerate)
				break; //Exit our loop, shut down.
		} else framesSinceMasterDisconnect = 0;

		const auto currentTime = std::chrono::high_resolution_clock::now();
		const float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
		lastTime = currentTime;

		Game::playerContainer.Update(deltaTime);

		//Check for packets here:
		//ReceiveFromMaster also handles the master packets if needed; it hands back the ones for us.
		if (auto* masterPacket = Game::server->ReceiveFromMaster()) {
			HandleMasterPacket(masterPacket);
			Game::server->DeallocateMasterPacket(masterPacket);
		}
		packet = Game::server->Receive();
		if (packet) {
			HandlePacket(packet);
			Game::server->DeallocatePacket(packet);
			packet = nullptr;
		}

		// Check and handle web requests:
		if (Game::web.IsEnabled()) Game::web.ReceiveRequests();

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
		t += std::chrono::milliseconds(chatFrameDelta); //Chat can run at a lower "fps"
		std::this_thread::sleep_until(t);
	}
	Game::playerContainer.Shutdown();
	TeamContainer::Shutdown();
	//Delete our objects here:
	Database::Destroy(serviceName);
	delete Game::server;
	Game::server = nullptr;
	delete Game::logger;
	Game::logger = nullptr;
	delete Game::config;
	Game::config = nullptr;

	return EXIT_SUCCESS;
}

namespace {
	// Packets from world servers
	const PacketDispatcher<MessageType::Chat>& ChatHandlers() {
		static const auto handlers = [] {
			PacketDispatcher<MessageType::Chat> handlers;
			using namespace ChatPackets;
			using MessageType::Chat;
			handlers.On<GMMute>(Chat::GM_MUTE, [](const GMMute& mute, const SystemAddress& sysAddr) { Game::playerContainer.MuteUpdate(mute, sysAddr); });
			handlers.On<CreateTeam>(Chat::CREATE_TEAM, TeamContainer::CreateTeamServer);
			handlers.On<GetFriendsList>(Chat::GET_FRIENDS_LIST, ChatPacketHandler::HandleFriendlistRequest);
			handlers.On<GetIgnoreList>(Chat::GET_IGNORE_LIST, ChatIgnoreList::GetIgnoreList);
			handlers.On<AddIgnore>(Chat::ADD_IGNORE, ChatIgnoreList::AddIgnore);
			handlers.On<RemoveIgnore>(Chat::REMOVE_IGNORE, ChatIgnoreList::RemoveIgnore);
			handlers.On<TeamGetStatus>(Chat::TEAM_GET_STATUS, TeamContainer::HandleTeamStatusRequest);
			//this involves someone sending the initial request, the response is below, response as in from the other player.
			//We basically just check to see if this player is online or not and route the packet.
			handlers.On<AddFriendRequest>(Chat::ADD_FRIEND_REQUEST, ChatPacketHandler::HandleFriendRequest);
			//This isn't the response a server sent, rather it is a player's response to a received request.
			//Here, we'll actually have to add them to eachother's friend lists depending on the response code.
			handlers.On<AddFriendResponse>(Chat::ADD_FRIEND_RESPONSE, ChatPacketHandler::HandleFriendResponse);
			handlers.On<RemoveFriend>(Chat::REMOVE_FRIEND, ChatPacketHandler::HandleRemoveFriend);
			handlers.On<GeneralChatMessage>(Chat::GENERAL_CHAT_MESSAGE, ChatPacketHandler::HandleChatMessage);
			//This message is supposed to be echo'd to both the sender and the receiver
			//BUT: they have to have different responseCodes, so we'll do some of the ol hacky wacky to fix that right up.
			handlers.On<PrivateChatMessage>(Chat::PRIVATE_CHAT_MESSAGE, ChatPacketHandler::HandlePrivateChatMessage);
			handlers.On<TeamInvite>(Chat::TEAM_INVITE, TeamContainer::HandleTeamInvite);
			handlers.On<TeamInviteResponse>(Chat::TEAM_INVITE_RESPONSE, TeamContainer::HandleTeamInviteResponse);
			handlers.On<TeamLeave>(Chat::TEAM_LEAVE, TeamContainer::HandleTeamLeave);
			handlers.On<TeamSetLeader>(Chat::TEAM_SET_LEADER, TeamContainer::HandleTeamPromote);
			handlers.On<TeamKick>(Chat::TEAM_KICK, TeamContainer::HandleTeamKick);
			handlers.On<TeamSetLoot>(Chat::TEAM_SET_LOOT, TeamContainer::HandleTeamLootOption);
			handlers.On<GMLevelUpdate>(Chat::GMLEVEL_UPDATE, ChatPacketHandler::HandleGMLevelUpdate);
			handlers.On<LoginSessionNotify>(Chat::LOGIN_SESSION_NOTIFY, [](const LoginSessionNotify& notify, const SystemAddress& sysAddr) { Game::playerContainer.InsertPlayer(notify, sysAddr); });
			// we just forward this packet to every connected server
			handlers.On<Announcement>(Chat::GM_ANNOUNCE, [](const Announcement& announcement, const SystemAddress& sysAddr) {
				RakNet::BitStream bitStream;
				announcement.WritePacket(bitStream);
				Game::server->Send(bitStream, sysAddr, true); // send to everyone except origin
			});
			handlers.On<UnexpectedDisconnect>(Chat::UNEXPECTED_DISCONNECT, [](const UnexpectedDisconnect& notify, const SystemAddress& sysAddr) { Game::playerContainer.ScheduleRemovePlayer(notify, sysAddr); });
			handlers.On<FindPlayerRequest>(Chat::WHO, ChatPacketHandler::HandleWho);
			handlers.On<ShowAllRequest>(Chat::SHOW_ALL, ChatPacketHandler::HandleShowAll);
			handlers.On<AchievementNotify>(Chat::ACHIEVEMENT_NOTIFY, ChatPacketHandler::OnAchievementNotify);
			return handlers;
		}();
		return handlers;
	}
}

// Messages from master that dServer doesn't handle itself
void HandleMasterPacket(Packet* packet) {
	if (packet->length < 4 || static_cast<ServiceType>(packet->data[1]) != ServiceType::MASTER) return;
	if (static_cast<MessageType::Master>(packet->data[3]) != MessageType::Master::PLAYER_ACTION) return;
	CINSTREAM_SKIP_HEADER;
	PlayerActionRequest request;
	if (!request.Deserialize(inStream)) return;
	// Words added or removed on the dashboard: web chat is checked with the same filter as the worlds
	if (request.action == ePlayerAction::RELOAD_CHAT_FILTER && Game::chatFilter) {
		Game::chatFilter->ReloadCustomWords();
		LOG("Reloaded the chat filter's words (changed on the dashboard)");
	}
}

void HandlePacket(Packet* packet) {
	if (packet->length < 1) return;
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		LOG("A server has disconnected, erasing their connected players from the list.");
	} else if (packet->data[0] == ID_NEW_INCOMING_CONNECTION) {
		LOG("A server is connecting, awaiting user list.");
	} else if (packet->length < 4 || packet->data[0] != ID_USER_PACKET_ENUM) return; // Nothing left to process or not the right packet type

	RakNet::BitStream inStream(packet->data, packet->length, false);
	LUBitStream header;
	if (!header.ReadHeader(inStream) || header.connectionType != ServiceType::CHAT) return;

	// Our packing byte wasnt there? Probably a false packet
	if (packet->length < 8) return;

	const auto chatMessageID = static_cast<MessageType::Chat>(header.internalPacketID);
	if (!ChatHandlers().Dispatch(chatMessageID, inStream, packet->systemAddress)) {
		LOG("Unhandled CHAT Message id: %s (%i)", StringifiedEnum::ToString(chatMessageID).data(), chatMessageID);
	}
}
