#include "AuthPackets.h"
#include "BitstreamUtils.h"

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

#include "PacketUtils.h"
#include "Game.h"
#include "dConfig.h"
#include "eServerDisconnectIdentifiers.h"
#include "eLoginResponse.h"
#include "eConnectionType.h"
#include "eServerMessageType.h"
#include "eMasterMessageType.h"
#include "eStamps.h"

void Stamp::Serialize(RakNet::BitStream* outBitStream){
	outBitStream->Write(type);
	outBitStream->Write(value);
	outBitStream->Write(timestamp);
};

void AuthPackets::HandleHandshake(dServer* server, Packet* packet) {
	CINSTREAM_SKIP_HEADER
	uint32_t clientVersion = 0;
	inStream.Read(clientVersion);

	uint32_t paddingZero;
	inStream.Read(paddingZero);
	if (paddingZero != 0) Game::logger->Log("AuthPackets::HandleHandShake", "WARNING: ZeroPadding is not Zero!");

	ServiceId serviceId;
	inStream.Read(serviceId);
	if (serviceId != ServiceId::Client) Game::logger->Log("AuthPackets::HandleHandShake", "WARNING: Service ID is not a Client!");

	uint32_t processID;
	inStream.Read(processID);

	uint16_t port;
	inStream.Read(port);

	LUWString paddingString(33);
	inStream.Read(paddingString);
	if (!paddingString.string.empty()) Game::logger->Log("AuthPackets::HandleHandShake", "WARNING: StringPadding is not empty!");

	if (inStream.GetNumberOfUnreadBits() != 8) Game::logger->Log("AuthPackets::HandleHandShake", "WARNING: End padding is incorrect!");
	Game::logger->Log("AuthPackets::HandleHandShake", "Client Data [Version: %i, PaddingZero: %i, Service: %u, Process: %u, Port: %u, paddingString: %s, Sysaddr Port: %u]", clientVersion, paddingZero, serviceId, processID, port, paddingString.string.c_str(), packet->systemAddress.port);
	SendHandshake(server, packet->systemAddress, server->GetIP(), server->GetPort(), server->GetServerType());
}

void AuthPackets::SendHandshake(dServer* server, const SystemAddress& sysAddr, const std::string& nextServerIP, uint16_t nextServerPort, const ServerType serverType) {
	RakNet::BitStream handshakeResponse;
	BitstreamUtils::WriteHeader(handshakeResponse, eConnectionType::SERVER, eServerMessageType::VERSION_CONFIRM);
	uint32_t netVersion;
	if (!GeneralUtils::TryParse(Game::config->GetValue("client_net_version"), netVersion)) {
		Game::logger->Log("AuthPackets", "Failed to parse client_net_version. Cannot authenticate to %s:%i", nextServerIP.c_str(), nextServerPort);
		return;
	}
	handshakeResponse.Write<uint32_t>(netVersion);
	handshakeResponse.Write<uint32_t>(0); // Unused/Unknown/Padding
	if (serverType == ServerType::Auth) handshakeResponse.Write(ServiceId::Auth);
	else if (serverType == ServerType::World) handshakeResponse.Write(ServiceId::World);
	else handshakeResponse.Write(ServiceId::General);

	server->Send(&handshakeResponse, sysAddr, false);
}

void AuthPackets::HandleLoginRequest(dServer* server, Packet* packet) {
	CINSTREAM_SKIP_HEADER;

	LUWString username(33);
	inStream.Read(username);
	std::string usernameString = GeneralUtils::UTF16ToWTF8(username.string);

	LUWString password(41);
	inStream.Read(password);

	LanguageCodeID locale_id;
	inStream.Read(locale_id);
	Game::logger->Log("AuthPackets", "Locale ID: %i", locale_id);

	ClientOS clientOS;
	inStream.Read(clientOS);
	std::string clientOSString = "Unknown";
	if (clientOS == ClientOS::WINDOWS) clientOSString = "Windows";
	else if (clientOS == ClientOS::MACOS) clientOSString = "MacOS";
	Game::logger->Log("AuthPackets", "Operating System: %s",clientOSString.c_str());

	LUWString memoryStats(256);
	inStream.Read(memoryStats);
	Game::logger->Log("AuthPackets", "Memory Stats [%s]", memoryStats.GetAsString().c_str());

	LUWString videoCard(128);
	inStream.Read(videoCard);
	Game::logger->Log("AuthPackets", "VideoCard Info: [%s]", videoCard.GetAsString().c_str());

	// Processor/CPU info
	uint32_t numOfProcessors;
	inStream.Read(numOfProcessors);
	uint32_t processorType;
	inStream.Read(processorType);
	uint16_t processorLevel;
	inStream.Read(processorLevel);
	uint16_t processorRevision;
	inStream.Read(processorRevision);
	Game::logger->Log("AuthPackets", "CPU Info: [#Processors: %i, Processor Type: %i, Processor Level: %i, Processor Revision: %i]", numOfProcessors, processorType, processorLevel, processorRevision);

	// OS Info
	uint32_t osVersionInfoSize;
	inStream.Read(osVersionInfoSize);
	uint32_t majorVersion;
	inStream.Read(majorVersion);
	uint32_t minorVersion;
	inStream.Read(minorVersion);
	uint32_t buildNumber;
	inStream.Read(buildNumber);
	uint32_t platformID;
	inStream.Read(platformID);
	Game::logger->Log("AuthPackets", "OS Info: [Size: %i, Major: %i, Minor %i, Buid#: %i, platformID: %i]", osVersionInfoSize, majorVersion, minorVersion, buildNumber, platformID);

	// Fetch account details
	std::unique_ptr<sql::PreparedStatement> stmt(Database::CreatePreppedStmt("SELECT password, banned, locked, play_key_id, gm_level FROM accounts WHERE name=? LIMIT 1;"));
	stmt->setString(1, usernameString.c_str());

	std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

	if (res->rowsCount() == 0) {
		server->GetLogger()->Log("AuthPackets", "No user found!");
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::INVALID_USER, "", "", 2001, usernameString);
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

	int32_t bcryptState = ::bcrypt_checkpw(GeneralUtils::UTF16ToWTF8(password.string).c_str(), sqlPass.c_str());
	if (bcryptState != 0) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::WRONG_PASS, "", "", 2001, usernameString);
		server->GetLogger()->Log("AuthPackets", "Wrong password used");
		return;
	}

	//If we aren't running in live mode, then only GMs are allowed to enter:
	const auto& closedToNonDevs = Game::config->GetValue("closed_to_non_devs");
	if (closedToNonDevs.size() > 0 && bool(std::stoi(closedToNonDevs)) && sqlGmLevel == 0) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "The server is currently only open to developers.", "", 2001, usernameString);
		return;
	}

	if (Game::config->GetValue("dont_use_keys") != "1") {
		//Check to see if we have a play key:
		if (sqlPlayKey == 0 && sqlGmLevel == 0) {
			AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a play key associated with it!", "", 2001, usernameString);
			server->GetLogger()->Log("AuthPackets", "User %s tried to log in, but they don't have a play key.", usernameString.c_str());
			return;
		}

		//Check if the play key is _valid_:
		auto keyCheckStmt = Database::CreatePreppedStmt("SELECT active FROM `play_keys` WHERE id=?");
		keyCheckStmt->setInt(1, sqlPlayKey);
		auto keyRes = keyCheckStmt->executeQuery();
		bool isKeyActive = false;

		if (keyRes->rowsCount() == 0 && sqlGmLevel == 0) {
			AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a play key associated with it!", "", 2001, usernameString);
			return;
		}

		while (keyRes->next()) {
			isKeyActive = (bool)keyRes->getInt(1);
		}

		if (!isKeyActive && sqlGmLevel == 0) {
			AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your play key has been disabled.", "", 2001, usernameString);
			server->GetLogger()->Log("AuthPackets", "User %s tried to log in, but their play key was disabled", usernameString.c_str());
			return;
		}
	}

	if (sqlBanned) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::BANNED, "", "", 2001, usernameString);
		return;
	}

	if (sqlLocked) {
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::ACCOUNT_LOCKED, "", "", 2001, usernameString);
		return;
	}

	auto system = packet->systemAddress; //Copy the sysAddr before the Packet gets destroyed from main

	if (!server->GetIsConnectedToMaster()) {
		AuthPackets::SendLoginResponse(server, system, eLoginResponse::GENERAL_FAILED, "Unable to Connect to Master Server", "", 0, usernameString);
		Game::logger->Log("AuthPackets", "Unable to complete login of user %s due to no connection to Master", usernameString.c_str());
		return;
	}

	ZoneInstanceManager::Instance()->RequestZoneTransfer(server, 0, 0, false, [system, server, usernameString](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string zoneIP, uint16_t zonePort) {
		AuthPackets::SendLoginResponse(server, system, eLoginResponse::SUCCESS, "", zoneIP, zonePort, usernameString);
		});
}

void AuthPackets::SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username) {
	RakNet::BitStream loginResponse;
	BitstreamUtils::WriteHeader(loginResponse, eConnectionType::CLIENT, eClientMessageType::LOGIN_RESPONSE);

	loginResponse.Write(responseCode);

	// Events
	loginResponse.Write(LUString("Talk_Like_A_Pirate"));
	loginResponse.Write(LUString(""));
	loginResponse.Write(LUString(""));
	loginResponse.Write(LUString(""));
	loginResponse.Write(LUString(""));
	loginResponse.Write(LUString(""));
	loginResponse.Write(LUString(""));
	loginResponse.Write(LUString(""));

	loginResponse.Write(static_cast<uint16_t>(1));         // Version Major
	loginResponse.Write(static_cast<uint16_t>(10));        // Version Current
	loginResponse.Write(static_cast<uint16_t>(64));        // Version Minor

	// Writes the user key
	uint32_t sessionKey = GeneralUtils::GenerateRandomNumber<uint32_t>();
	std::string userHash = std::to_string(sessionKey);
	userHash = md5(userHash);
	loginResponse.Write(LUWString(userHash));

	// World Server IP
	loginResponse.Write(LUString(wServerIP));
	// Chat Server IP (unused)
	loginResponse.Write(LUString(""));

	// World Server Redirect port
	loginResponse.Write(wServerPort);
	// Char Server Redirect port (unused)
	loginResponse.Write(static_cast<uint16_t>(0));

	// CDN Key
	loginResponse.Write(LUString(""));

	// CDN Ticket
	loginResponse.Write(LUString("00000000-0000-0000-0000-000000000000", 37));

	// Language
	loginResponse.Write(Language::en_US);

	// Country Code
	loginResponse.Write(LUString("US", 3));


	// Just upgraded from FTP
	loginResponse.Write(static_cast<uint8_t>(false));
	// Is FTP, static_cast<uint32_t>(errorMsg.length()))
	loginResponse.Write(static_cast<uint8_t>(false));
	// Time remaining in FTP
	loginResponse.Write(static_cast<uint64_t>(0));

	// Write custom error message
	loginResponse.Write(static_cast<uint16_t>(errorMsg.length()));
	loginResponse.Write(errorMsg);

	// Stamps
	std::vector<Stamp> stamps;
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_START, 0, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_LEGOINT_COMMUNICATION_START, 207, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_LEGOINT_RECEIVED, 13, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_LEGOINT_THREAD_SPAWN, 0, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_LEGOINT_WEBSERVICE_START, 0, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_LEGOINT_WEBSERVICE_FINISH, 1, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_LEGOINT_THREAD_FINISH, 1, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_LEGOINT_REPLY, 0, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_LEGOINT_COMMUNICATION_END, 0, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_DB_INSERT_START, 0, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_DB_INSERT_FINISH, 1, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_CLIENT_OS, 0, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_WORLD_COMMUNICATION_START, 16087, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_WORLD_PACKET_RECEIVED, 0, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_IM_COMMUNICATION_START, 0, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_IM_LOGIN_START, 5926, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_IM_LOGIN_RESPONSE, 5926, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_IM_COMMUNICATION_END, 1, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_WORLD_SESSION_CONFIRM_TO_AUTH, 0, time(nullptr)));
	stamps.push_back(Stamp(eStamps::PASSPORT_AUTH_WORLD_COMMUNICATION_FINISH, 16087, time(nullptr)));

	loginResponse.Write(static_cast<uint32_t>((sizeof(Stamp) * stamps.size()) + sizeof(uint32_t)));
	for (auto& stamp : stamps) {
		stamp.Serialize(&loginResponse);
	}
	PacketUtils::SavePacket("loginresponse",(const char*)loginResponse.GetData(), loginResponse.GetNumberOfBytesUsed());
	Game::logger->Log("AuthPackets", "Sending Login Response...");
	server->Send(&loginResponse, sysAddr, false);

	//Inform the master server that we've created a session for this user:
	{
		CBITSTREAM;
		BitstreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::SET_SESSION_KEY);
		bitStream.Write(sessionKey);
		bitStream.Write(LUString(username, 66));

		server->SendToMaster(&bitStream);

		server->GetLogger()->Log("AuthPackets", "Set sessionKey: %i for user %s", sessionKey, username.c_str());
	}
}
