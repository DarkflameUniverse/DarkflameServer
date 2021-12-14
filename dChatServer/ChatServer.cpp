#include <iostream>
#include <string>
#include <chrono>
#include <thread>

//DLU Includes:
#include "dCommonVars.h"
#include "dServer.h"
#include "dLogger.h"
#include "Database.h"
#include "dConfig.h"
#include "dMessageIdentifiers.h"
#include "dChatFilter.h"
#include "Diagnostics.h"

#include "PlayerContainer.h"
#include "ChatPacketHandler.h"

#include "Game.h"
namespace Game {
	dLogger* logger;
	dServer* server;
	dConfig* config;
	dChatFilter* chatFilter;
}

//RakNet includes:
#include "RakNetDefines.h"

dLogger* SetupLogger();
void HandlePacket(Packet* packet);

PlayerContainer playerContainer;

int main(int argc, char** argv) {
	Diagnostics::SetProcessName("Chat");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	//Create all the objects we need to run our service:
	Game::logger = SetupLogger();
	if (!Game::logger) return 0;
	Game::logger->Log("ChatServer", "Starting Chat server...\n");
	Game::logger->Log("ChatServer", "Version: %i.%i\n", PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR);
	Game::logger->Log("ChatServer", "Compiled on: %s\n", __TIMESTAMP__);

	//Read our config:
	dConfig config("chatconfig.ini");
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
	}
	catch (sql::SQLException& ex) {
		Game::logger->Log("ChatServer", "Got an error while connecting to the database: %s\n", ex.what());
		Database::Destroy();
		delete Game::server;
		delete Game::logger;
		return 0;
	}

	//Find out the master's IP:
	std::string masterIP;
	int masterPort = 1000;
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
	int ourPort = 1501;
	if (config.GetValue("max_clients") != "") maxClients = std::stoi(config.GetValue("max_clients"));
	if (config.GetValue("port") != "") ourPort = std::atoi(config.GetValue("port").c_str());

	Game::server = new dServer(config.GetValue("external_ip"), ourPort, 0, maxClients, false, true, Game::logger, masterIP, masterPort, ServerType::Chat);

	Game::chatFilter = new dChatFilter("./res/chatplus_en_us", bool(std::stoi(config.GetValue("dont_generate_dcf"))));

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
		}
		else framesSinceMasterDisconnect = 0;

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
		}
		else framesSinceLastFlush++;

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
		else framesSinceLastSQLPing++;

		//Sleep our thread since auth can afford to.
		t += std::chrono::milliseconds(mediumFramerate); //Chat can run at a lower "fps"
		std::this_thread::sleep_until(t);
	}

	//Delete our objects here:
	Database::Destroy();
	delete Game::server;
	delete Game::logger;

	return 0;
}

dLogger * SetupLogger() {
	std::string logPath = "./logs/ChatServer_" + std::to_string(time(nullptr)) + ".log";
	bool logToConsole = false;
	bool logDebugStatements = false;
#ifdef _DEBUG
	logToConsole = true;
	logDebugStatements = true;
#endif

	return new dLogger(logPath, logToConsole, logDebugStatements);
}

void HandlePacket(Packet* packet) {
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		Game::logger->Log("ChatServer", "A server has disconnected, erasing their connected players from the list.\n");
	}

	if (packet->data[0] == ID_NEW_INCOMING_CONNECTION) {
		Game::logger->Log("ChatServer", "A server is connecting, awaiting user list.\n");
	}

	if (packet->data[1] == CHAT_INTERNAL) {
		switch (packet->data[3]) {
		case MSG_CHAT_INTERNAL_PLAYER_ADDED_NOTIFICATION:
			playerContainer.InsertPlayer(packet);
			break;

		case MSG_CHAT_INTERNAL_PLAYER_REMOVED_NOTIFICATION:
			playerContainer.RemovePlayer(packet);
			break;

		case MSG_CHAT_INTERNAL_MUTE_UPDATE:
			playerContainer.MuteUpdate(packet);
			break;

		case MSG_CHAT_INTERNAL_CREATE_TEAM:
			playerContainer.CreateTeamServer(packet);
			break;

		case MSG_CHAT_INTERNAL_ANNOUNCEMENT: {
			//we just forward this packet to every connected server
			CINSTREAM;
			Game::server->Send(&inStream, packet->systemAddress, true); //send to everyone except origin
			break;
		}

		default:
			Game::logger->Log("ChatServer", "Unknown CHAT_INTERNAL id: %i\n", int(packet->data[3]));
		}
	}

	if (packet->data[1] == CHAT) {
		switch (packet->data[3]) {
		case MSG_CHAT_GET_FRIENDS_LIST:
			ChatPacketHandler::HandleFriendlistRequest(packet);
			break;

		case MSG_CHAT_GET_IGNORE_LIST:
			Game::logger->Log("ChatServer", "Asked for ignore list, but is unimplemented right now.\n");
			break;

		case MSG_CHAT_TEAM_GET_STATUS:
			ChatPacketHandler::HandleTeamStatusRequest(packet);
			break;

		case MSG_CHAT_ADD_FRIEND_REQUEST:
			//this involves someone sending the initial request, the response is below, response as in from the other player.
			//We basically just check to see if this player is online or not and route the packet.
			ChatPacketHandler::HandleFriendRequest(packet);
			break;

		case MSG_CHAT_ADD_FRIEND_RESPONSE:
			//This isn't the response a server sent, rather it is a player's response to a received request.
			//Here, we'll actually have to add them to eachother's friend lists depending on the response code.
			ChatPacketHandler::HandleFriendResponse(packet);
			break;

		case MSG_CHAT_REMOVE_FRIEND:
			ChatPacketHandler::HandleRemoveFriend(packet);
			break;

		case MSG_CHAT_GENERAL_CHAT_MESSAGE:
			ChatPacketHandler::HandleChatMessage(packet);
			break;

		case MSG_CHAT_PRIVATE_CHAT_MESSAGE:
			//This message is supposed to be echo'd to both the sender and the receiver
			//BUT: they have to have different responseCodes, so we'll do some of the ol hacky wacky to fix that right up.
			ChatPacketHandler::HandlePrivateChatMessage(packet);
			break;

		case MSG_CHAT_TEAM_INVITE:
			ChatPacketHandler::HandleTeamInvite(packet);
			break;

		case MSG_CHAT_TEAM_INVITE_RESPONSE:
			ChatPacketHandler::HandleTeamInviteResponse(packet);
			break;

		case MSG_CHAT_TEAM_LEAVE:
			ChatPacketHandler::HandleTeamLeave(packet);
			break;

		case MSG_CHAT_TEAM_SET_LEADER:
			ChatPacketHandler::HandleTeamPromote(packet);
			break;

		case MSG_CHAT_TEAM_KICK:
			ChatPacketHandler::HandleTeamKick(packet);
			break;

		case MSG_CHAT_TEAM_SET_LOOT:
			ChatPacketHandler::HandleTeamLootOption(packet);
			break;
		
		default:
			Game::logger->Log("ChatServer", "Unknown CHAT id: %i\n", int(packet->data[3]));
		}
	}

	if (packet->data[1] == WORLD) {
		switch (packet->data[3]) {
		case MSG_WORLD_CLIENT_ROUTE_PACKET: {
			printf("routing packet from world\n");
			break;
		}

		default:
			Game::logger->Log("ChatServer", "Unknown World id: %i\n", int(packet->data[3]));
		}
	}
}
