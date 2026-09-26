#include <gtest/gtest.h>
#include "PlayerAction.h"
#include "DataChanged.h"

TEST(PlayerActionTest, RequestRoundTrip) {
	PlayerActionRequest request;
	request.requestId = 42;
	request.action = ePlayerAction::RESCUE_CHARACTER;
	request.accountId = 7;
	request.characterId = 1152921504606846999LL;
	request.zoneId = 1200;
	request.disconnectReason = 11;

	RakNet::BitStream stream;
	request.Serialize(stream);

	PlayerActionRequest read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.requestId, 42u);
	EXPECT_EQ(read.action, ePlayerAction::RESCUE_CHARACTER);
	EXPECT_EQ(read.accountId, 7u);
	EXPECT_EQ(read.characterId, 1152921504606846999LL);
	EXPECT_EQ(read.zoneId, 1200u);
	EXPECT_EQ(read.disconnectReason, 11u);
}

TEST(PlayerActionTest, ResultRoundTrip) {
	PlayerActionResult result;
	result.requestId = 9;
	result.action = ePlayerAction::KICK_ACCOUNT;
	result.affected = 2;
	result.timedOut = true;

	RakNet::BitStream stream;
	result.Serialize(stream);

	PlayerActionResult read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.requestId, 9u);
	EXPECT_EQ(read.action, ePlayerAction::KICK_ACCOUNT);
	EXPECT_EQ(read.affected, 2u);
	EXPECT_TRUE(read.timedOut);
}

TEST(PlayerActionTest, TruncatedRequestFailsToDeserialize) {
	RakNet::BitStream stream;
	stream.Write<uint32_t>(1);
	PlayerActionRequest read;
	EXPECT_FALSE(read.Deserialize(stream));
}

TEST(DataChangedTest, RoundTrip) {
	DataChanged changed;
	changed.entries.push_back({ "characters", 1152921504606846999LL });
	changed.entries.push_back({ "economy", 0 });

	RakNet::BitStream stream;
	changed.Serialize(stream);

	DataChanged read;
	ASSERT_TRUE(read.Deserialize(stream));
	ASSERT_EQ(read.entries.size(), 2u);
	EXPECT_EQ(read.entries[0].table, "characters");
	EXPECT_EQ(read.entries[0].id, 1152921504606846999LL);
	EXPECT_EQ(read.entries[1].table, "economy");
	EXPECT_EQ(read.entries[1].id, 0);
}

TEST(DataChangedTest, RejectsBadInput) {
	RakNet::BitStream tooMany;
	tooMany.Write<uint16_t>(DataChanged::MAX_ENTRIES + 1);
	DataChanged read;
	EXPECT_FALSE(read.Deserialize(tooMany));

	RakNet::BitStream longName;
	longName.Write<uint16_t>(1);
	longName.Write<uint8_t>(DataChanged::MAX_TABLE_NAME + 1);
	EXPECT_FALSE(read.Deserialize(longName));

	RakNet::BitStream truncated;
	truncated.Write<uint16_t>(1);
	truncated.Write<uint8_t>(4);
	truncated.Write("mail", 4);
	EXPECT_FALSE(read.Deserialize(truncated)); // id missing
}

TEST(PlayerActionTest, ModerationFieldsRoundTrip) {
	PlayerActionRequest request;
	request.requestId = 7;
	request.action = ePlayerAction::PROPERTY_MODERATED;
	request.characterId = 1152921504606846999LL;
	request.targetId = 1152921510436607007LL;
	request.approved = false;
	request.text = "Please remove the offensive build";

	RakNet::BitStream stream;
	request.Serialize(stream);
	PlayerActionRequest read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.action, ePlayerAction::PROPERTY_MODERATED);
	EXPECT_EQ(read.targetId, 1152921510436607007LL);
	EXPECT_FALSE(read.approved);
	EXPECT_EQ(read.text, "Please remove the offensive build");

	// Text is capped so a bad packet can't ask for a huge allocation
	request.text = std::string(PlayerActionRequest::MAX_TEXT + 50, 'x');
	RakNet::BitStream capped;
	request.Serialize(capped);
	ASSERT_TRUE(read.Deserialize(capped));
	EXPECT_EQ(read.text.size(), PlayerActionRequest::MAX_TEXT);
}
