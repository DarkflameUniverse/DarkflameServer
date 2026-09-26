"""Talks to the dashboard's public API: GET /api/chat, POST /api/chat/send, and the /ws chat_message topic."""
from __future__ import annotations

import asyncio
import json
import logging
import urllib.error
import urllib.parse
import urllib.request

log = logging.getLogger("chat_bridge.dashboard")


class ApiError(Exception):
    def __init__(self, status: int, message: str):
        super().__init__(f"HTTP {status}: {message}")
        self.status = status


class Dashboard:
    def __init__(self, url: str, token: str, timeout: float = 15.0):
        self.url = url.rstrip("/")
        self.token = token
        self.timeout = timeout

    # --- HTTP (stdlib, run in a thread so the event loop keeps going) ---

    def _request(self, method: str, path: str, params: dict | None = None, body: dict | None = None) -> dict:
        url = self.url + path
        if params:
            url += "?" + urllib.parse.urlencode(params)
        data = json.dumps(body).encode() if body is not None else None
        req = urllib.request.Request(url, data=data, method=method, headers={
            "Authorization": f"Bearer {self.token}",
            "Accept": "application/json",
            **({"Content-Type": "application/json"} if data is not None else {}),
        })
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                return json.loads(resp.read() or b"{}")
        except urllib.error.HTTPError as e:
            try:
                message = json.loads(e.read()).get("message", e.reason)
            except Exception:
                message = e.reason
            raise ApiError(e.code, str(message)) from None

    async def get(self, path: str, params: dict | None = None) -> dict:
        return await asyncio.to_thread(self._request, "GET", path, params)

    async def post(self, path: str, body: dict) -> dict:
        return await asyncio.to_thread(self._request, "POST", path, None, body)

    async def me(self) -> dict:
        return await self.get("/api/auth/me")

    async def chat_page(self, params: dict) -> dict:
        return await self.get("/api/chat", params)

    async def send_chat(self, body: dict) -> dict:
        return await self.post("/api/chat/send", body)

    # --- WebSocket ---

    def ws_url(self) -> str:
        parts = urllib.parse.urlsplit(self.url)
        scheme = "wss" if parts.scheme == "https" else "ws"
        return urllib.parse.urlunsplit((scheme, parts.netloc, parts.path.rstrip("/") + "/ws", "", ""))

    async def watch_chat(self, on_message, on_connected=None) -> None:
        """Connect to /ws, subscribe to chat_message and call on_message(event) for each. Returns when the socket closes."""
        import websockets  # only needed for live updates; polling works without it

        headers = {"Authorization": f"Bearer {self.token}"}
        try:
            from websockets.asyncio.client import connect  # websockets 13+
            ctx = connect(self.ws_url(), additional_headers=headers, open_timeout=self.timeout)
        except ImportError:  # older websockets
            ctx = websockets.connect(self.ws_url(), extra_headers=headers, open_timeout=self.timeout)
        async with ctx as ws:
            await ws.send(json.dumps({"event": "subscribe", "subscription": "chat_message"}))
            reply = json.loads(await asyncio.wait_for(ws.recv(), self.timeout))
            if reply.get("error"):
                raise ApiError(403, f"can't subscribe to chat_message: {reply['error']} (the account needs chat_view)")
            if on_connected:
                on_connected()
            async for raw in ws:
                try:
                    event = json.loads(raw)
                except ValueError:
                    continue
                if event.get("event") == "chat_message":
                    on_message(event)
