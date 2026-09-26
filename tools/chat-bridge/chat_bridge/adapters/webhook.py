"""A send-only adapter: POSTs each game chat message as JSON to any URL (a Slack/Mattermost-style incoming webhook,
an automation tool, your own service). It shows how little an adapter needs.

Body: {"text": "[Nimbus Station] Bob: hi", "sender": "Bob", "message": "hi", "channel": "zone", "zone_id": 1200,
"zone_name": "Nimbus Station", "instance_id": 2, "id": 42}
"""
from __future__ import annotations

import asyncio
import json
import logging
import urllib.request

from ..core import GameMessage, plain_line
from .base import Adapter

log = logging.getLogger("chat_bridge.webhook")


def webhook_body(msg: GameMessage, skipped: int = 0) -> dict:
    body = {"text": plain_line(msg), "sender": msg.sender_name, "message": msg.message, "channel": msg.channel,
            "zone_id": msg.zone_id, "zone_name": msg.zone_name, "instance_id": msg.instance_id, "id": msg.id}
    if skipped:
        body["skipped"] = skipped
    return body


class WebhookAdapter(Adapter):
    name = "webhook"
    default_label = "Webhook"

    def _post(self, body: dict) -> None:
        req = urllib.request.Request(self.config["url"], data=json.dumps(body).encode(), method="POST",
                                     headers={"Content-Type": "application/json", **self.config.get("headers", {})})
        with urllib.request.urlopen(req, timeout=15) as resp:
            resp.read()

    async def send(self, msg: GameMessage, skipped: int = 0) -> None:
        await asyncio.to_thread(self._post, webhook_body(msg, skipped))
