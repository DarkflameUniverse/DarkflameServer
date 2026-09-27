#include "TeamContainer.h"

#include "ChatPackets.h"

#include "MessageType/Chat.h"
#include "MessageType/Game.h"

#include "ChatPacketHandler.h"
#include "PlayerContainer.h"

namespace {
	TeamContainer::Data g_TeamContainer{};
	LWOOBJID g_TeamIDCounter = 0;
}

const TeamContainer::Data& TeamContainer::GetTeamContainer() {
	return g_TeamContainer;
}

std::vector<TeamData*>& TeamContainer::GetTeamsMut() {
	return g_TeamContainer.mTeams;
}

const std::vector<TeamData*>& TeamContainer::GetTeams() {
	return GetTeamsMut();
}

void TeamContainer::Shutdown() {
	for (auto* team : g_TeamContainer.mTeams) if (team) delete team;
}

void TeamContainer::HandleTeamInvite(const ChatPackets::TeamInvite& invite, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = invite.playerID;
	const LUWString& invitedPlayer = invite.invitedPlayer;

	const auto& player = Game::playerContainer.GetPlayerData(playerID);

	if (!player) return;

	auto* team = GetTeam(playerID);

	if (team == nullptr) {
		team = CreateTeam(playerID);
	}

	const auto& other = Game::playerContainer.GetPlayerData(invitedPlayer.GetAsString());

	if (!other) return;

	if (GetTeam(other.playerID) != nullptr) {
		return;
	}

	if (team->memberIDs.size() > 3) {
		// no more teams greater than 4

		LOG("Someone tried to invite a 5th player to a team");
		return;
	}

	SendTeamInvite(other, player);

	LOG("Got team invite: %llu -> %s", playerID, invitedPlayer.GetAsString().c_str());

	bool failed = false;
	for (const auto& ignore : other.ignoredPlayers) {
		if (ignore.playerId == player.playerID) {
			failed = true;
			break;
		}
	}

	ClientPackets::TeamInviteInitialResponse response{};
	response.inviteFailedToSend = failed;
	response.playerName = invitedPlayer.string;
	ChatPacketHandler::SendRouted(playerID, player.worldServerSysAddr, response, player.worldServerSysAddr == UNASSIGNED_SYSTEM_ADDRESS);
}

void TeamContainer::HandleTeamInviteResponse(const ChatPackets::TeamInviteResponse& response, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = response.playerID;
	const char declined = response.declined;
	const LWOOBJID leaderID = response.leaderID;

	LOG("Invite reponse received: %llu -> %llu (%d)", playerID, leaderID, declined);

	if (declined) {
		return;
	}

	auto* team = GetTeam(leaderID);

	if (team == nullptr) {
		LOG("Failed to find team for leader (%llu)", leaderID);

		team = GetTeam(playerID);
	}

	if (team == nullptr) {
		LOG("Failed to find team for player (%llu)", playerID);
		return;
	}

	AddMember(team, playerID);
}

void TeamContainer::HandleTeamLeave(const ChatPackets::TeamLeave& request, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = request.playerID;

	auto* team = GetTeam(playerID);

	LOG("(%llu) leaving team", playerID);

	if (team != nullptr) {
		RemoveMember(team, playerID, false, false, true);
	}
}

void TeamContainer::HandleTeamKick(const ChatPackets::TeamKick& request, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = request.playerID;
	const LUWString& kickedPlayer = request.kickedPlayer;


	LOG("(%llu) kicking (%s) from team", playerID, kickedPlayer.GetAsString().c_str());

	const auto& kicked = Game::playerContainer.GetPlayerData(kickedPlayer.GetAsString());

	LWOOBJID kickedId = LWOOBJID_EMPTY;

	if (kicked) {
		kickedId = kicked.playerID;
	} else {
		kickedId = Game::playerContainer.GetId(kickedPlayer.string);
	}

	if (kickedId == LWOOBJID_EMPTY) return;

	auto* team = GetTeam(playerID);

	if (team != nullptr) {
		if (team->leaderID != playerID || team->leaderID == kickedId) return;

		RemoveMember(team, kickedId, false, true, false);
	}
}

void TeamContainer::HandleTeamPromote(const ChatPackets::TeamSetLeader& request, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = request.playerID;
	const LUWString& promotedPlayer = request.promotedPlayer;

	LOG("(%llu) promoting (%s) to team leader", playerID, promotedPlayer.GetAsString().c_str());

	const auto& promoted = Game::playerContainer.GetPlayerData(promotedPlayer.GetAsString());

	if (!promoted) return;

	auto* team = GetTeam(playerID);

	if (team != nullptr) {
		if (team->leaderID != playerID) return;

		PromoteMember(team, promoted.playerID);
	}
}

void TeamContainer::HandleTeamLootOption(const ChatPackets::TeamSetLoot& request, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = request.playerID;
	const char option = request.lootFlag;

	auto* team = GetTeam(playerID);

	if (team != nullptr) {
		if (team->leaderID != playerID) return;

		team->lootFlag = option;

		TeamStatusUpdate(team);

		UpdateTeamsOnWorld(team, false);
	}
}

void TeamContainer::HandleTeamStatusRequest(const ChatPackets::TeamGetStatus& request, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = request.playerID;

	auto* team = GetTeam(playerID);
	const auto& data = Game::playerContainer.GetPlayerData(playerID);

	if (team != nullptr && data) {
		LOG_DEBUG("Player %llu is requesting team status", playerID);
		if (team->local && data.zoneID.GetMapID() != team->zoneId.GetMapID() && data.zoneID.GetCloneID() != team->zoneId.GetCloneID()) {
			RemoveMember(team, playerID, false, false, false, true);

			return;
		}

		if (team->memberIDs.size() <= 1 && !team->local) {
			DisbandTeam(team, LWOOBJID_EMPTY, u"");

			return;
		}

		if (!team->local) {
			SendTeamSetLeader(data, team->leaderID);
		} else {
			SendTeamSetLeader(data, LWOOBJID_EMPTY);
		}

		TeamStatusUpdate(team);

		const auto leaderName = GeneralUtils::UTF8ToUTF16(data.playerName);

		for (const auto memberId : team->memberIDs) {
			const auto& otherMember = Game::playerContainer.GetPlayerData(memberId);

			if (memberId == playerID) continue;

			const auto memberName = Game::playerContainer.GetName(memberId);

			if (otherMember) {
				SendTeamSetOffWorldFlag(otherMember, data.playerID, data.zoneID);
			}
			SendTeamAddPlayer(data, false, team->local, false, memberId, memberName, otherMember ? otherMember.zoneID : LWOZONEID(0, 0, 0));
		}

		UpdateTeamsOnWorld(team, false);
	}
}

void TeamContainer::SendTeamInvite(const PlayerData& receiver, const PlayerData& sender) {
	ClientPackets::TeamInvite invite;
	invite.senderName = LUWString(sender.playerName.c_str());
	invite.senderID = sender.playerID;
	ChatPacketHandler::SendRouted(receiver.playerID, receiver.worldServerSysAddr, invite);
}

void TeamContainer::SendTeamInviteConfirm(const PlayerData& receiver, bool bLeaderIsFreeTrial, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, uint8_t ucResponseCode, std::u16string wsLeaderName) {
	ClientPackets::TeamInviteConfirm msg;
	msg.target = receiver.playerID;
	msg.bLeaderIsFreeTrial = bLeaderIsFreeTrial;
	msg.i64LeaderID = i64LeaderID;
	msg.i64LeaderZoneID = i64LeaderZoneID;
	msg.ucLootFlag = ucLootFlag;
	msg.ucNumOfOtherPlayers = ucNumOfOtherPlayers;
	msg.ucResponseCode = ucResponseCode;
	msg.wsLeaderName = wsLeaderName;
	ChatPacketHandler::SendRouted(receiver.playerID, receiver.worldServerSysAddr, msg);
}

void TeamContainer::SendTeamStatus(const PlayerData& receiver, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, std::u16string wsLeaderName) {
	ClientPackets::TeamGetStatusResponse msg;
	msg.target = receiver.playerID;
	msg.i64LeaderID = i64LeaderID;
	msg.i64LeaderZoneID = i64LeaderZoneID;
	msg.ucLootFlag = ucLootFlag;
	msg.ucNumOfOtherPlayers = ucNumOfOtherPlayers;
	msg.wsLeaderName = wsLeaderName;
	ChatPacketHandler::SendRouted(receiver.playerID, receiver.worldServerSysAddr, msg);
}

void TeamContainer::SendTeamSetLeader(const PlayerData& receiver, LWOOBJID i64PlayerID) {
	ClientPackets::TeamSetLeader msg;
	msg.target = receiver.playerID;
	msg.i64PlayerID = i64PlayerID;
	ChatPacketHandler::SendRouted(receiver.playerID, receiver.worldServerSysAddr, msg);
}

void TeamContainer::SendTeamAddPlayer(const PlayerData& receiver, bool bIsFreeTrial, bool bLocal, bool bNoLootOnDeath, LWOOBJID i64PlayerID, std::u16string wsPlayerName, LWOZONEID zoneID) {
	ClientPackets::TeamAddPlayer msg;
	msg.target = receiver.playerID;
	msg.bIsFreeTrial = bIsFreeTrial;
	msg.bLocal = bLocal;
	msg.bNoLootOnDeath = bNoLootOnDeath;
	msg.i64PlayerID = i64PlayerID;
	msg.wsPlayerName = wsPlayerName;
	if (receiver.zoneID.GetCloneID() == zoneID.GetCloneID()) {
		zoneID = LWOZONEID(zoneID.GetMapID(), zoneID.GetInstanceID(), 0);
	}
	msg.zoneID = zoneID;
	ChatPacketHandler::SendRouted(receiver.playerID, receiver.worldServerSysAddr, msg);
}

void TeamContainer::SendTeamRemovePlayer(const PlayerData& receiver, bool bDisband, bool bIsKicked, bool bIsLeaving, bool bLocal, LWOOBJID i64LeaderID, LWOOBJID i64PlayerID, std::u16string wsPlayerName) {
	ClientPackets::TeamRemovePlayer msg;
	msg.target = receiver.playerID;
	msg.bDisband = bDisband;
	msg.bIsKicked = bIsKicked;
	msg.bIsLeaving = bIsLeaving;
	msg.bLocal = bLocal;
	msg.i64LeaderID = i64LeaderID;
	msg.i64PlayerID = i64PlayerID;
	msg.wsPlayerName = wsPlayerName;
	ChatPacketHandler::SendRouted(receiver.playerID, receiver.worldServerSysAddr, msg);
}

void TeamContainer::SendTeamSetOffWorldFlag(const PlayerData& receiver, LWOOBJID i64PlayerID, LWOZONEID zoneID) {
	ClientPackets::TeamSetOffWorldFlag msg;
	msg.target = receiver.playerID;
	msg.i64PlayerID = i64PlayerID;
	if (receiver.zoneID.GetCloneID() == zoneID.GetCloneID()) {
		zoneID = LWOZONEID(zoneID.GetMapID(), zoneID.GetInstanceID(), 0);
	}
	msg.zoneID = zoneID;
	ChatPacketHandler::SendRouted(receiver.playerID, receiver.worldServerSysAddr, msg);
}

void TeamContainer::CreateTeamServer(const ChatPackets::CreateTeam& request, const SystemAddress& sysAddr) {
	// Requests for more than 4 players (ChatPackets::CreateTeam::MAX_MEMBERS) were dropped when read
	const std::vector<LWOOBJID>& members = request.members;
	const LWOZONEID zoneId = request.zoneID;

	auto* team = CreateLocalTeam(members);

	if (team != nullptr) {
		team->zoneId = zoneId;
		UpdateTeamsOnWorld(team, false);
	}
}

TeamData* TeamContainer::CreateLocalTeam(std::vector<LWOOBJID> members) {
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

	newTeam->lootFlag = 0;

	TeamStatusUpdate(newTeam);

	return newTeam;
}

TeamData* TeamContainer::CreateTeam(LWOOBJID leader, bool local) {
	auto* team = new TeamData();

	team->teamID = ++g_TeamIDCounter;
	team->leaderID = leader;
	team->local = local;

	GetTeamsMut().push_back(team);

	AddMember(team, leader);

	return team;
}

TeamData* TeamContainer::GetTeam(LWOOBJID playerID) {
	for (auto* team : GetTeams()) {
		if (std::find(team->memberIDs.begin(), team->memberIDs.end(), playerID) == team->memberIDs.end()) continue;

		return team;
	}

	return nullptr;
}

void TeamContainer::AddMember(TeamData* team, LWOOBJID playerID) {
	if (team->memberIDs.size() >= 4) {
		LOG("Tried to add player to team that already had 4 players");
		const auto& player = Game::playerContainer.GetPlayerData(playerID);
		if (!player) return;
		ChatPackets::SendSystemMessage(player.worldServerSysAddr, u"The teams is full! You have not been added to a team!");
		return;
	}

	const auto index = std::find(team->memberIDs.begin(), team->memberIDs.end(), playerID);

	if (index != team->memberIDs.end()) return;

	team->memberIDs.push_back(playerID);

	const auto& leader = Game::playerContainer.GetPlayerData(team->leaderID);
	const auto& member = Game::playerContainer.GetPlayerData(playerID);

	if (!leader || !member) return;

	const auto leaderName = GeneralUtils::UTF8ToUTF16(leader.playerName);
	const auto memberName = GeneralUtils::UTF8ToUTF16(member.playerName);

	SendTeamInviteConfirm(member, false, leader.playerID, leader.zoneID, team->lootFlag, 0, 0, leaderName);

	if (!team->local) {
		SendTeamSetLeader(member, leader.playerID);
	} else {
		SendTeamSetLeader(member, LWOOBJID_EMPTY);
	}

	UpdateTeamsOnWorld(team, false);

	for (const auto memberId : team->memberIDs) {
		const auto& otherMember = Game::playerContainer.GetPlayerData(memberId);

		if (otherMember == member) continue;

		const auto otherMemberName = Game::playerContainer.GetName(memberId);

		SendTeamAddPlayer(member, false, team->local, false, memberId, otherMemberName, otherMember ? otherMember.zoneID : LWOZONEID(0, 0, 0));

		if (otherMember) {
			SendTeamAddPlayer(otherMember, false, team->local, false, member.playerID, memberName, member.zoneID);
		}
	}
}

void TeamContainer::RemoveMember(TeamData* team, LWOOBJID causingPlayerID, bool disband, bool kicked, bool leaving, bool silent) {
	LOG_DEBUG("Player %llu is leaving team %i", causingPlayerID, team->teamID);
	const auto index = std::ranges::find(team->memberIDs, causingPlayerID);

	if (index == team->memberIDs.end()) return;

	team->memberIDs.erase(index);

	const auto& member = Game::playerContainer.GetPlayerData(causingPlayerID);

	const auto causingMemberName = Game::playerContainer.GetName(causingPlayerID);

	if (member && !silent) {
		SendTeamRemovePlayer(member, disband, kicked, leaving, team->local, LWOOBJID_EMPTY, causingPlayerID, causingMemberName);
	}

	if (team->memberIDs.size() <= 1) {
		DisbandTeam(team, causingPlayerID, causingMemberName);
	} else /* team has enough members to be a team still */ {
		team->leaderID = (causingPlayerID == team->leaderID) ? team->memberIDs[0] : team->leaderID;
		for (const auto memberId : team->memberIDs) {
			if (silent && memberId == causingPlayerID) {
				continue;
			}

			const auto& otherMember = Game::playerContainer.GetPlayerData(memberId);

			if (!otherMember) continue;

			SendTeamRemovePlayer(otherMember, disband, kicked, leaving, team->local, team->leaderID, causingPlayerID, causingMemberName);
		}

		UpdateTeamsOnWorld(team, false);
	}
}

void TeamContainer::PromoteMember(TeamData* team, LWOOBJID newLeader) {
	team->leaderID = newLeader;

	for (const auto memberId : team->memberIDs) {
		const auto& otherMember = Game::playerContainer.GetPlayerData(memberId);

		if (!otherMember) continue;

		SendTeamSetLeader(otherMember, newLeader);
	}
}

void TeamContainer::DisbandTeam(TeamData* team, const LWOOBJID causingPlayerID, const std::u16string& causingPlayerName) {
	const auto index = std::ranges::find(GetTeams(), team);

	if (index == GetTeams().end()) return;
	LOG_DEBUG("Disbanding team %i", (*index)->teamID);

	for (const auto memberId : team->memberIDs) {
		const auto& otherMember = Game::playerContainer.GetPlayerData(memberId);

		if (!otherMember) continue;

		SendTeamSetLeader(otherMember, LWOOBJID_EMPTY);
		SendTeamRemovePlayer(otherMember, true, false, false, team->local, team->leaderID, causingPlayerID, causingPlayerName);
	}

	UpdateTeamsOnWorld(team, true);

	GetTeamsMut().erase(index);

	delete team;
}

void TeamContainer::TeamStatusUpdate(TeamData* team) {
	const auto index = std::find(GetTeams().begin(), GetTeams().end(), team);

	if (index == GetTeams().end()) return;

	const auto& leader = Game::playerContainer.GetPlayerData(team->leaderID);

	if (!leader) return;

	const auto leaderName = GeneralUtils::UTF8ToUTF16(leader.playerName);

	for (const auto memberId : team->memberIDs) {
		const auto& otherMember = Game::playerContainer.GetPlayerData(memberId);

		if (!otherMember) continue;

		if (!team->local) {
			SendTeamStatus(otherMember, team->leaderID, leader.zoneID, team->lootFlag, 0, leaderName);
		}
	}

	UpdateTeamsOnWorld(team, false);
}

void TeamContainer::UpdateTeamsOnWorld(TeamData* team, bool deleteTeam) {
	ChatPackets::TeamUpdate update;
	update.teamID = team->teamID;
	update.deleteTeam = deleteTeam;

	if (!deleteTeam) {
		update.lootFlag = team->lootFlag;
		update.members = team->memberIDs;
	}

	update.Broadcast();
}
