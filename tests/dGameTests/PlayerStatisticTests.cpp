#include <gtest/gtest.h>

#include "GameDependencies.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "Entity.h"
#include "PlayerMessages.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"

// Live sent UpdatePlayerStatistic (1481) server -> client for the statistics the server counted (docs/CaptureUnknowns.md).
// The packets below are from 2011/2012 live captures of the player 0x100000008eea7d5e.

using namespace GameMessageTestUtils;

class PlayerStatisticTest : public GameDependenciesTest {
protected:
	static constexpr LWOOBJID PLAYER = 0x100000008eea7d5eLL;
	std::unique_ptr<Entity> entity;
	std::unique_ptr<Character> character;
	CharacterComponent* characterComponent = nullptr;

	void SetUp() override {
		SetUpDependencies();
		entity = std::make_unique<Entity>(PLAYER, info);
		character = std::make_unique<Character>(1, nullptr);
		entity->SetCharacter(character.get());
		characterComponent = entity->AddComponent<CharacterComponent>(-1, character.get(), ClientAddress());
		characterComponent->InitializeStatisticsFromString(""); // every statistic 0
	}

	void TearDown() override {
		entity->SetCharacter(nullptr);
		entity.reset();
		character.reset();
		TearDownDependencies();
	}

	// The UpdatePlayerStatistic packets sent, each checked to go to the player's client only
	static std::vector<GameMessages::UpdatePlayerStatistic> Statistics(const std::vector<CapturedPacket>& packets) {
		std::vector<GameMessages::UpdatePlayerStatistic> statistics;
		for (const auto& packet : packets) {
			RakNet::BitStream bitStream(const_cast<uint8_t*>(packet.bytes.data()), packet.bytes.size(), false);
			LWOOBJID target{};
			MessageType::Game msgId{};
			if (!GameMessages::NetGameMsg::ReadPacketHeader(bitStream, target, msgId) || msgId != MessageType::Game::UPDATE_PLAYER_STATISTIC) continue;
			EXPECT_EQ(packet.sysAddr, ClientAddress());
			EXPECT_FALSE(packet.broadcast);
			auto& statistic = statistics.emplace_back();
			EXPECT_TRUE(statistic.Deserialize(bitStream));
			statistic.target = target;
		}
		return statistics;
	}
};

TEST_F(PlayerStatisticTest, SendsWhatTheServerCountsAsLiveDid) {
	// CurrencyCollected 1 (the amount is left out, the client's default) and 500
	auto packets = Capture([&] { characterComponent->UpdatePlayerStatistic(CurrencyCollected); });
	ASSERT_EQ(packets.size(), 1);
	EXPECT_EQ(FromHex("5305000c000000005e7dea8e00000010c9050100000000").bytes, packets[0].bytes); // captures are padded to whole bytes
	EXPECT_EQ(packets[0].sysAddr, ClientAddress());
	EXPECT_FALSE(packets[0].broadcast);

	packets = Capture([&] { characterComponent->UpdatePlayerStatistic(CurrencyCollected, 500); });
	ASSERT_EQ(packets.size(), 1);
	EXPECT_EQ(FromHex("5305000c000000005e7dea8e00000010c90501000000fa0080000000000000").bytes, packets[0].bytes); // captures are padded to whole bytes
	EXPECT_EQ(characterComponent->StatisticsToString().substr(0, 4), "501;");
}

TEST_F(PlayerStatisticTest, WhatTheClientReportsIsCountedNotEchoed) {
	const auto packets = Capture([&] { characterComponent->UpdatePlayerStatistic(PetsTamed, 1, true); });
	EXPECT_TRUE(packets.empty());
	EXPECT_EQ(characterComponent->StatisticsToString().substr(0, 16), "0;0;0;0;0;0;0;1;");
}

TEST_F(PlayerStatisticTest, MetersTraveledGoOutInBatches) {
	// The entity has no physics, so every update is measured from the origin
	auto packets = Capture([&] {
		characterComponent->TrackPositionUpdate(NiPoint3(10.0f, 0.0f, 0.0f));
		characterComponent->TrackPositionUpdate(NiPoint3(0.0f, 0.0f, 10.5f));
	});
	EXPECT_TRUE(Statistics(packets).empty()); // 20 counted, not sent yet

	packets = Capture([&] { characterComponent->TrackPositionUpdate(NiPoint3(0.0f, 9.0f, 0.0f)); });
	auto statistics = Statistics(packets);
	ASSERT_EQ(statistics.size(), 1);
	EXPECT_EQ(statistics[0].target, PLAYER);
	EXPECT_EQ(statistics[0].updateID, static_cast<int32_t>(MetersTraveled));
	EXPECT_EQ(statistics[0].updateValue, 29); // 10 + 10 + 9, the half meter waits
	EXPECT_EQ(FromHex("5305000c000000005e7dea8e00000010c9050c0000008e8000000000000000").bytes, packets[0].bytes); // captures are padded to whole bytes

	// The half meters add up; what's left goes out when the player leaves the world
	packets = Capture([&] {
		characterComponent->TrackPositionUpdate(NiPoint3(0.5f, 0.0f, 0.0f));
		characterComponent->TrackPositionUpdate(NiPoint3(3.0f, 0.0f, 0.0f));
		characterComponent->FlushMovementStatistics();
	});
	statistics = Statistics(packets);
	ASSERT_EQ(statistics.size(), 1);
	EXPECT_EQ(statistics[0].updateID, static_cast<int32_t>(MetersTraveled));
	EXPECT_EQ(statistics[0].updateValue, 4);

	// Every meter was counted once
	const auto split = GeneralUtils::SplitString(characterComponent->StatisticsToString(), ';');
	EXPECT_EQ(split[11], "33");

	// Live sent the last one even when nothing was left
	statistics = Statistics(Capture([&] { characterComponent->FlushMovementStatistics(); }));
	ASSERT_EQ(statistics.size(), 1);
	EXPECT_EQ(statistics[0].updateValue, 0);
}
