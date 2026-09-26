import asyncio
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from chat_bridge.adapters.console import parse_line  # noqa: E402
from chat_bridge.adapters.webhook import webhook_body  # noqa: E402
from chat_bridge.bridge import Bridge, Link, load_state, save_state  # noqa: E402
from chat_bridge.config import Config, LinkConfig, expand_env, parse_config  # noqa: E402
from chat_bridge.core import (Cursor, GameFilter, GameMessage, RateLimiter, RemoteMessage, game_name,  # noqa: E402
                              is_own_message, plain_line, send_body, should_relay)


def row(id, channel="zone", sender="Bob", message="hi", zone=1200, instance=2, account=5, blocked=False, zone_name="Nimbus Station"):
    return {"id": id, "channel": channel, "sender_name": sender, "message": message, "zone_id": zone, "zone_name": zone_name,
            "instance_id": instance, "account_id": account, "blocked": blocked, "time": 0}


def msg(**kw):
    return GameMessage.from_json(row(kw.pop("id", 1), **kw))


class RelayRules(unittest.TestCase):
    def test_zone_chat_by_default(self):
        f = GameFilter()
        self.assertTrue(should_relay(msg(), f, "Discord"))
        self.assertFalse(should_relay(msg(channel="web", sender="[Web] admin"), f, "Discord"))

    def test_private_chat_never_without_opt_in(self):
        self.assertFalse(should_relay(msg(channel="whisper"), GameFilter(channels=frozenset({"zone", "whisper"})), "Discord"))
        self.assertFalse(should_relay(msg(channel="team"), GameFilter(channels=frozenset({"team"})), "Discord"))
        with self.assertRaises(ValueError):
            GameFilter.from_config({"channels": ["zone", "whisper"]})
        f = GameFilter.from_config({"channels": ["whisper"], "allow_private": True})
        self.assertTrue(should_relay(msg(channel="whisper"), f, "Discord"))

    def test_blocked_messages_are_not_relayed(self):
        self.assertFalse(should_relay(msg(blocked=True), GameFilter(), "Discord"))

    def test_zone_and_instance_filters(self):
        f = GameFilter(zones=frozenset({1100}))
        self.assertFalse(should_relay(msg(zone=1200), f, "Discord"))
        self.assertTrue(should_relay(msg(zone=1100), f, "Discord"))
        f = GameFilter(zones=frozenset({1200}), instances=frozenset({3}))
        self.assertFalse(should_relay(msg(zone=1200, instance=2), f, "Discord"))

    def test_empty_messages_skipped(self):
        self.assertFalse(should_relay(msg(message="   "), GameFilter(), "Discord"))

    def test_server_channel(self):
        self.assertEqual(GameFilter().server_channel(), "zone")
        self.assertEqual(GameFilter(channels=frozenset({"zone", "web"})).server_channel(), "")


class LoopPrevention(unittest.TestCase):
    def test_own_label_is_ignored(self):
        f = GameFilter(channels=frozenset({"zone", "web"}))
        own = msg(channel="web", sender="[Discord] Alice", account=99)
        self.assertTrue(is_own_message(own, "Discord"))
        self.assertFalse(should_relay(own, f, "Discord"))

    def test_own_account_is_ignored(self):
        f = GameFilter(channels=frozenset({"zone", "web"}))
        other_label = msg(channel="web", sender="[Matrix] Carol", account=99)
        self.assertFalse(should_relay(other_label, f, "Discord", own_account_id=99))
        self.assertTrue(should_relay(other_label, f, "Discord", own_account_id=7))

    def test_player_named_like_a_label_is_not_own(self):
        # Only the web channel carries labels; a player can't produce one
        self.assertFalse(is_own_message(msg(channel="zone", sender="[Discord] x"), "Discord"))


class Formatting(unittest.TestCase):
    def test_plain_line(self):
        self.assertEqual(plain_line(msg()), "[Nimbus Station] Bob: hi")
        self.assertEqual(plain_line(msg(zone=0, zone_name="")), "Bob: hi")
        self.assertEqual(plain_line(msg(zone=1234, zone_name="")), "[zone 1234] Bob: hi")

    def test_send_body(self):
        body = send_body(RemoteMessage("Alice", "hello\nthere"), "Discord", GameFilter())
        self.assertEqual(body, {"message": "hello there", "name": "Alice", "label": "Discord"})

    def test_send_body_targets(self):
        body = send_body(RemoteMessage("A", "x"), "Discord", GameFilter(send_zone=1200, send_instance=2))
        self.assertEqual((body["zone"], body["instance"]), (1200, 2))
        body = send_body(RemoteMessage("A", "x"), "Discord", GameFilter(send_instance=2))
        self.assertNotIn("zone", body)
        self.assertNotIn("instance", body)

    def test_send_body_limits(self):
        body = send_body(RemoteMessage("[mod] " + "n" * 50, "x" * 400), "Discord", GameFilter())
        self.assertEqual(len(body["message"]), 300)
        self.assertTrue(body["message"].endswith("..."))
        self.assertLessEqual(len(body["name"]), 32)
        self.assertNotIn("[", body["name"])
        self.assertIsNone(send_body(RemoteMessage("A", " \n "), "Discord", GameFilter()))
        with self.assertRaises(ValueError):
            send_body(RemoteMessage("A", "x"), "Bad[label]", GameFilter())

    def test_game_name_fallback(self):
        self.assertEqual(game_name("  \n"), "someone")

    def test_console_parse(self):
        self.assertEqual(parse_line("Alice: hi there\n", "Console"), RemoteMessage("Alice", "hi there"))
        self.assertEqual(parse_line("just text\n", "Console"), RemoteMessage("Console", "just text"))
        self.assertIsNone(parse_line("  \n", "Console"))

    def test_webhook_body(self):
        body = webhook_body(msg(id=42), skipped=3)
        self.assertEqual(body["text"], "[Nimbus Station] Bob: hi")
        self.assertEqual((body["id"], body["skipped"]), (42, 3))


class Resume(unittest.TestCase):
    def test_take_in_order_and_advance(self):
        c = Cursor(10)
        out = c.take([row(12), row(11), row(13)])
        self.assertEqual([m.id for m in out], [11, 12, 13])
        self.assertEqual(c.last_id, 13)

    def test_no_duplicates_across_overlapping_pages(self):
        c = Cursor(10)
        c.take([row(11), row(12)])
        out = c.take([row(12), row(13)])
        self.assertEqual([m.id for m in out], [13])

    def test_page_params(self):
        c = Cursor(7)
        self.assertEqual(c.page_params(500, "zone"), {"after": "7", "limit": "500", "channel": "zone"})
        self.assertEqual(c.page_params(500), {"after": "7", "limit": "500"})

    def test_state_file_roundtrip(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "s.json")
            self.assertEqual(load_state(path), 0)
            save_state(path, 1234)
            self.assertEqual(load_state(path), 1234)


class FakeDashboard:
    """Serves GET /api/chat from a list, like the server: after=, limit=, channel=."""

    def __init__(self, rows):
        self.rows = rows
        self.sent = []

    async def chat_page(self, params):
        after, limit = int(params["after"]), int(params["limit"])
        rows = [r for r in self.rows if r["id"] > after and (not params.get("channel") or r["channel"] == params["channel"])][:limit]
        return {"messages": rows, "last_id": max([after] + [r["id"] for r in rows])}

    async def send_chat(self, body):
        self.sent.append(body)
        return {"requestId": 1}


class Recorder:
    name = "rec"

    def __init__(self):
        self.got = []

    async def send(self, m, skipped=0):
        self.got.append(m)


class BridgeFlow(unittest.TestCase):
    def make(self, rows, state, game=None, per_min=0):
        cfg = Config(url="x", token="t", state_file=state)
        link = Link(LinkConfig("rec", "Discord", game or GameFilter(), {}, per_min, per_min), Recorder())
        dash = FakeDashboard(rows)
        return Bridge(cfg, dash, [link]), link, dash

    def test_resume_after_restart(self):
        rows = [row(i) for i in range(1, 8)]
        with tempfile.TemporaryDirectory() as d:
            state = os.path.join(d, "s.json")
            save_state(state, 3)
            bridge, link, _ = self.make(rows, state)
            asyncio.run(bridge.fetch())
            self.assertEqual([m.id for m, _ in self.drain(link)], [4, 5, 6, 7])
            self.assertEqual(load_state(state), 7)
            # A second read (e.g. the WebSocket woke it up again) repeats nothing
            asyncio.run(bridge.fetch())
            self.assertEqual(self.drain(link), [])
            # New messages after a restart (a new Bridge reading the saved state)
            rows.append(row(8))
            bridge2, link2, _ = self.make(rows, state)
            asyncio.run(bridge2.fetch())
            self.assertEqual([m.id for m, _ in self.drain(link2)], [8])

    def test_filtered_rows_still_move_the_cursor(self):
        rows = [row(1, channel="whisper"), row(2, blocked=True), row(3, channel="web", sender="[Discord] me")]
        with tempfile.TemporaryDirectory() as d:
            # Two channels: the server sends everything and the bridge drops what it mustn't relay
            bridge, link, _ = self.make(rows, os.path.join(d, "s.json"), game=GameFilter(channels=frozenset({"zone", "web"})))
            asyncio.run(bridge.fetch())
            self.assertEqual(self.drain(link), [])
            self.assertEqual(bridge.cursor.last_id, 3)

    def test_pages_through_a_backlog(self):
        rows = [row(i) for i in range(1, 1201)]
        with tempfile.TemporaryDirectory() as d:
            state = os.path.join(d, "s.json")
            save_state(state, 1)
            bridge, link, _ = self.make(rows, state)
            link.queue = asyncio.Queue()  # unbounded, to count them all
            asyncio.run(bridge.fetch())
            self.assertEqual(len(self.drain(link)), 1199)

    def test_rate_limit_to_remote(self):
        rows = [row(i) for i in range(1, 30)]
        with tempfile.TemporaryDirectory() as d:
            state = os.path.join(d, "s.json")
            save_state(state, 1)
            bridge, link, _ = self.make(rows, state, per_min=6)
            asyncio.run(bridge.fetch())
            self.assertEqual(len(self.drain(link)), 5)  # the burst
            self.assertGreater(link.out_limit.dropped, 0)

    def test_deliver_posts_with_label(self):
        with tempfile.TemporaryDirectory() as d:
            bridge, link, dash = self.make([], os.path.join(d, "s.json"), game=GameFilter(send_zone=1200))
            asyncio.run(bridge.deliverer(link)(RemoteMessage("Alice", "hey")))
            self.assertEqual(dash.sent, [{"message": "hey", "name": "Alice", "label": "Discord", "zone": 1200}])

    @staticmethod
    def drain(link):
        out = []
        while not link.queue.empty():
            out.append(link.queue.get_nowait())
        return out


class Limits(unittest.TestCase):
    def test_token_bucket(self):
        r = RateLimiter(per_minute=60, burst=2)
        self.assertTrue(r.allow(0.0))
        self.assertTrue(r.allow(0.0))
        self.assertFalse(r.allow(0.0))
        self.assertTrue(r.allow(1.0))  # one a second comes back
        self.assertEqual(r.take_dropped(), 1)
        self.assertEqual(r.dropped, 0)

    def test_zero_is_unlimited(self):
        r = RateLimiter(per_minute=0)
        self.assertTrue(all(r.allow(0.0) for _ in range(100)))


class ConfigParsing(unittest.TestCase):
    def test_env_expansion(self):
        env = {"TOK": "abc"}
        self.assertEqual(expand_env({"a": "$TOK", "b": ["${TOK}", "x$TOK"]}, env), {"a": "abc", "b": ["abc", "x$TOK"]})

    def test_parse(self):
        cfg = parse_config({"dashboard": {"token": "$T"}, "links": [{"adapter": "console", "console": {"name": "N"}}]},
                           environ={"T": "tok"}, default_labels={"console": "Console"})
        self.assertEqual(cfg.token, "tok")
        self.assertEqual(cfg.links[0].label, "Console")
        self.assertEqual(cfg.links[0].game.channels, frozenset({"zone"}))
        self.assertEqual(cfg.links[0].settings, {"name": "N"})

    def test_env_overrides_and_errors(self):
        cfg = parse_config({"links": [{"adapter": "console"}]}, environ={"DLU_API_TOKEN": "x", "DLU_DASHBOARD_URL": "https://d"})
        self.assertEqual((cfg.url, cfg.token), ("https://d", "x"))
        with self.assertRaises(ValueError):
            parse_config({"links": [{"adapter": "console"}]}, environ={})
        with self.assertRaises(ValueError):
            parse_config({"dashboard": {"token": "x"}, "links": [{"adapter": "console", "label": "[bad]"}]}, environ={})


if __name__ == "__main__":
    unittest.main()
