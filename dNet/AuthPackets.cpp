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
#include "eServerDisconnectIdentifiers.h"
#include "eLoginResponse.h"
#include "ServiceType.h"
#include "MessageType/Server.h"
#include "MessageType/Master.h"
#include "eGameMasterLevel.h"
#include "StringifiedEnum.h"

namespace {
	std::vector<uint32_t> claimCodes;
}

void Stamp::Serialize(RakNet::BitStream& outBitStream) {
	outBitStream.Write(type);
	outBitStream.Write(value);
	outBitStream.Write(timestamp);
};

namespace AuthPackets {
	std::map<eMessageID, std::function<std::unique_ptr<AuthLUBitStream>()>> g_Handlers = {
		{eMessageID::HandshakeRequest, []() {
			return std::make_unique<HandshakeRequest>();
		}},
		{eMessageID::LoginRequest, []() {
			return std::make_unique<LoginRequest>();
		}},
	};

	void AuthLUBitStream::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(messageID);
	}

	bool AuthLUBitStream::Deserialize(RakNet::BitStream& bitstream) {
		VALIDATE_READ(bitstream.Read(messageID));
		return true;
	}

	bool HandshakeRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(clientVersion));
		bitStream.IgnoreBytes(4);
		VALIDATE_READ(bitStream.Read(serviceType));
		if (serviceType != ServiceType::CLIENT) LOG("WARNING: Service is not a Client!");
		bitStream.IgnoreBytes(2);
		VALIDATE_READ(bitStream.Read(processID));
		VALIDATE_READ(bitStream.Read(port));
		if (port != sysAddr.port) LOG("WARNING: Port written in packet does not match the port the client is connecting over!");
		bitStream.IgnoreBytes(33);
		return true;
	}

	void HandshakeRequest::Handle() {
		LOG_DEBUG("Client Data [Version: %i, Service: %s, Process: %u, Port: %u, Sysaddr Port: %u]", 
			clientVersion, StringifiedEnum::ToString(serviceType).data(), processID, port, sysAddr.port);
		
		HandshakeResponse response(server->GetIP(), server->GetPort(), server->GetServerType());
		response.server = server;
		response.Send(sysAddr);
	}

	void HandshakeResponse::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::COMMON, MessageType::Server::VERSION_CONFIRM);

		const auto& clientNetVersionString = Game::config->GetValue("client_net_version");
		const uint32_t clientNetVersion = GeneralUtils::TryParse<uint32_t>(clientNetVersionString).value_or(171022);

		bitStream.Write<uint32_t>(clientNetVersion);
		bitStream.Write<uint32_t>(861228100);
		bitStream.Write(static_cast<uint32_t>(serverType));
		bitStream.Write<uint64_t>(219818307120);
	}

	std::string CleanReceivedString(const std::string& str) {
		std::string toReturn = str;
		const auto removed = std::ranges::find_if(toReturn, [](unsigned char c) { return isprint(c) == 0 && isblank(c) == 0; });
		toReturn.erase(removed, toReturn.end());
		return toReturn;
	}

	bool LoginRequest::Deserialize(RakNet::BitStream& bitStream) {
		LUWString usernameLUString;
		VALIDATE_READ(bitStream.Read(usernameLUString));
		username = usernameLUString.GetAsString();

		LUWString passwordLUString(41);
		VALIDATE_READ(bitStream.Read(passwordLUString));
		password = passwordLUString.GetAsString();

		VALIDATE_READ(bitStream.Read(locale_id));
		LOG_DEBUG("Locale ID: %s", StringifiedEnum::ToString(locale_id).data());

		VALIDATE_READ(bitStream.Read(clientOS));
		LOG_DEBUG("Operating System: %s", StringifiedEnum::ToString(clientOS).data());


		LUWString memoryStatsLU(256);
		VALIDATE_READ(bitStream.Read(memoryStatsLU));
		computerInfo.memoryStats = CleanReceivedString(memoryStatsLU.GetAsString());
		LOG_DEBUG("Memory Stats [%s]", computerInfo.memoryStats.c_str());

		LUWString videoCardLU(128);
		VALIDATE_READ(bitStream.Read(videoCardLU));
		computerInfo.videoCard = CleanReceivedString(videoCardLU.GetAsString());
		LOG_DEBUG("VideoCard Info: [%s]", computerInfo.videoCard.c_str());

		// Processor/CPU info
		VALIDATE_READ(bitStream.Read(computerInfo.processorInfo.count));
		VALIDATE_READ(bitStream.Read(computerInfo.processorInfo.type));
		VALIDATE_READ(bitStream.Read(computerInfo.processorInfo.level));
		VALIDATE_READ(bitStream.Read(computerInfo.processorInfo.revision));
		LOG_DEBUG("CPU Info: [#Processors: %i, Processor Type: %s, Processor Level: %s, Processor Revision: %s]", 
			computerInfo.processorInfo.count,
			StringifiedEnum::ToString(computerInfo.processorInfo.type).data(),
			StringifiedEnum::ToString(computerInfo.processorInfo.level).data(),
			StringifiedEnum::ToString(computerInfo.processorInfo.revision).data()
		);

		// OS Info
		VALIDATE_READ(bitStream.Read(computerInfo.osVersionInfo.infoSize));
		VALIDATE_READ(bitStream.Read(computerInfo.osVersionInfo.version));
		VALIDATE_READ(bitStream.Read(computerInfo.osVersionInfo.buildNumber));
		VALIDATE_READ(bitStream.Read(computerInfo.osVersionInfo.platformID));
		LOG_DEBUG("OS Info: [Size: %i, Version: %s, Buid#: %s, platformID: %s]", 
			computerInfo.osVersionInfo.infoSize,
			StringifiedEnum::ToString(computerInfo.osVersionInfo.version).data(),
			StringifiedEnum::ToString(computerInfo.osVersionInfo.buildNumber).data(),
			StringifiedEnum::ToString(computerInfo.osVersionInfo.platformID).data()
		);

		return true;
	}

	void LoginRequest::Handle() {
		std::vector<Stamp> stamps;
		stamps.emplace_back(eStamps::PASSPORT_AUTH_START, 0);
		stamps.emplace_back(eStamps::PASSPORT_AUTH_CLIENT_OS, 0);

		// Fetch account details
		auto accountInfo = Database::Get()->GetAccountInfo(username);

		if (!accountInfo) {
			LOG("No user by name %s found!", username.c_str());
			stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
			LoginResponse response(eLoginResponse::INVALID_USER, "", "", 2001, username, stamps);
			response.server = server;
			response.Send(sysAddr);
			return;
		}

		//If we aren't running in live mode, then only GMs are allowed to enter:
		const auto& closedToNonDevs = Game::config->GetValue("closed_to_non_devs");
		if (closedToNonDevs.size() > 0 && bool(std::stoi(closedToNonDevs)) && accountInfo->maxGmLevel == eGameMasterLevel::CIVILIAN) {
			stamps.emplace_back(eStamps::GM_REQUIRED, 1);
			LoginResponse response(eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "The server is currently only open to developers.", "", 2001, username, stamps);
			response.server = server;
			response.Send(sysAddr);
			return;
		}

		if (Game::config->GetValue("dont_use_keys") != "1" && accountInfo->maxGmLevel == eGameMasterLevel::CIVILIAN) {
			//Check to see if we have a play key:
			if (accountInfo->playKeyId == 0) {
				stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
				LoginResponse response(eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a play key associated with it!", "", 2001, username, stamps);
				response.server = server;
				response.Send(sysAddr);
				LOG("User %s tried to log in, but they don't have a play key.", username.c_str());
				return;
			}

			//Check if the play key is _valid_:
			auto playKeyStatus = Database::Get()->IsPlaykeyActive(accountInfo->playKeyId);

			if (!playKeyStatus) {
				stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
				LoginResponse response(eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your account doesn't have a valid play key associated with it!", "", 2001, username, stamps);
				response.server = server;
				response.Send(sysAddr);
				return;
			}

			if (!playKeyStatus.value()) {
				stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
				LoginResponse response(eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, "Your play key has been disabled.", "", 2001, username, stamps);
				response.server = server;
				response.Send(sysAddr);
				LOG("User %s tried to log in, but their play key was disabled", username.c_str());
				return;
			}
		} else if (Game::config->GetValue("dont_use_keys") == "1" || accountInfo->maxGmLevel > eGameMasterLevel::CIVILIAN) {
			stamps.emplace_back(eStamps::PASSPORT_AUTH_BYPASS, 1);
		}

		if (accountInfo->banned) {
			stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
			LoginResponse response(eLoginResponse::BANNED, "", "", 2001, username, stamps);
			response.server = server;
			response.Send(sysAddr);
			return;
		}

		if (accountInfo->locked) {
			stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
			LoginResponse response(eLoginResponse::ACCOUNT_LOCKED, "", "", 2001, username, stamps);
			response.server = server;
			response.Send(sysAddr);
			return;
		}

		bool loginSuccess = ::bcrypt_checkpw(password.c_str(), accountInfo->bcryptPassword.c_str()) == 0;

		if (!loginSuccess) {
			stamps.emplace_back(eStamps::PASSPORT_AUTH_ERROR, 1);
			LoginResponse response(eLoginResponse::WRONG_PASS, "", "", 2001, username, stamps);
			response.server = server;
			response.Send(sysAddr);
			LOG("Wrong password used");
		} else {
			SystemAddress system = sysAddr; //Copy the sysAddr
			auto serverPtr = server;
			auto usernameCopy = username;
			ZoneInstanceManager::Instance()->RequestZoneTransfer(server, 0, 0, false, [system, serverPtr, usernameCopy, stamps](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string zoneIP, uint16_t zonePort) mutable {
				LoginResponse response(eLoginResponse::SUCCESS, "", zoneIP, zonePort, usernameCopy, stamps);
				response.server = serverPtr;
				response.Send(system);
				return;
				}
			);

			for (auto const code : claimCodes) {
				Database::Get()->InsertRewardCode(accountInfo->id, code);
			}
		}
	}

	void LoginResponse::Serialize(RakNet::BitStream& bitStream) const {
		std::vector<Stamp> mutableStamps = stamps;
		mutableStamps.emplace_back(eStamps::PASSPORT_AUTH_IM_LOGIN_START, 1);
		
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::LOGIN_RESPONSE);

		bitStream.Write(responseCode);

		// Event Gating
		bitStream.Write(LUString(Game::config->GetValue("event_1")));
		bitStream.Write(LUString(Game::config->GetValue("event_2")));
		bitStream.Write(LUString(Game::config->GetValue("event_3")));
		bitStream.Write(LUString(Game::config->GetValue("event_4")));
		bitStream.Write(LUString(Game::config->GetValue("event_5")));
		bitStream.Write(LUString(Game::config->GetValue("event_6")));
		bitStream.Write(LUString(Game::config->GetValue("event_7")));
		bitStream.Write(LUString(Game::config->GetValue("event_8")));

		const uint16_t version_major =
			GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_major")).value_or(ClientVersion::major);
		const uint16_t version_current =
			GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_current")).value_or(ClientVersion::current);
		const uint16_t version_minor =
			GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("version_minor")).value_or(ClientVersion::minor);

		bitStream.Write(version_major);
		bitStream.Write(version_current);
		bitStream.Write(version_minor);

		// Writes the user key
		uint32_t sessionKey = GeneralUtils::GenerateRandomNumber<uint32_t>();
		std::string userHash = std::to_string(sessionKey);
		userHash = md5(userHash);
		bitStream.Write(LUWString(userHash));

		// World Server IP
		bitStream.Write(LUString(wServerIP));
		// Chat Server IP (unused)
		bitStream.Write(LUString(""));

		// World Server Redirect port
		bitStream.Write(wServerPort);
		// Char Server Redirect port (unused)
		bitStream.Write(static_cast<uint16_t>(0));

		// CDN Key
		bitStream.Write(LUString(""));

		// CDN Ticket
		bitStream.Write(LUString("00000000-0000-0000-0000-000000000000", 37));

		// Language
		bitStream.Write(Language::en_US);

		// Write the localization
		bitStream.Write(LUString("US", 3));

		bitStream.Write<uint8_t>(false); // Just upgraded from F2P
		bitStream.Write<uint8_t>(false); // User is F2P
		bitStream.Write<uint64_t>(0); // Time Remaining in F2P

		// Write custom error message
		bitStream.Write<uint16_t>(errorMsg.length());
		bitStream.Write(LUWString(errorMsg, static_cast<uint32_t>(errorMsg.length())));

		mutableStamps.emplace_back(eStamps::PASSPORT_AUTH_WORLD_COMMUNICATION_FINISH, 1);

		bitStream.Write<uint32_t>((sizeof(Stamp) * mutableStamps.size()) + sizeof(uint32_t));
		for (auto& stamp : mutableStamps) stamp.Serialize(bitStream);

		//Inform the master server that we've created a session for this user:
		if (responseCode == eLoginResponse::SUCCESS && server) {
			uint32_t masterSessionKey = GeneralUtils::GenerateRandomNumber<uint32_t>();
			RakNet::BitStream masterBitStream;
			BitStreamUtils::WriteHeader(masterBitStream, ServiceType::MASTER, MessageType::Master::SET_SESSION_KEY);
			masterBitStream.Write(masterSessionKey);
			masterBitStream.Write(LUString(username));
			server->SendToMaster(masterBitStream);

			LOG("Set sessionKey: %i for user %s", masterSessionKey, username.c_str());
		}
	}



	void LoadClaimCodes() {
		if(!claimCodes.empty()) return;
		auto rcstring = Game::config->GetValue("rewardcodes");
		auto codestrings = GeneralUtils::SplitString(rcstring, ',');
		for(auto const &codestring: codestrings){
			const auto code = GeneralUtils::TryParse<uint32_t>(codestring);

			if (code && code.value() != -1) claimCodes.push_back(code.value());
		}
	}
}

// Legacy wrapper functions for backward compatibility
void AuthPackets::HandleHandshake(dServer* server, Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	
	HandshakeRequest request;
	request.sysAddr = packet->systemAddress;
	request.server = server;
	
	if (request.Deserialize(inStream)) {
		request.Handle();
	} else {
		LOG_DEBUG("Error Reading Handshake Request");
	}
}

void AuthPackets::SendHandshake(dServer* server, const SystemAddress& sysAddr, const std::string& nextServerIP, uint16_t nextServerPort, const ServiceType serverType) {
	HandshakeResponse response(nextServerIP, nextServerPort, serverType);
	response.server = server;
	response.Send(sysAddr);
}

void AuthPackets::HandleLoginRequest(dServer* server, Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	
	LoginRequest request;
	request.sysAddr = packet->systemAddress;
	request.server = server;
	
	if (request.Deserialize(inStream)) {
		request.Handle();
	} else {
		LOG_DEBUG("Error Reading Login Request");
	}
}

void AuthPackets::SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username, std::vector<Stamp>& stamps) {
	LoginResponse response(responseCode, errorMsg, wServerIP, wServerPort, username, stamps);
	response.server = server;
	response.Send(sysAddr);
}

// Non Struct Functions
void AuthPackets::HandleAuth(RakNet::BitStream& inStream, const SystemAddress& sysAddr, dServer* server) {
	AuthLUBitStream data;
	if (!data.Deserialize(inStream)) {
		LOG_DEBUG("Error Reading Auth header");
		return;
	}

	auto it = g_Handlers.find(data.messageID);
	if (it != g_Handlers.end()) {
		auto request = it->second();
		request->sysAddr = sysAddr;
		request->server = server;
		if (!request->Deserialize(inStream)) {
			LOG_DEBUG("Error Reading Auth Request: %s", StringifiedEnum::ToString(data.messageID).data());
			return;
		}
		LOG("Received auth message %s", StringifiedEnum::ToString(data.messageID).data());
		request->Handle();
	} else {
		LOG_DEBUG("Unhandled Auth Request with ID: %i", data.messageID);
	}
}


