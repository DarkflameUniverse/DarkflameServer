"""Discord: one bot, one text channel.

Receiving uses the Discord gateway (a WebSocket) with the Guild Messages and Message Content intents; the Message
Content intent must be switched on for the bot in the Discord developer portal. Sending uses a channel webhook when
`webhook_url` is set (each player shows under their own name) or else the bot itself.

Only `websockets` is needed; the formatting functions at the top are plain Python and unit tested.
"""
from __future__ import annotations

import asyncio
import json
import logging
import random
import re
import urllib.error
import urllib.request

from ..core import GameMessage, RemoteMessage, where
from .base import Adapter, Deliver

log = logging.getLogger("chat_bridge.discord")

API = "https://discord.com/api/v10"
GATEWAY = "wss://gateway.discord.gg/?v=10&encoding=json"
INTENTS = (1 << 9) | (1 << 15)  # GUILD_MESSAGES | MESSAGE_CONTENT
FATAL_CLOSE_CODES = {4004: "the bot token is wrong", 4013: "invalid intents",
                     4014: "the Message Content intent isn't enabled for this bot in the developer portal"}
USER_AGENT = "DiscordBot (https://github.com/DarkflameUniverse/DarkflameServer, 1.0) dlu-chat-bridge"

# Nobody gets pinged by something said in the game
NO_MENTIONS = {"parse": []}

_MARKDOWN = re.compile(r"([\\*_~`|>#\[\]])")
_USER_MENTION = re.compile(r"<@!?(\d+)>")
_ROLE_MENTION = re.compile(r"<@&\d+>")
_CHANNEL_MENTION = re.compile(r"<#\d+>")
_EMOJI = re.compile(r"<a?:(\w+):\d+>")
_TIMESTAMP = re.compile(r"<t:(\d+)(?::\w)?>")


# --- formatting (pure) ---

def escape_markdown(text: str) -> str:
    return _MARKDOWN.sub(r"\\\1", text)


def webhook_username(name: str) -> str:
    """Discord refuses webhook names containing "discord" or "clyde", and they are 1-80 characters."""
    name = re.sub(r"(?i)discord", "D1scord", name)
    name = re.sub(r"(?i)clyde", "Clyd3", name)
    name = name.strip() or "Player"
    return name[:80]


def to_discord(msg: GameMessage, via_webhook: bool, skipped: int = 0) -> dict:
    """The JSON body that posts one game message in Discord."""
    place = where(msg)
    note = f"-# {skipped} message(s) skipped (rate limit)\n" if skipped else ""
    if via_webhook:
        username = msg.sender_name + (f" ({place})" if place else "")
        return {"username": webhook_username(username), "content": note + escape_markdown(msg.message),
                "allowed_mentions": NO_MENTIONS}
    head = f"**{escape_markdown(msg.sender_name)}**" + (f" ({escape_markdown(place)})" if place else "")
    return {"content": f"{note}{head}: {escape_markdown(msg.message)}", "allowed_mentions": NO_MENTIONS}


def display_name(event: dict) -> str:
    member = event.get("member") or {}
    author = event.get("author") or {}
    return member.get("nick") or author.get("global_name") or author.get("username") or "someone"


def from_discord(event: dict) -> str:
    """MESSAGE_CREATE content as plain text players can read: mentions and custom emoji spelled out."""
    names = {m.get("id"): (m.get("member") or {}).get("nick") or m.get("global_name") or m.get("username")
             for m in event.get("mentions", [])}
    text = event.get("content", "")
    text = _USER_MENTION.sub(lambda m: "@" + (names.get(m.group(1)) or "someone"), text)
    text = _ROLE_MENTION.sub("@role", text)
    text = _CHANNEL_MENTION.sub("#channel", text)
    text = _EMOJI.sub(r":\1:", text)
    text = _TIMESTAMP.sub("(a time)", text)
    extras = []
    if event.get("attachments"):
        extras.append("[attachment]")
    if event.get("sticker_items"):
        extras.append("[sticker]")
    return " ".join([text.strip(), *extras]).strip()


def should_forward(event: dict, channel_id: str, own_user_id: str | None) -> bool:
    """Whether a MESSAGE_CREATE goes into the game: our channel, a person (not a bot or webhook, so no loops)."""
    if str(event.get("channel_id")) != str(channel_id):
        return False
    if event.get("webhook_id"):
        return False
    author = event.get("author") or {}
    if author.get("bot") or (own_user_id and author.get("id") == own_user_id):
        return False
    return event.get("type", 0) in (0, 19)  # a normal message or a reply


# --- the adapter ---

class DiscordAdapter(Adapter):
    name = "discord"
    default_label = "Discord"

    def __init__(self, config: dict):
        super().__init__(config)
        self.token = config.get("bot_token", "")
        self.channel_id = str(config.get("channel_id", ""))
        self.webhook_url = config.get("webhook_url", "")
        self.user_id: str | None = None
        if not self.webhook_url and not (self.token and self.channel_id):
            raise ValueError("discord needs bot_token and channel_id (to read and send) or webhook_url (to send only)")

    # Sending

    def _post(self, url: str, body: dict, bot_auth: bool) -> float:
        """POST, returning seconds to wait and retry when Discord rate limits (0 when sent)."""
        headers = {"Content-Type": "application/json", "User-Agent": USER_AGENT}
        if bot_auth:
            headers["Authorization"] = f"Bot {self.token}"
        req = urllib.request.Request(url, data=json.dumps(body).encode(), method="POST", headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=15) as resp:
                resp.read()
            return 0.0
        except urllib.error.HTTPError as e:
            if e.code == 429:
                try:
                    return float(json.loads(e.read()).get("retry_after", 1.0))
                except ValueError:
                    return 1.0
            raise

    async def send(self, msg: GameMessage, skipped: int = 0) -> None:
        via_webhook = bool(self.webhook_url)
        body = to_discord(msg, via_webhook, skipped)
        url = self.webhook_url if via_webhook else f"{API}/channels/{self.channel_id}/messages"
        for _ in range(3):
            wait = await asyncio.to_thread(self._post, url, body, not via_webhook)
            if not wait:
                return
            await asyncio.sleep(wait)
        log.warning("gave up sending message %s to Discord: rate limited", msg.id)

    # Receiving (the gateway)

    async def run(self, deliver: Deliver) -> None:
        if not self.token or not self.channel_id:
            log.info("no bot_token/channel_id: sending to Discord only")
            return
        import websockets

        session_id = resume_url = None
        seq = None
        backoff = 1.0
        while True:
            url = resume_url.rstrip("/") + "/?v=10&encoding=json" if session_id and resume_url else GATEWAY
            try:
                async with websockets.connect(url, max_size=2 ** 22) as ws:
                    hello = json.loads(await ws.recv())
                    interval = hello["d"]["heartbeat_interval"] / 1000.0
                    if session_id:
                        await ws.send(json.dumps({"op": 6, "d": {"token": self.token, "session_id": session_id, "seq": seq}}))
                    else:
                        await ws.send(json.dumps({"op": 2, "d": {"token": self.token, "intents": INTENTS,
                                                                  "properties": {"os": "linux", "browser": "dlu-chat-bridge", "device": "dlu-chat-bridge"}}}))
                    last_seq = [seq]

                    async def heartbeat():
                        await asyncio.sleep(interval * random.random())
                        while True:
                            await ws.send(json.dumps({"op": 1, "d": last_seq[0]}))
                            await asyncio.sleep(interval)

                    beat = asyncio.create_task(heartbeat())
                    try:
                        async for raw in ws:
                            event = json.loads(raw)
                            op = event.get("op")
                            if event.get("s") is not None:
                                seq = last_seq[0] = event["s"]
                            if op == 0:
                                kind, data = event.get("t"), event.get("d") or {}
                                if kind == "READY":
                                    session_id, resume_url = data["session_id"], data.get("resume_gateway_url")
                                    self.user_id = data["user"]["id"]
                                    backoff = 1.0
                                    log.info("connected to Discord as %s", data["user"].get("username"))
                                elif kind == "RESUMED":
                                    backoff = 1.0
                                elif kind == "MESSAGE_CREATE" and should_forward(data, self.channel_id, self.user_id):
                                    text = from_discord(data)
                                    if text:
                                        await deliver(RemoteMessage(author=display_name(data), text=text))
                            elif op == 1:
                                await ws.send(json.dumps({"op": 1, "d": last_seq[0]}))
                            elif op == 7:  # reconnect and resume
                                break
                            elif op == 9:  # invalid session: start a new one unless it's resumable
                                if not event.get("d"):
                                    session_id = resume_url = seq = None
                                await asyncio.sleep(1 + random.random() * 4)
                                break
                    finally:
                        beat.cancel()
                    code = ws.close_code
                    if code in FATAL_CLOSE_CODES:
                        raise RuntimeError(f"Discord closed the connection: {FATAL_CLOSE_CODES[code]}")
            except (OSError, asyncio.TimeoutError, websockets.exceptions.WebSocketException) as e:
                code = getattr(e, "rcvd", None) and e.rcvd.code
                if code in FATAL_CLOSE_CODES:
                    raise RuntimeError(f"Discord closed the connection: {FATAL_CLOSE_CODES[code]}") from None
                log.warning("Discord gateway: %s; reconnecting in %.0fs", e, backoff)
            await asyncio.sleep(backoff)
            backoff = min(backoff * 2, 60.0)
