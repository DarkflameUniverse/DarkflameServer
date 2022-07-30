#include "AuthPackets.h"
#include "PacketUtils.h"
#include "dMessageIdentifiers.h"

#include "dNetCommon.h"
#include "dServer.h"
#include "dLogger.h"
#include "Database.h"
#include "ZoneInstanceManager.h"
#include "MD5.h"
#include "SHA512.h"

#ifdef _WIN32
#include <bcrypt/BCrypt.hpp>
#else
#include <bcrypt.h>
#endif

#include <BitStream.h>
#include <future>

#include "Game.h"
#include "dConfig.h"

void AuthPackets::HandleHandshake(dServer* server, Packet* packet) {
	RakNet::BitStream inStream(packet->data, packet->length, false);
	uint64_t header = inStream.Read(header);
	uint32_t clientVersion = 0;
	inStream.Read(clientVersion);

	server->GetLogger()->Log("AuthPackets", "Received client version: %i", clientVersion);
	SendHandshake(server, packet->systemAddress, server->GetIP(), server->GetPort());
}

void AuthPackets::SendHandshake(dServer* server, const SystemAddress& sysAddr, const std::string& nextServerIP, uint16_t nextServerPort) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, SERVER, MSG_SERVER_VERSION_CONFIRM);
	bitStream.Write<unsigned int>(NET_VERSION);
	bitStream.Write(uint32_t(0x93));

	if (nextServerPort == 1001) bitStream.Write(uint32_t(1)); //Conn: auth
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
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT password, banned, locked, play_key_id, gm_level FROM accounts WHERE name=? LIMIT 1;");
	stmt->setString(1, szUsername);

	sql::ResultSet* res = stmt->executeQuery();

	if (res->rowsCount() == 0) {
		server->GetLogger()->Log("AuthPackets", "No user found!");
		AuthPackets::SendLoginResponse(server, packet->systemAddress, LOGIN_RESPONSE_WRONG_PASS_OR_USER, "", "", 2001, username);
		return;
	}

	std::string sqlPass = "";
	bool sqlBanned = false;
	bool sqlLocked = false;
	uint32_t sqlPlayKey = 0;
	uint32_t sqlGmLevel = 0;

	while (res->next()) {
		sqlPass = res->getString(1).c_str();
		sqlBanned = res->getBoolean(2);
		sqlLocked = res->getBoolean(3);
		sqlPlayKey = res->getInt(4);
		sqlGmLevel = res->getInt(5);
	}

	delete stmt;
	delete res;

	//If we aren't running in live mode, then only GMs are allowed to enter:
	const auto& closedToNonDevs = Game::config->GetValue("closed_to_non_devs");
	if (closedToNonDevs.size() > 0 && bool(std::stoi(closedToNonDevs)) && sqlGmLevel == 0) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::LOGIN_RESPONSE_PERMISSIONS_NOT_HIGH_ENOUGH, "The server is currently only open to developers.", "", 2001, username);
		return;
	}

	if (Game::config->GetValue("dont_use_keys") != "1") {
		//Check to see if we have a play key:
		if (sqlPlayKey == 0 && sqlGmLevel == 0) {
			AuthPackets::SendLoginResponse(server, packet->systemAddress, LOGIN_RESPONSE_PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a play key associated with it!", "", 2001, username);
			server->GetLogger()->Log("AuthPackets", "User %s tried to log in, but they don't have a play key.", username.c_str());
			return;
		}

		//Check if the play key is _valid_:
		auto keyCheckStmt = Database::CreatePreppedStmt("SELECT active FROM `play_keys` WHERE id=?");
		keyCheckStmt->setInt(1, sqlPlayKey);
		auto keyRes = keyCheckStmt->executeQuery();
		bool isKeyActive = false;

		if (keyRes->rowsCount() == 0 && sqlGmLevel == 0) {
			AuthPackets::SendLoginResponse(server, packet->systemAddress, LOGIN_RESPONSE_PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a play key associated with it!", "", 2001, username);
			return;
		}

		while (keyRes->next()) {
			isKeyActive = (bool)keyRes->getInt(1);
		}

		if (!isKeyActive && sqlGmLevel == 0) {
			AuthPackets::SendLoginResponse(server, packet->systemAddress, LOGIN_RESPONSE_PERMISSIONS_NOT_HIGH_ENOUGH, "Your play key has been disabled.", "", 2001, username);
			server->GetLogger()->Log("AuthPackets", "User %s tried to log in, but their play key was disabled", username.c_str());
			return;
		}
	}

	if (sqlBanned) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, LOGIN_RESPONSE_BANNED, "", "", 2001, username); return;
	}

	if (sqlLocked) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, LOGIN_RESPONSE_ACCOUNT_LOCKED, "", "", 2001, username); return;
	}

	/*
	 * Updated hashing method:
	 * First attempt bcrypt.
	 * If that fails, fallback to old method and setup bcrypt for new login.
	 */

	bool loginSuccess = true;

	int32_t bcryptState = ::bcrypt_checkpw(password.c_str(), sqlPass.c_str());

	if (bcryptState != 0) {
		// Fallback on old method

		std::string oldPassword = sha512(password + username);

		if (sqlPass != oldPassword) {
			loginSuccess = false;
		} else {
			// Generate new hash for bcrypt

			char salt[BCRYPT_HASHSIZE];
			char hash[BCRYPT_HASHSIZE];

			bcryptState = ::bcrypt_gensalt(12, salt);

			assert(bcryptState == 0);

			bcryptState = ::bcrypt_hashpw(password.c_str(), salt, hash);

			assert(bcryptState == 0);

			sql::PreparedStatement* accountUpdate = Database::CreatePreppedStmt("UPDATE accounts SET password = ? WHERE name = ? LIMIT 1;");

			accountUpdate->setString(1, std::string(hash, BCRYPT_HASHSIZE).c_str());
			accountUpdate->setString(2, szUsername);

			accountUpdate->executeUpdate();
		}
	} else {
		// Login success with bcrypt
	}

	if (!loginSuccess) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, LOGIN_RESPONSE_WRONG_PASS_OR_USER, "", "", 2001, username);
		server->GetLogger()->Log("AuthPackets", "Wrong password used");
	} else {
		SystemAddress system = packet->systemAddress; //Copy the sysAddr before the Packet gets destroyed from main

		if (!server->GetIsConnectedToMaster()) {
			AuthPackets::SendLoginResponse(server, system, LOGIN_RESPONSE_GENERAL_FAILED, "", "", 0, username);
			return;
		}

		ZoneInstanceManager::Instance()->RequestZoneTransfer(server, 0, 0, false, [system, server, username](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string zoneIP, uint16_t zonePort) {
			AuthPackets::SendLoginResponse(server, system, LOGIN_RESPONSE_SUCCESS, "", zoneIP, zonePort, username);
			});
	}
}

void AuthPackets::SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username) {
	RakNet::BitStream packet;
	PacketUtils::WriteHeader(packet, CLIENT, MSG_CLIENT_LOGIN_RESPONSE);

	packet.Write(static_cast<uint8_t>(responseCode));

	PacketUtils::WritePacketString("Talk_Like_A_Pirate", 33, &packet);

	// 7 unknown strings - perhaps other IP addresses?
	PacketUtils::WritePacketString("", 33, &packet);
	PacketUtils::WritePacketString("", 33, &packet);
	PacketUtils::WritePacketString("", 33, &packet);
	PacketUtils::WritePacketString("", 33, &packet);
	PacketUtils::WritePacketString("", 33, &packet);
	PacketUtils::WritePacketString("", 33, &packet);
	PacketUtils::WritePacketString("", 33, &packet);

	packet.Write(static_cast<uint16_t>(1));         // Version Major
	packet.Write(static_cast<uint16_t>(10));        // Version Current
	packet.Write(static_cast<uint16_t>(64));        // Version Minor

	// Writes the user key
	uint32_t sessionKey = rand(); // not mt but whatever
	std::string userHash = std::to_string(sessionKey);
	userHash = md5(userHash);
	PacketUtils::WritePacketWString(userHash, 33, &packet);

	// Write the Character and Chat IPs
	PacketUtils::WritePacketString(wServerIP, 33, &packet);
	PacketUtils::WritePacketString("", 33, &packet);

	// Write the Character and Chat Ports
	packet.Write(static_cast<uint16_t>(wServerPort));
	packet.Write(static_cast<uint16_t>(0));

	// Write another IP
	PacketUtils::WritePacketString("", 33, &packet);

	// Write a GUID or something...
	PacketUtils::WritePacketString("00000000-0000-0000-0000-000000000000", 37, &packet);

	packet.Write(static_cast<uint32_t>(0));         // ???

	// Write the localization
	PacketUtils::WritePacketString("US", 3, &packet);

	packet.Write(static_cast<uint8_t>(false));      // User first logged in?
	packet.Write(static_cast<uint8_t>(false));      // User is F2P?
	packet.Write(static_cast<uint64_t>(0));         // ???

	// Write custom error message
	packet.Write(static_cast<uint16_t>(errorMsg.length()));
	PacketUtils::WritePacketWString(errorMsg, static_cast<uint32_t>(errorMsg.length()), &packet);

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
		PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_SET_SESSION_KEY);
		bitStream.Write(sessionKey);
		PacketUtils::WriteString(bitStream, username, 66);
		server->SendToMaster(&bitStream);

		server->GetLogger()->Log("AuthPackets", "Set sessionKey: %i for user %s", sessionKey, username.c_str());
	}
}
