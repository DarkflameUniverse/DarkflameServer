#include <iostream>
#include <string>
#include <chrono>
#include <thread>

//DLU Includes:
#include "dCommonVars.h"
#include "dServer.h"
#include "Logger.h"
#include "Database.h"
#include "dConfig.h"
#include "dChatFilter.h"
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

int main(int argc, char** argv) {
	constexpr uint32_t chatFramerate = mediumFramerate;
	constexpr uint32_t chatFrameDelta = mediumFrameDelta;
	Diagnostics::SetProcessName("Chat");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);

	Game::config = new dConfig("chatconfig.ini");

	//Create all the objects we need to run our service:
	Server::SetupLogger("ChatServer");
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
		Database::Destroy("ChatServer");
		delete Game::logger;
		delete Game::config;
		return EXIT_FAILURE;
	}

	// setup the chat api web server
	const uint32_t web_server_port = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("web_server_port")).value_or(2005);
	if (Game::config->GetValue("web_server_enabled") == "1" && !Game::web.Startup("localhost", web_server_port)) {
		// if we want the web server and it fails to start, exit
		LOG("Failed to start web server, shutting down.");
		Database::Destroy("ChatServer");
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
		Game::server->ReceiveFromMaster(); //ReceiveFromMaster also handles the master packets if needed.
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
	Database::Destroy("ChatServer");
	delete Game::server;
	delete Game::logger;
	delete Game::config;

	return EXIT_SUCCESS;
}

void HandlePacket(Packet* packet) {
	if (packet->length < 1) return;
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		LOG("A server has disconnected, erasing their connected players from the list.");
	} else if (packet->data[0] == ID_NEW_INCOMING_CONNECTION) {
		LOG("A server is connecting, awaiting user list.");
	} else if (packet->length < 4 || packet->data[0] != ID_USER_PACKET_ENUM) return; // Nothing left to process or not the right packet type

	CINSTREAM;
	inStream.SetReadOffset(BYTES_TO_BITS(1));

	ServiceType connection;
	inStream.Read(connection);
	if (connection != ServiceType::CHAT) return;

	MessageType::Chat chatMessageID;
	inStream.Read(chatMessageID);

	// Our packing byte wasnt there? Probably a false packet
	if (inStream.GetNumberOfUnreadBits() < 8) return;
	inStream.IgnoreBytes(1);

	switch (chatMessageID) {
	case MessageType::Chat::GM_MUTE:
		Game::playerContainer.MuteUpdate(packet);
		break;

	case MessageType::Chat::CREATE_TEAM:
		TeamContainer::CreateTeamServer(packet);
		break;

	case MessageType::Chat::GET_FRIENDS_LIST:
		ChatPacketHandler::HandleFriendlistRequest(packet);
		break;

	case MessageType::Chat::GET_IGNORE_LIST:
		ChatIgnoreList::GetIgnoreList(packet);
		break;

	case MessageType::Chat::ADD_IGNORE:
		ChatIgnoreList::AddIgnore(packet);
		break;

	case MessageType::Chat::REMOVE_IGNORE:
		ChatIgnoreList::RemoveIgnore(packet);
		break;

	case MessageType::Chat::TEAM_GET_STATUS:
		TeamContainer::HandleTeamStatusRequest(packet);
		break;

	case MessageType::Chat::ADD_FRIEND_REQUEST:
		//this involves someone sending the initial request, the response is below, response as in from the other player.
		//We basically just check to see if this player is online or not and route the packet.
		ChatPacketHandler::HandleFriendRequest(packet);
		break;

	case MessageType::Chat::ADD_FRIEND_RESPONSE:
		//This isn't the response a server sent, rather it is a player's response to a received request.
		//Here, we'll actually have to add them to eachother's friend lists depending on the response code.
		ChatPacketHandler::HandleFriendResponse(packet);
		break;

	case MessageType::Chat::REMOVE_FRIEND:
		ChatPacketHandler::HandleRemoveFriend(packet);
		break;

	case MessageType::Chat::GENERAL_CHAT_MESSAGE:
		ChatPacketHandler::HandleChatMessage(packet);
		break;

	case MessageType::Chat::PRIVATE_CHAT_MESSAGE:
		//This message is supposed to be echo'd to both the sender and the receiver
		//BUT: they have to have different responseCodes, so we'll do some of the ol hacky wacky to fix that right up.
		ChatPacketHandler::HandlePrivateChatMessage(packet);
		break;

	case MessageType::Chat::TEAM_INVITE:
		TeamContainer::HandleTeamInvite(packet);
		break;

	case MessageType::Chat::TEAM_INVITE_RESPONSE:
		TeamContainer::HandleTeamInviteResponse(packet);
		break;

	case MessageType::Chat::TEAM_LEAVE:
		TeamContainer::HandleTeamLeave(packet);
		break;

	case MessageType::Chat::TEAM_SET_LEADER:
		TeamContainer::HandleTeamPromote(packet);
		break;

	case MessageType::Chat::TEAM_KICK:
		TeamContainer::HandleTeamKick(packet);
		break;

	case MessageType::Chat::TEAM_SET_LOOT:
		TeamContainer::HandleTeamLootOption(packet);
		break;
	case MessageType::Chat::GMLEVEL_UPDATE:
		ChatPacketHandler::HandleGMLevelUpdate(packet);
		break;
	case MessageType::Chat::LOGIN_SESSION_NOTIFY:
		Game::playerContainer.InsertPlayer(packet);
		break;
	case MessageType::Chat::GM_ANNOUNCE:
		// we just forward this packet to every connected server
		inStream.ResetReadPointer();
		Game::server->Send(inStream, packet->systemAddress, true); // send to everyone except origin
		break;
	case MessageType::Chat::UNEXPECTED_DISCONNECT:
		Game::playerContainer.ScheduleRemovePlayer(packet);
		break;
	case MessageType::Chat::WHO:
		ChatPacketHandler::HandleWho(packet);
		break;
	case MessageType::Chat::SHOW_ALL:
		ChatPacketHandler::HandleShowAll(packet);
		break;
	case MessageType::Chat::ACHIEVEMENT_NOTIFY:
		ChatPacketHandler::OnAchievementNotify(inStream, packet->systemAddress);
		break;
	case MessageType::Chat::USER_CHANNEL_CHAT_MESSAGE:
	case MessageType::Chat::WORLD_DISCONNECT_REQUEST:
	case MessageType::Chat::WORLD_PROXIMITY_RESPONSE:
	case MessageType::Chat::WORLD_PARCEL_RESPONSE:
	case MessageType::Chat::TEAM_MISSED_INVITE_CHECK:
	case MessageType::Chat::GUILD_CREATE:
	case MessageType::Chat::GUILD_INVITE:
	case MessageType::Chat::GUILD_INVITE_RESPONSE:
	case MessageType::Chat::GUILD_LEAVE:
	case MessageType::Chat::GUILD_KICK:
	case MessageType::Chat::GUILD_GET_STATUS:
	case MessageType::Chat::GUILD_GET_ALL:
	case MessageType::Chat::BLUEPRINT_MODERATED:
	case MessageType::Chat::BLUEPRINT_MODEL_READY:
	case MessageType::Chat::PROPERTY_READY_FOR_APPROVAL:
	case MessageType::Chat::PROPERTY_MODERATION_CHANGED:
	case MessageType::Chat::PROPERTY_BUILDMODE_CHANGED:
	case MessageType::Chat::PROPERTY_BUILDMODE_CHANGED_REPORT:
	case MessageType::Chat::MAIL:
	case MessageType::Chat::WORLD_INSTANCE_LOCATION_REQUEST:
	case MessageType::Chat::REPUTATION_UPDATE:
	case MessageType::Chat::SEND_CANNED_TEXT:
	case MessageType::Chat::CHARACTER_NAME_CHANGE_REQUEST:
	case MessageType::Chat::CSR_REQUEST:
	case MessageType::Chat::CSR_REPLY:
	case MessageType::Chat::GM_KICK:
	case MessageType::Chat::WORLD_ROUTE_PACKET:
	case MessageType::Chat::GET_ZONE_POPULATIONS:
	case MessageType::Chat::REQUEST_MINIMUM_CHAT_MODE:
	case MessageType::Chat::MATCH_REQUEST:
	case MessageType::Chat::UGCMANIFEST_REPORT_MISSING_FILE:
	case MessageType::Chat::UGCMANIFEST_REPORT_DONE_FILE:
	case MessageType::Chat::UGCMANIFEST_REPORT_DONE_BLUEPRINT:
	case MessageType::Chat::UGCC_REQUEST:
	case MessageType::Chat::WORLD_PLAYERS_PET_MODERATED_ACKNOWLEDGE:
	case MessageType::Chat::GM_CLOSE_PRIVATE_CHAT_WINDOW:
	case MessageType::Chat::PLAYER_READY:
	case MessageType::Chat::GET_DONATION_TOTAL:
	case MessageType::Chat::UPDATE_DONATION:
	case MessageType::Chat::PRG_CSR_COMMAND:
	case MessageType::Chat::HEARTBEAT_REQUEST_FROM_WORLD:
	case MessageType::Chat::UPDATE_FREE_TRIAL_STATUS:
		LOG("Unhandled CHAT Message id: %s (%i)", StringifiedEnum::ToString(chatMessageID).data(), chatMessageID);
		break;
	default:
		LOG("Unknown CHAT Message id: %i", chatMessageID);
	}
}
