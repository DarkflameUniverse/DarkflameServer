#include <gtest/gtest.h>

#include "ChatHistory.h"
#include "Permissions.h"

using ChatHistory::Access;

namespace {
	constexpr LWOOBJID ALICE = 1152921510000000001;
	constexpr LWOOBJID BOB = 1152921510000000002;

	IChatLog::ChatMessage Message(uint64_t id, const std::string& channel, LWOOBJID sender, const std::string& name, const std::string& text) {
		IChatLog::ChatMessage m;
		m.id = id;
		m.time = 1700000000 + static_cast<int64_t>(id);
		m.channel = channel;
		m.senderId = sender;
		m.senderName = name;
		m.accountId = sender == ALICE ? 1 : 2;
		m.zoneId = 1100;
		m.instanceId = 3;
		m.message = text;
		return m;
	}

	IChatLog::ChatMessage Whisper(uint64_t id, LWOOBJID from, LWOOBJID to, const std::string& text) {
		auto m = Message(id, "whisper", from, from == ALICE ? "Alice" : "Bob", text);
		m.recipientId = to;
		m.recipientName = to == ALICE ? "Alice" : "Bob";
		return m;
	}
}

TEST(ChatHistoryTests, EachPrivateChannelNeedsItsPermission) {
	const Access none{}, group{ true, false }, dms{ false, true };
	for (const auto* open : { "zone", "web" }) {
		EXPECT_TRUE(ChatHistory::CanRead(open, none)) << open;
	}
	EXPECT_FALSE(ChatHistory::CanRead("team", none));
	EXPECT_FALSE(ChatHistory::CanRead("guild", dms));
	EXPECT_TRUE(ChatHistory::CanRead("guild", group));
	EXPECT_FALSE(ChatHistory::CanRead("whisper", group));
	EXPECT_TRUE(ChatHistory::CanRead("whisper", dms));
	EXPECT_FALSE(ChatHistory::CanRead("something new", Access{ true, true }));
}

TEST(ChatHistoryTests, WhispersAreTheirOwnPermission) {
	const auto* dms = Permissions::Find("chat_dms");
	const auto* group = Permissions::Find("chat_private");
	ASSERT_NE(dms, nullptr);
	ASSERT_NE(group, nullptr);
	EXPECT_EQ(dms->category, group->category);
	ASSERT_NE(Permissions::Find("chat_flag"), nullptr);
	ASSERT_NE(Permissions::Find("chat_flag_review"), nullptr);
	// Reading whispers stays with the highest staff levels by default, like team and guild chat
	EXPECT_GE(dms->defaultLevel, group->defaultLevel);
}

TEST(ChatHistoryTests, MessageTextIsLeftOutWithoutThePermission) {
	auto m = Whisper(5, ALICE, BOB, "secret");
	m.filtered = true;
	const auto hidden = ChatHistory::MessageJson(m, {});
	EXPECT_EQ(hidden["message"], "");
	EXPECT_TRUE(hidden["redacted"].get<bool>());
	EXPECT_EQ(hidden["recipient_id"], std::to_string(BOB)); // IDs are strings: 64-bit numbers don't survive JavaScript
	const auto shown = ChatHistory::MessageJson(m, { false, true });
	EXPECT_EQ(shown["message"], "secret");
	EXPECT_FALSE(shown["redacted"].get<bool>());
	EXPECT_TRUE(shown["filtered"].get<bool>());
}

TEST(ChatHistoryTests, ConversationQueryFollowsTheChannel) {
	const Access all{ true, true };
	const auto zone = ChatHistory::ConversationQuery(Message(1, "zone", ALICE, "Alice", "hi"), all);
	EXPECT_EQ(zone.channel, "zone");
	EXPECT_EQ(zone.zoneId, 1100u);
	EXPECT_EQ(zone.instanceId, 3);

	const auto whisper = ChatHistory::ConversationQuery(Whisper(2, ALICE, BOB, "hi"), all);
	EXPECT_EQ(whisper.channel, "whisper");
	EXPECT_EQ(whisper.characterId, ALICE);
	EXPECT_EQ(whisper.otherCharacterId, BOB);
	EXPECT_TRUE(whisper.includeWhispers);

	auto team = Message(3, "team", BOB, "Bob", "go");
	team.teamId = 42;
	EXPECT_EQ(ChatHistory::ConversationQuery(team, all).teamId, 42);
	// Team chat logged before teams were recorded can't be told apart: no conversation rather than every team's
	team.teamId = 0;
	EXPECT_EQ(ChatHistory::ConversationQuery(team, all).channel, "none");

	auto guild = Message(4, "guild", BOB, "Bob", "gg");
	guild.guildId = 9;
	const auto guildQuery = ChatHistory::ConversationQuery(guild, { false, false });
	EXPECT_EQ(guildQuery.guildId, 9);
	EXPECT_FALSE(guildQuery.includePrivate); // what the viewer may read still decides
}

TEST(ChatHistoryTests, SameConversation) {
	EXPECT_TRUE(ChatHistory::SameConversation(Whisper(1, ALICE, BOB, "a"), Whisper(2, BOB, ALICE, "b")));
	EXPECT_FALSE(ChatHistory::SameConversation(Whisper(1, ALICE, BOB, "a"), Whisper(2, ALICE, 7, "b")));
	auto zone1 = Message(1, "zone", ALICE, "Alice", "a");
	auto zone2 = Message(2, "zone", BOB, "Bob", "b");
	EXPECT_TRUE(ChatHistory::SameConversation(zone1, zone2));
	zone2.instanceId = 4;
	EXPECT_FALSE(ChatHistory::SameConversation(zone1, zone2));
	EXPECT_FALSE(ChatHistory::SameConversation(zone1, Whisper(3, ALICE, BOB, "c")));
	auto guildA = Message(1, "guild", ALICE, "Alice", "a"), guildB = Message(2, "guild", BOB, "Bob", "b");
	guildA.guildId = 1; guildB.guildId = 2;
	EXPECT_FALSE(ChatHistory::SameConversation(guildA, guildB));
}

TEST(ChatHistoryTests, FlagStatuses) {
	EXPECT_EQ(ChatHistory::ParseStatus("open"), "open");
	EXPECT_EQ(ChatHistory::ParseStatus("actioned"), "actioned");
	EXPECT_EQ(ChatHistory::ParseStatus("dismissed"), "dismissed");
	EXPECT_FALSE(ChatHistory::ParseStatus("closed").has_value());
	EXPECT_FALSE(ChatHistory::ParseStatus("").has_value());
	EXPECT_EQ(ChatHistory::StatusAction("open", "actioned"), "actioned");
	EXPECT_EQ(ChatHistory::StatusAction("actioned", "open"), "reopened");
	EXPECT_EQ(ChatHistory::StatusAction("dismissed", "dismissed"), "");
}

TEST(ChatHistoryTests, ExcerptCutsBetweenCharacters) {
	const std::vector<IChatLog::ChatMessage> two{ Message(1, "zone", ALICE, "Alice", "hi"), Message(2, "zone", BOB, "Bob", "yo") };
	EXPECT_EQ(ChatHistory::Excerpt(two), "Alice: hi / Bob: yo");
	// "é" is two bytes; a cut in its middle backs up to before it
	const std::vector<IChatLog::ChatMessage> accent{ Message(1, "zone", ALICE, "A", "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9") };
	const auto cut = ChatHistory::Excerpt(accent, 9); // "A: " + 6 bytes fit before the ellipsis, 3 of them whole characters
	EXPECT_EQ(cut, "A: \xC3\xA9\xE2\x80\xA6");
	EXPECT_LE(cut.size(), 9u);
}

TEST(ChatHistoryTests, SnapshotMarksTheFlaggedMessagesInOrder) {
	const auto snapshot = ChatHistory::Snapshot({ Message(1, "zone", BOB, "Bob", "before"), Message(4, "zone", BOB, "Bob", "between") },
		{ Message(3, "zone", ALICE, "Alice", "bad"), Message(5, "zone", ALICE, "Alice", "worse") }, { Message(7, "zone", BOB, "Bob", "after") });
	ASSERT_EQ(snapshot.size(), 5u);
	std::vector<uint64_t> order;
	for (const auto& m : snapshot) order.push_back(m["id"].get<uint64_t>());
	EXPECT_EQ(order, (std::vector<uint64_t>{ 1, 3, 4, 5, 7 }));
	EXPECT_FALSE(snapshot[0]["flagged"].get<bool>());
	EXPECT_TRUE(snapshot[1]["flagged"].get<bool>());
	EXPECT_FALSE(snapshot[2]["flagged"].get<bool>());
	EXPECT_EQ(snapshot[3]["message"], "worse");
	EXPECT_FALSE(snapshot[0].contains("redacted")); // kept whole; hidden when shown (RedactSnapshot)
}

TEST(ChatHistoryTests, RedactSnapshotHidesChannelsTheViewerMayNotRead) {
	const auto snapshot = ChatHistory::Snapshot({}, { Whisper(1, ALICE, BOB, "secret") }, {});
	const auto hidden = ChatHistory::RedactSnapshot(snapshot, { true, false });
	ASSERT_EQ(hidden.size(), 1u);
	EXPECT_EQ(hidden[0]["message"], "");
	EXPECT_TRUE(hidden[0]["redacted"].get<bool>());
	EXPECT_TRUE(hidden[0]["flagged"].get<bool>());
	EXPECT_EQ(ChatHistory::RedactSnapshot(snapshot, { false, true })[0]["message"], "secret");
	// A damaged copy shows as nothing, not an error
	EXPECT_TRUE(ChatHistory::RedactSnapshot(nlohmann::json("not a list"), {}).empty());
}

TEST(ChatHistoryTests, SubjectIsAChosenSenderOrTheFirst) {
	auto web = Message(1, "web", 0, "[Web] gm", "announcement");
	const std::vector<IChatLog::ChatMessage> flagged{ web, Message(2, "zone", BOB, "Bob", "a"), Message(3, "zone", ALICE, "Alice", "b") };
	EXPECT_EQ(ChatHistory::Subject(flagged, 0)->senderId, BOB);
	EXPECT_EQ(ChatHistory::Subject(flagged, ALICE)->senderId, ALICE);
	EXPECT_EQ(ChatHistory::Subject(flagged, 12345)->senderId, BOB); // not one of the senders
	EXPECT_FALSE(ChatHistory::Subject({ web }, 0).has_value());
}
