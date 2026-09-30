# Moving players between world instances

How the LEGO Universe client (1.10.64, `legouniverse.exe`) switches world servers, what that allows, and how
DarkflameServer uses it to **replace** an instance (live updates) or **merge** two quiet instances of a zone.

Addresses are from `legouniverse.exe` 1.10.64 in Ghidra; the functions below are named and commented there, with
bookmarks in the `SeamlessTransfer` category. Message layouts were checked against the client first, then
lu_packets.

## Short answer

* A normal transfer always shows a **loading screen**. `TRANSFER_TO_WORLD` makes the client connect to another world
  server, and the `LOAD_STATIC_ZONE` that server sends tears the scene down and loads the zone again. There is no
  "same zone already loaded" shortcut in the client.
* The **Mythran shift** flag is not an animation. It is LU's own maintenance feature: after connecting it shows
  "Mythran Dimensional Shift Succeeded! You have been moved to a new dimension to continue playing." The matching
  warning ("Mythran Maintenance Alert! The Mythrans have detected some problems in this dimension...") is sent by the
  server as a localized announcement. Live used this to evacuate instances.
* Moving within the same zone keeps the player where they stood. Merging instances therefore works, but the player sees
  the warning, a short loading screen, and the "shift succeeded" notice.
* **A transfer without a loading screen looks possible.** The client's replica code can **adopt** an object it already
  has for a new server's construction, and it keeps its own player object when the old server takes its ghost away. So
  a new server can take over if it skips `LOAD_STATIC_ZONE`. This is built as an opt-in experiment (see below) and has
  **not been tried with the real client yet**.

## What the client does (1.10.64)

### TRANSFER_TO_WORLD (client message 0x0e)

`PacketHandler_MSG_CLIENT_TRANSFER_TO_WORLD` @ `00b32c60`, packet `ClientTransferToWorldPacket`:
`char[33] ip`, `u16 port`, `bool mythranShift` (lu_packets calls the flag `is_maintenance_transfer`).

* `ip[0] == 0` means the transfer failed, and `port` is the reason: 0 "No empty servers available", 1 "Map couldn't
  load", 2 "Target specific instance was full", anything else "Non-specific failure". It is only shown in chat.
* Otherwise the client closes its world connection (`LwoNetClient::CloseConnection` @ `00a32080`, with a disconnection
  notification), clears `worldNetID` (`LwoNetClient::ResetNetID` @ `00b213c0`) and calls
  `LwoNetClient::ConnectToWorldServer(ip, port, requestCharacterList = false)` @ `00b325e0`.
* If the connection attempt starts and `mythranShift` is set: it hides the announcement `UI_INSTANCE_LOCKED_ANNOUNCE_TITLE`
  and shows `ToggleAnnounce` with the title `UI_INSTANCE_LOCKED_TRANSFERRED_TITLE` and message
  `UI_INSTANCE_LOCKED_TRANSFERRED_BODY`.

Nothing else happens yet. The old scene stays on screen and nothing is deleted. `ConnectToWorldServer` only posts a UI
network state message.

### Connecting to the new world

* `LwoNetClient::HandleServerConnectionType` @ `00b2df70`: once the version handshake with a world (service type 4)
  succeeds, it calls `WorldValidation` @ `00b2dc00`.
* `WorldValidation` sends `MSG_WORLD_CLIENT_VALIDATION` (username, session key, cdclient.fdb checksum). It sends
  `MSG_WORLD_CLIENT_CHARACTER_LIST_REQUEST` only when `requestCharacterList` (`LwoNetClient+0x79`) is set. The
  character select screen sets it; a transfer does not. **After a transfer the client waits for the server to act.**
* `PacketHandler_MSG_CLIENT_LOAD_STATIC_ZONE` @ `00b2f170` resets everything without any checks:
  `ResMgr2EnableLoadScreen`, stop game rendering, `DeleteAllGameObjects`, `RenderDumpAll`, audio flush,
  `LevelResetEnvironment`, `ResMgr2LoadZone`, and the zone's mixer program. `LoadThread_Run` @ `0105ccf0` loads the zone
  from scratch every time (a second request while one is loading gets queued). Afterwards the client sends
  `MSG_WORLD_CLIENT_LEVEL_LOAD_COMPLETE`.
* `PacketHandler_MSG_CLIENT_CREATE_CHARACTER` @ `00b2eb40` creates the local player (`LoadObject` with
  `isLocalPlayer`) from the LDF (objid, template, position, rotation, xmlData), then sends
  `ResMgr2NotifyLevelLoadComplete` and `UIReadyInGameUI`. **If an object with that ID already exists or is waiting to
  load, the packet is ignored.**
* `ServerDoneLoadingAllObjects` (in `LWOCharacterComponent::SendMessage` @ `00d34330`) moves the loading screen to its
  last phase. The client then sends `PlayerLoaded`, and the server answers with `PlayerReady`.

### Replicas: taking over existing objects

* `LwoClientGhostManager::OnReceiveConstruction` @ `00b31c30`: a construction for an object ID that already exists,
  or is waiting to load, does **not** create a second object. A new ghost is registered, and the existing object gets
  `UpdateFromGhost` followed by `OnUnserialize(construction)`. This is also how the local player made by
  `CREATE_CHARACTER` receives its replica.
* `LWOGhostComponent::OnGhostReceiveDestruction` @ `00c99410`: an object can hold several ghosts. When the last one is
  destroyed, other objects are deleted but **the local player is kept** (the code only clears `pGhost`).
* When the world connection closes, nothing is deleted. `ReplicaManager::RemoveParticipant` @ `00a478d0` leaves the
  objects alone.

### Connection loss vs. a transfer

`LwoNetClient::OnConnectionDropped_World` @ `00b22fb0` shows `NET_CONNDROP_WORLD_LOST`, `..._TIMEOUT` or
`..._REFUSED`, clears the character ID, and without a new connection shuts the network down and returns to the login
screen. `MSG_SERVER_DISCONNECT_NOTIFY` @ `00b30590` gives each disconnect reason its own message. A transfer avoids all
of this because the client clears `worldNetID` before the old connection closes.

### The warning announcement

`LocalizedAnnouncementServerToSingleClient` (game message 1580). The client reads it in
`GameMessage::LocalizedAnnouncementServerToSingleClient::Deserialize` @ `00f23c50`:

1. body LDF (`u32` length, then UTF-16 with a terminator)
2. `bit` forceOpenChatbox, `bit` showAnnouncementUI, `bit` showTextInChatbox
3. body (`u32` length, then UTF-16)
4. title (`u32` length, then UTF-16)
5. title LDF

`LWOTranslator::LocalizeWithLdf` @ `010e0430` looks the body and title up as locale phrase IDs and falls back to the
text itself. That makes `UI_INSTANCE_LOCKED_ANNOUNCE_TITLE` / `..._BODY` the game's own maintenance alert.

### Other client-side state

The Flash UI keeps its state across a transfer: chat history, the open chat and friends windows, and the minimap.
Friends, the team and guild live on the chat server, which the world forwards to. The client does not reset them
itself; the new world sends them again. Anything tied to objects is rebuilt: the pet, missions shown in the UI,
buffs, and possession.

### What live servers sent (packet captures)

These are live captures of 242 world connections from several players (the lcdr capture archive). They were read
locally, and none of it is in this repository. They agree with the client:

* **`TRANSFER_TO_WORLD`**: 168 captured. Every one is 44 bytes (8-byte header, `char[33]` ip, `u16` port, `u8`
  flag). All 168 have the Mythran shift flag at 0, and none were failures (empty ip). Ports were 2001-2009, one world
  server each. No maintenance transfer (flag 1) and no `LocalizedAnnouncementServerToSingleClient` (1580) is in the
  captures.
* **What followed a transfer on the old connection**: nothing in 89 of them. The rest show replica destructions (`0x25`,
  about 2,500) and disconnection notifications (`0x13`). Live took its objects away from the leaving client, but only
  after the transfer, when the client had already left. The experimental seamless mode sends them before it.
* **Every new world connection** started the same way: handshake, `MSG_WORLD_CLIENT_VALIDATION`, then at once
  `LOAD_STATIC_ZONE`. After that came the client's `LEVEL_LOAD_COMPLETE`, `CREATE_CHARACTER`, `SERVER_STATES` and the
  replica constructions. Some clients sent a few game messages in between.
  * A character list request appears in only 6 connections, the ones at character select. So after a transfer the
    client did not ask for its characters, as `WorldValidation` predicts.
* **Transfers within the same map**: 4 transfers went to another instance of the same map, all property → property
  (map 1150, different clones). Live still sent `LOAD_STATIC_ZONE`, a full reload. No live transfer kept the scene.

lu_packets' test packet `src/world/client/tests/TransferToWorld.bin` shows the same shape (ip `"171.20.35.42"` with
the rest of the 33 bytes uninitialised, port 2005, flag 0). lcdr-utils' `found.zip` has only replica constructions.

## What DarkflameServer does

### Normal mode (loading screen)

1. **A GM** (level DEVELOPER) in the instance types `/replaceinstance [warn seconds] [seamless]` or
   `/mergeinstance [target instance, 0 = best fit] [warn seconds] [seamless]`. The world sends `INSTANCE_MIGRATE` to
   master. Anything else connected to master (a web dashboard later) can send the same message.
2. **Master** (`dMasterServer/MigrationCoordinator`) checks the source. Character selection, private instances,
   properties/clones and activity zones (any `Activities.instanceMapID`) are refused, as is an instance already taking
   part in a migration.
   * *Replace*: starts a new instance of the zone. It runs the world binary on disk now, so an updated server takes
     effect.
   * *Merge*: uses the chosen target or picks one (`PickMergeTarget`). The target has to fit everyone under its hard
     cap; one that stays under the soft cap is preferred, and then the fullest.

   The source is marked **draining** (`FindInstance` skips it, so nobody new is sent there). Seats for its players are
   **reserved** on the target (`IsFull` counts them). Once the target is ready, master sends `MIGRATE_PLAYERS` to the
   source.
3. **Source world** (`dGame/dUtilities/WorldMigration`):
   * Sends everyone the Mythran Maintenance Alert, then waits `warnSeconds`.
   * Moves up to 10 players a second. Each player's open trade is cancelled (nothing changes hands) and their character
     is saved, including position. The zone stays the same and the target scene is cleared, so they land where they
     stood.
   * The player is then **locked**: packets from them are ignored, and the disconnect does not save them again. Nothing
     they do after the save can be lost or duplicated, and a late save cannot overwrite what the target saves.
   * The pet that was out is sent on through `MIGRATE_PLAYER_STATE`, and the player gets `TRANSFER_TO_WORLD` with
     `mythranShift`.
   * Dead players and players in build mode wait up to 15 s.
   * A client still connected 20 s after its transfer is disconnected; it is already saved.
   * Progress goes to master every second (`MIGRATE_STATUS`). The migration is done once nobody is left and everyone
     sent away has disconnected.
   * Master passes each status on to every world. The world where the GM who asked is now tells them in chat each time
     the state changes.
4. **Target world**: a normal login (validation, `LOAD_STATIC_ZONE`, level load, `CREATE_CHARACTER`, construction). On
   `PlayerLoaded` it summons the carried pet.
5. **Master** releases the reservation and shuts the source down, unless `keepSource` was set. If anything fails, the
   source stops draining and a fresh target nobody reached is shut down again. If the target stops, the source is told
   to stop moving players.
6. **Chat**: the old world reports the disconnect, and the chat server waits 20 s before removing the player (which
   would also take them out of their team). The new world's login notice cancels that. Friends and the team carry over
   as long as loading takes less than 20 s.

### Experimental seamless mode ("Without loading screen")

Built from the replica behaviour above:

* **Source**: after saving and locking the player, it sends a destruction for every object it sent them
  (`DestructAllEntities`) and then `TRANSFER_TO_WORLD` without `mythranShift`. Both go out on the same ordered channel,
  so the destructions arrive first. The client deletes those objects but keeps its player, terrain and UI.
* **Target**: the carried state is marked seamless. When that character's session is validated, the target skips
  `LOAD_STATIC_ZONE` and runs the level-load step at once (`LoadPlayer`): it creates the player entity, sends
  `CREATE_CHARACTER` (which the client ignores because the object exists), and constructs the player and everything
  else. The client adopts its existing player object for the new ghost. One second later the server does what the
  client's `PlayerLoaded` would have triggered, because the client never sends it again.
* **Untested**:
  * how the client reacts to `ServerDoneLoadingAllObjects` / `PlayerReady` without a load in progress
  * the UI state `ConnectToWorldServer` posts
  * the camera reset in `PlayerReady`
  * whether objects visibly pop out and back in

  If it goes wrong the worst case should be a stuck client that has to log in again. The character is already saved.

### What carries over

| Carried by | State |
| --- | --- |
| Character XML, saved before the transfer | position/rotation (same zone), health/armor/imagination, buffs (not `cancelOnZone` ones), inventory, missions, flags, currency, stats |
| `MIGRATE_PLAYER_STATE` | the active pet (summoned again) |
| Chat server | team, friends, guild (within the 20 s grace) |
| Client UI | chat history, open windows |
| **Lost** | an open trade (cancelled first), build mode in progress, possession/mounts, an activity lobby, dropped loot on the ground, things NPCs or enemies were doing (the target's world simulation is separate) |

### Messages (appended to `MessageType::Master`; nothing renumbered)

| Message | Direction | Payload |
| --- | --- | --- |
| `INSTANCE_MIGRATE` | world (GM command) or a future dashboard → master | `InstanceMigrationRequest` |
| `MIGRATE_PLAYERS` | master → source world | `MigratePlayersOrder` (target port 0 = cancel) |
| `MIGRATE_STATUS` | source world → master → every world | `MigrationStatus` (carries the requester's character ID) |
| `MIGRATE_PLAYER_STATE` | source world → master → target world | `CarriedPlayerState` |

All structs are in `dNet/InstanceMigration.h`, together with the pure planning functions (`CheckSource`,
`CheckMergeTarget`, `PickMergeTarget`, `SuggestMerges`, `DecidePlayer`). They are unit tested in
`tests/dCommonTests/InstanceMigrationTests.cpp`. The values are appended after `NEW_SESSION_ALERT`; a branch that also
appends there has to put one set after the other when merged.

### Live updates

A live update ([LiveUpdate.md](LiveUpdate.md)) replaces every instance with these migrations
(`MigrationCoordinator::Options::liveUpdate`). It also moves what the commands refuse: character selection (its users
are sent to the new one), private instances (the replacement gets the same password) and activity zones (after their
players had time to finish). Properties are not moved: building in progress there isn't saved, so they keep running on
the old build until everyone left (see [LiveUpdate.md](LiveUpdate.md), Properties). `MIGRATE_PREPARE` (save and freeze
a property first) remains in the protocol but live updates and world reloads no longer send it. The player's position
is carried in `CarriedPlayerState` and applied when the target creates them, so Moon Base keeps it too.

### Controls

* `/replaceinstance [warn seconds, 0-300, default 10] [seamless]`
* `/mergeinstance [target instance, 0 = best fit] [warn seconds] [seamless]`

Both need DEVELOPER, act on the instance the GM is in, and report progress to the GM in chat.
`SuggestMerges` is there for a dashboard to offer merges; nothing uses it yet. A dashboard only has to send
`INSTANCE_MIGRATE` and listen for `MIGRATE_STATUS`.

### Limits

* Normal mode always shows a loading screen, because the client has no way around it once `LOAD_STATIC_ZONE` is sent.
  Staying in the same zone keeps it short: the zone files are warm in the OS cache and the target is already running.
* Only public, non-clone, non-activity instances can be moved.
* The target world starts its own simulation. Enemies, smashables, quickbuilds and dropped loot are the target's, not
  the source's.
* The team survives only when loading takes less than the chat server's 20 s grace.
* Players still loading into the source when it starts draining are moved once they have loaded.
