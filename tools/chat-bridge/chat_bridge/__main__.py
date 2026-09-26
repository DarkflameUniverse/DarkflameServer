"""python3 -m chat_bridge --config config.json [--check] [-v]"""
from __future__ import annotations

import argparse
import asyncio
import logging
import os
import sys

from .adapters import ADAPTERS
from .bridge import Bridge, Link
from .config import load_config
from .dashboard import ApiError, Dashboard


class _Subscribed(Exception):
    pass


def _stop() -> None:
    raise _Subscribed


async def check(dashboard: Dashboard) -> int:
    """Try the token: who it signs in as, and whether it can read and subscribe (it doesn't post anything)."""
    me = await dashboard.me()
    if not me.get("valid"):
        print("token: not valid")
        return 1
    print(f"token: {me['username']} (account {me['accountId']}, GM {me['gmLevel']})")
    ok = True
    try:
        page = await dashboard.chat_page({"limit": "1"})
        print(f"read chat (chat_view): yes, newest id {page.get('last_id')}")
    except ApiError as e:
        print(f"read chat (chat_view): no ({e})")
        ok = False
    try:
        await asyncio.wait_for(dashboard.watch_chat(lambda _e: None, _stop), 10)
    except _Subscribed:
        print("live chat (WebSocket): yes")
    except ImportError:
        print("live chat (WebSocket): no, the websockets package isn't installed (the bridge will poll)")
    except Exception as e:
        print(f"live chat (WebSocket): no ({e}); the bridge will poll")
    print("send chat (chat_send): not tried here; see the Permissions page")
    return 0 if ok else 1


def main() -> int:
    parser = argparse.ArgumentParser(prog="chat_bridge", description="Bridge game chat to Discord and other services through the dashboard API")
    parser.add_argument("--config", default=os.environ.get("CHAT_BRIDGE_CONFIG", "config.json"))
    parser.add_argument("--check", action="store_true", help="check the token and permissions, then exit")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO, stream=sys.stderr,
                        format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    # websockets' debug log prints request headers, which carry the API token
    logging.getLogger("websockets").setLevel(logging.INFO)

    config = load_config(args.config, {name: cls.default_label for name, cls in ADAPTERS.items()})
    if not os.path.isabs(config.state_file):
        config.state_file = os.path.join(os.path.dirname(os.path.abspath(args.config)), config.state_file)
    dashboard = Dashboard(config.url, config.token)
    if args.check:
        return asyncio.run(check(dashboard))

    links = []
    for link_cfg in config.links:
        if link_cfg.adapter not in ADAPTERS:
            parser.error(f"unknown adapter {link_cfg.adapter!r}; known: {', '.join(ADAPTERS)}")
        links.append(Link(link_cfg, ADAPTERS[link_cfg.adapter](link_cfg.settings)))
    try:
        asyncio.run(Bridge(config, dashboard, links).run())
    except KeyboardInterrupt:
        pass
    except RuntimeError as e:
        logging.error("%s", e)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
