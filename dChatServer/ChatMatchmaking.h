#ifndef CHATMATCHMAKING_H
#define CHATMATCHMAKING_H

#include "dCommonVars.h"
#include "Matchmaking.h"

struct SystemAddress;

namespace ChatPackets {
	struct MatchRequest;
}

/**
 * The chat server's activity matchmaking (docs/Matchmaking.md): lobbies across every world. Worlds send the players'
 * lobby joins, readies and leaves; lobby updates go to the clients through their worlds; when a countdown runs out
 * master starts one instance of the activity's zone and each player's world is told to send them there.
 * Runs on the chat server's main thread only.
 */
namespace ChatMatchmaking {
	void HandleMatchRequest(const ChatPackets::MatchRequest& request, const SystemAddress& sysAddr);

	// The player left their world (logged out, or loaded into another one): they leave their lobby
	void PlayerLeftWorld(LWOOBJID playerID);

	// Once a frame: the lobby countdowns
	void Update(float deltaTime);

	// Starts the instance of a formed match and tells the players' worlds (also used by tests)
	void StartMatch(const Matchmaking::Match& match);

	Matchmaking::Lobbies& GetLobbies();
}

#endif // CHATMATCHMAKING_H
