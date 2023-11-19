#include "ChatIgnoreList.h"
#include "PlayerContainer.h"
#include "eChatInternalMessageType.h"
#include "BitStreamUtils.h"
#include "PacketUtils.h"
#include "Game.h"
#include "Logger.h"
#include "eObjectBits.h"

#include "Database.h"

enum IgnoreReponse : uint8_t {
	AddIgnoreResponse = 32,
	RemoveIgnoreResponse = 33,
	GetIgnoreListResponse = 34,
};

void ChatIgnoreList::GetIgnoreList(Packet* packet) {
	LOG_DEBUG("Getting ignore list");

	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerId;
	inStream.Read(playerId);

	auto* receiver = Game::playerContainer.GetPlayerData(playerId);
	if (!receiver) {
		LOG("Tried to get ignore list, but player %llu not found in container", playerId);
		return;
	}

	auto ignoreList = Database::Get()->GetIgnoreList(static_cast<uint32_t>(playerId));
	if (ignoreList.empty()) {
		LOG_DEBUG("Player %llu has no ignores", playerId);
		return;
	}

	for (auto& ignoredPlayer : ignoreList) {
		receiver->ignoredPlayers.push_back(IgnoreData{ ignoredPlayer.id, ignoredPlayer.name });
		GeneralUtils::SetBit(receiver->ignoredPlayers.back().playerId, eObjectBits::CHARACTER);
		GeneralUtils::SetBit(receiver->ignoredPlayers.back().playerId, eObjectBits::PERSISTENT);
	}

	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, IgnoreReponse::GetIgnoreListResponse);
	bitStream.Write<uint8_t>(false); // Probably is Is Free Trial, but we don't care about that
	bitStream.Write<uint16_t>(0); // literally spacing due to struct alignment
	bitStream.Write<uint16_t>(receiver->ignoredPlayers.size());
	for (const auto& ignoredPlayer : receiver->ignoredPlayers) {
		bitStream.Write(ignoredPlayer.playerId);
		bitStream.Write(LUWString(ignoredPlayer.playerName, 36));
	}
	Game::server->Send(&bitStream, packet->systemAddress, false);
}

// Remove from ignore list and response
// Not much else to do with editing the list, maybe more messages are needed for refreshes or something
// but you can only add or remove from the list, and you only request the list on world start so pretty small file
// After the above all work, move to implement the actual ignore functionality in the chat server
enum class IgnoreResponse : uint8_t {
	SUCCESS,
	ALREADY_IGNORED,
	PLAYER_NOT_FOUND,
	GENERAL_ERROR,
};

void ChatIgnoreList::AddIgnore(Packet* packet) {
	LOG_DEBUG("Adding ignore");

	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerId;
	inStream.Read(playerId);

	auto* receiver = Game::playerContainer.GetPlayerData(playerId);
	if (!receiver) {
		LOG("Tried to get ignore list, but player %llu not found in container", playerId);
		return;
	}

	inStream.IgnoreBytes(4); // ignore some garbage zeros idk

	LUWString toIgnoreName(33);
	inStream.Read(toIgnoreName);
	std::string toIgnoreStr = toIgnoreName.GetAsString();

	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, IgnoreReponse::AddIgnoreResponse);

	// Check if the player exists
	LWOOBJID ignoredPlayerId = LWOOBJID_EMPTY;
	if (toIgnoreStr == receiver->playerName || toIgnoreStr.find("[GM]") == 0) {
		LOG_DEBUG("Player %llu tried to ignore themselves", playerId);

		bitStream.Write(IgnoreResponse::GENERAL_ERROR);
	} else if (std::count(receiver->ignoredPlayers.begin(), receiver->ignoredPlayers.end(), toIgnoreStr) > 0) {
		LOG_DEBUG("Player %llu is already ignoring %s", playerId, toIgnoreStr.c_str());

		bitStream.Write(IgnoreResponse::ALREADY_IGNORED);
	} else {
		auto* playerData = Game::playerContainer.GetPlayerData(toIgnoreStr);
		if (!playerData) {
			// Fall back to query
			auto player = Database::Get()->GetCharacterInfo(toIgnoreStr);
			if (!player || player->name != toIgnoreStr) {
				LOG_DEBUG("Player %s not found", toIgnoreStr.c_str());

				bitStream.Write(IgnoreResponse::PLAYER_NOT_FOUND);
			} else {
				ignoredPlayerId = player->id;
				Database::Get()->AddIgnore(static_cast<uint32_t>(playerId), static_cast<uint32_t>(ignoredPlayerId));
				GeneralUtils::SetBit(ignoredPlayerId, eObjectBits::CHARACTER);
				GeneralUtils::SetBit(ignoredPlayerId, eObjectBits::PERSISTENT);

				receiver->ignoredPlayers.push_back(IgnoreData{ ignoredPlayerId, toIgnoreStr });
				LOG_DEBUG("Player %llu is ignoring %s", playerId, toIgnoreStr.c_str());

				bitStream.Write(IgnoreResponse::SUCCESS);
			}
		}
	}

	LUWString playerNameSend(toIgnoreStr, 33);
	bitStream.Write(playerNameSend);
	bitStream.Write(ignoredPlayerId);

	Game::server->Send(&bitStream, packet->systemAddress, false);
}

void ChatIgnoreList::RemoveIgnore(Packet* packet) {
	LOG_DEBUG("Removing ignore");

	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerId;
	inStream.Read(playerId);

	auto* receiver = Game::playerContainer.GetPlayerData(playerId);
	if (!receiver) {
		LOG("Tried to get ignore list, but player %llu not found in container", playerId);
		return;
	}

	inStream.IgnoreBytes(4); // ignore some garbage zeros idk

	LUWString removedIgnoreName(33);
	inStream.Read(removedIgnoreName);
	std::string removedIgnoreStr = removedIgnoreName.GetAsString();

	LOG("Removing ignore for %s", removedIgnoreStr.c_str());
	auto toRemove = std::remove(receiver->ignoredPlayers.begin(), receiver->ignoredPlayers.end(), removedIgnoreStr);
	if (toRemove == receiver->ignoredPlayers.end()) {
		LOG_DEBUG("Player %llu is not ignoring %s", playerId, removedIgnoreStr.c_str());
		return;
	}

	Database::Get()->RemoveIgnore(static_cast<uint32_t>(playerId), static_cast<uint32_t>(toRemove->playerId));
	receiver->ignoredPlayers.erase(toRemove, receiver->ignoredPlayers.end());
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);

	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, IgnoreReponse::RemoveIgnoreResponse);

	bitStream.Write<int8_t>(0);
	LUWString playerNameSend(removedIgnoreStr, 33);
	bitStream.Write(playerNameSend);

	Game::server->Send(&bitStream, packet->systemAddress, false);
}
