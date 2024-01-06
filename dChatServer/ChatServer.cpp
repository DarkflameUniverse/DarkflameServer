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
#include "eConnectionType.h"
#include "PlayerContainer.h"
#include "ChatPacketHandler.h"
#include "eChatMessageType.h"
#include "eChatInternalMessageType.h"
#include "eWorldMessageType.h"
#include "ChatIgnoreList.h"

#include "Game.h"
#include "Server.h"

//RakNet includes:
#include "RakNetDefines.h"
#include "MessageIdentifiers.h"

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

		return EXIT_FAILURE;
	}

	//Connect to the MySQL Database
	try {
		Database::Connect();
	} catch (sql::SQLException& ex) {
		LOG("Got an error while connecting to the database: %s", ex.what());
		Database::Destroy("ChatServer");
		delete Game::server;
		delete Game::logger;
		return EXIT_FAILURE;
	}

	//Find out the master's IP:
	std::string masterIP;
	uint32_t masterPort = 1000;
	auto masterInfo = Database::Get()->GetMasterInfo();
	if (masterInfo) {
		masterIP = masterInfo->ip;
		masterPort = masterInfo->port;
	}
	//It's safe to pass 'localhost' here, as the IP is only used as the external IP.
	uint32_t maxClients = 999;
	uint32_t ourPort = 1501;
	std::string ourIP = "localhost";
	GeneralUtils::TryParse(Game::config->GetValue("max_clients"), maxClients);
	GeneralUtils::TryParse(Game::config->GetValue("chat_server_port"), ourPort);
	const auto externalIPString = Game::config->GetValue("external_ip");
	if (!externalIPString.empty()) ourIP = externalIPString;

	Game::server = new dServer(ourIP, ourPort, 0, maxClients, false, true, Game::logger, masterIP, masterPort, ServerType::Chat, Game::config, &Game::lastSignal);

	bool dontGenerateDCF = false;
	GeneralUtils::TryParse(Game::config->GetValue("dont_generate_dcf"), dontGenerateDCF);
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

	Game::logger->Flush(); // once immediately before main loop
	while (!Game::ShouldShutdown()) {
		//Check if we're still connected to master:
		if (!Game::server->GetIsConnectedToMaster()) {
			framesSinceMasterDisconnect++;

			if (framesSinceMasterDisconnect >= chatFramerate)
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

	//Delete our objects here:
	Database::Destroy("ChatServer");
	delete Game::server;
	delete Game::logger;
	delete Game::config;

	return EXIT_SUCCESS;
}

void HandlePacket(Packet* packet) {
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		LOG("A server has disconnected, erasing their connected players from the list.");
	}

	if (packet->data[0] == ID_NEW_INCOMING_CONNECTION) {
		LOG("A server is connecting, awaiting user list.");
	}

	if (packet->length < 4) return; // Nothing left to process.  Need 4 bytes to continue.

	if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::CHAT_INTERNAL) {
		switch (static_cast<eChatInternalMessageType>(packet->data[3])) {
		case eChatInternalMessageType::PLAYER_ADDED_NOTIFICATION:
			Game::playerContainer.InsertPlayer(packet);
			break;

		case eChatInternalMessageType::PLAYER_REMOVED_NOTIFICATION:
			Game::playerContainer.RemovePlayer(packet);
			break;

		case eChatInternalMessageType::MUTE_UPDATE:
			Game::playerContainer.MuteUpdate(packet);
			break;

		case eChatInternalMessageType::CREATE_TEAM:
			Game::playerContainer.CreateTeamServer(packet);
			break;

		case eChatInternalMessageType::ANNOUNCEMENT: {
			//we just forward this packet to every connected server
			CINSTREAM;
			Game::server->Send(&inStream, packet->systemAddress, true); //send to everyone except origin
			break;
		}

		default:
			LOG("Unknown CHAT_INTERNAL id: %i", int(packet->data[3]));
		}
	}

	if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::CHAT) {
		switch (static_cast<eChatMessageType>(packet->data[3])) {
		case eChatMessageType::GET_FRIENDS_LIST:
			ChatPacketHandler::HandleFriendlistRequest(packet);
			break;

		case eChatMessageType::GET_IGNORE_LIST:
			ChatIgnoreList::GetIgnoreList(packet);
			break;

		case eChatMessageType::ADD_IGNORE:
			ChatIgnoreList::AddIgnore(packet);
			break;

		case eChatMessageType::REMOVE_IGNORE:
			ChatIgnoreList::RemoveIgnore(packet);
			break;

		case eChatMessageType::TEAM_GET_STATUS:
			ChatPacketHandler::HandleTeamStatusRequest(packet);
			break;

		case eChatMessageType::ADD_FRIEND_REQUEST:
			//this involves someone sending the initial request, the response is below, response as in from the other player.
			//We basically just check to see if this player is online or not and route the packet.
			ChatPacketHandler::HandleFriendRequest(packet);
			break;

		case eChatMessageType::ADD_FRIEND_RESPONSE:
			//This isn't the response a server sent, rather it is a player's response to a received request.
			//Here, we'll actually have to add them to eachother's friend lists depending on the response code.
			ChatPacketHandler::HandleFriendResponse(packet);
			break;

		case eChatMessageType::REMOVE_FRIEND:
			ChatPacketHandler::HandleRemoveFriend(packet);
			break;

		case eChatMessageType::GENERAL_CHAT_MESSAGE:
			ChatPacketHandler::HandleChatMessage(packet);
			break;

		case eChatMessageType::PRIVATE_CHAT_MESSAGE:
			//This message is supposed to be echo'd to both the sender and the receiver
			//BUT: they have to have different responseCodes, so we'll do some of the ol hacky wacky to fix that right up.
			ChatPacketHandler::HandlePrivateChatMessage(packet);
			break;

		case eChatMessageType::TEAM_INVITE:
			ChatPacketHandler::HandleTeamInvite(packet);
			break;

		case eChatMessageType::TEAM_INVITE_RESPONSE:
			ChatPacketHandler::HandleTeamInviteResponse(packet);
			break;

		case eChatMessageType::TEAM_LEAVE:
			ChatPacketHandler::HandleTeamLeave(packet);
			break;

		case eChatMessageType::TEAM_SET_LEADER:
			ChatPacketHandler::HandleTeamPromote(packet);
			break;

		case eChatMessageType::TEAM_KICK:
			ChatPacketHandler::HandleTeamKick(packet);
			break;

		case eChatMessageType::TEAM_SET_LOOT:
			ChatPacketHandler::HandleTeamLootOption(packet);
			break;

		default:
			LOG("Unknown CHAT id: %i", int(packet->data[3]));
		}
	}

	if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::WORLD) {
		switch (static_cast<eWorldMessageType>(packet->data[3])) {
		case eWorldMessageType::ROUTE_PACKET: {
			LOG("Routing packet from world");
			break;
		}

		default:
			LOG("Unknown World id: %i", int(packet->data[3]));
		}
	}
}
