#include <gtest/gtest.h>

#include "GameDependencies.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "Entity.h"
#include "PlayerMessages.h"
#include "DestroyableComponent.h"
#include "CDClientDatabase.h"
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

TEST_F(PlayerStatisticTest, BricksAddedAreCollected) {
	auto statistics = Statistics(Capture([&] { characterComponent->TrackItemsAdded(eInventoryType::BRICKS, 3); }));
	ASSERT_EQ(statistics.size(), 1);
	EXPECT_EQ(statistics[0].updateID, static_cast<int32_t>(BricksCollected));
	EXPECT_EQ(statistics[0].updateValue, 3);
	EXPECT_TRUE(Statistics(Capture([&] { characterComponent->TrackItemsAdded(eInventoryType::ITEMS, 1); })).empty());
}

TEST_F(PlayerStatisticTest, ZoneStatisticsFromTheClientLeaveThePassportTotalsAlone) {
	// The client reports these per zone; the totals come from what the server sent (not counted twice)
	characterComponent->HandleZoneStatisticsUpdate(1100, u"CoinsCollected", 10);
	characterComponent->HandleZoneStatisticsUpdate(1100, u"BricksCollected", 2);
	characterComponent->HandleZoneStatisticsUpdate(1100, u"EnemiesSmashed", 1);
	EXPECT_EQ(characterComponent->StatisticsToString().substr(0, 10), "0;0;0;0;0;");
}

// Live: Die, then (to the killer) EnemiesSmashed for an NPC or SmashablesSmashed for another smashable, then the loot
TEST_F(PlayerStatisticTest, SmashingCountsEnemiesAndSmashables) {
	// Smash finds the killer through the entity manager, which looks the new entity up in the CDClient
	CDClientDatabase::Connect(":memory:");
	CDClientDatabase::ExecuteDML("CREATE TABLE ComponentsRegistry (id INTEGER, component_type INTEGER, component_id INTEGER);");
	ASSERT_NE(Game::entityManager->CreateEntity(info, nullptr, nullptr, true, 0x3FFFFFFFFFFELL), nullptr); // the zone control object loot asks
	auto* killer = Game::entityManager->CreateEntity(info, nullptr, nullptr, false, PLAYER + 1);
	ASSERT_NE(killer, nullptr);
	auto* killerCharacter = killer->AddComponent<CharacterComponent>(-1, character.get(), ClientAddress());
	killerCharacter->InitializeStatisticsFromString("");

	LWOOBJID victimID = 0x3FFF000000000001LL;
	const auto smash = [&](bool npc, bool smashable) {
		auto* victim = Game::entityManager->CreateEntity(info, nullptr, nullptr, false, victimID++);
		auto* destroyable = victim->AddComponent<DestroyableComponent>(-1);
		destroyable->SetMaxHealth(10.0f);
		destroyable->SetHealth(10);
		destroyable->SetIsNPC(npc);
		destroyable->SetIsSmashable(smashable);
		return Capture([&] { destroyable->Smash(killer->GetObjectID()); });
	};

	for (const auto& [npc, smashable, expected] : { std::tuple{ true, true, EnemiesSmashed }, std::tuple{ false, true, SmashablesSmashed } }) {
		const auto packets = smash(npc, smashable);
		size_t die = packets.size(), statistic = packets.size();
		for (size_t i = 0; i < packets.size(); i++) {
			RakNet::BitStream bitStream(const_cast<uint8_t*>(packets[i].bytes.data()), packets[i].bytes.size(), false);
			LWOOBJID target{};
			MessageType::Game msgId{};
			if (!GameMessages::NetGameMsg::ReadPacketHeader(bitStream, target, msgId)) continue;
			if (msgId == MessageType::Game::DIE && die == packets.size()) die = i;
			if (msgId == MessageType::Game::UPDATE_PLAYER_STATISTIC) statistic = i;
		}
		ASSERT_LT(die, packets.size());
		ASSERT_LT(statistic, packets.size());
		EXPECT_LT(die, statistic);
		const auto statistics = Statistics({ packets[statistic] });
		ASSERT_EQ(statistics.size(), 1);
		EXPECT_EQ(statistics[0].target, killer->GetObjectID());
		EXPECT_EQ(statistics[0].updateID, static_cast<int32_t>(expected));
	}

	// Neither an NPC nor smashable (e.g. a collectible spawner): nothing
	EXPECT_TRUE(Statistics(smash(false, false)).empty());
	killer->SetCharacter(nullptr);
}
