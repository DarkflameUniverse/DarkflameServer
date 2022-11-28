#include "GameMessages.h"
#include "GameDependencies.h"
#include <gtest/gtest.h>

class GameMessageTests : public GameDependenciesTest {
	protected:
		void SetUp() override {
			SetUpDependencies();
		}
		void TearDown() override {
			TearDownDependencies();
		}
};

/**
 * @brief Tests that the serialization struct BlueprintLoadItemResponse is serialized correctly
 *
 */
TEST_F(GameMessageTests, SendBlueprintLoadItemResponse) {
	GameMessages::SendBlueprintLoadItemResponse(UNASSIGNED_SYSTEM_ADDRESS, true, 515, 990);
	auto* bitStream = static_cast<dServerMock*>(Game::server)->GetMostRecentBitStream();
	ASSERT_NE(bitStream, nullptr);
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 200);
	// First read in the packets' header
	uint8_t rakNetPacketId{};
	uint16_t remoteConnectionType{};
	uint32_t packetId{};
	uint8_t always0{};

	bitStream->Read(rakNetPacketId);
	bitStream->Read(remoteConnectionType);
	bitStream->Read(packetId);
	bitStream->Read(always0);
	ASSERT_EQ(rakNetPacketId, 0x53);
	ASSERT_EQ(remoteConnectionType, 0x05);
	ASSERT_EQ(packetId, 0x17);
	ASSERT_EQ(always0, 0x00);

	// Next read in packet data

	uint8_t bSuccess{}; // unsigned bool
	LWOOBJID previousId{};
	LWOOBJID newId{};
	bitStream->Read(bSuccess);
	bitStream->Read(previousId);
	bitStream->Read(newId);
	ASSERT_EQ(bSuccess, static_cast<uint8_t>(true));
	ASSERT_EQ(previousId, 515);
	ASSERT_EQ(newId, 990);

	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}
