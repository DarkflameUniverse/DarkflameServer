"""The smallest adapter: game chat is printed, and each line typed on stdin is posted in the game.

Lines are `Name: message`, or just `message` (sent as the configured default name). Useful for trying the bridge
and as a template for new adapters.
"""
from __future__ import annotations

import asyncio
import sys

from ..core import GameMessage, RemoteMessage, plain_line
from .base import Adapter, Deliver


def parse_line(line: str, default_name: str) -> RemoteMessage | None:
    line = line.strip()
    if not line:
        return None
    name, sep, text = line.partition(": ")
    if sep and 0 < len(name) <= 32 and text.strip():
        return RemoteMessage(author=name, text=text)
    return RemoteMessage(author=default_name, text=line)


class ConsoleAdapter(Adapter):
    name = "console"
    default_label = "Console"

    async def run(self, deliver: Deliver) -> None:
        if not self.config.get("read_stdin", True):
            return
        default_name = self.config.get("name", "Console")
        while True:
            line = await asyncio.to_thread(sys.stdin.readline)
            if not line:  # end of input
                return
            msg = parse_line(line, default_name)
            if msg:
                await deliver(msg)

    async def send(self, msg: GameMessage, skipped: int = 0) -> None:
        if skipped:
            print(f"({skipped} message(s) skipped: rate limit)", flush=True)
        print(plain_line(msg), flush=True)
