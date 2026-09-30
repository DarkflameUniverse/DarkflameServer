# Activity matchmaking

Activity lobbies (survival, racing, Battle of Nimbus Station, the other activities whose `Activities.instanceMapID`
is another zone) are kept by the chat server, across every world. Players on different instances of a zone who join
the same activity wait in the same lobby and are sent to the same new instance of the activity's zone.

Live worked this way: in the 2014 captures of an Avant Gardens survival lobby, the lobby's other players never appear
in the joining player's world (18 other players are constructed there, none of them from the lobby), and the four
who stayed in the lobby are then all constructed together in the same survival instance (zone 1101). The chat
service's message list has `MATCH_REQUEST` (52), the world to chat message for this.

## Flow

1. The client sends the game message `MatchRequest` (type 0, value = activity ID) to its world. The world answers
   `MatchResponse` 0 (not for quickbuild activities, as before), saves the zone to come back to, and sends chat
   `ChatPackets::MatchRequest` JOIN with the player's name, the client's choices text and the activity's settings as
   the world read them (`instanceMapID`, `minTeams`, `maxTeams`, `minTeamSize`, `maxTeamSize`, `waitTime`,
   `startDelay`, with the world's overrides: `solo_racing`, `transferZoneID`). Chat needs no CDClient.
2. Ready (`MatchRequest` type 1) is answered with `MatchResponse` 0 (as live) and sent to chat as READY. The
   `LobbyExit` message box button sends LEAVE.
3. Chat keeps the lobbies (`dChatServer/Matchmaking.{h,cpp}`, no network code) and sends the `MatchUpdate` game
   messages to the clients through their worlds (`WORLD_ROUTE_PACKET`, `ClientPackets::MatchUpdate`, byte for byte the
   world's `GameMessages::MatchUpdate`).
4. When a lobby's countdown runs out, chat asks master for a new instance of the zone (the usual
   `REQUEST_ZONE_TRANSFER` with a random clone ID) and sends each world that has players of the match
   `MATCH_TRANSFER` (DLU, chat id 71) with the instance's zone, address and that world's players. The world checks
   each player's activity cost, records the activity instance and sends them `TransferToWorld`.
5. Chat makes the players a team when there are 2 to 3 of them (as the world did before: `CreateTeam` holds at most
   3 members).

A player leaves their lobby when their world reports them gone (`UNEXPECTED_DISCONNECT` from the world they are in)
or when they load into a world (`LOGIN_SESSION_NOTIFY`).

## Lobby rules

| | |
|---|---|
| Lobby key | activity ID and `instanceMapID` |
| Capacity | `maxTeams` when `maxTeamSize` is 1, else `maxTeamSize` |
| Minimum | `minTeams` when `maxTeamSize` is 1, else `minTeamSize` |
| Joining | the oldest lobby of the activity with room, else a new one with the timer at `waitTime` |
| Countdown | runs only while the lobby has its minimum; players leaving below it pause it, it does not start over |
| All ready | with the minimum, the timer drops to `startDelay` |
| Start | timer at 0: every player in the lobby goes to one instance |
| Joining another activity | leaves the first lobby |

## MatchUpdate messages

| When | To | Type | Text |
|---|---|---|---|
| Join | joining player | `PLAYER_ADDED` for themselves, then for each waiting player followed by `PLAYER_READY` if that player is ready | `[droppedItem=9:<id>\n]player=9:<id>\nplayerName=0:<name>` |
| Join | the others | `PLAYER_ADDED` | same |
| Join while counting | joining player | `PHASE_WAIT_READY` | `time=3:<seconds left>` |
| Countdown starts | everyone | `PHASE_WAIT_READY` | `time=3:<seconds>` |
| Ready / not ready | everyone | `PLAYER_READY` / `PLAYER_NOT_READY` | `player=9:<id>` |
| All ready | everyone | `PHASE_WAIT_START` | `time=3:<startDelay>` |
| Leave | everyone, the leaver included | `PLAYER_REMOVED` | `player=9:<id>` |

`droppedItem` is the racing car the client names in its `MatchRequest` choices (`droppedItem=13:<id>`); live showed
it to the lobby as an object ID before the player's line.

Differences from the world lobbies DLU had before: the joining player gets only the ready players' states (live) where
every state went to everyone; the all-ready cut needs the lobby's minimum; the ready request gets a `MatchResponse`.

## Limits

* Lobbies live in the chat server: a chat restart (live update) empties them, and players waiting have to join again.
  Without a chat server nobody can join a lobby.
