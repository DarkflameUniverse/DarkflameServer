# Chat bridge (example)

An example program that connects the game's chat to another chat service through the dashboard's public chat API.
Discord is the first adapter; a console adapter (read/type chat in a terminal) and a send-only JSON webhook adapter
show how to add others (Matrix, IRC, Slack, ...). The server knows nothing about any of them: the bridge uses only the
documented API (`docs/Dashboard.md`, "Chat log and chat bridges" and "API"), so you can replace it with your own.

What it does:

- **Game to the other side.** Reads chat with `GET /api/chat?after=<last id>`. It keeps a WebSocket open to `/ws`
  (subscribed to `chat_message`) only to know *when* to read, so a message is never lost or sent twice: when the
  socket drops it polls every few seconds, and after a restart it carries on from the last id it saved
  (`state_file`). On its first run it starts from the newest message rather than replaying the log.
- **The other side to the game.** Posts with `POST /api/chat/send` as `{"message", "name", "label"}`, optionally
  only in one zone (`send_zone`) or one world of it (`send_instance`). Players see `[Discord] Alice: hi`.
- **Only zone chat by default.** Whispers and team chat are never relayed unless a link lists them *and* sets
  `"allow_private": true`. When a link wants one channel, the bridge asks the server for only that channel, so other
  rows (like whispers, which a GM 8 account can read) don't even leave the server.
- **Nothing the chat filter stopped** (`blocked`) is relayed: no player saw it, so no one else should.
- **No loops.** It skips `web` messages with its own label or sent by its own account, and the Discord adapter
  skips bots and webhooks (including its own).
- **Rate limits both ways**, per link (a token bucket; `0` turns it off). Game messages over the limit are dropped
  and the next one says how many were skipped; Discord messages over the limit aren't posted in the game.
- Messages into the game are made to fit the API: one line, up to 300 characters, names up to 32 without brackets.
  Discord mentions, channels and custom emoji are spelled out (`@bob`, `#channel`, `:lego:`), and attachments show
  as `[attachment]`. Nothing said in the game can ping anyone on Discord (`allowed_mentions` is empty) and its
  Markdown is escaped.

## Requirements

Python 3.10+ and, for live updates and Discord, [`websockets`](https://pypi.org/project/websockets/)
(`pip install -r requirements.txt`, or your distribution's `python3-websockets`). Everything else is the standard
library. Without `websockets` the bridge still works with the console and webhook adapters, by polling.

Python was chosen because the repository's tooling (`tests/smoke`) already uses it, it runs anywhere the server does,
and with one small, widely packaged dependency there's nothing to build.

## Dashboard account and API token

1. Make an account for the bot (**Create Account** on the Accounts page), not a staff member's own: messages from
   the bot's account are ignored as its own, and its token can be revoked on its own.
2. Give it a GM level with these permissions (see the **Permissions** page):
   - `chat_view` (read chat; GM 3+ by default),
   - `chat_send` (post in the game; GM 8+ by default),
   - `api_access` (use API tokens; everyone by default).

   With the default levels that means GM 8. GM 8 also has `chat_private`, so the account *can* read whispers and
   team chat; this bridge still won't relay them unless you turn them on (see above).
3. Sign in to the dashboard as the bot, open its account page and **Generate** a token on the **API Token** card
   (1 to 365 days). Copy it: it is shown once. Or through the API:

   ```sh
   curl -s -X POST http://localhost:2006/api/auth/login -H 'X-Requested-With: x' \
        -H 'Content-Type: application/json' -d '{"username":"chatbot","password":"..."}'   # -> {"token": ...}
   curl -s -X POST http://localhost:2006/api/auth/token -H 'Authorization: Bearer <login token>' \
        -H 'Content-Type: application/json' -d '{"days":365}'                              # -> {"token": ...}
   ```

To revoke the token, sign the account out everywhere (its account page, or
`POST /api/accounts/<id>/sessions/revoke`); that ends every session and token of that account, which is another
reason to give the bot its own.

Check it: `DLU_API_TOKEN=... python3 -m chat_bridge --config config.json --check`

## Discord

1. In the [Discord developer portal](https://discord.com/developers/applications), make an application with a bot,
   and under **Bot** turn on **Message Content Intent** (without it the bot can't read what people write).
2. Invite it to your server with the `bot` scope and the **View Channel**, **Read Message History** and
   **Send Messages** permissions in the channel you want to bridge.
3. Copy the bot token and the channel ID (Discord's developer mode, right-click the channel, **Copy Channel ID**).
4. Optional: make a webhook in that channel (channel settings, **Integrations**, **Webhooks**) and set it as
   `webhook_url`. Game messages then show under each player's name (`Bob (Nimbus Station)`) instead of as the bot.
   With only a `webhook_url` and no bot token the link is one way (game to Discord).

## Config

Copy `config.example.json` to `config.json`. Any value written as `"$NAME"` is read from the environment, and
`DLU_DASHBOARD_URL` / `DLU_API_TOKEN` override the dashboard settings, so secrets needn't be in the file.

```json
{
  "dashboard": { "url": "http://localhost:2006", "token": "$DLU_API_TOKEN" },
  "state_file": "chat-bridge.state.json",
  "poll_seconds": 5,
  "resync_seconds": 60,
  "links": [
    {
      "adapter": "discord",
      "label": "Discord",
      "discord": { "bot_token": "$DISCORD_BOT_TOKEN", "channel_id": "123456789012345678", "webhook_url": "$DISCORD_WEBHOOK_URL" },
      "game": { "channels": ["zone"], "zones": [], "instances": [], "send_zone": 0 },
      "rate": { "to_game_per_minute": 20, "to_remote_per_minute": 60 }
    }
  ]
}
```

- `state_file`: where the last message id is kept (relative to the config file).
- `poll_seconds`: how often to read while the WebSocket is down; `resync_seconds`: how often to read anyway while it's up.
- `links`: one per bridged room. Several links share one connection to the dashboard, e.g. a Discord channel for all
  zone chat and a webhook that gets only Nimbus Station's.
  - `adapter`: `discord`, `console` or `webhook`; its settings go under a key of the same name.
  - `label`: what players see in brackets (up to 16 letters, digits, spaces, `-`, `_`; default the adapter's name).
  - `game.channels`: `zone` (default), `web` (messages posted from the dashboard or other bridges), and only with
    `"allow_private": true`, `whisper` and `team`.
  - `game.zones` / `game.instances`: only relay chat from these zone IDs / world instance IDs (empty: all).
  - `game.send_zone` / `game.send_instance`: post incoming messages only in this zone / world (0 / absent: everywhere).
  - `rate`: messages per minute each way (bursts of 5).

Adapter settings:

| Adapter   | Settings |
|-----------|----------|
| `discord` | `bot_token`, `channel_id` (to read and send); `webhook_url` (optional, to send as each player) |
| `console` | `name` (who lines without `Name: ` are from), `read_stdin` (default true) |
| `webhook` | `url`, `headers` (optional). Send only: POSTs `{"text", "sender", "message", "channel", "zone_id", "zone_name", "instance_id", "id"}` |

## Running

```sh
cd tools/chat-bridge
pip install -r requirements.txt
DLU_API_TOKEN=... DISCORD_BOT_TOKEN=... python3 -m chat_bridge --config config.json
```

Try it in a terminal first with `config.console.json`: game chat is printed, and each line you type
(`Alice: hello` or just `hello`) is posted in the game as `[Console] Alice: hello`.

As a service: `chat-bridge.service` is a systemd unit; copy it to `/etc/systemd/system/`, fix the paths and user,
put the secrets in `/etc/chat-bridge.env` (mode 600), then `systemctl enable --now chat-bridge`.
`journalctl -u chat-bridge` shows its log.

## Adding a service

Write `chat_bridge/adapters/<name>.py` with a subclass of `Adapter` (`adapters/base.py`):

- `send(msg, skipped)`: post one game message (a `GameMessage`: `sender_name`, `message`, `zone_name`, ...).
- `run(deliver)`: receive messages and `await deliver(RemoteMessage(author, text))` for each; ignore your own posts.
  A send-only adapter doesn't need it.

Then add it to `ADAPTERS` in `adapters/__init__.py`. `console.py` is the smallest two-way example and `webhook.py` the
smallest one-way one. Filtering, loop prevention, rate limits and resuming are done by the bridge.

## Privacy

- Players can't see that their zone chat is being relayed. Tell them (rules, a message of the day, the news).
- Whispers and team chat are private; relaying them needs two explicit settings. Don't.
- The bridge only sends out what players could already see in that zone. Messages the chat filter stopped aren't
  relayed.
- The game logs what the bridge posts (the Chat Log's `web` channel), with the bridge's account.
- Keep the API token and bot token secret: the API token acts as the bot's account with everything its GM level can
  do, not only chat. Use a dedicated account with the lowest level that has the permissions above.

## Limits

- Messages posted in the game go through the same chat filter as players' (`chat_bridge_filter`, on by default). If
  the filter stops one, no player sees it; the API only answers with a request id, so the bridge can't tell the
  Discord user. (The Chat Log does not mark such web messages as stopped.)
- Live messages arrive with the dashboard's update tick (`broadcast_interval`, 2 seconds by default).
- One Discord channel per link; threads, edits, deletions and reactions aren't relayed.
- The rate limits drop messages rather than queueing them for long; at most 100 wait per link.
- A message can be relayed twice only if the bridge is killed between sending a message and saving the state file.

## Tests

```sh
cd tools/chat-bridge && python3 -m unittest discover -s tests
```

Standard library only (no network): relay rules, loop prevention, formatting (game and Discord), resuming from the
last id, paging, rate limits and config parsing.
