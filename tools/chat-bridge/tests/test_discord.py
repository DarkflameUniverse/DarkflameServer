import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from chat_bridge.adapters.discord import (DiscordAdapter, display_name, escape_markdown, from_discord,  # noqa: E402
                                          should_forward, to_discord, webhook_username)
from chat_bridge.core import GameMessage  # noqa: E402


def game(**kw):
    base = dict(id=1, channel="zone", sender_name="Bob", message="hi", zone_id=1200, zone_name="Nimbus Station")
    base.update(kw)
    return GameMessage(**base)


class ToDiscord(unittest.TestCase):
    def test_bot_message(self):
        body = to_discord(game(message="*bold* @everyone"), via_webhook=False)
        self.assertEqual(body["content"], "**Bob** (Nimbus Station): \\*bold\\* @everyone")
        self.assertEqual(body["allowed_mentions"], {"parse": []})  # nobody is pinged

    def test_webhook_message(self):
        body = to_discord(game(sender_name="DiscordFan", message="`x`"), via_webhook=True)
        self.assertEqual(body["username"], "D1scordFan (Nimbus Station)")
        self.assertEqual(body["content"], "\\`x\\`")
        self.assertEqual(body["allowed_mentions"], {"parse": []})

    def test_no_zone(self):
        self.assertEqual(to_discord(game(zone_id=0, zone_name=""), False)["content"], "**Bob**: hi")

    def test_skipped_note(self):
        self.assertTrue(to_discord(game(), False, skipped=4)["content"].startswith("-# 4 message(s) skipped"))

    def test_escape(self):
        self.assertEqual(escape_markdown("a_b|c>d[e](f)"), "a\\_b\\|c\\>d\\[e\\](f)")

    def test_webhook_username(self):
        self.assertEqual(webhook_username("clyde"), "Clyd3")
        self.assertEqual(webhook_username("   "), "Player")
        self.assertEqual(len(webhook_username("x" * 200)), 80)


class FromDiscord(unittest.TestCase):
    def event(self, **kw):
        base = {"type": 0, "channel_id": "55", "content": "hello", "author": {"id": "1", "username": "alice", "global_name": "Alice"},
                "member": {"nick": None}, "mentions": [], "attachments": []}
        base.update(kw)
        return base

    def test_display_name(self):
        self.assertEqual(display_name(self.event()), "Alice")
        self.assertEqual(display_name(self.event(member={"nick": "Ally"})), "Ally")
        self.assertEqual(display_name(self.event(author={"id": "1", "username": "alice"})), "alice")

    def test_text(self):
        e = self.event(content="hi <@2> and <@!3> in <#9> <:lego:123> <@&4> <t:1700000000:R>",
                       mentions=[{"id": "2", "username": "bob"}, {"id": "3", "username": "c", "member": {"nick": "Cee"}}],
                       attachments=[{"id": "x"}])
        self.assertEqual(from_discord(e), "hi @bob and @Cee in #channel :lego: @role (a time) [attachment]")

    def test_forward_rules(self):
        self.assertTrue(should_forward(self.event(), "55", "999"))
        self.assertFalse(should_forward(self.event(channel_id="56"), "55", "999"))  # another channel
        self.assertFalse(should_forward(self.event(webhook_id="7"), "55", "999"))  # our own webhook posts
        self.assertFalse(should_forward(self.event(author={"id": "8", "bot": True}), "55", "999"))
        self.assertFalse(should_forward(self.event(author={"id": "999"}), "55", "999"))  # the bot itself
        self.assertFalse(should_forward(self.event(type=7), "55", "999"))  # a join notice
        self.assertTrue(should_forward(self.event(type=19), "55", "999"))  # a reply

    def test_config_validation(self):
        with self.assertRaises(ValueError):
            DiscordAdapter({})
        DiscordAdapter({"webhook_url": "https://example.invalid/hook"})
        DiscordAdapter({"bot_token": "t", "channel_id": "1"})


if __name__ == "__main__":
    unittest.main()
