#include "GameMessages.h"
#include "GameDependencies.h"
#include "PacketTestUtils.h"
#include "Entity.h"
#include "EntityManager.h"

#include <type_traits>

#include <gtest/gtest.h>

// Wire messages (NetGameMsg) and server-internal events (GameMsg) are separate types, so a message can only be
// sent where it belongs. These checks fail to compile if the split is undone.
namespace {
	template<typename T>
	concept CanSendLocally = requires(T msg) { msg.Send(); msg.Send(LWOOBJID{}); };

	template<typename T>
	concept CanSendToClient = requires(const T msg) { msg.Send(UNASSIGNED_SYSTEM_ADDRESS); };

	template<typename T>
	concept CanHandleLocally = requires(Entity & entity, T msg) { entity.HandleMsg(msg); };

	static_assert(!std::is_base_of_v<GameMessages::GameMsg, GameMessages::NetGameMsg>);
	static_assert(!std::is_base_of_v<GameMessages::NetGameMsg, GameMessages::GameMsg>);

	// A wire message can go to a client, never to local handlers.
	static_assert(CanSendToClient<GameMessages::DropClientLoot>);
	static_assert(!CanSendLocally<GameMessages::DropClientLoot>);
	static_assert(!CanHandleLocally<GameMessages::DropClientLoot>);

	// A local event can go to local handlers, never to a client.
	static_assert(CanSendLocally<GameMessages::GetPosition>);
	static_assert(!CanSendToClient<GameMessages::GetPosition>);
	static_assert(CanHandleLocally<GameMessages::GetPosition>);

	// A wire message wrapped for local delivery is a local event.
	static_assert(CanSendLocally<GameMessages::DropClientLootEvent>);
	static_assert(!CanSendToClient<GameMessages::DropClientLootEvent>);
}

class GameMsgSplitTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(GameMsgSplitTests, EventCarriesACopyWithTheTarget) {
	auto entity = std::make_unique<Entity>(15, info);

	LWOOBJID seenTarget = LWOOBJID_EMPTY;
	LOT seenItem = LOT_NULL;
	entity->RegisterMsg(MessageType::Game::DROP_CLIENT_LOOT, [&](GameMessages::GameMsg& msg) {
		auto& event = static_cast<GameMessages::DropClientLootEvent&>(msg);
		seenTarget = event.target;
		seenItem = event.msg.item;
		event.msg.item = 1; // handlers only modify the copy
		return true;
		});

	GameMessages::DropClientLoot loot;
	loot.target = 15;
	loot.item = 1234;

	GameMessages::DropClientLootEvent event(loot);
	EXPECT_EQ(event.msgId, MessageType::Game::DROP_CLIENT_LOOT);
	const auto sent = PacketTestUtils::Capture([&] { EXPECT_TRUE(entity->HandleMsg(event)); });
	EXPECT_TRUE(sent.empty()); // nothing went on the wire
	EXPECT_EQ(seenTarget, 15);
	EXPECT_EQ(seenItem, 1234);
	EXPECT_EQ(loot.item, 1234);
}

TEST_F(GameMsgSplitTests, DeliverLocallyNeverSendsToAClient) {
	GameMessages::DropClientLoot loot;
	loot.target = 0x7777; // no such entity
	const auto sent = PacketTestUtils::Capture([&] { EXPECT_FALSE(GameMessages::DeliverLocally(loot)); });
	EXPECT_TRUE(sent.empty());
}

TEST_F(GameMsgSplitTests, SendToClientNeverBroadcasts) {
	GameMessages::DropClientLoot loot;
	for (const auto& address : { SystemAddress(), UNASSIGNED_SYSTEM_ADDRESS }) {
		const auto sent = PacketTestUtils::Capture([&] { loot.SendToClient(address); });
		ASSERT_EQ(sent.size(), 1);
		EXPECT_FALSE(sent[0].broadcast);
		EXPECT_EQ(sent[0].sysAddr, address);
	}
	const auto broadcast = PacketTestUtils::Capture([&] { loot.Send(UNASSIGNED_SYSTEM_ADDRESS); });
	ASSERT_EQ(broadcast.size(), 1);
	EXPECT_TRUE(broadcast[0].broadcast);
}
