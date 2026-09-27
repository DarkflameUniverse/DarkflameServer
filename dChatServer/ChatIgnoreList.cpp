#include "ChatIgnoreList.h"
#include "PlayerContainer.h"
#include "MessageType/Chat.h"
#include "BitStreamUtils.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"
#include "eObjectBits.h"

#include "Database.h"
#include "ChatPacketHandler.h"

// A note to future readers, The client handles all the actual ignoring logic:
// not allowing teams, rejecting DMs, friends requets etc.
// The only thing not auto-handled is instance activities force joining the team on the server.

void ChatIgnoreList::GetIgnoreList(const ChatPackets::GetIgnoreList& request, const SystemAddress& sysAddr) {
	const LWOOBJID playerId = request.playerID;

	auto& receiver = Game::playerContainer.GetPlayerDataMutable(playerId);
	if (!receiver) {
		LOG("Tried to get ignore list, but player %llu not found in container", playerId);
		return;
	}

	if (!receiver.ignoredPlayers.empty()) {
		LOG_DEBUG("Player %llu already has an ignore list, but is requesting it again.", playerId);
	} else {
		auto ignoreList = Database::Get()->GetIgnoreList(playerId);
		if (ignoreList.empty()) {
			LOG_DEBUG("Player %llu has no ignores", playerId);
			return;
		}

		for (auto& ignoredPlayer : ignoreList) {
			receiver.ignoredPlayers.emplace_back(ignoredPlayer.name, ignoredPlayer.id);
			GeneralUtils::SetBit(receiver.ignoredPlayers.back().playerId, eObjectBits::CHARACTER);
		}
	}

	ClientPackets::GetIgnoreListResponse response;
	response.isFreeTrial = false; // Is Free Trial, but we don't care about that
	for (const auto& ignoredPlayer : receiver.ignoredPlayers) {
		response.ignored.push_back({ ignoredPlayer.playerId, LUWString(ignoredPlayer.playerName, 36) });
	}

	ChatPacketHandler::SendRouted(receiver.playerID, sysAddr, response);
}

void ChatIgnoreList::AddIgnore(const ChatPackets::AddIgnore& request, const SystemAddress& sysAddr) {
	const LWOOBJID playerId = request.playerID;

	auto& receiver = Game::playerContainer.GetPlayerDataMutable(playerId);
	if (!receiver) {
		LOG("Tried to get ignore list, but player %llu not found in container", playerId);
		return;
	}

	const int32_t MAX_IGNORES = Game::config->GetValue("max_ignores", 32);
	if (receiver.ignoredPlayers.size() >= MAX_IGNORES) {
		LOG_DEBUG("Player %llu has too many ignores", playerId);
		return;
	}

	std::string toIgnoreStr = request.playerName.GetAsString();

	ClientPackets::AddIgnoreResponse response;

	// Check if the player exists
	LWOOBJID ignoredPlayerId = LWOOBJID_EMPTY;
	if (toIgnoreStr == receiver.playerName || toIgnoreStr.find("[GM]") == 0) {
		LOG_DEBUG("Player %llu tried to ignore themselves", playerId);

		response.responseCode = eAddIgnoreResponse::GENERAL_ERROR;
	} else if (std::count(receiver.ignoredPlayers.begin(), receiver.ignoredPlayers.end(), toIgnoreStr) > 0) {
		LOG_DEBUG("Player %llu is already ignoring %s", playerId, toIgnoreStr.c_str());

		response.responseCode = eAddIgnoreResponse::ALREADY_IGNORED;
	} else {
		// Get the playerId falling back to query if not online
		const auto& playerData = Game::playerContainer.GetPlayerData(toIgnoreStr);
		if (!playerData) {
			// Fall back to query
			auto player = Database::Get()->GetCharacterInfo(toIgnoreStr);
			if (!player || player->name != toIgnoreStr) {
				LOG_DEBUG("Player %s not found", toIgnoreStr.c_str());
			} else {
				ignoredPlayerId = player->id;
			}
		} else {
			ignoredPlayerId = playerData.playerID;
		}

		if (ignoredPlayerId != LWOOBJID_EMPTY) {
			Database::Get()->AddIgnore(playerId, ignoredPlayerId);
			GeneralUtils::SetBit(ignoredPlayerId, eObjectBits::CHARACTER);

			receiver.ignoredPlayers.emplace_back(toIgnoreStr, ignoredPlayerId);
			LOG_DEBUG("Player %llu is ignoring %s", playerId, toIgnoreStr.c_str());

			response.responseCode = eAddIgnoreResponse::SUCCESS;
		} else {
			response.responseCode = eAddIgnoreResponse::PLAYER_NOT_FOUND;
		}
	}

	response.playerName = LUWString(toIgnoreStr, 33);
	response.playerID = ignoredPlayerId;

	ChatPacketHandler::SendRouted(receiver.playerID, sysAddr, response);
}

void ChatIgnoreList::RemoveIgnore(const ChatPackets::RemoveIgnore& request, const SystemAddress& sysAddr) {
	const LWOOBJID playerId = request.playerID;

	auto& receiver = Game::playerContainer.GetPlayerDataMutable(playerId);
	if (!receiver) {
		LOG("Tried to get ignore list, but player %llu not found in container", playerId);
		return;
	}

	std::string removedIgnoreStr = request.playerName.GetAsString();

	auto toRemove = std::remove(receiver.ignoredPlayers.begin(), receiver.ignoredPlayers.end(), removedIgnoreStr);
	if (toRemove == receiver.ignoredPlayers.end()) {
		LOG_DEBUG("Player %llu is not ignoring %s", playerId, removedIgnoreStr.c_str());
		return;
	}

	Database::Get()->RemoveIgnore(playerId, toRemove->playerId);
	receiver.ignoredPlayers.erase(toRemove, receiver.ignoredPlayers.end());

	ClientPackets::RemoveIgnoreResponse response;
	response.responseCode = 0;
	response.playerName = LUWString(removedIgnoreStr, 33);

	ChatPacketHandler::SendRouted(receiver.playerID, sysAddr, response);
}
