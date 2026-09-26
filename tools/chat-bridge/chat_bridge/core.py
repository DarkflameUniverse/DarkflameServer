"""The parts of the bridge that don't touch the network: what to relay, how to word it, where to resume.

Everything here is plain Python so it can be unit tested without a dashboard or a chat service.
"""
from __future__ import annotations

import re
import time
from dataclasses import dataclass, field
from typing import Iterable, Optional

# Limits of POST /api/chat/send (dDashboardServer/routes/ChatRoutes.cpp)
GAME_MESSAGE_MAX = 300
GAME_NAME_MAX = 32
LABEL_PATTERN = re.compile(r"^[A-Za-z0-9 _-]{1,16}$")

PRIVATE_CHANNELS = frozenset({"whisper", "team"})


@dataclass(frozen=True)
class GameMessage:
    """One row of GET /api/chat or one chat_message WebSocket event."""
    id: int
    channel: str
    sender_name: str
    message: str
    zone_id: int = 0
    zone_name: str = ""
    instance_id: int = 0
    account_id: int = 0
    blocked: bool = False
    time: int = 0

    @classmethod
    def from_json(cls, data: dict) -> "GameMessage":
        return cls(
            id=int(data.get("id", 0)),
            channel=str(data.get("channel", "")),
            sender_name=str(data.get("sender_name", "")),
            message=str(data.get("message", "")),
            zone_id=int(data.get("zone_id", 0) or 0),
            zone_name=str(data.get("zone_name", "") or ""),
            instance_id=int(data.get("instance_id", 0) or 0),
            account_id=int(data.get("account_id", 0) or 0),
            blocked=bool(data.get("blocked", False)),
            time=int(data.get("time", 0) or 0),
        )


@dataclass(frozen=True)
class RemoteMessage:
    """A message from the other side (Discord, the console, ...) to post in the game."""
    author: str
    text: str


@dataclass
class GameFilter:
    """Which game chat a link relays out, and where messages coming in are posted."""
    channels: frozenset = frozenset({"zone"})
    zones: frozenset = frozenset()          # empty: every zone
    instances: frozenset = frozenset()      # empty: every world of those zones
    allow_private: bool = False             # whispers and team chat; off unless explicitly turned on
    send_zone: int = 0                      # 0: every zone
    send_instance: Optional[int] = None     # with send_zone: only that world

    @classmethod
    def from_config(cls, cfg: dict) -> "GameFilter":
        channels = frozenset(cfg.get("channels", ["zone"]))
        allow_private = bool(cfg.get("allow_private", False))
        private = channels & PRIVATE_CHANNELS
        if private and not allow_private:
            raise ValueError(f"channels {sorted(private)} are private: set \"allow_private\": true to relay them")
        send_instance = cfg.get("send_instance")
        return cls(
            channels=channels,
            zones=frozenset(int(z) for z in cfg.get("zones", [])),
            instances=frozenset(int(i) for i in cfg.get("instances", [])),
            allow_private=allow_private,
            send_zone=int(cfg.get("send_zone", 0) or 0),
            send_instance=int(send_instance) if send_instance is not None else None,
        )

    def server_channel(self) -> str:
        """The channel= to ask the API for, so rows the bridge would drop (like whispers) never leave the server."""
        return next(iter(self.channels)) if len(self.channels) == 1 else ""


def is_own_message(msg: GameMessage, label: str, own_account_id: int = 0) -> bool:
    """A message this bridge (or anything posting with the same label) sent into the game.

    POST /api/chat/send logs what it posts in the "web" channel as "[label] name", from the token's account.
    """
    if msg.channel != "web":
        return False
    if msg.sender_name.startswith(f"[{label}] "):
        return True
    return bool(own_account_id) and msg.account_id == own_account_id


def should_relay(msg: GameMessage, flt: GameFilter, label: str, own_account_id: int = 0) -> bool:
    """Whether a game message goes to the other side of a link."""
    if msg.blocked:  # the chat filter stopped it: no player saw it, so neither should anyone else
        return False
    if msg.channel in PRIVATE_CHANNELS and not flt.allow_private:
        return False
    if msg.channel not in flt.channels:
        return False
    if is_own_message(msg, label, own_account_id):  # loop prevention
        return False
    if flt.zones and msg.zone_id not in flt.zones:
        return False
    if flt.instances and msg.instance_id not in flt.instances:
        return False
    return bool(msg.message.strip())


def one_line(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def truncate(text: str, limit: int) -> str:
    return text if len(text) <= limit else text[: limit - 3].rstrip() + "..."


def game_name(author: str, fallback: str = "someone") -> str:
    """A display name the game endpoint accepts: one line, no brackets, 1-32 characters."""
    name = one_line(author).replace("[", "(").replace("]", ")")
    return truncate(name, GAME_NAME_MAX) or fallback


def game_text(text: str) -> str:
    """Message text the game endpoint accepts: one line, 1-300 characters (empty: nothing to send)."""
    return truncate(one_line(text), GAME_MESSAGE_MAX)


def send_body(remote: RemoteMessage, label: str, flt: GameFilter) -> Optional[dict]:
    """The JSON for POST /api/chat/send, or None when there's nothing to post."""
    if not LABEL_PATTERN.match(label):
        raise ValueError("label is up to 16 letters, digits, spaces, - or _")
    text = game_text(remote.text)
    if not text:
        return None
    body = {"message": text, "name": game_name(remote.author), "label": label}
    if flt.send_zone:
        body["zone"] = flt.send_zone
        if flt.send_instance is not None:
            body["instance"] = flt.send_instance
    return body


def where(msg: GameMessage) -> str:
    """Where in the game a message was said, for the other side."""
    if not msg.zone_id:
        return ""
    return msg.zone_name or f"zone {msg.zone_id}"


def plain_line(msg: GameMessage) -> str:
    """A plain one-line rendering: `[Nimbus Station] Bob: hi` (web messages already carry their [label])."""
    place = where(msg)
    prefix = f"[{place}] " if place else ""
    return f"{prefix}{msg.sender_name}: {msg.message}"


class Cursor:
    """Remembers the newest chat id handled so a restart or a dropped socket resumes without gaps or repeats.

    The bridge always reads with GET /api/chat?after=<last_id> (the WebSocket only says when to read), so ids
    arrive in order; take() still drops anything at or below last_id in case pages overlap.
    """

    def __init__(self, last_id: int = 0):
        self.last_id = int(last_id)

    @property
    def started(self) -> bool:
        return self.last_id > 0

    def take(self, rows: Iterable[dict]) -> list[GameMessage]:
        fresh = sorted((GameMessage.from_json(r) for r in rows), key=lambda m: m.id)
        out = []
        for msg in fresh:
            if msg.id <= self.last_id:
                continue
            out.append(msg)
            self.last_id = msg.id
        return out

    def page_params(self, limit: int, channel: str = "") -> dict:
        params = {"after": str(self.last_id), "limit": str(limit)}
        if channel:
            params["channel"] = channel
        return params


@dataclass
class RateLimiter:
    """A token bucket: `per_minute` messages a minute, with bursts of up to `burst`."""
    per_minute: float
    burst: int = 5
    _tokens: float = field(default=-1.0, repr=False)
    _stamp: float = field(default=0.0, repr=False)
    dropped: int = 0

    def allow(self, now: Optional[float] = None) -> bool:
        now = time.monotonic() if now is None else now
        if self.per_minute <= 0:
            return True
        if self._tokens < 0:
            self._tokens, self._stamp = float(self.burst), now
        self._tokens = min(float(self.burst), self._tokens + (now - self._stamp) * self.per_minute / 60.0)
        self._stamp = now
        if self._tokens >= 1.0:
            self._tokens -= 1.0
            return True
        self.dropped += 1
        return False

    def take_dropped(self) -> int:
        n, self.dropped = self.dropped, 0
        return n
