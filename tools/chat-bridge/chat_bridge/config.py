"""Reading the bridge's JSON config. Any string value "$NAME" or "${NAME}" is read from the environment, so secrets
can stay out of the file; DLU_DASHBOARD_URL and DLU_API_TOKEN override the dashboard settings outright."""
from __future__ import annotations

import json
import os
import re
from dataclasses import dataclass, field

from .core import LABEL_PATTERN, GameFilter

_ENV = re.compile(r"^\$\{?([A-Za-z_][A-Za-z0-9_]*)\}?$")


def expand_env(value, environ=None):
    environ = os.environ if environ is None else environ
    if isinstance(value, dict):
        return {k: expand_env(v, environ) for k, v in value.items()}
    if isinstance(value, list):
        return [expand_env(v, environ) for v in value]
    if isinstance(value, str):
        m = _ENV.match(value)
        if m:
            return environ.get(m.group(1), "")
    return value


@dataclass
class LinkConfig:
    adapter: str
    label: str
    game: GameFilter
    settings: dict
    to_game_per_minute: float = 20
    to_remote_per_minute: float = 60


@dataclass
class Config:
    url: str
    token: str
    links: list = field(default_factory=list)
    state_file: str = "chat-bridge.state.json"
    poll_seconds: float = 5
    resync_seconds: float = 60


def parse_config(raw: dict, environ=None, default_labels: dict | None = None) -> Config:
    environ = os.environ if environ is None else environ
    raw = expand_env(raw, environ)
    dash = raw.get("dashboard", {})
    url = environ.get("DLU_DASHBOARD_URL") or dash.get("url") or "http://localhost:2006"
    token = environ.get("DLU_API_TOKEN") or dash.get("token", "")
    if not token:
        raise ValueError("no API token: set dashboard.token in the config or DLU_API_TOKEN")
    links = []
    for i, link in enumerate(raw.get("links", [])):
        adapter = link.get("adapter")
        if not adapter:
            raise ValueError(f"links[{i}] has no adapter")
        label = link.get("label") or (default_labels or {}).get(adapter, adapter.capitalize())
        if not LABEL_PATTERN.match(label):
            raise ValueError(f"links[{i}].label: up to 16 letters, digits, spaces, - or _")
        rate = link.get("rate", {})
        links.append(LinkConfig(adapter=adapter, label=label, game=GameFilter.from_config(link.get("game", {})),
                                settings=link.get(adapter, {}),
                                to_game_per_minute=float(rate.get("to_game_per_minute", 20)),
                                to_remote_per_minute=float(rate.get("to_remote_per_minute", 60))))
    if not links:
        raise ValueError("no links configured")
    return Config(url=url, token=token, links=links, state_file=raw.get("state_file", "chat-bridge.state.json"),
                  poll_seconds=float(raw.get("poll_seconds", 5)), resync_seconds=float(raw.get("resync_seconds", 60)))


def load_config(path: str, default_labels: dict | None = None) -> Config:
    with open(path, encoding="utf-8") as f:
        return parse_config(json.load(f), default_labels=default_labels)
