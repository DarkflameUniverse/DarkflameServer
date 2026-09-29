# Guilds

The 1.10.64 client's guild system, and what DLU does with it.

Evidence tags: **[G]** verified in Ghidra (legouniverse.exe 1.10.64), **[F]** the client's Scaleform UI
(`res/ui/ingame/*.gfx`, decompiled with JPEXS), **[D]** client data (cdclient, locale, scripts, maps), **[I]** inferred
(no client code decides it; DLU's choice). No live capture contains guild traffic (the available captures were scanned
for chat ids 0x16-0x1c, client ids 0x25-0x30 and game message 626: none).

## Enabling the client side

- Everything guild related in the client is gated by `FeatureGating::GetIsFeatureEnabled("guilds")` [G]: the guild
  component registers no handlers, the create/invite/leave UI does nothing and `PlayerReady` sends nothing.
- The feature is read from the **client's** `cdclient.fdb` table `FeatureGating` (`LWODataCache::Load`) [G]. The shipped
  1.10.64 table has no `guilds` row [D], so guilds are off in a stock client. A row `featureName=guilds, major=1,
  current=0, minor=0` turns them on (any version at or below the client's). DLU cannot send this; it is client data.
- The Guild Master NPC (LOT 3001, script `scripts\ai\FV\L_GUILD_CREATE.lua`: `onUse` → `DisplayGuildCreateBox`) exists
  in the cdclient [D] but is placed in no 1.10.64 `.lvl` file [D].

## UI

| UI file | Calls to C++ | Messages it listens for |
|---|---|---|
| `guildcreate.gfx` [F] | `RequestGuildCreate {guildName}` | `ToggleGuildCreate` |
| `guild.gfx` [F] | `RequestGuildInvite {playerName}`, `LeaveGuild {}`, UI `GuildUIClosed` | `ToggleGuildUI`, `LoadGuildList {guildName, guildMates[] {name, online, zone, rank}}` |
| `statusbar.gfx` [F] | `RequestGuildUI {}` (guild button) | `EnableGuild {bDisplay}` (shows the guild button) |
| `chatbox.gfx` [F] | `ChatString_FlashToCPP {chatField}` | `PlayerInGuild`, `PlayerLeftGuild`, `SendChat` channel 10 → guild tab |

- Guild create input: `maxlength 32` [F]; `LWOGuildComponent::SendTMPGuildCreate` copies at most 30 characters [G].
- The guild window has no kick, promote or disband control [F]. No slash command for guilds is registered in
  `SlashCommandHandler::InitializeCommands` [G].
- Guild chat: typing in the guild tab sends `/g <text>` [F]. `/g` is not a client command [G], so it reaches the server
  as a `ParseChatMessage` like any unknown command.
- The guild list shows rank as text: 1 `Leader`, 2 `Officer`, 3 `Veteran`, 4 `Recruit`, anything else nothing [G]
  (`LWOGuildComponent::SendGuildListToUI`). Online members' `zone` is `ZoneTable.DisplayDescription` of the map id [G].
  Online members are listed first, each group sorted by name case-insensitively [G] (`RefreshGuildUI`).

## Client component state

- `LWOCharacterComponent` holds `guildId` (u64) and `guildName` [G]. They come only from its serialization (below).
  `GetCharIsInGuild` / `GetCharGuildInfo` answer from them (`guildId != 0`) [G]. `SetCharGuildInfo` and
  `GuildSetStatus` have no handler anywhere [G].
- `LWOGuildComponent` holds the member map (`GuildData`: name, two dates, `map<LWOOBJID, {name, zone, rank, online}>`)
  [G], filled by `GUILD_DATA` and the add/remove/online packets.

### Character component serialization (social info) [G]

`LWOCharacterComponent::Deserialize`, after the GM info and activity blocks:

| Field | Type |
|---|---|
| dirty flag | bit |
| guild id | u64 |
| guild name length | u8 |
| guild name | length × UTF-16 |
| is LEGO club member | bit |
| country code | u32 |

On a non-construction update the client re-renders the name billboard (`GuildRenderName`, `ReRenderNameBillboard`).

## Triggers (client) [G]

| Event | Client does |
|---|---|
| `PlayerReady` | if in guild (component guild id ≠ 0): send `GUILD_GET_ALL` |
| `ChatConnectionUpdate` (connected) | same |
| `RequestGuildData` (sent by the client itself after `GUILD_CREATE_RESPONSE` created or `GUILD_INVITE_CONFIRM` success) | send `GUILD_GET_ALL` |
| UI `RequestGuildCreate` | if not in guild: world packet `TMP_GUILD_CREATE`; else local "MSG_GUILD_ALREADY_IN_GUILD" |
| UI `RequestGuildInvite` | `CheckInviteSpamming` (type 4, local rate limit), then `GUILD_INVITE` |
| UI `LeaveGuild` | if in guild: `GUILD_LEAVE` |
| Message box `MSG_GUILD_NAME_INVITED_YOU_TO_THE_GUILDNAME_GUILD!` | `GUILD_INVITE_RESPONSE`, button 1 → declined 0, otherwise declined 1 |
| `GuildInvite` from an ignored player | `GUILD_INVITE_RESPONSE` declined 1, no box |
| `DisplayGuildCreateBox` (network GM 626) | UI `ToggleGuildCreate {bShow: true}` (bShow from the message is not used) |

## Packets

Offsets are into the packet after the 1-byte `0x53`: u16 connection type, u32 id, u8 padding, then data at 7 (wire
offset 8). All strings are fixed-size UTF-16 buffers read to the first NUL.

### Client → server

The client sends chat-service packets (connection type 2) through its world as `ROUTE_PACKET`; DLU's world forwards
them to chat with the player's object ID (`WorldPackets::RoutePacket::ToChat`, which drops the first 4 data bytes).

| Packet | Id | Bytes | Layout (from offset 7) | Evidence |
|---|---|---|---|---|
| World `TMP_GUILD_CREATE` | 4 / 20 | 69 | wchar[31] name (≤ 30 characters) | [G] `SendTMPGuildCreate` |
| Chat `GUILD_INVITE` | 2 / 23 | 81 | u64 0, wchar[33] player name | [G] `SendGuildInvitePacket` |
| Chat `GUILD_INVITE_RESPONSE` | 2 / 24 | 16 | u64 0, u8 declined | [G] `LWOGuildComponent::SendMessage` |
| Chat `GUILD_LEAVE` | 2 / 25 | 81 | u64 0, then 66 bytes of stack (the client's length is wrong) | [G] `LeaveGuild` |
| Chat `GUILD_GET_ALL` | 2 / 28 | 15 | u64 0 | [G] `SendGuildGetAll` |

`GUILD_CREATE` (22), `GUILD_KICK` (26) and `GUILD_GET_STATUS` (27) are never sent by the client [G].

### Server → client (`MessageType::Client`, connection type 5)

| Packet | Id | Layout (from offset 7) | Client does | Evidence |
|---|---|---|---|---|
| `GUILD_CREATE_RESPONSE` | 37 | u8 result, u64 at 8 (unread; DLU: guild id [I]), wchar[31] name at 16 | 0 → "MSG_GUILD_GUILD_NAME_CREATED" (name) + `RequestGuildData`; 1 → "…NAME_CANT_BE_USED"; 2 → "…NAME_ALREADY_IN_USE"; else "…COULD_NOT_BE_CREATED" (chat channel 0) | [G] |
| `GUILD_GET_STATUS_RESPONSE` | 38 | — | no handler | [G] |
| `GUILD_INVITE` | 39 | wchar[33] inviter name, wchar[31] guild name at 73 | "MSG_GUILD_NAME_WANTS_YOU_TO_BE_IN_NAME_GUILD!" unless the inviter is ignored, then game message `GuildInvite` → message box | [G] |
| `GUILD_INVITE_INITIAL_RESPONSE` | 40 | u8 code, wchar[33] name | 0 "…INVITE_SENT_TO_NAME", 1 "MSG_GENERIC_NAME_IS_NOT_ONLINE", 2 "…NAME_IS_ALREADY_IN_A_GUILD", 3 "…ALREADY_HAS_A_GUILD_INVITE_PENDING", else "…COULD_NOT_INVITE_NAME" | [G] |
| `GUILD_INVITE_FINAL_RESPONSE` | 41 | u8 code, wchar[33] name | 0 "…NAME_HAS_JOINED_THE_GUILD", 1 "…DECLINED_YOUR_INVITATION", 2 "…IS_NOT_ONLINE", else "CLIENTMSG_COULD_NOT_INVITE_NAME" | [G] |
| `GUILD_INVITE_CONFIRM` | 42 | u8 failed, wchar[33] guild name | 0 → "MSG_GUILD_YOU_JOINED_THE_GUILD_NAME" + `RequestGuildData`; else "…YOU_COULD_NOT_BE_ADDED…" | [G] |
| `GUILD_ADD_PLAYER` | 43 | wchar[33] name, u64 id at 73, u8 rank at 81, LWOZoneID at 82, u8 online at 90 | "…NAME_HAS_JOINED_THE_GUILD" (channel 10); adds the member if the guild data is loaded | [G] |
| `GUILD_REMOVE_PLAYER` | 44 | u8 reason (0 left, 1 kicked), wchar[33] name, u64 id at 74, u64 new leader at 82 | message (channel 10); id = self → guild UI cleared (`PlayerLeftGuild`, `EnableGuild` false); else member removed, new leader's rank set to 1 | [G] |
| `GUILD_LOGIN_LOGOUT` | 45 | wchar[33] name, u64 id at 73, u8 online at 81, LWOZoneID at 82, u8 world-update-only at 90 | unless world-update-only: "…GUILDMATE_NAME_LOGGED_IN/OFF" (channel 10); sets online + zone | [G] |
| `GUILD_RANK_CHANGE` | 46 | — | no handler | [G] |
| `GUILD_DATA` | 47 | below | `PopulateGuildData` | [G] |
| `GUILD_STATUS` | 48 | — | no handler | [G] |

`GuildSetPlayerRank` (game message 590) is a no-op in `LWOGuildComponent` [G]: a rank change reaches the client only
through a new `GUILD_DATA`.

#### `GUILD_DATA` [G]

| Offset | Type | Field |
|---|---|---|
| 7 | u8 | status; only 0 is used (anything else: ignored) |
| 8 | wchar[31] | guild name |
| 70 | wchar[11] | date (ends up in `GuildData.wsJoinDate`) |
| 92 | wchar[11] | date (ends up in `GuildData.wsFoundDate`) |
| 114 | i32 | reputation (not used) |
| 118 | i32, i32, u16 | not read |
| 128 | u16 | member count (0: ignored) |
| 130 | member × count | |

Member (84 bytes): u8 rank, u8 online, LWOZoneID zone (8 bytes, read only when online), u64 object id, wchar[33] name.
The dates are not shown by the UI (`setGuildInfo(name, "")` [F]); their format is unknown ([I] DLU sends `MM/DD/YYYY`).
On success the client shows the guild button and guild chat tab (`PlayerInGuild`, `EnableGuild` true).

### Game message

| Message | Id | Direction | Fields | Evidence |
|---|---|---|---|---|
| `DisplayGuildCreateBox` | 626 | server → client | bit bShow | [G] Serialize 0x00dbb260, Deserialize 0x00dbb2a0 |

Game messages 578-596 (`GuildGetSize` … `GuildRenderName`) are client-local [G]: their Serialize/Deserialize slots are
the shared no-op stubs (e.g. `GuildInvite` vtable 0x015932ac: 0x00411820, 0x004175d0); the packet handlers above create
them.

## Chat channels

`eChatChannel` 10 `GUILD`, 11 `GUILDNOTIFY` [G][F]. Guild chat arrives at the client as `MSG_CHAT_PRIVATE_CHAT_MESSAGE`
(or general chat) with channel 10; `PacketHandler_MSG_CHAT_PRIVATE_CHAT_MESSAGE` passes the channel to the UI, which
routes 10 to the guild tab [G][F]. Channel 11 has no UI box [F].

## Limits

| Rule | Value | Evidence |
|---|---|---|
| Name length | ≤ 30 characters (client), ≥ 3 [I] | [G] / [I] |
| Name characters | letters, digits, space, `'` `-` `.`; no leading/trailing or double spaces [I] | [I] |
| Name uniqueness | case-insensitive [I] | "…NAME_ALREADY_IN_USE" exists [G] |
| Members | `guild_max_members` (chat config, default 100) [I] | none in client |
| Ranks | 1 Leader, 2 Officer, 3 Veteran, 4 Recruit | [G] |
| One guild per character, one pending invite per character | | "…ALREADY_IN_A_GUILD", "…INVITE_PENDING" [G] |

## Locale strings [D]

`MSG_GUILD_*` (41 ids) and `UI_GUILD_*`, `UI_GUILDCREATE_*`, `UI_CHAT_CHGUILD`, `UI_CHAT_GUILD_CHAT` in `locale.xml`.
No cdclient table is about guilds (besides `FeatureGating`, which lacks the row).
