# Packet and Game Message Architecture

This document describes how DLU reads and writes network packets and game messages, the struct-based
architecture everything is being moved to, and the plan for getting there.

**Hard rule: the wire format never changes as part of a conversion.** The LEGO Universe 1.10.64 client depends
on every message ID, enum value, field order, field width and bit of padding. A conversion may only change how
the server *produces* the bytes, never the bytes. Every conversion proves this with byte-equality tests against
the old code before the old code is deleted (see [Verification](#3-verification)). Where DLU's bytes are known to
be wrong, the fix is its own clearly labelled change, verified against the client, never part of a conversion.

Sources for layouts, in order of trust: the 1.10.64 client in Ghidra, then lu_packets, then lcdr-utils packet
definitions. When they disagree, the client wins.

## 1. Where we are

### 1.1 Final state

Every packet and every game message is a struct. The migration in [section 4](#4-migration-plan) is complete.

| Family | Base | Where | Dispatch |
|---|---|---|---|
| Game messages (wire) | `GameMessages::NetGameMsg` | 17 per-domain files `dGame/dGameMessages/<Domain>Messages.{h,cpp}`: Activity, Building, Combat, Effects, Inventory, Mission, Movement, Object, Pet, Player, Property, QuickBuild, Racing, Skill, Trade, Vendor, Zone (about 280 structs) | `g_MessageHandlers` in `GameMessageHandler.cpp` (118 inbound messages); no switch. Unknown IDs are logged at debug level, messages that fail `Deserialize` are logged and dropped. |
| Game messages (internal) | `GameMessages::GameMsg`, `NetGameMsgEvent<Msg>` | `GameMessages.h` (base types and the server-internal events only) | `Entity::RegisterMsg` / `HandleMsg` |
| Packets | `LUBitStream` | one file pair per `ServiceType` in `dNet/` (`CommonPackets`, `AuthPackets`, `ChatPackets`, `WorldPackets`, `ClientPackets`, `MasterPackets` + `master/*`), `Stamps`, `WorldRoutePacket`, Mail in `dGame/dUtilities/Mail.*` | per-service maps and `PacketDispatcher` |

`GameMessages.cpp` holds only the base code (`GameMsg::Send`, `NetGameMsg::WritePacket`/`ReadPacketHeader`/`Send`/
`SendToClient`/`BroadcastExcept`). There are no `GameMessages::Send*`/`Handle*` functions left. Inbound messages are
handled by the struct's `Handle`, which delegates to a component method where the behaviour belongs to one
component (for example `PossessorComponent::OnDismountComplete`, `InventoryComponent::On*`) or keeps logic that
spans several entities (platform resyncs, rails, activities) in the handler.

Removed: the `CBITSTREAM`, `CMSGHEADER`, `CINSTREAM`, `CINSTREAM_SKIP_HEADER`, `SEND_PACKET`, `SEND_PACKET_BROADCAST`
and `HEADER_SIZE` macros, the free `BitStreamUtils::WriteHeader` (use `LUBitStream::WriteHeader`) and `PacketUtils`.
The frozen oracles in `tests/**/Legacy/` still use the macros verbatim through the test-only
`tests/dGameTests/LegacyPacketMacros.h`.

`ChatPackets::SendSystemMessage` stays as a thin helper: it builds the general chat struct and sends it.

### 1.2 What still touches raw bytes, and why

| Where | What | Why it stays |
|---|---|---|
| `AuthServer`, `ChatServer`, `MasterServer`, `WorldServer`, `dServer` `HandlePacket` | `packet->data[0]` compared with RakNet IDs (`ID_USER_PACKET_ENUM`, `ID_DISCONNECTION_NOTIFICATION`, `ID_CONNECTION_LOST`, `ID_NEW_INCOMING_CONNECTION`, ...) | RakNet's own connection messages, not LU packets. Everything after the RakNet ID is read with `LUBitStream::ReadHeader` and a struct. |
| `EntityManager` | `ID_REPLICA_MANAGER_CONSTRUCTION`/`SERIALIZE`/`DESTRUCTION` headers written before the components | Replica serialization, out of scope (see below). |
| `dGame/dBehaviors/*`, the `sBitStream` of skill messages | Behavior bit streams | The skill payload is its own format, carried as bytes inside the skill structs. |
| `MessageInspector` | Copies the payload bytes of sent/received game messages | A capture tap; the header is read with `NetGameMsg::ReadPacketHeader`. |

Out of scope: replica/component serialization (`Component::Serialize`) and LDF/AMF, which are separate formats
with their own tests.

Before the migration (survey of `origin/main` @ 129199e4) there were 161 hand written `GameMessages::Send*`
functions, 101 `Handle*` functions behind a 112-case switch, about 120 hand written packet functions across the
servers, 90 raw header writes and 58 `CINSTREAM` / 29 `packet->data[i]` reads.

### 1.3 Inconsistencies inside the new style, and how they are settled

| # | Inconsistency | Decision |
|---|---|---|
| 1 | Three packet base designs in flight: `LUBitStream` on main, the unmerged `auth-packet-re-write` redesign (per-service bases), the old local `packet-refactor` branch (`dNet/packets/*`). | **`LUBitStream` as on main.** The other branches are not used. |
| 2 | `GameMessageHandler` ignored the result of `Deserialize`. | **Drop and log** messages that fail to deserialize (PR 0). |
| 3 | The same `GameMsg` type was used for wire messages and server-internal events; `msg.Send()` and `msg.Send(sysAddr)` went to different places. | **Split** into `GameMsg` (internal) and `NetGameMsg` (wire); mixing them is a compile error (PR 0). |
| 4 | Registration is ad hoc: one GM map + switch, a Mail map, switches or if-chains in every server. | One map per message family (see 2.1, 2.2). |
| 5 | Struct locations: 34 structs in the 1000-line `GameMessages.h`, 6 one-off headers, Mail in `Mail.h`, chat structs partly outside any namespace. | Per-domain files (see 2.3). |
| 6 | Naming: some fields use the client's names, some don't; stream parameters are `stream`/`bitStream`/`bitstream`/`inStream`; some overrides lack `override`. | Client names, `bitStream`, always `override` (new code; old structs as they are touched). |
| 7 | Optional ("default flag") fields written by hand; this produced a wire bug in `UnSmash` (flag checks `duration`, value guarded by `builderID != 3.0f`). | `BitStreamUtils::WriteOptional`/`ReadOptional` (PR 0). `UnSmash` fixed in its own wire-change commit. |
| 8 | Length-prefixed strings written with per-character loops. | `BitStreamUtils::WriteLengthPrefixed`/`ReadLengthPrefixed` (PR 0). |
| 9 | 73 `Send*` functions broadcast and then call `Send(UNASSIGNED, broadcast = false)`, which RakNet rejects (harmless). | Goes away with `NetGameMsg::Send`. |
| 10 | `dServerMock::Send` kept a pointer to the caller's stack `BitStream`; `LUBitStream` had virtual functions but no virtual destructor while dispatchers delete through the base. | Fixed in PR 0. |

## 2. Target architecture

### 2.1 Game messages

There are two kinds of game message and they are different types:

- **`GameMessages::NetGameMsg`** - goes on the wire. `Send(sysAddr)` (UNASSIGNED broadcasts), `SendToClient(sysAddr)`
  (one client only, never broadcasts; used where the old function only did `SEND_PACKET`), `WritePacket`,
  `Serialize`, `Deserialize`, `Handle(Entity&, sysAddr)`, `requiredGmLevel`.
- **`GameMessages::GameMsg`** - a server-internal event. `Send()` / `Send(target)` deliver it to the handlers
  entities, components and scripts registered with `RegisterMsg`. It cannot reach a client.
- **`GameMessages::NetGameMsgEvent<Msg>`** (aliases such as `RequestUseEvent`, `DropClientLootEvent`) wraps a copy of
  a wire message so local handlers can react to it. `GameMessages::DeliverLocally(msg)` sends one to `msg.target`.

Every wire message is a `NetGameMsg` subclass in a per-domain file `dGame/dGameMessages/<Domain>Messages.{h,cpp}`
(pilot: `ActivityMessages`). Fields use the client's names and are declared in wire order. Every wire message
implements **both** `Serialize` and `Deserialize`, even if DLU only uses one direction, so it can be round-trip
tested.

```cpp
// dGame/dGameMessages/ActivityMessages.h
namespace GameMessages {
	// Server -> client.
	struct ShowActivityCountdown : public NetGameMsg {
		ShowActivityCountdown() : NetGameMsg(MessageType::Game::SHOW_ACTIVITY_COUNTDOWN) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bPlayAdditionalSound{};
		bool bPlayCountdownSound{};
		std::u16string sndName{};
		int32_t stateToPlaySoundOn{};
	};

	// Client -> server.
	struct RequestActivityExit : public NetGameMsg {
		RequestActivityExit() : NetGameMsg(MessageType::Game::REQUEST_ACTIVITY_EXIT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bUserCancel{};
		LWOOBJID userID{};
	};
}

// dGame/dGameMessages/ActivityMessages.cpp
void ShowActivityCountdown::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(bPlayAdditionalSound);
	bitStream.Write(bPlayCountdownSound);
	BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, sndName);
	bitStream.Write(stateToPlaySoundOn);
}

bool ShowActivityCountdown::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(bPlayAdditionalSound));
	VALIDATE_READ(bitStream.Read(bPlayCountdownSound));
	VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, sndName));
	VALIDATE_READ(bitStream.Read(stateToPlaySoundOn));
	return true;
}
```

Sending replaces the `Send*` free function:

```cpp
GameMessages::ShowActivityCountdown countdown;
countdown.target = self->GetObjectID();
countdown.Send(sender->GetSystemAddress()); // UNASSIGNED_SYSTEM_ADDRESS broadcasts
```

Inbound messages are registered with one line in `GameMessageHandler.cpp`; the GM-level gate comes from the
struct, and a message whose `Deserialize` fails is logged and dropped before `Handle`:

```cpp
std::map<MessageType::Game, MessageCreator> g_MessageHandlers = {
	...
	{ REQUEST_ACTIVITY_EXIT, []() { return std::make_unique<RequestActivityExit>(); } },
};
```

When every case is converted the switch is deleted and `GameMessages.cpp` goes away.

### 2.2 Packets (`LUBitStream`, as on main)

Every non-game-message packet is an `LUBitStream` subclass. The base owns the full header:

- `LUBitStream(ServiceType, packetId)` - `WriteHeader` writes `0x53`, the `ServiceType` (u16), the packet id (u32)
  and one pad byte; `ReadHeader` reads and checks the same.
- `WritePacket(bitStream)` = `WriteHeader` + `Serialize`; `Send(sysAddr)` sends that (`UNASSIGNED` broadcasts via
  `Broadcast()`). Tests use `WritePacket`.
- Leaf structs only (de)serialize their own fields. A feature with its own sub-header (Mail's `eMessageID`) has an
  intermediate base whose `Serialize`/`Deserialize` handle the sub-header, and leaf structs call it first
  (`MailLUBitStream` is the model).
- `Handle()` takes no arguments; the dispatcher sets context members (`sysAddr`, and per-service extras such as
  Mail's `player`) before calling `Deserialize` and `Handle`.
- Each service has one dispatch function with a `std::map<MessageType::X, factory>`, exactly like
  `Mail::HandleMail`: read the id, create, set context, `Deserialize` (log and drop on failure), `Handle`. Each
  server's `HandlePacket` only routes by `ServiceType`.
- **One file pair per `ServiceType`; a packet goes in the file of the `ServiceType` written in its header**, in
  that namespace: `CommonPackets` (COMMON), `AuthPackets` (AUTH), `ChatPackets` (CHAT), `WorldPackets` (WORLD:
  what a client sends to a world server), `ClientPackets` (CLIENT: what any server sends to the client),
  `MasterPackets` (MASTER), all in `dNet/`. So the auth server's login response is in `ClientPackets`, and a
  world server's message to chat is in `ChatPackets`. Anything used by several services (for example `Stamps`,
  which rides in auth, master and client packets, or generic helpers) gets its own small file named for what it
  is (`dNet/Stamps.h`), not one service's file. A packet found in the wrong file is moved (a pure move, same
  bytes). Mail stays in `dGame/dUtilities/Mail.*`. Handlers that need game or server state stay in the server that owns that state
  (`dChatServer`, `dMasterServer`, `dWorldServer`, `dAuthServer`) and are attached to the struct by overriding
  `Handle` in that server's translation unit.
- Fixed-width strings use `LUString`/`LUWString` with the width spelled out (`LUWString password(41)`).
- `PacketDispatcher<MessageType::X>` (`dNet/PacketDispatcher.h`) is the dispatch map for servers where the same
  struct is handled differently depending on who receives it (chat, master, the dashboard): each entry names the
  struct and a handler function `(const Msg&, const SystemAddress&)`. `Dispatch(packet, service)` reads the header,
  reads the struct and drops (and logs) packets that fail to `Deserialize`.
- Structs go in the file of the `ServiceType` in their header (`ChatPackets` for CHAT, `ClientPackets` for CLIENT,
  `MasterPackets` for MASTER, ...), whoever sends them. What chat sends a client in the CLIENT service (friends,
  ignore list and team responses, and the team game messages it writes as `ClientPackets::TeamGameMsg`, since chat
  doesn't link `dGame`) is in `ClientPackets`; chat-service packets the client receives are in `ChatPackets::Client`.
- Chat -> client packets are wrapped in `ChatPackets::WorldRoutePacket` (`dNet/WorldRoutePacket.h`: target object ID
  + the inner packet of any service); the world passes the inner bytes on unchanged.
- MASTER structs: `MasterPackets.h` for the core ones (session keys, zone transfer, private zones, player counts,
  world ready, shutdown, server list); topic groups (dashboard player actions, data changes, positions and
  announcements, message capture, instance migration) in `dNet/master/<Topic>.h`, all included by `MasterPackets.h`.

```cpp
// dNet/ChatPackets.h
struct AchievementNotify : public LUBitStream {
	LUWString targetPlayerName{};
	uint32_t missionEmailID{};
	LWOOBJID earningPlayerID{};
	LUWString earnerName{};

	AchievementNotify() : LUBitStream(ServiceType::CHAT, MessageType::Chat::ACHIEVEMENT_NOTIFY) {}
	void Serialize(RakNet::BitStream& bitStream) const override;
	bool Deserialize(RakNet::BitStream& bitStream) override;
};
```

### 2.3 Conventions

- Wire order = declaration order. Comment the direction (`// Server -> client.`) and anything odd (padding,
  unknown fields: write them as named constants, never drop them).
- Explicit widths for everything written: `bitStream.Write<uint32_t>(x.size())`, never `size_t`, `int`, `long` or
  `float_t`. Enums keep their declared underlying type.
- `Deserialize` returns `false` on the first failed read (`VALIDATE_READ`) and bounds-checks lengths
  (`ReadLengthPrefixed` rejects negative sizes and anything above `MAX_MESSAGE_LENGTH` by default).
- Optional/default-flag fields: `BitStreamUtils::WriteOptional(bs, value, default)` / `ReadOptional`.
- Length-prefixed strings: `BitStreamUtils::WriteLengthPrefixed<LenT>` / `ReadLengthPrefixed<LenT>`.
- No new `CBITSTREAM` / `CMSGHEADER` / `SEND_PACKET` macros or `Send*` wrapper functions; build the struct at the
  call site.
- Message IDs are append-only: never renumber, reuse or delete a value. `tests/dCommonTests/MessageIdPinTests.cpp`
  and `tests/dGameTests/MailIdPinTests.cpp` pin every enumerator at compile time.
- Behaviour changes and wire fixes are separate, clearly labelled commits, never part of a conversion.

### 2.4 Testing helpers

- `tests/dGameTests/GameDependencies.h`: `dServerMock` copies every sent packet (`GetSentPackets()`, each with
  bytes, exact bit count, address and broadcast flag).
- `tests/dGameTests/PacketTestUtils.h`: `Capture(fn)`, `FromBitStream`, `FromHex`, `ToHex`, `EXPECT_PACKET_EQ`
  (compares bit count and bytes and prints both as hex on failure).
- `NetGameMsg::WritePacket` / `LUBitStream::WritePacket` produce exactly what `Send` puts on the wire.

## 3. Verification

Every conversion carries these tests, in `tests/dGameTests/dGameMessagesTests/<Domain>MessagesTests.cpp` (or a
`dNet` test directory for packets):

1. **Frozen oracle.** Copy the old functions *verbatim* into `tests/.../Legacy/<Domain>Legacy.h` (namespace
   `Legacy*`; only the namespace changes). A temporary test sends the same inputs through the production function
   and the oracle and requires identical bytes; it is deleted in the commit that deletes the production code. From
   then on the oracle pins the old bytes.
2. **Byte equality.** For a grid of inputs (every bool combination, empty/ASCII/non-ASCII/long strings,
   0/negative/max numbers, default and non-default values of every optional field) send through the oracle and
   through the struct, to one client and as a broadcast. Require identical bits, identical bytes and the same
   effective destination.
3. **Golden bytes.** At least one hand-computed hex packet per message family, independent of both
   implementations.
4. **Round trip.** `Serialize` -> `Deserialize` -> `Serialize` gives the same bytes and fields and consumes every
   bit.
5. **Inbound.** For handlers, compare the struct's `Deserialize` with the oracle's read sequence, and check a
   truncated stream returns `false`.
6. **Mutation check** while writing the tests: break one field width and confirm the tests fail.
7. **Live captures, locally only.** Decoding real captured packets with the struct and re-serializing them is a
   useful extra check, but it is done with a local script outside the repository. **No capture-derived bytes are
   ever committed, not even anonymized.**
8. **Smoke test** the affected feature with the real 1.10.64 client before merging (tests cannot cover handler
   behaviour).

## 4. Migration plan

Small, self-contained PRs by subsystem: add structs + oracle + tests, switch callers, delete old code. Each PR is a
clean range of commits on the working branch. Target: under ~1.5k changed lines per PR.

| # | PR | Contents | Size |
|---|---|---|---|
| 0 | Foundations | Test infra (`dServerMock`, `PacketTestUtils`, `WritePacket`), string and optional helpers, compile-time pins of every message ID enum, drop GMs that fail to deserialize, `LUBitStream` virtual destructor and `WritePacket`, the `GameMsg`/`NetGameMsg` split, this document. | S |
| W1 | Wire fix: `UnSmash` | The one intended wire change: stop writing `duration` when it has its default, matching the client. | XS |
| 1 | Activity GMs | `ActivityMessages`: 8 outbound + `RequestActivityExit` inbound. | S |
| 2 | GMs: racing and vehicles | `SendVehicle*`, `SendRacing*`, `SendNotifyRacingClient`, `HandleRacing*`, `HandleVehicle*`, module assembly | M |
| 3 | GMs: effects, audio, animation, UI text | FX, animations, ND audio, 2D ambient, UI messages, message boxes, chat bubbles, billboards, cinematics, tooltips | M |
| 4 | GMs: missions, flags, levels | Offer/notify mission(+task), respond, dialog OK, linked missions, flags, collectibles, level rewards | M |
| 5 | GMs: inventory and items | Add/remove/move/equip/unequip, inventory size and groups, use/consume item, `UpdateInventoryUi` | M |
| 6 | GMs: vendors, donation, trade | Vendor window/status/transactions, buyback, donation vendor, trade | M |
| 7 | GMs: combat and skills | Skill add/remove, stun, buffs, die/resurrect/smash, knockback; the one-off `EchoStartSkill`/`EchoSyncSkill`/`StartSkill`/`SyncSkill`/projectile classes become `NetGameMsg`s | M |
| 8 | GMs: pets | Taming minigame, pet naming, commands | M |
| 9 | GMs: property and building | Property management, models, BBB, modular build, `PropertyDataMessage`, `PropertySelectQueryProperty`, `ControlBehaviors` (may split in two) | L |
| 10 | GMs: remaining + switch removal | Movement, teleport, platforms, rails, camera, control scheme, misc, team GMs sent from `TeamContainer`; delete the switch and `GameMessages.cpp` (done: `Movement`, `Zone`, `Player`, `Object`, `QuickBuild` files; `GameMessages.cpp` keeps only the base code) | M |
| 11 | Common + Auth packets | `CommonPackets` (version confirm, disconnect notify, general notify; `dServer::Disconnect`), `AuthPackets` login request, `ClientPackets` login response + stamps; `AuthServer` dispatch map | S |
| 12 | World packets | Validation, character list/create/delete/rename, world login, level load complete, position update, string check, general chat, route packet, top-5, funness, and the `ClientPackets` responses they send; handlers move out of the `WorldServer.cpp` switch into a dispatch map | M |
| 13 | Chat packets | Friends, ignore list, teams, who/show-all, private/general chat, routing (`WORLD_ROUTE_PACKET` wraps an `LUBitStream`), achievement notify, GM announce/mute, plus the chat-side of `WorldServer`'s chat cases (may split friends/teams) | L |
| 14 | Master packets | Session keys, zone transfer, private zones, player added/removed, world ready, prep zone, shutdown; `MasterServer` switch becomes a dispatch map; `InstanceManager`/`ZoneInstanceManager` senders | M |
| 15 | Cleanup | Remove `CBITSTREAM`/`CMSGHEADER`/`SEND_PACKET*`/`CINSTREAM*`, the free `BitStreamUtils::WriteHeader`, `PacketUtils`; update this document (done) | S |

Game-message PRs (2-10) are independent of each other and of the packet PRs (11-14), so they can be reviewed in
any order after PR 0.

### Risks

- **Silent wire fixes.** A struct written from the client's definition can differ from what DLU has been sending
  (e.g. `UnSmash`). The oracle test catches it; the conversion must reproduce the old bytes, and the fix goes in its
  own commit.
- **Implicit widths at old call sites** (`int`, `char`, `size_t`, `float_t`, enum underlying types, 1-bit bools vs
  `uint8_t`). The byte-equality grid catches these; that is why the oracle is copied verbatim.
- **Default arguments.** Old `Send*` defaults must become the struct's member initializers exactly.
- **Handler behaviour.** Keep each handler's logic verbatim in the conversion; behaviour changes are separate.
- **Merge conflicts** with feature work in `GameMessages.cpp`: small PRs, landed domain by domain.
- **Personal data.** Captures contain account names, chat and IDs; they stay out of the repository entirely.

## 5. Known wire discrepancies

Found while converting; none of them is fixed by a conversion (the structs reproduce DLU's bytes, pinned by the
oracle tests). Each fix, if wanted, is its own labelled wire change verified against the client. Addresses are in
the 1.10.64 client.

### Game messages

| Message | DLU | Client / reference |
|---|---|---|
| `PlaceModelResponse` | Fixed (wire fix, see docs/BuildWorkflow.md): used to write a 4-byte `response` where the client expects the rotation. | The client reads a 16-byte w, x, y, z quaternion when the rotation isn't identity (`0x00dc0170`); a live server echoed the rotation the client placed the model with. |
| `NotifyPetTamingPuzzleSelected` | Written as the client's `Serialize` (`0x00db6880`) writes it. | The client's own `Deserialize` (`0x00e3a7c0`) reads an extra `u32` its `Serialize` never writes. |
| `SetBuildModeConfirmed` | Always sends the default flags. | The client has non-default flag fields. |
| `NotifyNotEnoughInvSpace` | Fixed (wire fix): used to be sent with message ID `VEHICLE_NOTIFY_FINISHED_RACE` (1396). | Its ID is `NOTIFY_NOT_ENOUGH_INV_SPACE` (1516, `0x00545c90`); payload read at `0x00d8b850`. |
| `MoveInventoryBatch` | Now follows the client layout. | |
| `UnEquipInventory` | The trailing optional `replacementObjectID` is never read. | The client can send it. |
| `SetStatusImmunity` | Writes the flags in DLU's order. | The client reads DOT, ImaginationGain, ImaginationLoss, Interrupt, Knockback, PullToPoint, QuickbuildInterrupt, Speed, BasicAttack (`0x00d8f140`). |
| `RequestDie` | Read with the `Die` layout. | Starts with one `bDieAccepted` bit and has a mandatory `lootOwnerID` (`0x00e02d90`). |
| `SetCurrency` | `sourceTradeID` is an optional `int32_t`. | lu_packets has an object ID (8 bytes). DLU only ever sends 0 (flag bit 0), so no bytes differ today. |
| `FireEventClientSide` | Never writes `param1`/`param2` (both flag bits 0), whatever the caller passed: `RocketEquipped` loses the clone ID. | Optional `i64 param1` (default 0) and `i32 param2` (default -1). |
| `PickupCurrency` | Reads only the amount. | lu_packets has a position after it (ignored, harmless). |
| `MatchUpdate`, `MatchRequest` | Name-value text is widened/narrowed one byte per UTF-16 unit, so non-ASCII names are garbled. | UTF-16 text. |
| `ScriptNetworkVarUpdate` | The text goes through `ASCIIToUTF16`. | UTF-16 text (non-ASCII values are garbled). |
| `SetShootingGalleryParams` | Removed: had no callers, and its field order was a guess ("No clue about the order here"). | Not verified. |

Behaviour changes that come with dropping malformed messages: `ParseChatMessage` longer than `MAX_MESSAGE_LENGTH`
is dropped instead of truncated; `PLAYER_LOADED` (`0x00dc36f0`), `READY_FOR_UPDATES` and `MISSION_DIALOGUE_CANCELLED`
(`0x00d9cc10`) now read the fields the client sends (DLU ignores them) and would be dropped if they were missing.

### Packets

| Packet | DLU | Client / reference |
|---|---|---|
| `VERSION_CONFIRM` (server -> client) | Sends 8 trailing bytes. | |
| `LoadStaticZone` | Always sends clone 0. | The zone's clone ID. |
| `ChatModerationString` | The accepted byte is `segments.empty()`. | |
| `StringCheck` | Keeps 42 narrowed characters, including whatever garbage follows the text. | |
| Route packets | Forwarded from byte 23, using the low byte of the routed packet ID. | |
