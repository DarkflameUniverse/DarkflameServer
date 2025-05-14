// Darkflame Universe
// Copyright 2025

#ifndef TEAMCONTAINER_H
#define TEAMCONTAINER_H

#include <cstdint>
#include <string>
#include <vector>

#include "dCommonVars.h"

struct Packet;
struct PlayerData;
struct TeamData;

namespace TeamContainer {
	struct Data {
		std::vector<TeamData*> mTeams;
	};

	void Shutdown();

	void HandleTeamInvite(Packet* packet);
	void HandleTeamInviteResponse(Packet* packet);
	void HandleTeamLeave(Packet* packet);
	void HandleTeamKick(Packet* packet);
	void HandleTeamPromote(Packet* packet);
	void HandleTeamLootOption(Packet* packet);
	void HandleTeamStatusRequest(Packet* packet);

	void SendTeamInvite(const PlayerData& receiver, const PlayerData& sender);
	void SendTeamInviteConfirm(const PlayerData& receiver, bool bLeaderIsFreeTrial, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, uint8_t ucResponseCode, std::u16string wsLeaderName);
	void SendTeamStatus(const PlayerData& receiver, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, std::u16string wsLeaderName);
	void SendTeamSetLeader(const PlayerData& receiver, LWOOBJID i64PlayerID);
	void SendTeamAddPlayer(const PlayerData& receiver, bool bIsFreeTrial, bool bLocal, bool bNoLootOnDeath, LWOOBJID i64PlayerID, std::u16string wsPlayerName, LWOZONEID zoneID);

	/* Sends a message to the provided `receiver` with information about the updated team. If `i64LeaderID` is not LWOOBJID_EMPTY, the client will update the leader to that new playerID. */
	void SendTeamRemovePlayer(const PlayerData& receiver, bool bDisband, bool bIsKicked, bool bIsLeaving, bool bLocal, LWOOBJID i64LeaderID, LWOOBJID i64PlayerID, std::u16string wsPlayerName);
	void SendTeamSetOffWorldFlag(const PlayerData& receiver, LWOOBJID i64PlayerID, LWOZONEID zoneID);

	void CreateTeamServer(Packet* packet);

	TeamData* CreateLocalTeam(std::vector<LWOOBJID> members);
	TeamData* CreateTeam(LWOOBJID leader, bool local = false);
	TeamData* GetTeam(LWOOBJID playerID);
	void AddMember(TeamData* team, LWOOBJID playerID);
	void RemoveMember(TeamData* team, LWOOBJID playerID, bool disband, bool kicked, bool leaving, bool silent = false);
	void PromoteMember(TeamData* team, LWOOBJID newLeader);
	void DisbandTeam(TeamData* team, const LWOOBJID causingPlayerID, const std::u16string& causingPlayerName);
	void TeamStatusUpdate(TeamData* team);
	void UpdateTeamsOnWorld(TeamData* team, bool deleteTeam);

	const TeamContainer::Data& GetTeamContainer();
	std::vector<TeamData*>& GetTeamsMut();
	const std::vector<TeamData*>& GetTeams();
};

#endif //!TEAMCONTAINER_H
