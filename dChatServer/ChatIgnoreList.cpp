#include "ChatIgnoreList.h"
#include "PlayerContainer.h"
#include "eChatInternalMessageType.h"
#include "BitStreamUtils.h"
#include "PacketUtils.h"
#include "Game.h"
#include "Logger.h"
#include "eObjectBits.h"

#include "Database.h"

// A note to future readers, The client handles all the actual ignoring logic:
// not allowing teams, rejecting DMs, friends requets etc.
// The only thing not auto-handled is instance activities force joining the team on the server.

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

	if (!receiver->ignoredPlayers.empty()) {
		LOG_DEBUG("Player %llu already has an ignore list", playerId);
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
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, ChatIgnoreList::Response::GET_IGNORE);

	bitStream.Write<uint8_t>(false); // Probably is Is Free Trial, but we don't care about that
	bitStream.Write<uint16_t>(0); // literally spacing due to struct alignment

	bitStream.Write<uint16_t>(receiver->ignoredPlayers.size());
	for (const auto& ignoredPlayer : receiver->ignoredPlayers) {
		bitStream.Write(ignoredPlayer.playerId);
		bitStream.Write(LUWString(ignoredPlayer.playerName, 36));
	}

	Game::server->Send(&bitStream, packet->systemAddress, false);
}

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

	constexpr int32_t MAX_IGNORES = 32;
	if (receiver->ignoredPlayers.size() > MAX_IGNORES) {
		LOG_DEBUG("Player %llu has too many ignores", playerId);
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
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, ChatIgnoreList::Response::ADD_IGNORE);

	// Check if the player exists
	LWOOBJID ignoredPlayerId = LWOOBJID_EMPTY;
	if (toIgnoreStr == receiver->playerName || toIgnoreStr.find("[GM]") == 0) {
		LOG_DEBUG("Player %llu tried to ignore themselves", playerId);

		bitStream.Write(ChatIgnoreList::AddResponse::GENERAL_ERROR);
	} else if (std::count(receiver->ignoredPlayers.begin(), receiver->ignoredPlayers.end(), toIgnoreStr) > 0) {
		LOG_DEBUG("Player %llu is already ignoring %s", playerId, toIgnoreStr.c_str());

		bitStream.Write(ChatIgnoreList::AddResponse::ALREADY_IGNORED);
	} else {
		// Get the playerId falling back to query if not online
		auto* playerData = Game::playerContainer.GetPlayerData(toIgnoreStr);
		if (!playerData) {
			// Fall back to query
			auto player = Database::Get()->GetCharacterInfo(toIgnoreStr);
			if (!player || player->name != toIgnoreStr) {
				LOG_DEBUG("Player %s not found", toIgnoreStr.c_str());
			} else {
				ignoredPlayerId = player->id;
			}
		} else {
			ignoredPlayerId = playerData->playerID;
		}

		if (ignoredPlayerId != LWOOBJID_EMPTY) {
			Database::Get()->AddIgnore(static_cast<uint32_t>(playerId), static_cast<uint32_t>(ignoredPlayerId));
			GeneralUtils::SetBit(ignoredPlayerId, eObjectBits::CHARACTER);
			GeneralUtils::SetBit(ignoredPlayerId, eObjectBits::PERSISTENT);

			receiver->ignoredPlayers.push_back(IgnoreData{ ignoredPlayerId, toIgnoreStr });
			LOG_DEBUG("Player %llu is ignoring %s", playerId, toIgnoreStr.c_str());

			bitStream.Write(ChatIgnoreList::AddResponse::SUCCESS);
		} else {
			bitStream.Write(ChatIgnoreList::AddResponse::PLAYER_NOT_FOUND);
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
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, ChatIgnoreList::Response::REMOVE_IGNORE);

	bitStream.Write<int8_t>(0);
	LUWString playerNameSend(removedIgnoreStr, 33);
	bitStream.Write(playerNameSend);

	Game::server->Send(&bitStream, packet->systemAddress, false);
}
