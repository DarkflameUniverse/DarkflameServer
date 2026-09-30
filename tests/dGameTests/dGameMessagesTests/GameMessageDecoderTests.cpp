#include <gtest/gtest.h>

#include "GameMessageDecoder.h"
#include "dCommonVars.h"
#include "MessageType/Game.h"
#include "SkillMessages.h"

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
	// The decoder gets the payload, what follows the message ID
	GameMessages::StartSkill sent;
	sent.optionalOriginatorID = 1234;
	sent.sBitStream = std::string("\x01\x02", 2);
	sent.skillID = 42;
	sent.optionalTargetID = 77;
	RakNet::BitStream stream;
	sent.Serialize(stream);

	const auto fields = GameMessageDecoder::Decode(MessageType::Game::START_SKILL, true, stream);
	ASSERT_TRUE(fields);
	EXPECT_EQ((*fields)["skillID"], 42);
	EXPECT_EQ((*fields)["optionalOriginatorID"], "1234");
	EXPECT_EQ((*fields)["optionalTargetID"], "77");
	EXPECT_EQ((*fields)["sBitStream"], nlohmann::json({ {"hex", "0102"} }));
}

// A message only sent one way is read with its struct in either direction (the layout is the same)
TEST(GameMessageDecoderTest, SentMessagesDecode) {
	GameMessages::EchoSyncSkill sent;
	sent.bDone = true;
	sent.uiSkillHandle = 5;
	RakNet::BitStream stream;
	sent.Serialize(stream);

	EXPECT_TRUE(GameMessageDecoder::CanDecode(MessageType::Game::ECHO_SYNC_SKILL, false));
	const auto fields = GameMessageDecoder::Decode(MessageType::Game::ECHO_SYNC_SKILL, false, stream);
	ASSERT_TRUE(fields);
	EXPECT_EQ((*fields)["bDone"], true);
	EXPECT_EQ((*fields)["uiSkillHandle"], 5);
}

// A message the server has no struct for (live only) isn't decoded
TEST(GameMessageDecoderTest, MessagesWithoutAStructAreNotDecoded) {
	RakNet::BitStream payload;
	payload.Write<uint32_t>(1);
	EXPECT_FALSE(GameMessageDecoder::HasStruct(MessageType::Game::SET_PVP_STATUS));
	EXPECT_FALSE(GameMessageDecoder::CanDecode(MessageType::Game::SET_PVP_STATUS, true));
	EXPECT_FALSE(GameMessageDecoder::Decode(MessageType::Game::SET_PVP_STATUS, true, payload));
}

TEST(GameMessageDecoderTest, ShortPayloadsFail) {
	RakNet::BitStream payload;
	payload.Write<LWOOBJID>(1); // PickupItem needs two IDs
	EXPECT_FALSE(GameMessageDecoder::Decode(MessageType::Game::PICKUP_ITEM, true, payload));
}
