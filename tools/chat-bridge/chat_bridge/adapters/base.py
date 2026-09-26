"""The adapter interface: everything specific to one chat service lives behind it.

To add a service (Matrix, IRC, ...): subclass Adapter, implement run() and send(), and register it in
adapters/__init__.py. The bridge handles the dashboard, filtering, loop prevention and rate limits.
"""
from __future__ import annotations

from typing import Awaitable, Callable

from ..core import GameMessage, RemoteMessage

# Given to run(): call it with each message the service receives, to post it in the game
Deliver = Callable[[RemoteMessage], Awaitable[None]]


class Adapter:
    #: The config key under a link that holds this adapter's settings, e.g. "discord"
    name = "base"
    #: The default label players see ("[Discord] Bob: hi"); a link's "label" overrides it
    default_label = "Bridge"

    def __init__(self, config: dict):
        self.config = config

    async def run(self, deliver: Deliver) -> None:
        """Receive messages from the service for as long as the bridge runs, calling deliver() for each.

        Ignore messages the adapter itself posted (e.g. Discord bot/webhook messages) so they don't loop back.
        An adapter that only sends can just return.
        """

    async def send(self, msg: GameMessage, skipped: int = 0) -> None:
        """Post one game chat message on the service. `skipped` is how many were dropped by the rate limit before it."""
        raise NotImplementedError

    async def close(self) -> None:
        pass
