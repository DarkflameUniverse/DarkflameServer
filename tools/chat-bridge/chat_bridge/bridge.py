"""Runs the links: reads game chat from the dashboard (WebSocket to know when, GET /api/chat?after= to read), hands
each message to the links that want it, and posts what the other side says with POST /api/chat/send."""
from __future__ import annotations

import asyncio
import json
import logging
import os

from .adapters import Adapter
from .config import Config, LinkConfig
from .core import Cursor, GameMessage, RateLimiter, RemoteMessage, send_body, should_relay
from .dashboard import ApiError, Dashboard

log = logging.getLogger("chat_bridge")

PAGE = 500  # the API's largest page
QUEUE = 100  # messages waiting to go out per link before new ones are dropped


def load_state(path: str) -> int:
    try:
        with open(path, encoding="utf-8") as f:
            return int(json.load(f).get("last_id", 0))
    except (OSError, ValueError):
        return 0


def save_state(path: str, last_id: int) -> None:
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump({"last_id": last_id}, f)
    os.replace(tmp, path)


class Link:
    def __init__(self, cfg: LinkConfig, adapter: Adapter):
        self.cfg = cfg
        self.adapter = adapter
        self.out_limit = RateLimiter(cfg.to_remote_per_minute)
        self.in_limit = RateLimiter(cfg.to_game_per_minute)
        self.queue: asyncio.Queue = asyncio.Queue(QUEUE)


class Bridge:
    def __init__(self, config: Config, dashboard: Dashboard, links: list[Link]):
        self.config = config
        self.dashboard = dashboard
        self.links = links
        self.cursor = Cursor(load_state(config.state_file))
        self.account_id = 0
        self.wake = asyncio.Event()
        self.ws_up = False
        # Ask the server for just one channel when every link wants the same single one (e.g. only zone chat), so
        # rows the bridge would drop anyway (whispers, for an account allowed to read them) never leave the server
        channels = {link.cfg.game.server_channel() for link in links}
        self.server_channel = channels.pop() if len(channels) == 1 else ""

    # --- game -> other side ---

    async def fetch(self) -> None:
        while True:
            page = await self.dashboard.chat_page(self.cursor.page_params(PAGE, self.server_channel))
            rows = page.get("messages", [])
            for msg in self.cursor.take(rows):
                self.dispatch(msg)
            # Rows the server filtered out still move the cursor on
            self.cursor.last_id = max(self.cursor.last_id, int(page.get("last_id", 0)))
            save_state(self.config.state_file, self.cursor.last_id)
            if len(rows) < PAGE:
                return

    def dispatch(self, msg: GameMessage) -> None:
        for link in self.links:
            if not should_relay(msg, link.cfg.game, link.cfg.label, self.account_id):
                continue
            if not link.out_limit.allow():
                continue
            try:
                link.queue.put_nowait((msg, link.out_limit.take_dropped()))
            except asyncio.QueueFull:
                link.out_limit.dropped += 1

    async def send_loop(self, link: Link) -> None:
        while True:
            msg, skipped = await link.queue.get()
            try:
                await link.adapter.send(msg, skipped)
            except Exception as e:  # one failed send mustn't stop the link
                log.warning("%s: couldn't send message %s: %s", link.cfg.label, msg.id, e)

    async def read_loop(self) -> None:
        backoff = 1.0
        while True:
            try:
                await self.fetch()
                backoff = 1.0
            except ApiError as e:
                if e.status in (401, 403):
                    raise RuntimeError(f"the dashboard refused the token reading chat ({e}); it needs chat_view and api_access") from None
                log.warning("reading chat failed: %s", e)
            except OSError as e:
                log.warning("reading chat failed: %s; retrying in %.0fs", e, backoff)
                await asyncio.sleep(backoff)
                backoff = min(backoff * 2, 60.0)
                continue
            # The WebSocket says when there's something new; without it, poll. Either way re-read now and then.
            timeout = self.config.resync_seconds if self.ws_up else self.config.poll_seconds
            try:
                await asyncio.wait_for(self.wake.wait(), timeout)
            except asyncio.TimeoutError:
                pass
            self.wake.clear()

    async def ws_loop(self) -> None:
        backoff = 1.0

        def connected():
            nonlocal backoff
            self.ws_up, backoff = True, 1.0
            log.info("live: subscribed to chat_message")
            self.wake.set()  # catch up on anything said while the socket was down

        while True:
            try:
                await self.dashboard.watch_chat(lambda _event: self.wake.set(), connected)
                log.warning("WebSocket closed; polling every %.0fs until it's back", self.config.poll_seconds)
            except ImportError:
                log.warning("the websockets package isn't installed: polling every %.0fs", self.config.poll_seconds)
                return
            except ApiError as e:
                log.warning("%s; polling instead", e)
                return
            except Exception as e:
                log.warning("WebSocket: %s; polling every %.0fs, reconnecting in %.0fs", e, self.config.poll_seconds, backoff)
            self.ws_up = False
            self.wake.set()
            await asyncio.sleep(backoff)
            backoff = min(backoff * 2, 60.0)

    # --- other side -> game ---

    def deliverer(self, link: Link):
        async def deliver(remote: RemoteMessage) -> None:
            if not link.in_limit.allow():
                log.info("%s: rate limit, not posting %s's message in the game", link.cfg.label, remote.author)
                return
            body = send_body(remote, link.cfg.label, link.cfg.game)
            if body is None:
                return
            try:
                await self.dashboard.send_chat(body)
            except (ApiError, OSError) as e:
                log.warning("%s: couldn't post in the game: %s", link.cfg.label, e)
        return deliver

    async def run_adapter(self, link: Link) -> None:
        await link.adapter.run(self.deliverer(link))
        # An adapter that only sends (or reached the end of its input) keeps relaying out
        await asyncio.Event().wait()

    # --- start ---

    async def start(self) -> None:
        me = await self.dashboard.me()
        if not me.get("valid"):
            raise RuntimeError("the API token isn't valid (expired, revoked, or the account was signed out everywhere)")
        self.account_id = int(me.get("accountId", 0))
        log.info("signed in as %s (GM %s)", me.get("username"), me.get("gmLevel"))
        if not self.cursor.started:
            # First run: start from now rather than replaying the whole log
            page = await self.dashboard.chat_page({"limit": "1"})
            self.cursor.last_id = int(page.get("last_id", 0))
            save_state(self.config.state_file, self.cursor.last_id)
        log.info("relaying chat after id %d", self.cursor.last_id)

    async def run(self) -> None:
        await self.start()
        tasks = [self.read_loop(), self.ws_loop()]
        for link in self.links:
            tasks += [self.send_loop(link), self.run_adapter(link)]
        try:
            await asyncio.gather(*tasks)
        finally:
            for link in self.links:
                await link.adapter.close()
