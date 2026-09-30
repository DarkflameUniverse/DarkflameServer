# Packet capture and replay

Staff record every packet of one account, one character, or everything, on all servers at once; play the recording
back on the dashboard; and replay it against a throwaway server to see how the server answers now. The same replay
takes the 2014 live captures, which makes them a conformance test for the server. The dashboard side is described in
[Dashboard.md](Dashboard.md#packet-captures); this document is how it works and the rules it follows.

## Playback across worlds

An account or character capture puts every server on one timeline, so one character's packets move from one world
server to the next when they transfer. `CaptureTools::Worlds` lists each character's world visits in time order: one
entry each time the packets their client sends (never the server's, since the old world can still send a few after
the client reached the new one) carry another zone or instance in the record header; zone 0 is character select.
The dashboard serves them with the zone's name from the client's locale (`/api/inspector/sessions/:id/worlds`, and
`worlds` in `/positions`, which takes `?zone=all` for every zone's movement). The capture page marks each change on
its slider; World 3D switches its scene to the followed character's world at that time (`world3d-core.js`
`captureSwitch`, `worldMarkers`).

## Properties in a capture

`CaptureProperty::Build` (`/api/inspector/sessions/:id/property`, `CaptureTool property`) reads the property each
world of a capture was, from its packets alone:

- **Models:** objects constructed with a model component (and brick built models, LOT 14), each with its LOT, object
  ID, spawner, blueprint (the UGC ID, from its config) and behavior count, and where it stood per time span (its
  physics component's position and rotation, from the construction and each serialization that moves it) until its
  destruction.
- **Events:** placed (a `PlaceModelResponse` saying a model was placed, with the model made at that exact position:
  this server makes it before answering, a live server after), moved (a serialization with a new position), removed
  (a destruction, with the reason from the `DeleteModelFromClient` that asked for it), editing, and behavior messages
  (those named for behaviors, and any game message sent to or from a model while it stood there).
- **Property data:** each `DownloadPropertyData` for this world's map (a live server also sent the player's other
  properties), kept once while it says the same, and each `GetModelsOnProperty`'s model count.

The dashboard adds the zone's and the models' names from the locale and the property saved now for that zone and clone
(its link, and the `?property=` that lets the UGC mesh route serve its models). World 3D draws each model where it stood
at the playhead (the UGC server's mesh for a brick built model when it has one, a box otherwise), shows the property
data of that moment in its Property tab and ticks the behavior messages on the timeline; seeking either way reads the
spans again, nothing is replayed. The capture page lists each property with its owner and every model placed, moved or
removed, each jumping to its packet. A replay of recorded positions on one property instance shows the models placed
now (`/api/world3d/property_models`), labelled as such, since position history doesn't record models.

Captures, bundles and fixtures are player data. None of them is ever committed: `captures/`, `*.bundle` and
`tests/fixtures-local/` are in `.gitignore`.

## Capturing

Staff arm a capture on the Packet Captures page (`dev_message_inspector`; arming, stopping and exporting are audited).
The dashboard sends `MESSAGE_CAPTURE_CONTROL` with the appended `ARM` action to master, which passes it to every
world, auth and chat, and arms itself. The dashboard repeats it every 10 seconds (servers that started since, and
characters the account made since, are picked up) and sends `DISARM` at the end; every server also stops on its own at
the time limit. Up to 8 captures run at once; each has one bit (its *slot*) in every record's mask.

| Target | What is recorded |
|---|---|
| Account | Its connections from its next login (or now, if online): auth, character select, every zone, chat, and its master link messages. Packets of a connection before it is known whose it is (the handshake, the login request, a world's session check) are held per connection and added once auth or the world names the account. |
| Character | The same, from when the character is picked in a world. |
| Everything | Every packet on every server's listening socket, and master's server-to-server traffic (not the dashboard's). |

Where each server taps (`dNet/PacketCapture.*`, all on the server's main thread, since RakNet isn't thread safe):

- received: `dServer::Receive` and `dServer::ReceiveFromMaster`;
- sent: a hook in `RakPeer::Send` (`g_RakPeerSendHook`, set only while armed), so replica constructions and
  serializations that RakNet's ReplicaManager sends are seen too;
- who a packet belongs to: auth binds the connection when the login names the account, worlds when the session is
  validated and when a character is picked; chat reads the player's object ID each chat packet starts with; master
  link messages are matched by account name (session keys), by request (zone transfers answered later), by the
  connection being handled when they are sent (player added and removed), and instance-wide ones (migration) go to
  every captured player in that world. A capture's own traffic (`MESSAGE_CAPTURE_*`) is never recorded.

### Secrets are never stored

Packets that carry secrets are rewritten before they are recorded, per struct: the decoder registry
(`dNet/PacketDecoder.cpp`) declares a *redactor* for each such packet, which reads it with the struct's
`Deserialize`, blanks the secret fields and writes it again with `Serialize`. A packet that declares secrets but doesn't
read is dropped, never stored as is. Today that is:

| Packet | Blanked |
|---|---|
| AUTH `LOGIN_REQUEST` | username and password |
| CLIENT `LOGIN_RESPONSE` | user key (the session key), CDN key |
| WORLD `VALIDATION` | session key |
| MASTER `SET_SESSION_KEY`, `SESSION_KEY_RESPONSE`, `NEW_SESSION_ALERT` | session key |

Auth records only the handshake and the login request and response; anything else auth sees is left out. A new packet
with a secret opts in by adding its redactor next to its decoder. `PacketCaptureTests` checks that a captured login
and session never contain the test account's password, user key or session key.

*Why not start capturing at character select?* The login is where most "can't log in" reports happen, and its
response code, stamps and timing are what staff need. With redaction by struct there is nothing secret left in it,
and the replay fills in its own account and session key anyway, so recording it costs nothing. Everything from the
world's validation on is recorded the same way.

### Buffering, flushing and overhead

Nothing is written or sent per packet. Each server appends records to one preallocated chunk; the chunk is sealed
when it reaches `capture_flush_bytes` (default 256 KB) or `capture_flush_interval_ms` has passed (default 1000), and
sealed chunks are sent to master (master sends its own straight to the dashboard) from the main loop. While master
can't take them they are kept up to `capture_buffer_max_mb` (default 16); past that the oldest are dropped, the next
batch says how many, and the dashboard writes a gap marker ("N packets lost here"). All three are shared settings
(Settings, Packet capture). With nothing armed, a received packet costs one flag check and a sent one a null check.

The dashboard appends each batch to the capture's file with one write, and saves the session row (counts, end) every
5 seconds. Measured by `PacketCaptureTest.OverheadOfCapturingEverything` (one core, unoptimised build, 300,000
position updates with EVERYTHING armed):

| | per packet | throughput |
|---|---|---|
| server tap (record, redact check, buffer) | about 580 ns | about 1.7 million packets/s, 173 MB/s of records |
| dashboard: append batch to file | about 43 ns | |
| dashboard: a SQLite row per packet, one transaction per batch (for comparison) | about 3,500 ns | |

A busy world sends a few thousand packets a second, so capturing everything there costs well under 1% of a core.
Appending to a file is about 80 times cheaper than a database row per packet, so packets go to files and the
database keeps only the session row (the game message inspector keeps its rows as before).

## Decoding

The viewer shows every packet with named fields and values, and its bytes below. Nothing is parsed twice: packets are
read with the server's own structs and their `Deserialize`.

| Packets | How |
|---|---|
| LU packets (auth, chat, client, master, world, common) | `dNet/PacketDecoder.cpp`: the struct for its (service, ID). An ID with a struct per direction is read with the first that reads the whole packet, the direction's own first. |
| Game messages, both directions | `dGame/dGameMessages/GameMessageDecoder.cpp`: the `NetGameMsg` struct for its ID (the one the server reads a client's message with, else the one it sends). |
| Replica constructions, serializations, destructions | `dGame/dUtilities/ReplicaDecoder.cpp`, below. |

The members of each struct are listed by `tools/gen_game_message_fields.py`, which writes `dNet/PacketFields.inc` and
`dGame/dGameMessages/GameMessageFields.inc` from the struct definitions; run it after changing a packet struct (the
`PacketFieldsUpToDate` test fails until then). Every member type needs a `ToJson` overload (`dNet/PacketJson.h`), or
the build fails. Members holding secrets (passwords, session and user keys) are never listed. Whole bytes a struct
leaves unread are shown as `(unread bits)`.

What isn't decoded shows its name (from the `MessageType` enums) and its bytes: LU packet IDs this server never sends
or handles (listed in `PacketDecoderCoverageTests`, which fails for any other ID without a struct), game message IDs
the server has no struct for (live-only messages), and the client's `MAIL` packet (its sub-messages are read by the
game's mail code). Every game message the server sends or reads has a struct.

### Replica packets

Replica packets depend on what came before, so a capture is read once in timeline order, on a dashboard worker
thread, and the result is kept with the loaded capture:

- **Destruction:** the network ID, and the object it was when the capture had its construction.
- **Construction:** the object header as `Entity::WriteBaseReplicaData` writes it (network ID, object ID, LOT, name,
  age, config, trigger, spawner, spawner node, scale, world state, GM level, parent and children), then each
  component's data in the order the client reads them (`Entity::WriteComponents`). Which components an object has
  comes from the ComponentsRegistry rows of its LOT, read from the CDClient at startup, with the ones the server adds
  itself (a destroyable for collectibles, quick builds and models, the character's parts, a buff with a destroyable)
  and without the item component of a model (its model component writes the item info). A compressed config (live
  servers compressed model configs) is inflated and read.
- **Serialization:** the object's LOT and components from its construction earlier on the same world instance, then
  each component's update. An object constructed before the capture started shows "object not constructed in this
  capture" and the bytes.

Each component reader mirrors the component's `Serialize(bIsInitialUpdate)` (`ReplicaDecoderTests` writes real
components with the server's serializers and reads them back). Some objects have components their LOT doesn't list
(a smashable, moving platform or script set up by the zone file): when the registry's list doesn't read the packet
exactly (to the last whole byte, padding zero), those variants are tried. When none fits, what read is shown with
`(layout did not match)` and the rest as bits; parts the server never writes (only live did, such as local space
info) stop the reader with `(... present, not read)`. Nothing is guessed.

The capture tool reads game messages the same way (it links the game), so replays compare their fields, not only their
size.

## The bundle format

One format for the dashboard's capture files, exported bundles and converted live captures (`dNet/CaptureBundle.h`):

```
"DLUBNDL1"            8 bytes: the format and its version
u32 length            little endian
metadata              UTF-8 JSON, `length` bytes
records               to the end of the file
```

Each record is a 52 byte little-endian header (`PacketRecordHeader` in `dNet/PacketRecord.h`: time in µs, per-server
sequence, capture mask, source server, direction, flags, peer, account, character, zone, instance, clone, full size in
bits, stored length) followed by the packet's bytes exactly as they went over RakNet (up to 256 KB each; longer ones
are cut and flagged). Flags: master link, broadcast, cut, gap.

Metadata keys:

| Key | |
|---|---|
| `format` | 1 |
| `origin` | `dlu-capture` or `live-2014` |
| `server` | `version`, `commit` of the server that recorded it |
| `target`, `captureId`, `startedAt`, `exportedAt`, `scenario` | where it came from |
| `zones` | map id -> `mapChecksum` from its `LOAD_STATIC_ZONE`s |
| `fdbChecksum` | the client data checksum from `VALIDATION` |
| `portable`, `anonymised` | see below |
| `ids` | `char#1`, `account#1`, ... -> placeholder |
| `setup.characters` | per character: `symbol`, `placeholder`, `name`, `xml` (its saved character XML, without the account) |

## Portability

A bundle made on one server replays on another (a copy, a fresh install, a friend's server):

- **IDs are symbolic.** Exporting replaces each captured character's object ID, wherever it appears in a packet's
  bytes and in the headers, with a placeholder (`0x1FEDC00000000000 + n`, listed as `char#n` in `ids`); accounts
  become `account#n`. The replay maps placeholders to the IDs the target gave the characters it made. Object IDs the
  server makes (spawned objects, loot) are learned during the replay by pairing the target's replica constructions
  with the recorded ones by LOT and order.
- **Setup travels with it.** `setup.characters` holds what the target needs to make the characters: their saved
  XML (appearance, level, stats, inventory, missions, flags), from the database for DLU captures and from
  `CREATE_CHARACTER` for live ones. Account names and secrets are never in a bundle; the replay uses its own account.
- **Mismatches are reported, not diffed.** The bundle names the server version and commit it was recorded on, and the
  zone and client data checksums. The replay report lists zones whose checksum differs on the target, so different
  data isn't read as a server bug.
- **Anonymised** bundles (`Export anonymised`, `CaptureTool anonymise`) also replace character names and what players
  typed (chat, whispers, character names in lists) with as many `x`, so packet sizes stay the same.

## Replaying

`CaptureTool` (built next to the servers) replays bundles:

```
CaptureTool replay <bundle>... --client <game client folder> [--cdserver <CDServer.sqlite>] [--mode setup|as-is]
                               [--speed 4] [--port 41000] [--sandbox-root <dir>] [--keep | --keep-on-failure] [--report <file.json>]
CaptureTool import-live <folder> <out-dir>      convert live captures
CaptureTool anonymise <in> <out>                make a fixture
CaptureTool info|decode <bundle> [--cdserver <CDServer.sqlite>]   look inside (replica packets too with --cdserver)
CaptureTool property <bundle> --cdserver <CDServer.sqlite>        the properties it saw (JSON)
```

### The sandbox

Every replay runs in its own sandbox and never touches a real game database:

- The tool makes a folder (under `--sandbox-root`, default the system's temporary folder), copies the server binaries
  into it (they read their settings and database from their own folder), links the migrations and navmeshes, and
  copies `CDServer.sqlite`. The game client's files are shared read-only.
- The settings are rewritten there: `replay_sandbox=1`, `database_type=sqlite`, a fresh
  `sqlite_database_path=resServer/sandbox.sqlite`, the live database's path as `replay_live_sqlite_path`, ports from
  `--port` on (master, auth +10, chat +20, worlds +100), prestarted servers, no dashboard. The tool reads the
  settings back as the servers will and refuses to start if any of them didn't take, and clears the environment
  variables that could override them.
- **The sandbox database is always SQLite**, a new file per replay, whatever the source or target server normally
  runs on; there are no throwaway MySQL schemas.
- **Enforced by the servers**: with `replay_sandbox=1` every server refuses to connect to a database that isn't SQLite,
  isn't inside its own folder, or is the file named by `replay_live_sqlite_path` (`Database::Connect`).
- The tool runs itself inside the sandbox (`sandbox-setup`, which also refuses to run without `replay_sandbox=1`) to
  apply the migrations, make the replay account and, in `setup` mode, the bundle's characters; then starts master and
  waits for auth. Afterwards the whole stack is stopped (one process group) and the folder deleted (`--keep`,
  `--keep-on-failure` keep it). Nothing from a sandbox is merged anywhere.
- `replay-target` replays against a server you run yourself; it refuses unless given `--i-know-this-is-not-a-sandbox`,
  and says loudly that it isn't one. Never point it at a live server.

### The fake client

A headless RakNet client (`dCaptureTool/FakeClient.*`). The recording is split into connections (each starts with
the client's `VERSION_CONFIRM`). It logs in on the target itself when the recording has no login, and picks the
character itself when the recording starts in a zone (with a new plain character, made by the server's own character
creation, when the bundle has no character data). Before each client packet goes out it fills in what must differ:
the target account and password, the session key the target's auth gave, and the target's IDs. Timing follows the
recording (4 times faster by default, at most 3 seconds between packets), and it waits for what a real client waits
for: the handshake answer, the character list, the zone, and, before each packet, the answer the recorded server had
sent just before it (10 seconds the first time; an answer the target never sends isn't waited for again).

### The diff

Only what the server answered (auth and world packets to the client) is compared. Each recorded answer pairs with the
target's next answer of the same name (constructions: the same LOT). Paired answers compare by their decoded fields,
leaving out what legitimately differs between runs: object and request IDs, handles, timestamps and stamps, instance
and clone IDs, server addresses and ports, player IDs and account names (`CaptureTools::IsVolatileField`); packets
without decoded fields compare by size. The report counts same, different, missing and extra answers by name, with
examples, plus notes (answers waited for in vain, zone data that differs).

## Live captures

`CaptureTool import-live <folder> <out-dir>` converts every folder of `*_traffic.zip` under `<folder>` (the 2014
captures as extracted from the packet capture archives: one packet per `.bin`, named
`<n>_<from port>-<to port>[_<part>|_joined]_[<header bytes>]...bin`) into one bundle per folder, zips in play order
(auth, char, world, world1, ...). Split packets are taken from their joined file. Only those zips are read: raw
`.pcap` files and encrypted captures (key files, XML exports) are left alone. Secrets are removed as when capturing,
and `CREATE_CHARACTER` gives the setup section. Converted bundles stay local like any other.

### Results against this branch

<!-- results: filled from the replay run below -->

## Local fixtures

Export a capture anonymised (or `CaptureTool anonymise`) and put it in `tests/fixtures-local/` (never committed).
`CaptureFixtureTests.RecordedPacketsRoundTrip` (in `dGameTests`) reads every packet of every fixture whose struct the
decoder registry knows and checks it writes back to the same bytes; without fixtures it is skipped.

## To check in game

- Arm an account capture, log in with a real client: the auth, character select and zone packets appear on one
  timeline, the login request shows a blank username and password, and chat (a whisper, a friend request) shows up
  from the chat server.
- Arm a character capture before picking another character of the same account: nothing is recorded until the
  captured character is picked.
- Arm everything on a busy test server for a minute: no stutter; the capture's size grows about once a second.
- Play a capture with movement back and open World 3D: the player moves with the playhead.
- Capture a character that changes worlds (a rocket or a portal): the capture page's slider has a mark at each change
  named after the new zone; in World 3D, following the character switches the scene at the mark and the camera stays
  on them, forward and when seeking back.
- In a capture of a zone load: every packet shows fields; constructions list their components (no
  `(layout did not match)` on players, enemies, smashables, NPCs); a later `ID_REPLICA_MANAGER_SERIALIZE` of an enemy
  hit in the capture shows its new health; game messages both ways (a skill, an emote, a vendor purchase) show fields.
- Capture a property visit where you place, pick up and put away a model, change the name or privacy, and run a
  model's behavior: the capture page lists the property and its owner with each placement and removal (clicking one
  selects its packet); in World 3D the models stand where they were at the playhead (brick built ones as the UGC
  server made them), appear and vanish at those moments forward and when seeking back, the Property tab's name,
  privacy and model count change when the update arrives, and the behavior messages are ticks on the timeline.
- Replay recorded positions on one property instance: its placed models show, marked as how they are now.
- Export a bundle, replay it with `CaptureTool replay`, and open a kept sandbox's logs.
