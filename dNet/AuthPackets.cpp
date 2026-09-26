#include "AuthPackets.h"

#include <ctime>
#include "BitStreamUtils.h"

#include "dNetCommon.h"
#include "dServer.h"
#include "Logger.h"
#include "Database.h"
#include "ZoneInstanceManager.h"
#include "MD5.h"
#include "GeneralUtils.h"
#include "dClient/ClientVersion.h"

#include <bcrypt/BCrypt.hpp>

#include "BitStream.h"
#include <future>

#include "Game.h"
#include "dConfig.h"
#include "eServerDisconnectIdentifiers.h"
#include "eLoginResponse.h"
#include "ServiceType.h"
#include "MessageType/Server.h"
#include "MessageType/Master.h"
#include "eGameMasterLevel.h"
#include "StringifiedEnum.h"

#include <functional>
#include <map>
#include <memory>
namespace {
	std::vector<uint32_t> claimCodes;
}


void AuthPackets::LoadClaimCodes() {
	if(!claimCodes.empty()) return;
	auto rcstring = Game::config->GetValue("rewardcodes");
	auto codestrings = GeneralUtils::SplitString(rcstring, ',');
	for(auto const &codestring: codestrings){
		const auto code = GeneralUtils::TryParse<uint32_t>(codestring);

		if (code && code.value() != -1) claimCodes.push_back(code.value());
	}
}

std::string CleanReceivedString(const std::string& str) {
	std::string toReturn = str;
	const auto removed = std::ranges::find_if(toReturn, [](unsigned char c) { return isprint(c) == 0 && isblank(c) == 0; });
	toReturn.erase(removed, toReturn.end());
	return toReturn;
}

void AuthPackets::LoginRequest::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(username);
	bitStream.Write(password);
	bitStream.Write(localeID);
	bitStream.Write(clientOS);
	bitStream.Write(memoryStats);
	bitStream.Write(videoCard);
	bitStream.Write(numberOfProcessors);
	bitStream.Write(processorType);
	bitStream.Write(processorLevel);
	bitStream.Write(processorRevision);
	bitStream.Write(osVersionInfoSize);
	bitStream.Write(majorVersion);
	bitStream.Write(minorVersion);
	bitStream.Write(buildNumber);
	bitStream.Write(platformID);
}

bool AuthPackets::LoginRequest::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(username));
	VALIDATE_READ(bitStream.Read(password));
	VALIDATE_READ(bitStream.Read(localeID));
	VALIDATE_READ(bitStream.Read(clientOS));
	VALIDATE_READ(bitStream.Read(memoryStats));
	VALIDATE_READ(bitStream.Read(videoCard));
	VALIDATE_READ(bitStream.Read(numberOfProcessors));
	VALIDATE_READ(bitStream.Read(processorType));
	VALIDATE_READ(bitStream.Read(processorLevel));
	VALIDATE_READ(bitStream.Read(processorRevision));
	VALIDATE_READ(bitStream.Read(osVersionInfoSize));
	VALIDATE_READ(bitStream.Read(majorVersion));
	VALIDATE_READ(bitStream.Read(minorVersion));
	VALIDATE_READ(bitStream.Read(buildNumber));
	VALIDATE_READ(bitStream.Read(platformID));
	return true;
}

namespace {
	const std::map<MessageType::Auth, std::function<std::unique_ptr<AuthPackets::LoginRequest>()>> g_Handlers = {
		{ MessageType::Auth::LOGIN_REQUEST, []() { return std::make_unique<AuthPackets::LoginRequest>(); } },
	};
}

void AuthPackets::Handle(RakNet::BitStream& inStream, const SystemAddress& sysAddr, const uint32_t packetID) {
	const auto messageID = static_cast<MessageType::Auth>(packetID);
	const auto it = g_Handlers.find(messageID);
	if (it == g_Handlers.end()) {
		LOG_DEBUG("Unhandled auth packet %i", packetID);
		return;
	}

	auto request = it->second();
	request->sysAddr = sysAddr;
	if (!request->Deserialize(inStream)) {
		LOG("Failed to read auth packet %s", StringifiedEnum::ToString(messageID).data());
		return;
	}
	request->Handle();
}

void AuthPackets::LoginRequest::Handle() {
	auto* const server = Game::server;
	const auto& packet = *this; // the old handler's sysAddr

	// Each step of the login stamps itself here as it happens; the response carries them to the client
	Stamps stamps;
	stamps.Add(eStamps::PASSPORT_AUTH_START);

	const auto username = this->username.GetAsString();

	LOG_DEBUG("Locale ID: %s", StringifiedEnum::ToString(localeID).data());

	LOG_DEBUG("Operating System: %s", StringifiedEnum::ToString(clientOS).data());
	stamps.Add(eStamps::PASSPORT_AUTH_CLIENT_OS, static_cast<uint32_t>(clientOS));

	LOG_DEBUG("Memory Stats [%s]", CleanReceivedString(memoryStats.GetAsString()).c_str());

	LOG_DEBUG("VideoCard Info: [%s]", CleanReceivedString(videoCard.GetAsString()).c_str());

	// Processor/CPU info
	LOG_DEBUG("CPU Info: [#Processors: %i, Processor Type: %i, Processor Level: %i, Processor Revision: %i]", numberOfProcessors, processorType, processorLevel, processorRevision);

	// OS Info
	LOG_DEBUG("OS Info: [Size: %i, Major: %i, Minor %i, Buid#: %i, platformID: %i]", osVersionInfoSize, majorVersion, minorVersion, buildNumber, platformID);

	// Fetch account details
	stamps.Add(eStamps::PASSPORT_AUTH_DB_SELECT_START);
	auto accountInfo = Database::Get()->GetAccountInfo(username);
	stamps.Add(eStamps::PASSPORT_AUTH_DB_SELECT_FINISH, accountInfo ? 1 : 0);

	if (!accountInfo) {
		LOG("No user by name %s found!", username.c_str());
		stamps.Add(eStamps::PASSPORT_AUTH_ERROR, 1);
		AuthPackets::SendLoginResponse(server, sysAddr, eLoginResponse::INVALID_USER, "", "", 2001, username, stamps);
		return;
	}

	// The password first: someone who doesn't know it learns nothing about the account (ban details, lock, play key),
	// and a failed attempt changes nothing (an expired ban is only lifted for the real owner)
	stamps.Add(eStamps::PASSPORT_AUTH_LEGOINT_WEBSERVICE_START);
	const bool passwordMatches = ::bcrypt_checkpw(password.GetAsString().c_str(), accountInfo->bcryptPassword.c_str()) == 0;
	stamps.Add(eStamps::PASSPORT_AUTH_LEGOINT_WEBSERVICE_FINISH, passwordMatches ? 1 : 0);
	if (!passwordMatches) {
		stamps.Add(eStamps::PASSPORT_AUTH_ERROR, 1);
		AuthPackets::SendLoginResponse(server, sysAddr, eLoginResponse::WRONG_PASS, "", "", 2001, username, stamps);
		LOG("Wrong password used");
		return;
	}

	//If we aren't running in live mode, then only GMs are allowed to enter:
	if (Game::config->GetValue<bool>("closed_to_non_devs", false) && accountInfo->maxGmLevel == eGameMasterLevel::CIVILIAN) {
		stamps.Add(eStamps::GM_REQUIRED, 1);
		AuthPackets::SendLoginResponse(server, sysAddr, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "The server is currently only open to developers.", "", 2001, username, stamps);
		return;
	}

	if (Game::config->GetValue("dont_use_keys") != "1" && accountInfo->maxGmLevel == eGameMasterLevel::CIVILIAN) {
		//Check to see if we have a play key:
		if (accountInfo->playKeyId == 0) {
			stamps.Add(eStamps::PASSPORT_AUTH_ERROR, 1);
			AuthPackets::SendLoginResponse(server, sysAddr, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a play key associated with it!", "", 2001, username, stamps);
			LOG("User %s tried to log in, but they don't have a play key.", username.c_str());
			return;
		}

		//Check if the play key is _valid_:
		auto playKeyStatus = Database::Get()->IsPlaykeyActive(accountInfo->playKeyId);

		if (!playKeyStatus) {
			stamps.Add(eStamps::PASSPORT_AUTH_ERROR, 1);
			AuthPackets::SendLoginResponse(server, sysAddr, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a valid play key associated with it!", "", 2001, username, stamps);
			return;
		}

		if (!playKeyStatus.value()) {
			stamps.Add(eStamps::PASSPORT_AUTH_ERROR, 1);
			AuthPackets::SendLoginResponse(server, sysAddr, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your play key has been disabled.", "", 2001, username, stamps);
			LOG("User %s tried to log in, but their play key was disabled", username.c_str());
			return;
		}
	} else if (Game::config->GetValue("dont_use_keys") == "1" || accountInfo->maxGmLevel > eGameMasterLevel::CIVILIAN){
		stamps.Add(eStamps::PASSPORT_AUTH_BYPASS, 1);
	}

	// A temporary ban that has run out is lifted as the player logs in
	if (accountInfo->banned && accountInfo->banExpires > 0 && accountInfo->banExpires <= static_cast<int64_t>(std::time(nullptr))) {
		stamps.Add(eStamps::PASSPORT_AUTH_DB_INSERT_START);
		Database::Get()->SetAccountBan(accountInfo->id, false, 0, "");
		Database::Get()->InsertAccountNote({ 0, accountInfo->id, "unban", "Temporary ban ended", "[server]", static_cast<int64_t>(std::time(nullptr)) });
		stamps.Add(eStamps::PASSPORT_AUTH_DB_INSERT_FINISH, 1);
		accountInfo->banned = false;
		LOG("Temporary ban of %s ended", username.c_str());
	}

	if (accountInfo->banned) {
		stamps.Add(eStamps::PASSPORT_AUTH_ERROR, 1);
		std::string message;
		if (accountInfo->banExpires > 0) {
			char until[32];
			const std::time_t expires = accountInfo->banExpires;
			std::strftime(until, sizeof(until), "%Y-%m-%d %H:%M UTC", std::gmtime(&expires));
			message = std::string("You are banned until ") + until + ".";
		}
		if (!accountInfo->banReason.empty()) message += (message.empty() ? "" : " ") + std::string("Reason: ") + accountInfo->banReason;
		AuthPackets::SendLoginResponse(server, sysAddr, eLoginResponse::BANNED, message, "", 2001, username, stamps);
		return;
	}

	if (accountInfo->locked) {
		stamps.Add(eStamps::PASSPORT_AUTH_ERROR, 1);
		AuthPackets::SendLoginResponse(server, sysAddr, eLoginResponse::ACCOUNT_LOCKED, "", "", 2001, username, stamps);
		return;
	}

	{
		SystemAddress system = sysAddr; //Copy the sysAddr before the Packet gets destroyed from main

		// Where accounts log in from, so staff can see accounts that share a connection (log_login_addresses, on by default)
		if (Game::config->GetValue("log_login_addresses") != "0") {
			stamps.Add(eStamps::PASSPORT_AUTH_DB_INSERT_START);
			Database::Get()->RecordLoginAddress(accountInfo->id, system.ToString(false), static_cast<int64_t>(std::time(nullptr)));
			stamps.Add(eStamps::PASSPORT_AUTH_DB_INSERT_FINISH, 1);
		}

		if (!server->GetIsConnectedToMaster()) {
			stamps.Add(eStamps::PASSPORT_AUTH_WORLD_DISCONNECT, 1);
			AuthPackets::SendLoginResponse(server, system, eLoginResponse::GENERAL_FAILED, "", "", 0, username, stamps);
			return;
		}
		// Ask master for a world server to send the player to
		stamps.Add(eStamps::PASSPORT_AUTH_WORLD_COMMUNICATION_START);
		ZoneInstanceManager::Instance()->RequestZoneTransfer(server, 0, 0, false, [system, server, username, stamps](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string zoneIP, uint16_t zonePort) mutable {
			stamps.Add(eStamps::PASSPORT_AUTH_WORLD_PACKET_RECEIVED, zoneInstance);
			AuthPackets::SendLoginResponse(server, system, eLoginResponse::SUCCESS, "", zoneIP, zonePort, username, stamps);
			});
	}

	for(auto const code: claimCodes){
		Database::Get()->InsertRewardCode(accountInfo->id, code);
	}
}

void AuthPackets::SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username, Stamps& stamps) {
	ClientPackets::LoginResponse loginResponse;

	loginResponse.responseCode = responseCode;

	// Event Gating
	loginResponse.events[0] = LUString(Game::config->GetValue("event_1"));
	loginResponse.events[1] = LUString(Game::config->GetValue("event_2"));
	loginResponse.events[2] = LUString(Game::config->GetValue("event_3"));
	loginResponse.events[3] = LUString(Game::config->GetValue("event_4"));
	loginResponse.events[4] = LUString(Game::config->GetValue("event_5"));
	loginResponse.events[5] = LUString(Game::config->GetValue("event_6"));
	loginResponse.events[6] = LUString(Game::config->GetValue("event_7"));
	loginResponse.events[7] = LUString(Game::config->GetValue("event_8"));

	loginResponse.versionMajor =
		GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_major")).value_or(ClientVersion::major);
	loginResponse.versionCurrent =
		GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_current")).value_or(ClientVersion::current);
	loginResponse.versionMinor =
		GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_minor")).value_or(ClientVersion::minor);

	// The user key
	uint32_t sessionKey = GeneralUtils::GenerateRandomNumber<uint32_t>();
	std::string userHash = std::to_string(sessionKey);
	userHash = md5(userHash);
	loginResponse.userKey = LUWString(userHash);

	// World Server IP
	loginResponse.worldServerIP = LUString(wServerIP);
	// World Server Redirect port
	loginResponse.worldServerPort = wServerPort;

	// Custom error message
	loginResponse.errorMessage = errorMsg;

	//Inform the master server that we've created a session for this user, before the client can reach the world server:
	if (responseCode == eLoginResponse::SUCCESS) {
		stamps.Add(eStamps::PASSPORT_AUTH_IM_COMMUNICATION_START);
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::SET_SESSION_KEY);
		bitStream.Write(sessionKey);
		bitStream.Write(LUString(username));
		stamps.Add(eStamps::PASSPORT_AUTH_IM_LOGIN_START);
		server->SendToMaster(bitStream);
		stamps.Add(eStamps::PASSPORT_AUTH_IM_COMMUNICATION_END, 1);

		LOG("Set session key for user %s", username.c_str());

		stamps.Add(eStamps::PASSPORT_AUTH_WORLD_SESSION_CONFIRM_TO_AUTH, 1);
		stamps.Add(eStamps::PASSPORT_AUTH_WORLD_COMMUNICATION_FINISH, wServerPort);
	}

	stamps.Log("Login of " + username);
	loginResponse.stamps = stamps;
	loginResponse.Send(sysAddr);
}
