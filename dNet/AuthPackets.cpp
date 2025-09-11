#include "AuthPackets.h"
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
#include "ServiceType.h"
#include "MessageType/Server.h"
#include "MessageType/Master.h"
#include "eGameMasterLevel.h"
#include "StringifiedEnum.h"
#include "CommonPackets.h"
#include "ClientPackets.h"

namespace {
	std::vector<uint32_t> claimCodes;
}

namespace AuthPackets {
	std::map<MessageType::Auth, std::function<std::unique_ptr<LUBitStream>()>> g_Handlers = {
		{MessageType::Auth::LOGIN_REQUEST, []() {
			return std::make_unique<LoginRequest>();
		}}
	};

	bool LoginRequest::Deserialize(RakNet::BitStream& bitStream) {
		LUWString usernameLUString;
		VALIDATE_READ(bitStream.Read(usernameLUString));
		username = usernameLUString.GetAsString();

		LUWString passwordLUString(41);
		VALIDATE_READ(bitStream.Read(passwordLUString));
		password = passwordLUString.GetAsString();
		VALIDATE_READ(bitStream.Read(locale_id));
		VALIDATE_READ(bitStream.Read(clientOS));

		LUWString memoryStatsLU(256);
		VALIDATE_READ(bitStream.Read(memoryStatsLU));
		computerInfo.memoryStats = memoryStatsLU.GetAsString();

		LUWString videoCardLU(128);
		VALIDATE_READ(bitStream.Read(videoCardLU));
		computerInfo.videoCard = videoCardLU.GetAsString();

		// Processor/CPU info
		VALIDATE_READ(bitStream.Read(computerInfo.processorInfo.count));
		VALIDATE_READ(bitStream.Read(computerInfo.processorInfo.type));
		VALIDATE_READ(bitStream.Read(computerInfo.processorInfo.level));
		VALIDATE_READ(bitStream.Read(computerInfo.processorInfo.revision));

		// OS Info
		VALIDATE_READ(bitStream.Read(computerInfo.osVersionInfo.infoSize));
		VALIDATE_READ(bitStream.Read(computerInfo.osVersionInfo.majorVersion));
		VALIDATE_READ(bitStream.Read(computerInfo.osVersionInfo.minorVersion));
		VALIDATE_READ(bitStream.Read(computerInfo.osVersionInfo.buildNumber));
		VALIDATE_READ(bitStream.Read(computerInfo.osVersionInfo.platformID));

		return true;
	}

	void LoginRequest::Handle() {
		LOG_DEBUG("Login Request from %s", username.c_str());
		LOG_DEBUG("Password: %s", password.c_str());
		
		LOG_DEBUG("Locale ID: %s", StringifiedEnum::ToString(locale_id).data());

		LOG_DEBUG("Operating System: %s", StringifiedEnum::ToString(clientOS).data());

		LOG_DEBUG("Memory Stats [%s]", computerInfo.memoryStats.c_str());

		LOG_DEBUG("VideoCard Info: [%s]", computerInfo.videoCard.c_str());

		LOG_DEBUG("CPU Info: [#Processors: %i, Processor Type: %i, Processor Level: %i, Processor Revision: %i]",
			computerInfo.processorInfo.count,
			computerInfo.processorInfo.type,
			computerInfo.processorInfo.level,
			computerInfo.processorInfo.revision
		);

		LOG_DEBUG("OS Info: [Size: %i, Version: %i.%i, Buid#: %i, platformID: %i]",
			computerInfo.osVersionInfo.infoSize,
			computerInfo.osVersionInfo.majorVersion,
			computerInfo.osVersionInfo.minorVersion,
			computerInfo.osVersionInfo.buildNumber,
			computerInfo.osVersionInfo.platformID
		);

		ClientPackets::LoginResponse response;
		response.sysAddr = this->sysAddr;
		response.stamps.emplace_back(eStamps::PASSPORT_AUTH_START, 0);
		response.stamps.emplace_back(eStamps::PASSPORT_AUTH_CLIENT_OS, 0);
		response.events.push_back(LUString(Game::config->GetValue("event_1")));
		response.events.push_back(LUString(Game::config->GetValue("event_2")));
		response.events.push_back(LUString(Game::config->GetValue("event_3")));
		response.events.push_back(LUString(Game::config->GetValue("event_4")));
		response.events.push_back(LUString(Game::config->GetValue("event_5")));
		response.events.push_back(LUString(Game::config->GetValue("event_6")));
		response.events.push_back(LUString(Game::config->GetValue("event_7")));
		response.events.push_back(LUString(Game::config->GetValue("event_8")));
		response.version_major = GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_major")).value_or(ClientVersion::major);
		response.version_current = GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_current")).value_or(ClientVersion::current);
		response.version_minor = GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_minor")).value_or(ClientVersion::minor);

		uint32_t sessionKey = GeneralUtils::GenerateRandomNumber<uint32_t>();
		std::string userHash = std::to_string(sessionKey);
		response.userKey = md5(userHash);

		// Fetch account details
		auto accountInfo = Database::Get()->GetAccountInfo(username);

		if (!accountInfo) {
			LOG("No user by name %s found!", username.c_str());
			response.stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
			response.Send(sysAddr);
			return;
		}

		//If we aren't running in live mode, then only GMs are allowed to enter:
		const auto& closedToNonDevs = Game::config->GetValue("closed_to_non_devs");
		if (closedToNonDevs.size() > 0 && bool(std::stoi(closedToNonDevs)) && accountInfo->maxGmLevel == eGameMasterLevel::CIVILIAN) {
			response.stamps.emplace_back(eStamps::GM_REQUIRED, 1);
			response.Send(sysAddr);
			return;
		}

		if (Game::config->GetValue("dont_use_keys") != "1" && accountInfo->maxGmLevel == eGameMasterLevel::CIVILIAN) {
			//Check to see if we have a play key:
			if (accountInfo->playKeyId == 0) {
				response.stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
				response.responseCode = eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH;
				response.errorMessage = "Your account doesn't have a play key associated with it!";
				response.Send(sysAddr);
				LOG("User %s tried to log in, but they don't have a play key.", username.c_str());
				return;
			}

			//Check if the play key is _valid_:
			auto playKeyStatus = Database::Get()->IsPlaykeyActive(accountInfo->playKeyId);

			if (!playKeyStatus) {
				response.stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
				response.responseCode = eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH;
				response.errorMessage = "Your account doesn't have a valid play key associated with it!";
				response.Send(sysAddr);
				return;
			}

			if (!playKeyStatus.value()) {
				response.stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
				response.responseCode = eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH;
				response.errorMessage = "Your play key has been disabled.";
				response.Send(sysAddr);
				LOG("User %s tried to log in, but their play key was disabled", username.c_str());
				return;
			}
		} else if (Game::config->GetValue("dont_use_keys") == "1" || accountInfo->maxGmLevel > eGameMasterLevel::CIVILIAN) {
			response.stamps.emplace_back(eStamps::PASSPORT_AUTH_BYPASS, 1);
		}

		if (accountInfo->banned) {
			response.stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
			response.responseCode = eLoginResponse::BANNED;
			response.errorMessage = "Your account has been banned.";
			response.Send(sysAddr);
			return;
		}

		if (accountInfo->locked) {
			response.stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
			response.responseCode = eLoginResponse::ACCOUNT_LOCKED;
			response.errorMessage = "Your account is locked.";
			response.Send(sysAddr);
			return;
		}

		bool loginSuccess = ::bcrypt_checkpw(password.c_str(), accountInfo->bcryptPassword.c_str()) == 0;

		if (!loginSuccess) {
			response.stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
			response.responseCode = eLoginResponse::WRONG_PASS;
			response.Send(sysAddr);
		} else {
			ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, 0, 0, false, [this, response, sessionKey](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string zoneIP, uint16_t zonePort) mutable {
				response.responseCode = eLoginResponse::SUCCESS;
				response.worldServerIP = LUString(zoneIP);
				response.worldServerPort = zonePort;
				response.Send();

				CBITSTREAM;
				BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::SET_SESSION_KEY);
				bitStream.Write(sessionKey);
				LOG_DEBUG("Sending session key for %s to master server", this->username.c_str());
				bitStream.Write(LUString(this->username));
				Game::server->SendToMaster(bitStream);
			});

			for (auto const code : claimCodes) {
				Database::Get()->InsertRewardCode(accountInfo->id, code);
			}
		}
	}
}

// Non Stuct Functions
void AuthPackets::LoadClaimCodes() {
	if (!claimCodes.empty()) return;
	auto rcstring = Game::config->GetValue("rewardcodes");
	auto codestrings = GeneralUtils::SplitString(rcstring, ',');
	for (auto const& codestring : codestrings) {
		const auto code = GeneralUtils::TryParse<uint32_t>(codestring);

		if (code && code.value() != -1) claimCodes.push_back(code.value());
	}
}

// Non Stuct Functions
void AuthPackets::Handle(RakNet::BitStream& inStream, const SystemAddress& sysAddr) {
	inStream.ResetReadPointer();
	LUBitStream lubitstream;
	if (!lubitstream.ReadHeader(inStream)) return;

	auto it = g_Handlers.find(static_cast<MessageType::Auth>(lubitstream.internalPacketID));
	if (it != g_Handlers.end()) {
		auto request = it->second();
		request->sysAddr = sysAddr;
		if (!request->Deserialize(inStream)) {
			LOG_DEBUG("Error Reading Auth Packet: %s", StringifiedEnum::ToString(static_cast<MessageType::Auth>(lubitstream.internalPacketID)).data());
			return;
		}
		LOG_DEBUG("Received Auth Packet: %s", StringifiedEnum::ToString(static_cast<MessageType::Auth>(lubitstream.internalPacketID)).data());
		request->Handle();
	} else {
		LOG_DEBUG("Unhandled Auth Packet with ID: %i", lubitstream.internalPacketID);
	}
}

