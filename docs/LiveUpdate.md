# Live updates

Moving every running server onto a new build of the server binaries without taking the server down. Players are moved
to new world instances started from the new binaries; the other servers restart one by one. Master keeps running.

Code: `dMasterServer/LiveUpdateMachine.h` (the order, no master state; unit tested), `dMasterServer/LiveUpdateCoordinator`
(master's glue), `dMasterServer/MigrationCoordinator` and `dGame/dUtilities/WorldMigration` (moving one instance's
players, see [SeamlessTransfer.md](SeamlessTransfer.md)), `dNet/master/LiveUpdate.h` (messages).

## Starting one

Put the new build in place (the binaries master starts are the ones in its own directory), then:

| Trigger | Who |
| --- | --- |
| Dashboard, home page, **Live update** card | `server_live_update` (default GM 9) |
| `/liveupdate start [warn seconds]`, `/liveupdate cancel`, `/liveupdate status` | GM 9 (paired with `server_live_update`) |
| `kill -USR2 <master pid>` (not on Windows) | whoever can signal master |

Nothing happens without one of these. Master refuses when one is already running, when it is shutting down, or when
`WorldServer`, `AuthServer`, `ChatServer` (and `DashboardServer` / `UgcServer` when enabled) are missing or empty in
the binary directory (a build still being written).

Cancelling starts nothing new; what is under way finishes.

## Sequence

1. **Database**: the new build's migrations (`MigrationRunner::RunMigrations`, `RunSQLiteMigrations`). Failure stops the
   update before anything else is touched. The running servers must cope with the new schema until they are replaced.
2. **UGC server, auth, chat** (together):
   * UGC: `LIVE_UPDATE_RETIRE`. It drops its queue (the rows stay pending in the database), finishes and records the jobs
     it is running, then exits. After `live_update_ugc_drain_timeout` it gets `SHUTDOWN` (running jobs are made again).
   * Auth: `SHUTDOWN`. Logins fail until the new one is up (a few seconds); players already in game are not affected.
   * Chat: `LIVE_UPDATE_RETIRE`. It sends its teams to master (`CHAT_HANDOFF`) and exits without logging anyone out.
   * Master starts the new process when the old one disconnects, as it always does. A server counts as replaced once a
     new one connects. One that doesn't come back within `live_update_service_timeout` is started again (3 tries).
   * When the new chat server connects, master gives it the teams, and once it is up sends `CHAT_SERVER_READY` to every
     world: each connects at once and sends its loaded players again (`LoginSessionNotify` with `resync`). The new chat
     server takes them over without logging a login, and reads their friends lists.
3. **World instances**, after chat is back and 3 s for the worlds to reconnect. Character selection first, then the
   busiest; `live_update_parallel_worlds` at a time (waiting instances don't take a slot). Instances started after the
   update began are already on the new build and are not touched. What happens to each is decided when its turn comes:

   | Instance | Plan |
   | --- | --- |
   | Nobody there | Stopped. Zones in `prestart_worlds` (and character selection) get a new instance first; the old one stops once it is ready. |
   | Public world with players | Replaced: a new instance starts, players are moved, the old one stops. |
   | Property (clone) | Saved and frozen first (`MIGRATE_PREPARE`), then replaced as above. |
   | Private instance | Replaced by a new private instance with the same password. |
   | Activity zone (any `Activities.instanceMapID`: races, minigames) | Draining: nobody new goes there; its players finish. After `live_update_activity_wait` whoever is left is moved to a new instance (the activity is lost). |
   | Character selection | A new one starts at once and takes all logins. The old one drains; after `live_update_char_select_wait` whoever is still there is moved to the new one. |

4. **Dashboard**, last: `SHUTDOWN`; master starts the new one. Sessions survive (JWT, secret in `dashboard_jwt_secret`
   or `jwt_secret`). The new dashboard asks master for the status when it connects.

## States

A world: `pending` → (`preparing`: property being saved) → `starting` (new instance launching) → `ready` (new instance
up, players warned) → `draining` (players being moved) → `stopping` (old instance shutting down) → `stopped`.
`waiting`: an activity zone or character selection waiting for its players to leave by themselves.

A server: `pending` → `stopping` (UGC: `draining`) → `starting` → `stopped` (chat: `ready` for 3 s in between).

`failed`: left as it was; a world keeps running on the old build and takes players again (a property some players
already went to stays with the new instance). `skipped`: not running, not enabled, or cancelled before its turn.

The update: `running` → `done` (possibly with failed rows), or `failed` (database migrations), `cancelling` →
`cancelled`.

The dashboard's world list shows instances being emptied as **Moving players** (`ServerListResponse` state `DRAINING`).
Master logs every change of every row (`Live update N: ...`).

## Routing during an update

`InstanceManager::FindInstance` (`InstanceMigration::AcceptsNewPlayers`) skips draining instances, so new zone requests,
logins and friend transfers go to new instances (started if needed, waiting for them to be ready). A private
instance's password finds its replacement (`FindPrivateInstance` skips draining ones). While a property is being saved
its visitors still go to the old instance, where nobody can build.

## Moving players

Each move is an instance migration (`MigrationCoordinator::Start` with `Options::liveUpdate`), see
[SeamlessTransfer.md](SeamlessTransfer.md):

* Players get the game's Mythran Maintenance Alert, then after `live_update_warn_seconds` (or the value picked for this
  update) up to 10 a second are saved, locked and sent `TRANSFER_TO_WORLD` with the Mythran shift flag.
* Dead or building players wait up to `live_update_player_wait`, then go anyway. An open trade is cancelled.
* Where they stood is carried (`CarriedPlayerState` position) and applied when the new instance creates them, also on
  properties and Moon Base where the saved character doesn't keep it. The pet that was out is summoned again.
* Character selection has no characters loaded: its users are just sent to the new one, which sends them their
  characters (no maintenance notice).

### Properties

The new instance loads the property from the database when it starts, so before it is started the old one:

1. tells builders "building on this property ends in N seconds" and waits up to `live_update_property_build_wait` for
   nobody to be building;
2. takes anyone still building out of build mode, saves the property, and freezes it: nobody can build, place, pick up,
   claim or BBB-save there, and it is never saved again (disconnects and shutdown included), so the new instance's
   saves can't be overwritten;
3. answers `MIGRATE_STATUS` `PREPARED`; master starts the new instance and the move goes on as above.

If the move is cancelled before anyone reached the new instance, the property is unfrozen.

## Messages (appended to `MessageType::Master`)

| Message | Direction | Payload |
| --- | --- | --- |
| `MIGRATE_PREPARE` | master → property world | `MigratePrepare` (migration ID, max wait) |
| `LIVE_UPDATE_REQUEST` | dashboard / world (GM) → master | `LiveUpdateRequest` (start, cancel, status; warn seconds; who) |
| `LIVE_UPDATE_STATUS` | master → dashboard; → worlds for the GM who asked | `LiveUpdateStatus` (phase, every row) |
| `LIVE_UPDATE_RETIRE` | master → chat, UGC | none |
| `CHAT_HANDOFF` | chat → master → next chat | `ChatHandoff` (teams) |
| `CHAT_SERVER_READY` | master → worlds | none |

Changed, compatibly: `MigrationStatus` states `PREPARING` and `PREPARED` (appended), `MigratePlayersOrder.maxWaitSeconds`
and `CarriedPlayerState` position (appended, read only when present), `ChatPackets::LoginSessionNotify.resync` (written
only when set), `ServerListResponse` state `DRAINING` (appended).

Master is not replaced, so the master ↔ server messages of the running master must still be understood by the new
binaries: add fields at the end and read them only when present. A change master itself needs takes a normal restart.

## Settings (`masterconfig.ini`, read when an update starts)

| Setting | Default | |
| --- | --- | --- |
| `live_update_warn_seconds` | 10 | Warning before players are moved (0-300) |
| `live_update_parallel_worlds` | 4 | World instances replaced at once |
| `live_update_player_wait` | 30 | Dead or building players, seconds |
| `live_update_property_build_wait` | 60 | Property builders, seconds |
| `live_update_char_select_wait` | 60 | Character selection, seconds |
| `live_update_activity_wait` | 1800 | Activity zones, seconds |
| `live_update_ugc_drain_timeout` | 300 | UGC server finishing its jobs, seconds |
| `live_update_service_timeout` | 30 | A server stopping or coming back, seconds (3 starts) |
| `live_update_run_migrations` | 1 | Run database migrations first |

`prestart_worlds` decides which zones always keep an instance.

## What a player sees

* In a world: the Mythran Maintenance Alert, a loading screen of the same zone, "Mythran Dimensional Shift Succeeded!",
  standing where they were. Chat history, open windows, team, friends and pet stay.
* On a property: the same; builders are told building ends first and leave build mode.
* In a race or minigame: nothing until it ends (they leave normally); only after `live_update_activity_wait` are they
  moved, losing the activity.
* At character selection: nothing, unless still there after `live_update_char_select_wait`; then a reconnect to the new
  character selection, which lists their characters again.
* Logging in: a few seconds in which auth doesn't answer (the client reports a connection error; logging in again works).
* Friends and whispers: a few seconds without them while chat restarts.

## Limitations

* **Master is not updated.** A new master would need every server to survive master's absence and re-register (instance
  table, clone/private/password, caps, player counts, session keys). Worlds shut down after 5 s without master, and the
  session keys of logged-in accounts live only in master. Updating master takes a normal restart.
* A loading screen is always shown; the experimental seamless mode of instance migrations is not used here.
* Each world's simulation starts fresh: enemies, smashables, quick builds, dropped loot and scripted events restart.
* Lost when moved: an open trade (cancelled first), build mode in progress (players wait for it first), possession and
  mounts, an activity lobby, anything else not in the saved character.
* A BBB model not saved when a property's builders are taken out of build mode is lost (the BBB autosave remains).
* Chat: messages, whispers and team invites sent in the seconds chat is down are lost. A player who logs out while chat is
  down stays in their team until it next changes.
* Auth: no second auth process on the same port (RakNet binds the port exclusively); logins pause while it restarts.
* Players already on their way to an old instance when it starts draining are moved once they arrive; one arriving after
  the old instance finished is disconnected when it shuts down (and logs in again).
* A world whose migration fails keeps running the old build; start another update (only instances started before it
  began are replaced) or shut it down from the dashboard.
* The new build's database migrations run while the old servers still run; a migration that breaks the old code breaks
  them until they are replaced.
* Transfers were not tested with the game client when this was written.
