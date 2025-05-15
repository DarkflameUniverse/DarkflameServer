#include "PlayerContainer.h"
#include "dNetCommon.h"
#include <iostream>
#include <algorithm>
#include "Game.h"
#include "Logger.h"
#include "ChatPacketHandler.h"
#include "GeneralUtils.h"
#include "BitStreamUtils.h"
#include "Database.h"
#include "eConnectionType.h"
#include "ChatPackets.h"
#include "dConfig.h"
#include "MessageType/Chat.h"
#include "ChatWeb.h"
#include "TeamContainer.h"

void PlayerContainer::Initialize() {
	m_MaxNumberOfBestFriends =
		GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("max_number_of_best_friends")).value_or(m_MaxNumberOfBestFriends);
	m_MaxNumberOfFriends =
		GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("max_number_of_friends")).value_or(m_MaxNumberOfFriends);
}

TeamData::TeamData() {
	lootFlag = Game::config->GetValue("default_team_loot") == "0" ? 0 : 1;
}

void PlayerContainer::InsertPlayer(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerId;
	if (!inStream.Read(playerId)) {
		LOG("Failed to read player ID");
		return;
	}

	auto isLogin = !m_Players.contains(playerId);
	auto& data = m_Players[playerId];
	data = PlayerData();
	data.isLogin = isLogin;
	data.playerID = playerId;

	uint32_t len;
	if (!inStream.Read<uint32_t>(len)) return;

	if (len > 33) {
		LOG("Received a really long player name, probably a fake packet %i.", len);
		return;
	}

	data.playerName.resize(len);
	inStream.ReadAlignedBytes(reinterpret_cast<unsigned char*>(data.playerName.data()), len);

	if (!inStream.Read(data.zoneID)) return;
	if (!inStream.Read(data.muteExpire)) return;
	if (!inStream.Read(data.gmLevel)) return;
	data.worldServerSysAddr = packet->systemAddress;

	m_Names[data.playerID] = GeneralUtils::UTF8ToUTF16(data.playerName);
	m_PlayerCount++;

	LOG("Added user: %s (%llu), zone: %i", data.playerName.c_str(), data.playerID, data.zoneID.GetMapID());
	ChatWeb::SendWSPlayerUpdate(data, isLogin ? eActivityType::PlayerLoggedIn : eActivityType::PlayerChangedZone);

	Database::Get()->UpdateActivityLog(data.playerID, isLogin ? eActivityType::PlayerLoggedIn : eActivityType::PlayerChangedZone, data.zoneID.GetMapID());
	m_PlayersToRemove.erase(playerId);
}

void PlayerContainer::ScheduleRemovePlayer(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID{ LWOOBJID_EMPTY };
	inStream.Read(playerID);
	constexpr float updatePlayerOnLogoutTime = 20.0f;
	if (playerID != LWOOBJID_EMPTY) m_PlayersToRemove.insert_or_assign(playerID, updatePlayerOnLogoutTime);
}

void PlayerContainer::Update(const float deltaTime) {
	for (auto it = m_PlayersToRemove.begin(); it != m_PlayersToRemove.end();) {
		auto& [id, time] = *it;
		time -= deltaTime;

		if (time <= 0.0f) {
			RemovePlayer(id);
			it = m_PlayersToRemove.erase(it);
		} else {
			++it;
		}
	}
}

void PlayerContainer::RemovePlayer(const LWOOBJID playerID) {
	//Before they get kicked, we need to also send a message to their friends saying that they disconnected.
	const auto& player = GetPlayerData(playerID);

	if (!player) {
		LOG("Failed to find user: %llu", playerID);
		return;
	}

	for (const auto& fr : player.friends) {
		const auto& fd = this->GetPlayerData(fr.friendID);
		if (fd) ChatPacketHandler::SendFriendUpdate(fd, player, 0, fr.isBestFriend);
	}

	auto* team = TeamContainer::GetTeam(playerID);

	if (team != nullptr) {
		const auto memberName = GeneralUtils::UTF8ToUTF16(player.playerName);

		for (const auto memberId : team->memberIDs) {
			const auto& otherMember = GetPlayerData(memberId);

			if (!otherMember) continue;

			TeamContainer::SendTeamSetOffWorldFlag(otherMember, playerID, { 0, 0, 0 });
		}
	}

	ChatWeb::SendWSPlayerUpdate(player, eActivityType::PlayerLoggedOut);

	m_PlayerCount--;
	LOG("Removed user: %llu", playerID);
	m_Players.erase(playerID);

	Database::Get()->UpdateActivityLog(playerID, eActivityType::PlayerLoggedOut, player.zoneID.GetMapID());
}

void PlayerContainer::MuteUpdate(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID;
	inStream.Read(playerID);
	time_t expire = 0;
	inStream.Read(expire);

	auto& player = this->GetPlayerDataMutable(playerID);

	if (!player) {
		LOG("Failed to find user: %llu", playerID);

		return;
	}

	player.muteExpire = expire;

	BroadcastMuteUpdate(playerID, expire);
}

void PlayerContainer::BroadcastMuteUpdate(LWOOBJID player, time_t time) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::GM_MUTE);

	bitStream.Write(player);
	bitStream.Write(time);

	Game::server->Send(bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
}

std::u16string PlayerContainer::GetName(LWOOBJID playerID) {
	const auto iter = m_Names.find(playerID);

	if (iter == m_Names.end()) return u"";

	return iter->second;
}

LWOOBJID PlayerContainer::GetId(const std::u16string& playerName) {
	LWOOBJID toReturn = LWOOBJID_EMPTY;

	for (const auto& [id, name] : m_Names) {
		if (name == playerName) {
			toReturn = id;
			break;
		}
	}

	return toReturn;
}

PlayerData& PlayerContainer::GetPlayerDataMutable(const LWOOBJID& playerID) {
	return m_Players.contains(playerID) ? m_Players[playerID] : m_Players[LWOOBJID_EMPTY];
}

PlayerData& PlayerContainer::GetPlayerDataMutable(const std::string& playerName) {
	for (auto& [id, player] : m_Players) {
		if (!player) continue;
		if (player.playerName == playerName) return player;
	}
	return m_Players[LWOOBJID_EMPTY];
}

const PlayerData& PlayerContainer::GetPlayerData(const LWOOBJID& playerID) {
	return GetPlayerDataMutable(playerID);
}

const PlayerData& PlayerContainer::GetPlayerData(const std::string& playerName) {
	return GetPlayerDataMutable(playerName);
}

void PlayerContainer::Shutdown() {
	m_Players.erase(LWOOBJID_EMPTY);
	while (!m_Players.empty()) {
		const auto& [id, playerData] = *m_Players.begin();
		Database::Get()->UpdateActivityLog(id, eActivityType::PlayerLoggedOut, playerData.zoneID.GetMapID());
		m_Players.erase(m_Players.begin());
	}
}
