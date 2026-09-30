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
#include "ServiceType.h"
#include "ChatPackets.h"
#include "dConfig.h"
#include "MessageType/Chat.h"
#include "TeamContainer.h"
#include "ChatGuilds.h"
#include "GuildManager.h"

void PlayerContainer::Initialize() {
	m_MaxNumberOfBestFriends =
		GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("max_number_of_best_friends")).value_or(m_MaxNumberOfBestFriends);
	m_MaxNumberOfFriends =
		GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("max_number_of_friends")).value_or(m_MaxNumberOfFriends);
}

TeamData::TeamData() {
	lootFlag = Game::config->GetValue("default_team_loot") == "0" ? 0 : 1;
}

void PlayerContainer::InsertPlayer(const ChatPackets::LoginSessionNotify& notify, const SystemAddress& sysAddr) {
	// Names longer than ChatPackets::LoginSessionNotify::MAX_NAME_LENGTH were dropped when read
	const LWOOBJID playerId = notify.playerID;

	// A world sending its players to a new chat server (live update): nobody logged in or changed zones
	if (notify.resync) {
		if (m_Players.contains(playerId)) {
			m_Players[playerId].worldServerSysAddr = sysAddr;
			m_PlayersToRemove.erase(playerId);
			return;
		}
		auto& data = m_Players[playerId];
		data = PlayerData();
		data.playerID = playerId;
		data.playerName = notify.playerName;
		data.zoneID = notify.zoneID;
		data.muteExpire = static_cast<time_t>(notify.muteExpire);
		data.gmLevel = notify.gmLevel;
		data.worldServerSysAddr = sysAddr;
		m_Names[data.playerID] = GeneralUtils::UTF8ToUTF16(data.playerName);
		m_PlayerCount++;
		// Their friends list is read again when their client next asks (every zone load); until then the friends
		// they have are what friend updates go to
		ChatPacketHandler::LoadFriends(data);
		m_PlayersToRemove.erase(playerId);
		LOG("Took over user: %s (%llu), zone: %i", data.playerName.c_str(), data.playerID, data.zoneID.GetMapID());
		// Guildmates' clients only get the zone again (no "logged in" line)
		ChatGuilds::Get().PlayerOnline(playerId, false);
		return;
	}

	auto isLogin = !m_Players.contains(playerId);
	auto& data = m_Players[playerId];
	data = PlayerData();
	data.isLogin = isLogin;
	data.playerID = playerId;

	data.playerName = notify.playerName;
	data.zoneID = notify.zoneID;
	data.muteExpire = static_cast<time_t>(notify.muteExpire);
	data.gmLevel = notify.gmLevel;
	data.worldServerSysAddr = sysAddr;

	m_Names[data.playerID] = GeneralUtils::UTF8ToUTF16(data.playerName);
	m_PlayerCount++;

	LOG("Added user: %s (%llu), zone: %i", data.playerName.c_str(), data.playerID, data.zoneID.GetMapID());

	Database::Get()->UpdateActivityLog(data.playerID, isLogin ? eActivityType::PlayerLoggedIn : eActivityType::PlayerChangedZone, data.zoneID.GetMapID());
	m_PlayersToRemove.erase(playerId);
	ChatGuilds::Get().PlayerOnline(playerId, isLogin);
}

void PlayerContainer::ScheduleRemovePlayer(const ChatPackets::UnexpectedDisconnect& notify, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = notify.playerID;
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
		TeamContainer::RemoveMember(team, playerID, false, false, true);
	}

	ChatGuilds::Get().PlayerOffline(playerID);


	m_PlayerCount--;
	LOG("Removed user: %llu", playerID);
	m_Players.erase(playerID);

	Database::Get()->UpdateActivityLog(playerID, eActivityType::PlayerLoggedOut, player.zoneID.GetMapID());
}

void PlayerContainer::MuteUpdate(const ChatPackets::GMMute& mute, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = mute.playerID;
	const time_t expire = static_cast<time_t>(mute.expire);

	auto& player = this->GetPlayerDataMutable(playerID);

	if (!player) {
		LOG("Failed to find user: %llu", playerID);

		return;
	}

	player.muteExpire = expire;

	BroadcastMuteUpdate(playerID, expire);
}

void PlayerContainer::BroadcastMuteUpdate(LWOOBJID player, time_t time) {
	ChatPackets::GMMute mute;
	mute.playerID = player;
	mute.expire = time;
	mute.Broadcast();
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

// TODO Make this a pointer again or do something to make it so you cant edit the LWOOBJID_EMPTY entry?  it should be ignored in all cases anyways though...
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
	// Handed over to the next chat server (live update): everyone is still online
	if (m_Retiring) {
		m_Players.clear();
		return;
	}
	while (!m_Players.empty()) {
		const auto& [id, playerData] = *m_Players.begin();
		Database::Get()->UpdateActivityLog(id, eActivityType::PlayerLoggedOut, playerData.zoneID.GetMapID());
		m_Players.erase(m_Players.begin());
	}
}
