#include <gtest/gtest.h>

#include "GameMessageDecoder.h"
#include "dCommonVars.h"
#include "MessageType/Game.h"
#include "EchoSyncSkill.h"
#include "StartSkill.h"

TEST(GameMessageDecoderTest, DecodesATypedClientMessage) {
	RakNet::BitStream payload;
	payload.Write(true);                      // bIsMultiInteractUse
	payload.Write<uint32_t>(3);               // multiInteractID
	payload.Write<int32_t>(2);                // multiInteractType
	payload.Write<LWOOBJID>(1152921504606846999LL); // object
	payload.Write(false);                     // secondary

	ASSERT_TRUE(GameMessageDecoder::CanDecode(MessageType::Game::REQUEST_USE, true));
	const auto fields = GameMessageDecoder::Decode(MessageType::Game::REQUEST_USE, true, payload);
	ASSERT_TRUE(fields);
	EXPECT_EQ((*fields)["bIsMultiInteractUse"], true);
	EXPECT_EQ((*fields)["multiInteractID"], 3);
	EXPECT_EQ((*fields)["multiInteractType"], 2);
	// Object IDs are strings, since they don't fit in a JavaScript number
	EXPECT_EQ((*fields)["object"], "1152921504606846999");
	EXPECT_EQ((*fields)["secondary"], false);
}

TEST(GameMessageDecoderTest, ReadsWhatTheServerWrites) {
	// The server's own Serialize writes the message ID first; the decoder gets what follows it
	StartSkill sent(1234, std::string("\x01\x02", 2), 42);
	sent.optionalTargetID = 77;
	RakNet::BitStream stream;
	sent.Serialize(stream);
	MessageType::Game id{};
	ASSERT_TRUE(stream.Read(id));
	ASSERT_EQ(id, MessageType::Game::START_SKILL);

	const auto fields = GameMessageDecoder::Decode(MessageType::Game::START_SKILL, true, stream);
	ASSERT_TRUE(fields);
	EXPECT_EQ((*fields)["skillID"], 42);
	EXPECT_EQ((*fields)["optionalOriginatorID"], "1234");
	EXPECT_EQ((*fields)["optionalTargetID"], "77");
	EXPECT_EQ((*fields)["sBitStream"], "0102");
}

TEST(GameMessageDecoderTest, DirectionMatters) {
	EchoSyncSkill sent;
	sent.bDone = true;
	sent.uiSkillHandle = 5;
	RakNet::BitStream stream;
	sent.Serialize(stream);
	MessageType::Game id{};
	ASSERT_TRUE(stream.Read(id));

	EXPECT_FALSE(GameMessageDecoder::CanDecode(MessageType::Game::ECHO_SYNC_SKILL, true));
	const auto fields = GameMessageDecoder::Decode(MessageType::Game::ECHO_SYNC_SKILL, false, stream);
	ASSERT_TRUE(fields);
	EXPECT_EQ((*fields)["bDone"], true);
	EXPECT_EQ((*fields)["uiSkillHandle"], 5);
}

TEST(GameMessageDecoderTest, UntypedMessagesAreNotDecoded) {
	RakNet::BitStream payload;
	payload.Write<uint32_t>(1);
	EXPECT_FALSE(GameMessageDecoder::CanDecode(MessageType::Game::PLAY_EMOTE, true));
	EXPECT_FALSE(GameMessageDecoder::Decode(MessageType::Game::PLAY_EMOTE, true, payload));
}

TEST(GameMessageDecoderTest, ShortPayloadsFail) {
	RakNet::BitStream payload;
	payload.Write<LWOOBJID>(1); // PickupItem needs two IDs
	EXPECT_FALSE(GameMessageDecoder::Decode(MessageType::Game::PICKUP_ITEM, true, payload));
}
