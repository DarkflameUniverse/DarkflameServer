#include "AuthPackets.h"
#include "BitStreamUtils.h"

#include "dNetCommon.h"
#include "dServer.h"
#include "Logger.h"
#include "Database.h"
#include "ZoneInstanceManager.h"
#include "MD5.h"
#include "GeneralUtils.h"
#include "ClientVersion.h"

#include <bcrypt/BCrypt.hpp>

#include "BitStream.h"
#include <future>

#include "Game.h"
#include "dConfig.h"
#include "eServerDisconnectIdentifiers.h"
#include "eLoginResponse.h"
#include "eConnectionType.h"
#include "eServerMessageType.h"
#include "eMasterMessageType.h"
#include "eGameMasterLevel.h"
#include "StringifiedEnum.h"
namespace {
	std::vector<uint32_t> claimCodes;
}

void Stamp::Serialize(RakNet::BitStream& outBitStream){
	outBitStream.Write(type);
	outBitStream.Write(value);
	outBitStream.Write(timestamp);
};

void AuthPackets::LoadClaimCodes() {
	if(!claimCodes.empty()) return;
	auto rcstring = Game::config->GetValue("rewardcodes");
	auto codestrings = GeneralUtils::SplitString(rcstring, ',');
	for(auto const &codestring: codestrings){
		const auto code = GeneralUtils::TryParse<uint32_t>(codestring);

		if (code && code.value() != -1) claimCodes.push_back(code.value());
	}
}

void AuthPackets::HandleHandshake(dServer* server, Packet* packet) {
	CINSTREAM_SKIP_HEADER
	uint32_t clientVersion = 0;
	inStream.Read(clientVersion);
	inStream.IgnoreBytes(4);

	ServiceId serviceId;
	inStream.Read(serviceId);
	if (serviceId != ServiceId::Client) LOG("WARNING: Service ID is not a Client!");

	uint32_t processID;
	inStream.Read(processID);

	uint16_t port;
	inStream.Read(port);
	if (port != packet->systemAddress.port) LOG("WARNING: Port written in packet does not match the port the client is connecting over!");

	inStream.IgnoreBytes(33);
	
	LOG_DEBUG("Client Data [Version: %i, Service: %s, Process: %u, Port: %u, Sysaddr Port: %u]", clientVersion, StringifiedEnum::ToString(serviceId).data(), processID, port, packet->systemAddress.port);

	SendHandshake(server, packet->systemAddress, server->GetIP(), server->GetPort(), server->GetServerType());
}

void AuthPackets::SendHandshake(dServer* server, const SystemAddress& sysAddr, const std::string& nextServerIP, uint16_t nextServerPort, const ServerType serverType) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::SERVER, eServerMessageType::VERSION_CONFIRM);
	
	const auto clientNetVersionString = Game::config->GetValue("client_net_version");
	const uint32_t clientNetVersion = GeneralUtils::TryParse<uint32_t>(clientNetVersionString).value_or(171022);

	bitStream.Write<uint32_t>(clientNetVersion);
	bitStream.Write<uint32_t>(861228100);

	if (serverType == ServerType::Auth) bitStream.Write(ServiceId::Auth);
	else if (serverType == ServerType::World) bitStream.Write(ServiceId::World);
	else bitStream.Write(ServiceId::General);
	bitStream.Write<uint64_t>(215523405360);

	server->Send(bitStream, sysAddr, false);
}

void AuthPackets::HandleLoginRequest(dServer* server, Packet* packet) {
	CINSTREAM_SKIP_HEADER;

	std::vector<Stamp> stamps;
	stamps.emplace_back(eStamps::PASSPORT_AUTH_START, 0);

	LUWString usernameLUString;
	inStream.Read(usernameLUString);
	const auto username = usernameLUString.GetAsString();

	LUWString password(41);
	inStream.Read(password);

	LanguageCodeID locale_id;
	inStream.Read(locale_id);
	LOG_DEBUG("Locale ID: %s", StringifiedEnum::ToString(locale_id).data());

	ClientOS clientOS;
	inStream.Read(clientOS);
	LOG_DEBUG("Operating System: %s", StringifiedEnum::ToString(clientOS).data());
	stamps.emplace_back(eStamps::PASSPORT_AUTH_CLIENT_OS, 0);

	LUWString memoryStats(256);
	inStream.Read(memoryStats);
	LOG_DEBUG("Memory Stats [%s]", memoryStats.GetAsString().c_str());

	LUWString videoCard(128);
	inStream.Read(videoCard);
	LOG_DEBUG("VideoCard Info: [%s]", videoCard.GetAsString().c_str());

	// Processor/CPU info
	uint32_t numOfProcessors;
	inStream.Read(numOfProcessors);
	uint32_t processorType;
	inStream.Read(processorType);
	uint16_t processorLevel;
	inStream.Read(processorLevel);
	uint16_t processorRevision;
	inStream.Read(processorRevision);
	LOG_DEBUG("CPU Info: [#Processors: %i, Processor Type: %i, Processor Level: %i, Processor Revision: %i]", numOfProcessors, processorType, processorLevel, processorRevision);

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
	LOG_DEBUG("OS Info: [Size: %i, Major: %i, Minor %i, Buid#: %i, platformID: %i]", osVersionInfoSize, majorVersion, minorVersion, buildNumber, platformID);

	// Fetch account details
	auto accountInfo = Database::Get()->GetAccountInfo(username);

	if (!accountInfo) {
		LOG("No user by name %s found!", username.c_str());
		stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::INVALID_USER, "", "", 2001, username, stamps);
		return;
	}

	//If we aren't running in live mode, then only GMs are allowed to enter:
	const auto& closedToNonDevs = Game::config->GetValue("closed_to_non_devs");
	if (closedToNonDevs.size() > 0 && bool(std::stoi(closedToNonDevs)) && accountInfo->maxGmLevel == eGameMasterLevel::CIVILIAN) {
		stamps.emplace_back(eStamps::GM_REQUIRED, 1);
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "The server is currently only open to developers.", "", 2001, username, stamps);
		return;
	}

	if (Game::config->GetValue("dont_use_keys") != "1" && accountInfo->maxGmLevel == eGameMasterLevel::CIVILIAN) {
		//Check to see if we have a play key:
		if (accountInfo->playKeyId == 0) {
			stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
			AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a play key associated with it!", "", 2001, username, stamps);
			LOG("User %s tried to log in, but they don't have a play key.", username.c_str());
			return;
		}

		//Check if the play key is _valid_:
		auto playKeyStatus = Database::Get()->IsPlaykeyActive(accountInfo->playKeyId);

		if (!playKeyStatus) {
			stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
			AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a valid play key associated with it!", "", 2001, username, stamps);
			return;
		}

		if (!playKeyStatus.value()) {
			stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
			AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your play key has been disabled.", "", 2001, username, stamps);
			LOG("User %s tried to log in, but their play key was disabled", username.c_str());
			return;
		}
	} else if (Game::config->GetValue("dont_use_keys") == "1" || accountInfo->maxGmLevel > eGameMasterLevel::CIVILIAN){
		stamps.emplace_back(eStamps::PASSPORT_AUTH_BYPASS, 1);
	}

	if (accountInfo->banned) {
		stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::BANNED, "", "", 2001, username, stamps);
		return;
	}

	if (accountInfo->locked) {
		stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::ACCOUNT_LOCKED, "", "", 2001, username, stamps);
		return;
	}

	bool loginSuccess = ::bcrypt_checkpw(password.GetAsString().c_str(), accountInfo->bcryptPassword.c_str()) == 0;

	if (!loginSuccess) {
		stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
		AuthPackets::SendLoginResponse(server, packet->systemAddress, eLoginResponse::WRONG_PASS, "", "", 2001, username, stamps);
		LOG("Wrong password used");
	} else {
		SystemAddress system = packet->systemAddress; //Copy the sysAddr before the Packet gets destroyed from main

		if (!server->GetIsConnectedToMaster()) {
			stamps.emplace_back(eStamps::PASSPORT_AUTH_WORLD_DISCONNECT, 1);
			AuthPackets::SendLoginResponse(server, system, eLoginResponse::GENERAL_FAILED, "", "", 0, username, stamps);
			return;
		}
		stamps.emplace_back(eStamps::PASSPORT_AUTH_WORLD_SESSION_CONFIRM_TO_AUTH, 1);
		ZoneInstanceManager::Instance()->RequestZoneTransfer(server, 0, 0, false, [system, server, username, stamps](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string zoneIP, uint16_t zonePort) mutable {
			AuthPackets::SendLoginResponse(server, system, eLoginResponse::SUCCESS, "", zoneIP, zonePort, username, stamps);
			});
	}

	for(auto const code: claimCodes){
		Database::Get()->InsertRewardCode(accountInfo->id, code);
	}
}

void AuthPackets::SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username, std::vector<Stamp>& stamps) {
	stamps.emplace_back(eStamps::PASSPORT_AUTH_IM_LOGIN_START, 1);
	RakNet::BitStream loginResponse;
	BitStreamUtils::WriteHeader(loginResponse, eConnectionType::CLIENT, eClientMessageType::LOGIN_RESPONSE);

	loginResponse.Write<uint8_t>(GeneralUtils::CastUnderlyingType(responseCode));

	// Event Gating
	loginResponse.Write(LUString(Game::config->GetValue("event_1")));
	loginResponse.Write(LUString(Game::config->GetValue("event_2")));
	loginResponse.Write(LUString(Game::config->GetValue("event_3")));
	loginResponse.Write(LUString(Game::config->GetValue("event_4")));
	loginResponse.Write(LUString(Game::config->GetValue("event_5")));
	loginResponse.Write(LUString(Game::config->GetValue("event_6")));
	loginResponse.Write(LUString(Game::config->GetValue("event_7")));
	loginResponse.Write(LUString(Game::config->GetValue("event_8")));

	const uint16_t version_major =
		GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_major")).value_or(ClientVersion::major);
	const uint16_t version_current =
		GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_current")).value_or(ClientVersion::current);
	const uint16_t version_minor =
		GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_minor")).value_or(ClientVersion::minor);

	loginResponse.Write(version_major);
	loginResponse.Write(version_current);
	loginResponse.Write(version_minor);

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

	// Write the localization
	loginResponse.Write(LUString("US", 3));

	loginResponse.Write<uint8_t>(false); // Just upgraded from F2P
	loginResponse.Write<uint8_t>(false); // User is F2P
	loginResponse.Write<uint64_t>(0); // Time Remaining in F2P

	// Write custom error message
	loginResponse.Write<uint16_t>(errorMsg.length());
	loginResponse.Write(LUWString(errorMsg, static_cast<uint32_t>(errorMsg.length())));

	stamps.emplace_back(eStamps::PASSPORT_AUTH_WORLD_COMMUNICATION_FINISH, 1);

	loginResponse.Write<uint32_t>((sizeof(Stamp) * stamps.size()) + sizeof(uint32_t));
	for (auto& stamp : stamps) stamp.Serialize(loginResponse);

	server->Send(loginResponse, sysAddr, false);
	//Inform the master server that we've created a session for this user:
	if (responseCode == eLoginResponse::SUCCESS) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::SET_SESSION_KEY);
		bitStream.Write(sessionKey);
		bitStream.Write(LUString(username));
		server->SendToMaster(bitStream);

		LOG("Set sessionKey: %i for user %s", sessionKey, username.c_str());
	}
}
