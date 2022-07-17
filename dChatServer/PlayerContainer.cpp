#include "PlayerContainer.h"
#include "dNetCommon.h"
#include <iostream>
#include <algorithm>
#include "Game.h"
#include "dLogger.h"
#include "ChatPacketHandler.h"
#include "GeneralUtils.h"
#include "dMessageIdentifiers.h"
#include "PacketUtils.h"
#include "Database.h"

PlayerContainer::PlayerContainer() {
}

PlayerContainer::~PlayerContainer() {
	mPlayers.clear();
}

void PlayerContainer::InsertPlayer(Packet* packet) {
	CINSTREAM;
	PlayerData* data = new PlayerData();
	inStream.SetReadOffset(inStream.GetReadOffset() + 64);
	inStream.Read(data->playerID);

	uint32_t len;
	inStream.Read<uint32_t>(len);

	for (int i = 0; i < len; i++) {
		char character; inStream.Read<char>(character);
		data->playerName += character; 
	}

	inStream.Read(data->zoneID);
	inStream.Read(data->muteExpire);
	data->sysAddr = packet->systemAddress;

	mNames[data->playerID] = GeneralUtils::ASCIIToUTF16(std::string(data->playerName.c_str()));

	mPlayers.insert(std::make_pair(data->playerID, data));
	Game::logger->Log("PlayerContainer", "Added user: %s (%llu), zone: %i\n", data->playerName.c_str(), data->playerID, data->zoneID.GetMapID());

	auto* insertLog = Database::CreatePreppedStmt("INSERT INTO activity_log (character_id, activity, time, map_id) VALUES (?, ?, ?, ?);");

	insertLog->setInt(1, data->playerID);
	insertLog->setInt(2, 0);
	insertLog->setUInt64(3, time(nullptr));
	insertLog->setInt(4, data->zoneID.GetMapID());

	insertLog->executeUpdate();
}

void PlayerContainer::RemovePlayer(Packet* packet) {
	CINSTREAM;
	LWOOBJID playerID;
	inStream.Read(playerID); //skip header
	inStream.Read(playerID);

	//Before they get kicked, we need to also send a message to their friends saying that they disconnected.
	std::unique_ptr<PlayerData> player(this->GetPlayerData(playerID));

	if (player == nullptr) {
		return;
	}

	for (auto& fr : player->friends) {
		auto fd = this->GetPlayerData(fr.friendID);
		if (fd) ChatPacketHandler::SendFriendUpdate(fd, player.get(), 0, fr.isBestFriend);
	}

	auto* team = GetTeam(playerID);

	if (team != nullptr)
	{
		const auto memberName = GeneralUtils::ASCIIToUTF16(std::string(player->playerName.c_str()));
		
		for (const auto memberId : team->memberIDs)
		{
			auto* otherMember = GetPlayerData(memberId);

			if (otherMember == nullptr) continue;

			ChatPacketHandler::SendTeamSetOffWorldFlag(otherMember, playerID, {0, 0, 0});
		}
	}

	Game::logger->Log("PlayerContainer", "Removed user: %llu\n", playerID);
	mPlayers.erase(playerID);

	auto* insertLog = Database::CreatePreppedStmt("INSERT INTO activity_log (character_id, activity, time, map_id) VALUES (?, ?, ?, ?);");

	insertLog->setInt(1, playerID);
	insertLog->setInt(2, 1);
	insertLog->setUInt64(3, time(nullptr));
	insertLog->setInt(4, player->zoneID.GetMapID());

	insertLog->executeUpdate();
}

void PlayerContainer::MuteUpdate(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID;
	inStream.Read(playerID); //skip header
	inStream.Read(playerID);
	time_t expire = 0;
	inStream.Read(expire);

	auto* player = this->GetPlayerData(playerID);

	if (player == nullptr)
	{
		Game::logger->Log("PlayerContainer", "Failed to find user: %llu\n", playerID);

		return;
	}

	player->muteExpire = expire;

	BroadcastMuteUpdate(playerID, expire);
}

void PlayerContainer::CreateTeamServer(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID;
	inStream.Read(playerID); //skip header
	inStream.Read(playerID);
	size_t membersSize = 0;
	inStream.Read(membersSize);

	std::vector<LWOOBJID> members;

	members.reserve(membersSize);

	for (size_t i = 0; i < membersSize; i++)
	{
		LWOOBJID member;
		inStream.Read(member);
		members.push_back(member);
	}

	LWOZONEID zoneId;

	inStream.Read(zoneId);
	
	auto* team = CreateLocalTeam(members);

	if (team != nullptr)
	{
		team->zoneId = zoneId;
	}

	UpdateTeamsOnWorld(team, false);
}

void PlayerContainer::BroadcastMuteUpdate(LWOOBJID player, time_t time) 
{
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_MUTE_UPDATE);

	bitStream.Write(player);
	bitStream.Write(time);

	Game::server->Send(&bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
}

TeamData* PlayerContainer::CreateLocalTeam(std::vector<LWOOBJID> members) 
{
	if (members.empty())
	{
		return nullptr;
	}

	TeamData* newTeam = nullptr;

	for (const auto member : members)
	{
		auto* team = GetTeam(member);

		if (team != nullptr)
		{
			RemoveMember(team, member, false, false, true);
		}

		if (newTeam == nullptr)
		{
			newTeam = CreateTeam(member, true);
		}
		else
		{
			AddMember(newTeam, member);
		}
	}

	newTeam->lootFlag = 1;

	TeamStatusUpdate(newTeam);

	return newTeam;
}

TeamData* PlayerContainer::CreateTeam(LWOOBJID leader, bool local) 
{
	auto* team = new TeamData();
	
	team->teamID = ++mTeamIDCounter;
	team->leaderID = leader;
	team->local = local;
	
	mTeams.push_back(team);

	AddMember(team, leader);

	return team;
}

TeamData* PlayerContainer::GetTeam(LWOOBJID playerID) 
{
	for (auto* team : mTeams)
	{
		if (std::find(team->memberIDs.begin(), team->memberIDs.end(), playerID) == team->memberIDs.end()) continue;

		return team;
	}
	
	return nullptr;
}

void PlayerContainer::AddMember(TeamData* team, LWOOBJID playerID) 
{
	const auto index = std::find(team->memberIDs.begin(), team->memberIDs.end(), playerID);

	if (index != team->memberIDs.end()) return;

	team->memberIDs.push_back(playerID);

	auto* leader = GetPlayerData(team->leaderID);
	auto* member = GetPlayerData(playerID);

	if (leader == nullptr || member == nullptr) return;

	const auto leaderName = GeneralUtils::ASCIIToUTF16(std::string(leader->playerName.c_str()));
	const auto memberName = GeneralUtils::ASCIIToUTF16(std::string(member->playerName.c_str()));

	ChatPacketHandler::SendTeamInviteConfirm(member, false, leader->playerID, leader->zoneID, team->lootFlag, 0, 0, leaderName);

	if (!team->local)
	{
		ChatPacketHandler::SendTeamSetLeader(member, leader->playerID);
	}
	else
	{
		ChatPacketHandler::SendTeamSetLeader(member, LWOOBJID_EMPTY);
	}

	UpdateTeamsOnWorld(team, false);

	for (const auto memberId : team->memberIDs)
	{
		auto* otherMember = GetPlayerData(memberId);

		if (otherMember == member) continue;

		const auto otherMemberName = GetName(memberId);
		
		ChatPacketHandler::SendTeamAddPlayer(member, false, team->local, false, memberId, otherMemberName, otherMember != nullptr ? otherMember->zoneID : LWOZONEID(0, 0, 0));

		if (otherMember != nullptr)
		{
			ChatPacketHandler::SendTeamAddPlayer(otherMember, false, team->local, false, member->playerID, memberName, member->zoneID);
		}
	}
}

void PlayerContainer::RemoveMember(TeamData* team, LWOOBJID playerID, bool disband, bool kicked, bool leaving, bool silent) 
{
	const auto index = std::find(team->memberIDs.begin(), team->memberIDs.end(), playerID);

	if (index == team->memberIDs.end()) return;

	auto* member = GetPlayerData(playerID);
	
	if (member != nullptr && !silent)
	{
		ChatPacketHandler::SendTeamSetLeader(member, LWOOBJID_EMPTY);
	}

	const auto memberName = GetName(playerID);

	for (const auto memberId : team->memberIDs)
	{
		if (silent && memberId == playerID)
		{
			continue;
		}

		auto* otherMember = GetPlayerData(memberId);

		if (otherMember == nullptr) continue;

		ChatPacketHandler::SendTeamRemovePlayer(otherMember, disband, kicked, leaving, false, team->leaderID, playerID, memberName);
	}

	team->memberIDs.erase(index);

	UpdateTeamsOnWorld(team, false);

	if (team->memberIDs.size() <= 1)
	{
		DisbandTeam(team);
	}
	else
	{
		if (playerID == team->leaderID)
		{
			PromoteMember(team, team->memberIDs[0]);
		}
	}
}

void PlayerContainer::PromoteMember(TeamData* team, LWOOBJID newLeader) 
{
	team->leaderID = newLeader;

	for (const auto memberId : team->memberIDs)
	{
		auto* otherMember = GetPlayerData(memberId);

		if (otherMember == nullptr) continue;

		ChatPacketHandler::SendTeamSetLeader(otherMember, newLeader);
	}
}

void PlayerContainer::DisbandTeam(TeamData* team) 
{
	const auto index = std::find(mTeams.begin(), mTeams.end(), team);

	if (index == mTeams.end()) return;

	for (const auto memberId : team->memberIDs)
	{
		auto* otherMember = GetPlayerData(memberId);

		if (otherMember == nullptr) continue;

		const auto memberName = GeneralUtils::ASCIIToUTF16(std::string(otherMember->playerName.c_str()));

		ChatPacketHandler::SendTeamSetLeader(otherMember, LWOOBJID_EMPTY);
		ChatPacketHandler::SendTeamRemovePlayer(otherMember, true, false, false, team->local, team->leaderID, otherMember->playerID, memberName);
	}
	
	UpdateTeamsOnWorld(team, true);

	mTeams.erase(index);

	delete team;
}

void PlayerContainer::TeamStatusUpdate(TeamData* team) 
{
	const auto index = std::find(mTeams.begin(), mTeams.end(), team);

	if (index == mTeams.end()) return;

	auto* leader = GetPlayerData(team->leaderID);

	if (leader == nullptr) return;

	const auto leaderName = GeneralUtils::ASCIIToUTF16(std::string(leader->playerName.c_str()));

	for (const auto memberId : team->memberIDs)
	{
		auto* otherMember = GetPlayerData(memberId);

		if (otherMember == nullptr) continue;

		if (!team->local)
		{
			ChatPacketHandler::SendTeamStatus(otherMember, team->leaderID, leader->zoneID, team->lootFlag, 0, leaderName);
		}
	}

	UpdateTeamsOnWorld(team, false);
}

void PlayerContainer::UpdateTeamsOnWorld(TeamData* team, bool deleteTeam) 
{
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_TEAM_UPDATE);

	bitStream.Write(team->teamID);
	bitStream.Write(deleteTeam);

	if (!deleteTeam)
	{
		bitStream.Write(team->lootFlag);
		bitStream.Write(static_cast<char>(team->memberIDs.size()));
		for (const auto memberID : team->memberIDs)
		{
			bitStream.Write(memberID);
		}
	}

	Game::server->Send(&bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
}

std::u16string PlayerContainer::GetName(LWOOBJID playerID) 
{
	const auto& pair = mNames.find(playerID);

	if (pair == mNames.end()) return u"";

	return pair->second;
}

LWOOBJID PlayerContainer::GetId(const std::u16string& playerName) 
{
	for (const auto& pair : mNames)
	{
		if (pair.second == playerName)
		{
			return pair.first;
		}
	}
	
	return LWOOBJID_EMPTY;
}

bool PlayerContainer::GetIsMuted(PlayerData* data) 
{
	return data->muteExpire == 1 || data->muteExpire > time(NULL);
}
