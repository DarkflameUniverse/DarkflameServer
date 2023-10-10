#include "AuthPackets.h"
#include "PacketUtils.h"
#include "BitStreamUtils.h"

#include "dNetCommon.h"
#include "dServer.h"
#include "dLogger.h"
#include "Database.h"
#include "ZoneInstanceManager.h"
#include "MD5.h"
#include "SHA512.h"
#include "GeneralUtils.h"

#ifdef _WIN32
#include <bcrypt/BCrypt.hpp>
#else
#include <bcrypt.h>
#endif

#include <BitStream.h>
#include <future>

#include "Game.h"
#include "dConfig.h"
#include "eServerDisconnectIdentifiers.h"
#include "eLoginResponse.h"
#include "eConnectionType.h"
#include "eServerMessageType.h"
#include "eMasterMessageType.h"

void AuthPackets::HandleHandshake(dServer* server, Packet* packet) {
	RakNet::BitStream inStream(packet->data, packet->length, false);
	uint64_t header = inStream.Read(header);
	uint32_t clientVersion = 0;
	inStream.Read(clientVersion);

	server->GetLogger()->Log("AuthPackets", "Received client version: %i", clientVersion);
	SendHandshake(server, packet->systemAddress, server->GetIP(), server->GetPort(), server->GetServerType());
}

void AuthPackets::SendHandshake(dServer* server, const SystemAddress& sysAddr, const std::string& nextServerIP, uint16_t nextServerPort, const ServerType serverType) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::SERVER, eServerMessageType::VERSION_CONFIRM);
	uint32_t netVersion;
	if (!GeneralUtils::TryParse(Game::config->GetValue("client_net_version"), netVersion)) {
		Game::logger->Log("AuthPackets", "Failed to parse client_net_version. Cannot authenticate to %s:%i", nextServerIP.c_str(), nextServerPort);
		return;
	}
	bitStream.Write<uint32_t>(netVersion);
	bitStream.Write(uint32_t(0x93));

	if (serverType == ServerType::Auth) bitStream.Write(uint32_t(1)); //Conn: auth
	else bitStream.Write(uint32_t(4)); //Conn: world

	bitStream.Write(uint32_t(0)); //Server process ID
	bitStream.Write(nextServerPort);

	server->Send(&bitStream, sysAddr, false);
}

void AuthPackets::HandleLoginRequest(dServer* server, Packet* packet) {
	std::string username = PacketUtils::ReadString(8, packet, true);
	std::string password = PacketUtils::ReadString(0x4A, packet, true);
	const char* szUsername = username.c_str();

	// Fetch account details
	auto account = Database::Connection->GetAccountByName(username);

	if (account.ID == 0) {
		server->GetLogger()->Log("AuthPackets", "No user found!");
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::INVALID_USER, "", "", 2001, username);
		return;
	}
	

	//If we aren't running in live mode, then only GMs are allowed to enter:
	const auto& closedToNonDevs = Game::config->GetValue("closed_to_non_devs");
	if (closedToNonDevs.size() > 0 && bool(std::stoi(closedToNonDevs)) && account.MaxGMLevel == 0) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "The server is currently only open to developers.", "", 2001, username);
		return;
	}

	if (Game::config->GetValue("dont_use_keys") != "1") {
		// Check to see if we have a play key
		if (account.PlayKeyID == 0 && account.MaxGMLevel == 0) {
			AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a play key associated with it!", "", 2001, username);
			server->GetLogger()->Log("AuthPackets", "User %s tried to log in, but they don't have a play key.", username.c_str());
			return;
		}

		// Check if the play key is _valid_
		bool isKeyActive = Database::Connection->IsKeyActive(account.PlayKeyID);

		if (!isKeyActive && account.MaxGMLevel == 0) {
			AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a play key associated with it!", "", 2001, username);
			server->GetLogger()->Log("AuthPackets", "User %s tried to log in, but they lacked a play key", username.c_str());
			return;
		}
	}

	if (account.Banned) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::BANNED, "", "", 2001, username); return;
	}

	if (account.Locked) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::ACCOUNT_LOCKED, "", "", 2001, username); return;
	}

	if (!account.Password.empty()) {
		if (account.Password[0] != '$') {
			Game::logger->Log("AuthPackets", "Invalid password being parsed for user %s", username.c_str());
		}
	}

	int32_t bcryptState = ::bcrypt_checkpw(password.c_str(), account.Password.c_str());

	if (bcryptState != 0) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::WRONG_PASS, "", "", 2001, username);
		server->GetLogger()->Log("AuthPackets", "Wrong password used");
	} else {
		SystemAddress system = packet->systemAddress; //Copy the sysAddr before the Packet gets destroyed from main

		if (!server->GetIsConnectedToMaster()) {
			AuthPackets::SendLoginResponse(server, system, eLoginResponse::GENERAL_FAILED, "", "", 0, username);
			return;
		}

		ZoneInstanceManager::Instance()->RequestZoneTransfer(server, 0, 0, false, [system, server, username](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string zoneIP, uint16_t zonePort) {
			AuthPackets::SendLoginResponse(server, system, eLoginResponse::SUCCESS, "", zoneIP, zonePort, username);
			});
	}
}

void AuthPackets::SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username) {
	RakNet::BitStream packet;
	BitStreamUtils::WriteHeader(packet, eConnectionType::CLIENT, eClientMessageType::LOGIN_RESPONSE);

	packet.Write(static_cast<uint8_t>(responseCode));

	// Event Gating
	packet.Write(LUString("Talk_Like_A_Pirate"));
	packet.Write(LUString(""));
	packet.Write(LUString(""));
	packet.Write(LUString(""));
	packet.Write(LUString(""));
	packet.Write(LUString(""));
	packet.Write(LUString(""));
	packet.Write(LUString(""));

	packet.Write(static_cast<uint16_t>(1));         // Version Major
	packet.Write(static_cast<uint16_t>(10));        // Version Current
	packet.Write(static_cast<uint16_t>(64));        // Version Minor

	// Writes the user key
	uint32_t sessionKey = GeneralUtils::GenerateRandomNumber<uint32_t>();
	std::string userHash = std::to_string(sessionKey);
	userHash = md5(userHash);
	packet.Write(LUWString(userHash));

	// Write the Character and Chat IPs
	packet.Write(LUString(wServerIP));
	packet.Write(LUString(""));

	// Write the Character and Chat Ports
	packet.Write(static_cast<uint16_t>(wServerPort));
	packet.Write(static_cast<uint16_t>(0));

	// CDN Key
	packet.Write(LUString(""));

	// CDN Ticket
	packet.Write(LUString("00000000-0000-0000-0000-000000000000", 37));

	packet.Write(static_cast<uint32_t>(0)); // Language

	// Write the localization
	packet.Write(LUString("US", 3));

	packet.Write(static_cast<uint8_t>(false)); // Just upgraded from F2P
	packet.Write(static_cast<uint8_t>(false)); // User is F2P
	packet.Write(static_cast<uint64_t>(0)); // Time Remaining in F2P

	// Write custom error message
	packet.Write(static_cast<uint16_t>(errorMsg.length()));
	packet.Write(LUWString(errorMsg, static_cast<uint32_t>(errorMsg.length())));

	// Here write auth logs
	packet.Write(static_cast<uint32_t>(20));
	for (uint32_t i = 0; i < 20; ++i) {
		packet.Write(static_cast<uint32_t>(8));
		packet.Write(static_cast<uint32_t>(44));
		packet.Write(static_cast<uint32_t>(14000));
		packet.Write(static_cast<uint32_t>(0));
	}

	server->Send(&packet, sysAddr, false);

	//Inform the master server that we've created a session for this user:
	{
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::SET_SESSION_KEY);
		bitStream.Write(sessionKey);
		bitStream.Write(LUString(username, 66));
		server->SendToMaster(&bitStream);

		server->GetLogger()->Log("AuthPackets", "Set sessionKey: %i for user %s", sessionKey, username.c_str());
	}
}
