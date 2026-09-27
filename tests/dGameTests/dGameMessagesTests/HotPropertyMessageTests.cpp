#include "PropertyMessages.h"
#include "BitStream.h"

#include <gtest/gtest.h>

namespace {
	std::u16string ReadWString(RakNet::BitStream& stream) {
		uint32_t size{};
		EXPECT_TRUE(stream.Read(size));
		std::u16string text;
		for (uint32_t i = 0; i < size; i++) {
			uint16_t character{};
			EXPECT_TRUE(stream.Read(character));
			text.push_back(character);
		}
		return text;
	}
}

// The layout NewsHotPropertyInfo::Deserialize (0x00c0cc20 in client 1.10.64) reads, and live servers sent
TEST(HotPropertyMessageTests, SerializesEntriesInClientOrder) {
	GameMessages::NewsSendHotPropertiesInfoToClient message;
	GameMessages::NewsSendHotPropertiesInfoToClient::HotPropertyInfo info;
	info.propertyId = 0x1000000000000123;
	info.ownerId = 0x1000000000000456;
	info.ownerName = u"Owner";
	info.reputation = 12345;
	info.templateId = 25166;
	info.name = u"Name";
	info.description = u"Desc";
	info.performanceCost = 42.5f;
	info.lastPublished = 1318111406;
	info.cloneId = 1278333;
	message.properties = { info, info };
	message.properties[1].templateId = 25188;

	RakNet::BitStream stream;
	message.Serialize(stream);

	uint32_t count{};
	ASSERT_TRUE(stream.Read(count));
	ASSERT_EQ(count, 2u);
	for (const int32_t expectedTemplate : { 25166, 25188 }) {
		LWOOBJID propertyId{}, ownerId{};
		uint64_t reputation{}, lastPublished{};
		int32_t templateId{};
		float performanceCost{};
		uint32_t cloneId{};
		ASSERT_TRUE(stream.Read(propertyId));
		ASSERT_TRUE(stream.Read(ownerId));
		EXPECT_EQ(ReadWString(stream), u"Owner");
		ASSERT_TRUE(stream.Read(reputation));
		ASSERT_TRUE(stream.Read(templateId));
		EXPECT_EQ(ReadWString(stream), u"Name");
		EXPECT_EQ(ReadWString(stream), u"Desc");
		ASSERT_TRUE(stream.Read(performanceCost));
		ASSERT_TRUE(stream.Read(lastPublished));
		ASSERT_TRUE(stream.Read(cloneId));
		EXPECT_EQ(propertyId, info.propertyId);
		EXPECT_EQ(ownerId, info.ownerId);
		EXPECT_EQ(reputation, 12345u);
		EXPECT_EQ(templateId, expectedTemplate);
		EXPECT_FLOAT_EQ(performanceCost, 42.5f);
		EXPECT_EQ(lastPublished, 1318111406u);
		EXPECT_EQ(cloneId, 1278333u);
	}
	// 4 + 2 * (8 + 8 + (4 + 5*2) + 8 + 4 + (4 + 4*2) + (4 + 4*2) + 4 + 8 + 4) = 4 + 2 * 82 bytes, nothing more
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
	EXPECT_EQ(stream.GetNumberOfBytesUsed(), 4u + 2u * 82u);
}

TEST(HotPropertyMessageTests, EmptyListIsJustTheCount) {
	GameMessages::NewsSendHotPropertiesInfoToClient message;
	RakNet::BitStream stream;
	message.Serialize(stream);
	EXPECT_EQ(stream.GetNumberOfBytesUsed(), 4u);
}
