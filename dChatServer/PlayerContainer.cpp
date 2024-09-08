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
#include "eChatMessageType.h"

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

	auto& data = m_Players[playerId];
	data.playerID = playerId;

	uint32_t len;
	inStream.Read<uint32_t>(len);

	for (int i = 0; i < len; i++) {
		char character; inStream.Read<char>(character);
		data.playerName += character;
	}

	inStream.Read(data.zoneID);
	inStream.Read(data.muteExpire);
	inStream.Read(data.gmLevel);
	data.sysAddr = packet->systemAddress;

	m_Names[data.playerID] = GeneralUtils::UTF8ToUTF16(data.playerName);
	m_PlayerCount++;

	LOG("Added user: %s (%llu), zone: %i", data.playerName.c_str(), data.playerID, data.zoneID.GetMapID());

	Database::Get()->UpdateActivityLog(data.playerID, eActivityType::PlayerLoggedIn, data.zoneID.GetMapID());
}

void PlayerContainer::RemovePlayer(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID;
	inStream.Read(playerID);

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

	auto* team = GetTeam(playerID);

	if (team != nullptr) {
		const auto memberName = GeneralUtils::UTF8ToUTF16(player.playerName);

		for (const auto memberId : team->memberIDs) {
			const auto& otherMember = GetPlayerData(memberId);

			if (!otherMember) continue;

			ChatPacketHandler::SendTeamSetOffWorldFlag(otherMember, playerID, { 0, 0, 0 });
		}
	}

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

void PlayerContainer::CreateTeamServer(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID;
	inStream.Read(playerID);
	size_t membersSize = 0;
	inStream.Read(membersSize);

	std::vector<LWOOBJID> members;

	members.reserve(membersSize);

	for (size_t i = 0; i < membersSize; i++) {
		LWOOBJID member;
		inStream.Read(member);
		members.push_back(member);
	}

	LWOZONEID zoneId;

	inStream.Read(zoneId);

	auto* team = CreateLocalTeam(members);

	if (team != nullptr) {
		team->zoneId = zoneId;
	}

	UpdateTeamsOnWorld(team, false);
}

void PlayerContainer::BroadcastMuteUpdate(LWOOBJID player, time_t time) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::GM_MUTE);

	bitStream.Write(player);
	bitStream.Write(time);

	Game::server->Send(bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
}

TeamData* PlayerContainer::CreateLocalTeam(std::vector<LWOOBJID> members) {
	if (members.empty()) {
		return nullptr;
	}

	TeamData* newTeam = nullptr;

	for (const auto member : members) {
		auto* team = GetTeam(member);

		if (team != nullptr) {
			RemoveMember(team, member, false, false, true);
		}

		if (newTeam == nullptr) {
			newTeam = CreateTeam(member, true);
		} else {
			AddMember(newTeam, member);
		}
	}

	newTeam->lootFlag = 1;

	TeamStatusUpdate(newTeam);

	return newTeam;
}

TeamData* PlayerContainer::CreateTeam(LWOOBJID leader, bool local) {
	auto* team = new TeamData();

	team->teamID = ++m_TeamIDCounter;
	team->leaderID = leader;
	team->local = local;

	mTeams.push_back(team);

	AddMember(team, leader);

	return team;
}

TeamData* PlayerContainer::GetTeam(LWOOBJID playerID) {
	for (auto* team : mTeams) {
		if (std::find(team->memberIDs.begin(), team->memberIDs.end(), playerID) == team->memberIDs.end()) continue;

		return team;
	}

	return nullptr;
}

void PlayerContainer::AddMember(TeamData* team, LWOOBJID playerID) {
	if (team->memberIDs.size() >= 4) {
		LOG("Tried to add player to team that already had 4 players");
		const auto& player = GetPlayerData(playerID);
		if (!player) return;
		ChatPackets::SendSystemMessage(player.sysAddr, u"The teams is full! You have not been added to a team!");
		return;
	}

	const auto index = std::find(team->memberIDs.begin(), team->memberIDs.end(), playerID);

	if (index != team->memberIDs.end()) return;

	team->memberIDs.push_back(playerID);

	const auto& leader = GetPlayerData(team->leaderID);
	const auto& member = GetPlayerData(playerID);

	if (!leader || !member) return;

	const auto leaderName = GeneralUtils::UTF8ToUTF16(leader.playerName);
	const auto memberName = GeneralUtils::UTF8ToUTF16(member.playerName);

	ChatPacketHandler::SendTeamInviteConfirm(member, false, leader.playerID, leader.zoneID, team->lootFlag, 0, 0, leaderName);

	if (!team->local) {
		ChatPacketHandler::SendTeamSetLeader(member, leader.playerID);
	} else {
		ChatPacketHandler::SendTeamSetLeader(member, LWOOBJID_EMPTY);
	}

	UpdateTeamsOnWorld(team, false);

	for (const auto memberId : team->memberIDs) {
		const auto& otherMember = GetPlayerData(memberId);

		if (otherMember == member) continue;

		const auto otherMemberName = GetName(memberId);

		ChatPacketHandler::SendTeamAddPlayer(member, false, team->local, false, memberId, otherMemberName, otherMember ? otherMember.zoneID : LWOZONEID(0, 0, 0));

		if (otherMember) {
			ChatPacketHandler::SendTeamAddPlayer(otherMember, false, team->local, false, member.playerID, memberName, member.zoneID);
		}
	}
}

void PlayerContainer::RemoveMember(TeamData* team, LWOOBJID playerID, bool disband, bool kicked, bool leaving, bool silent) {
	const auto index = std::find(team->memberIDs.begin(), team->memberIDs.end(), playerID);

	if (index == team->memberIDs.end()) return;

	const auto& member = GetPlayerData(playerID);

	if (member && !silent) {
		ChatPacketHandler::SendTeamSetLeader(member, LWOOBJID_EMPTY);
	}

	const auto memberName = GetName(playerID);

	for (const auto memberId : team->memberIDs) {
		if (silent && memberId == playerID) {
			continue;
		}

		const auto& otherMember = GetPlayerData(memberId);

		if (!otherMember) continue;

		ChatPacketHandler::SendTeamRemovePlayer(otherMember, disband, kicked, leaving, false, team->leaderID, playerID, memberName);
	}

	team->memberIDs.erase(index);

	UpdateTeamsOnWorld(team, false);

	if (team->memberIDs.size() <= 1) {
		DisbandTeam(team);
	} else {
		if (playerID == team->leaderID) {
			PromoteMember(team, team->memberIDs[0]);
		}
	}
}

void PlayerContainer::PromoteMember(TeamData* team, LWOOBJID newLeader) {
	team->leaderID = newLeader;

	for (const auto memberId : team->memberIDs) {
		const auto& otherMember = GetPlayerData(memberId);

		if (!otherMember) continue;

		ChatPacketHandler::SendTeamSetLeader(otherMember, newLeader);
	}
}

void PlayerContainer::DisbandTeam(TeamData* team) {
	const auto index = std::find(mTeams.begin(), mTeams.end(), team);

	if (index == mTeams.end()) return;

	for (const auto memberId : team->memberIDs) {
		const auto& otherMember = GetPlayerData(memberId);

		if (!otherMember) continue;

		const auto memberName = GeneralUtils::UTF8ToUTF16(otherMember.playerName);

		ChatPacketHandler::SendTeamSetLeader(otherMember, LWOOBJID_EMPTY);
		ChatPacketHandler::SendTeamRemovePlayer(otherMember, true, false, false, team->local, team->leaderID, otherMember.playerID, memberName);
	}

	UpdateTeamsOnWorld(team, true);

	mTeams.erase(index);

	delete team;
}

void PlayerContainer::TeamStatusUpdate(TeamData* team) {
	const auto index = std::find(mTeams.begin(), mTeams.end(), team);

	if (index == mTeams.end()) return;

	const auto& leader = GetPlayerData(team->leaderID);

	if (!leader) return;

	const auto leaderName = GeneralUtils::UTF8ToUTF16(leader.playerName);

	for (const auto memberId : team->memberIDs) {
		const auto& otherMember = GetPlayerData(memberId);

		if (!otherMember) continue;

		if (!team->local) {
			ChatPacketHandler::SendTeamStatus(otherMember, team->leaderID, leader.zoneID, team->lootFlag, 0, leaderName);
		}
	}

	UpdateTeamsOnWorld(team, false);
}

void PlayerContainer::UpdateTeamsOnWorld(TeamData* team, bool deleteTeam) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::TEAM_GET_STATUS);

	bitStream.Write(team->teamID);
	bitStream.Write(deleteTeam);

	if (!deleteTeam) {
		bitStream.Write(team->lootFlag);
		bitStream.Write<char>(team->memberIDs.size());
		for (const auto memberID : team->memberIDs) {
			bitStream.Write(memberID);
		}
	}

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
