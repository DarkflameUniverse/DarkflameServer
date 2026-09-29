// Object construction data as live sent it. The expected bits are built field by field in the order the client reads
// them (lu_packets replica structs), with the values of live constructions from the 2011/2012 captures.
#include "GameDependencies.h"
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "BitStream.h"
#include "CDClientDatabase.h"
#include "CDComponentsRegistryTable.h"
#include "CDInventoryComponentTable.h"
#include "CDItemComponentTable.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "ControllablePhysicsComponent.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "ItemComponent.h"
#include "Item.h"
#include "ModelComponent.h"
#include "MovingPlatformComponent.h"
#include "SimplePhysicsComponent.h"
#include "User.h"
#include "eReplicaComponentType.h"
#include "eReplicaPacketType.h"

namespace {
	// Copies the bits of `stream` from bit `from` on into `tail`
	void Tail(RakNet::BitStream& stream, const uint32_t from, RakNet::BitStream& tail) {
		stream.SetReadOffset(from);
		const auto bits = stream.GetNumberOfBitsUsed() - from;
		std::vector<uint8_t> buffer(BITS_TO_BYTES(bits) + 1);
		stream.ReadBits(buffer.data(), bits, false);
		tail.WriteBits(buffer.data(), bits, false);
	}

	// The stream's bits as '0'/'1' characters, so a failure shows where they differ
	std::string Bits(const RakNet::BitStream& stream) {
		std::string bits;
		for (uint32_t i = 0; i < stream.GetNumberOfBitsUsed(); ++i) bits += (stream.GetData()[i / 8] & (0x80 >> (i % 8))) ? '1' : '0';
		return bits;
	}

	void ExpectSameBits(const RakNet::BitStream& actual, const RakNet::BitStream& expected) {
		EXPECT_EQ(Bits(actual), Bits(expected));
	}
}

class ReplicaConstructionTest : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

// time_since_created_on_server: live sent each object's age on the server in milliseconds (0 for the local player's own
// construction right after it was made, the zone's uptime for objects loaded with the zone). DLU sent 0.
TEST_F(ReplicaConstructionTest, TimeSinceCreatedOnServerIsTheObjectsAge) {
	info.lot = 12266;
	Entity entity(288300744895979394, info);

	const auto readTime = [&entity]() {
		RakNet::BitStream stream;
		entity.WriteBaseReplicaData(stream, eReplicaPacketType::CONSTRUCTION);
		LWOOBJID objectID{};
		LOT lot{};
		uint8_t nameLength{};
		uint32_t time{};
		EXPECT_TRUE(stream.Read(objectID));
		EXPECT_TRUE(stream.Read(lot));
		EXPECT_TRUE(stream.Read(nameLength));
		EXPECT_EQ(nameLength, 0);
		EXPECT_TRUE(stream.Read(time));
		EXPECT_EQ(objectID, 288300744895979394);
		EXPECT_EQ(lot, 12266);
		return time;
	};

	EXPECT_LT(readTime(), 1000u);
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	const auto later = readTime();
	EXPECT_GE(later, 30u);
	EXPECT_LT(later, 60000u);
	EXPECT_GE(entity.GetTimeSinceCreatedMs(), later);
}

// A civilian's character component: live always wrote the GM, current-activity and social blocks on construction
// (9,726 LOT 1 constructions: is_gm false, gm_level 0, current_activity Some(None), social_info always Some with guild 0
// and an empty guild name). DLU wrote them only after something dirtied them, so a civilian got none of them.
TEST_F(ReplicaConstructionTest, CharacterConstructionAlwaysWritesGmActivityAndSocialBlocks) {
	User user(UNASSIGNED_SYSTEM_ADDRESS, "tester", "key");
	Character character(1, &user);
	info.lot = 1;
	Entity player(1152921506064087003, info);
	player.SetCharacter(&character);
	character.SetEntity(&player);
	auto* const component = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);

	RakNet::BitStream construction;
	component->Serialize(construction, true);

	// Everything before the GM block: 4 absent claim codes, 10 u32 appearance fields, 4 u64s (account, last logout,
	// prop mod display time, u-score), the free-trial bit, 27 u64 statistics and the 2-bit transition state.
	constexpr uint32_t beforeGm = 4 + 10 * 32 + 4 * 64 + 1 + 27 * 64 + 2;
	RakNet::BitStream tail;
	Tail(construction, beforeGm, tail);

	RakNet::BitStream expected;
	expected.Write1(); // gm_pvp_info Some
	expected.Write0(); // pvp_enabled
	expected.Write0(); // is_gm
	expected.Write<uint8_t>(0); // gm_level
	expected.Write0(); // editor_enabled
	expected.Write<uint8_t>(0); // editor_level
	expected.Write1(); // current_activity Some
	expected.Write<uint32_t>(0); // GameActivity::None
	expected.Write1(); // social_info Some
	expected.Write<LWOOBJID>(0); // guild_id
	expected.Write<uint8_t>(0); // guild_name ""
	expected.Write1(); // is_lego_club_member (DLU treats everyone as a member)
	expected.Write<uint32_t>(0); // country code
	ExpectSameBits(tail, expected);

	// A serialization with nothing changed still writes none of them
	RakNet::BitStream serialization;
	component->Serialize(serialization, false);
	EXPECT_EQ(serialization.GetNumberOfBitsUsed(), 3u);

	player.SetCharacter(nullptr);
}

// Live wrote the cheat block (gravity scale, speed multiplier) on construction only when one of them was changed: 216
// of 20,617 controllable-physics constructions had it (players at run speed 1.05, enemies with gravity 0), never 1/1.
TEST_F(ReplicaConstructionTest, ControllablePhysicsConstructionWritesCheatsOnlyWhenChanged) {
	Entity entity(15, info);
	auto* const physics = entity.AddComponent<ControllablePhysicsComponent>(-1);

	const auto cheatBits = [physics]() {
		RakNet::BitStream stream;
		physics->Serialize(stream, true);
		stream.IgnoreBits(1 + 1 + 7 * 32); // no jetpack, stun immunities
		bool hasCheats{};
		EXPECT_TRUE(stream.Read(hasCheats));
		std::pair<float, float> cheats{ -1.0f, -1.0f };
		if (hasCheats) {
			EXPECT_TRUE(stream.Read(cheats.first));
			EXPECT_TRUE(stream.Read(cheats.second));
		}
		return std::make_pair(hasCheats, cheats);
	};

	EXPECT_FALSE(cheatBits().first);

	physics->SetSpeedMultiplier(1.05f);
	const auto [hasCheats, cheats] = cheatBits();
	EXPECT_TRUE(hasCheats);
	EXPECT_FLOAT_EQ(cheats.first, 1.0f);
	EXPECT_FLOAT_EQ(cheats.second, 1.05f);
}

// Simple physics: live sent motion type Fixed for objects without a motionType (never the invalid 0 DLU sent when the
// level gave none) and no velocity for fixed objects (38,970 Fixed constructions, all without velocity); moving
// platforms were Keyframed with a velocity (1,059 of 1,059).
TEST_F(ReplicaConstructionTest, SimplePhysicsConstructionLikeLive) {
	info.lot = 12266;
	Entity smashable(288300744895979394, info);
	auto* const physics = smashable.AddComponent<SimplePhysicsComponent>(-1);
	physics->SetPosition(NiPoint3(-259.89438f, 77.147575f, 485.42264f));
	NiQuaternion rotation = QuatUtils::IDENTITY;
	rotation.x = 0.51204455f;
	rotation.y = 0.0f;
	rotation.z = 0.0f;
	rotation.w = 0.8589589f;
	physics->SetRotation(rotation);

	RakNet::BitStream construction;
	physics->Serialize(construction, true);
	RakNet::BitStream expected;
	expected.Write0(); // is_climbable
	expected.Write<int32_t>(0); // climbing_property
	expected.Write0(); // velocity_info None
	expected.Write1(); // motion_type Some
	expected.Write<uint32_t>(5); // Fixed
	expected.Write1(); // position_rotation_info
	expected.Write(-259.89438f);
	expected.Write(77.147575f);
	expected.Write(485.42264f);
	expected.Write(0.51204455f); // x, y, z, w
	expected.Write(0.0f);
	expected.Write(0.0f);
	expected.Write(0.8589589f);
	ExpectSameBits(construction, expected);

	info.lot = 11950;
	Entity platform(288300744895900003, info);
	auto* const platformPhysics = platform.AddComponent<SimplePhysicsComponent>(-1);
	platform.AddComponent<MovingPlatformComponent>(-1, "");
	EXPECT_EQ(platformPhysics->GetPhysicsMotionState(), SimplePhysicsComponent::MOTION_TYPE_KEYFRAMED);
	RakNet::BitStream platformConstruction;
	platformPhysics->Serialize(platformConstruction, true);
	platformConstruction.IgnoreBits(1 + 32);
	bool hasVelocity{};
	EXPECT_TRUE(platformConstruction.Read(hasVelocity));
	EXPECT_TRUE(hasVelocity);

	// A property model without behaviors is keyframed
	info.lot = 14;
	Entity model(288300744895908946, info);
	auto* const modelPhysics = model.AddComponent<SimplePhysicsComponent>(-1);
	model.AddComponent<ModelComponent>(-1)->LoadBehaviors();
	EXPECT_EQ(modelPhysics->GetPhysicsMotionState(), SimplePhysicsComponent::MOTION_TYPE_KEYFRAMED);

	// A level-set motion type wins
	info.settings.Insert<uint32_t>(u"motionType", 1);
	Entity dynamic(288300744895900004, info);
	EXPECT_EQ(dynamic.AddComponent<SimplePhysicsComponent>(-1)->GetPhysicsMotionState(), SimplePhysicsComponent::MOTION_TYPE_DYNAMIC);
	info.settings.values.clear();
}

// Live replicated the DestructibleComponent factionList, so a list of -1 as [-1] (12,829 constructions: vendors,
// quickbuilds, bouncers; DLU dropped it and sent []), and a row with no faction but factionList 6 as [6].
TEST_F(ReplicaConstructionTest, TemplateFactionMinusOneIsReplicated) {
	CDClientDatabase::Connect(":memory:");
	CDClientDatabase::ExecuteDML("CREATE TABLE Factions (faction INTEGER, enemyList TEXT);");
	CDClientDatabase::ExecuteDML("INSERT INTO Factions VALUES (4, '1'), (6, '');");

	Entity vendor(15, info);
	auto* const destroyable = vendor.AddComponent<DestroyableComponent>(-1);
	destroyable->AddTemplateFactions("-1");
	EXPECT_EQ(destroyable->GetFactionIDs(), std::vector<int32_t>{ -1 });

	Entity enemy(16, info);
	auto* const enemyDestroyable = enemy.AddComponent<DestroyableComponent>(-1);
	enemyDestroyable->AddTemplateFactions("4");
	EXPECT_EQ(enemyDestroyable->GetFactionIDs(), std::vector<int32_t>{ 4 });
	EXPECT_EQ(enemyDestroyable->GetEnemyFactionsIDs(), std::vector<int32_t>{ 1 });

	Entity smashable(17, info);
	auto* const smashableDestroyable = smashable.AddComponent<DestroyableComponent>(-1);
	smashableDestroyable->AddTemplateFactions("6");
	EXPECT_EQ(smashableDestroyable->GetFactionIDs(), std::vector<int32_t>{ 6 });
}

class InventoryConstructionTest : public GameDependenciesTest {
protected:
	// The items of the live sample below (CDClient 1.10.64 values)
	static constexpr LOT HOOD = 2642; // hair, bind on pickup, proxy 10482
	static constexpr LOT HOOD_PROXY = 10482; // clavicle, bind on equip
	static constexpr LOT NPC_HELMET = 8520; // hair, item type 22 (a model), bind on equip
	static constexpr LOT NPC_KNIFE = 16682; // special_r, bind on equip
	static constexpr LOT PLAIN_SHIRT = 4000; // chest, bound neither way

	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:");
		CDClientDatabase::ExecuteDML("CREATE TABLE ItemSets (setID INTEGER, itemIDs TEXT);");
		RegisterItem(HOOD, "hair", 2, true, false, std::to_string(HOOD_PROXY));
		RegisterItem(HOOD_PROXY, "clavicle", 4, false, true);
		RegisterItem(NPC_HELMET, "hair", 22, false, true);
		RegisterItem(NPC_KNIFE, "special_r", 6, false, true);
		RegisterItem(PLAIN_SHIRT, "chest", 15, false, false);
	}

	void TearDown() override { TearDownDependencies(); }

	static void RegisterItem(const LOT lot, const std::string& equipLocation, const int32_t itemType, const bool isBOP, const bool isBOE, const std::string& subItems = "") {
		const auto componentID = static_cast<uint32_t>(97000 + lot);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(lot), componentID);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(lot), componentID);
		CDItemComponent component{};
		component.id = componentID;
		component.equipLocation = equipLocation;
		component.itemType = itemType;
		component.isBOP = isBOP;
		component.isBOE = isBOE;
		component.subItems = subItems;
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(componentID, component);
	}

	static void RegisterNpc(const LOT npc, const uint32_t componentID, const std::vector<LOT>& items) {
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(npc), 0); // the LOT's components are cached
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::INVENTORY) << 32 | static_cast<uint64_t>(npc), componentID);
		auto& table = CDClientManager::GetEntriesMutable<CDInventoryComponentTable>();
		for (const auto item : items) table.push_back({ componentID, static_cast<uint32_t>(item), 1, true });
	}

	// One EquippedItemInfo the way lu_packets reads it
	static void WriteItem(RakNet::BitStream& out, const LWOOBJID id, const LOT lot, const uint16_t slot, const uint32_t inventoryType, const bool bound) {
		out.Write(id);
		out.Write(lot);
		out.Write0(); // subkey
		out.Write1(); // count
		out.Write<uint32_t>(1);
		out.Write(slot != 0);
		if (slot != 0) out.Write(slot);
		out.Write(inventoryType != 0);
		if (inventoryType != 0) out.Write(inventoryType);
		out.Write0(); // extra_info
		out.Write(bound);
	}

	static std::vector<EquippedItem> Equipped(InventoryComponent& inventory) {
		std::vector<EquippedItem> items;
		for (const auto& item : inventory.GetEquippedItems() | std::views::values) items.push_back(item);
		return items;
	}
};

// Live, NPC LOT 7426 (FV Numb Chuck; capture 60a58346ece1 idx 58): the helmet, a model, is in the MODELS inventory, so
// it has inventory_type Model and slot 0 like the knife in ITEMS; both bind on equip, so both are bound.
// Live, NPC LOT 13790 (NJ Cole; capture 8846569a7d52 idx 10614): the hood's proxy is TempEquip, slot 0, bound.
// Every live construction ended with an empty equipped_model_transforms list.
TEST_F(InventoryConstructionTest, NpcItemsLikeLive) {
	RegisterNpc(7426, 263, { NPC_HELMET, NPC_KNIFE });
	info.lot = 7426;
	Entity numbChuck(288300744895900001, info);
	auto* const inventory = numbChuck.AddComponent<InventoryComponent>(-1);
	auto items = Equipped(*inventory);
	ASSERT_EQ(items.size(), 2u);

	RakNet::BitStream construction;
	inventory->Serialize(construction, true);
	RakNet::BitStream expected;
	expected.Write1();
	expected.Write<uint32_t>(2);
	WriteItem(expected, items[0].id, NPC_HELMET, 0, 5 /* Model */, true); // "hair" sorts before "special_r"
	WriteItem(expected, items[1].id, NPC_KNIFE, 0, 0, true);
	expected.Write1(); // equipped_model_transforms Some([])
	expected.Write<uint32_t>(0);
	ExpectSameBits(construction, expected);

	RegisterNpc(13790, 516, { HOOD });
	info.lot = 13790;
	Entity cole(288300744895900002, info);
	auto* const coleInventory = cole.AddComponent<InventoryComponent>(-1);
	items = Equipped(*coleInventory);
	ASSERT_EQ(items.size(), 2u);

	RakNet::BitStream coleConstruction;
	coleInventory->Serialize(coleConstruction, true);
	RakNet::BitStream coleExpected;
	coleExpected.Write1();
	coleExpected.Write<uint32_t>(2);
	WriteItem(coleExpected, items[0].id, HOOD_PROXY, 0, 4 /* TempEquip */, true); // "clavicle" before "hair"
	WriteItem(coleExpected, items[1].id, HOOD, 0, 0, true);
	coleExpected.Write1();
	coleExpected.Write<uint32_t>(0);
	ExpectSameBits(coleConstruction, coleExpected);

	// A serialization with nothing changed writes neither list
	RakNet::BitStream serialization;
	coleInventory->Serialize(serialization, false);
	EXPECT_EQ(serialization.GetNumberOfBitsUsed(), 2u);
}

// A player's items: is_bound is the item's bound state (live: bind-on-pickup and equipped bind-on-equip items true,
// items that bind neither way false, 2,990 of 49,534), and proxies are TempEquip.
TEST_F(InventoryConstructionTest, PlayerItemsLikeLive) {
	info.lot = 1;
	CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
	Entity player(0x1000000000000001LL, info);
	auto* const inventory = player.AddComponent<InventoryComponent>(-1);
	Character character(1, nullptr);
	player.SetCharacter(&character);
	player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS)->InitializeStatisticsFromString("");

	auto* const bag = inventory->GetInventory(eInventoryType::ITEMS);
	// A bind-on-pickup item is bound from the moment it is picked up
	auto* const hood = new Item(0x1000000000007000LL, HOOD, bag, 0, 1, true, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
	auto* const shirt = new Item(0x1000000000007001LL, PLAIN_SHIRT, bag, 1, 1, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
	inventory->EquipItem(hood);
	inventory->EquipItem(shirt);

	LWOOBJID proxyID = LWOOBJID_EMPTY;
	for (const auto& item : Equipped(*inventory)) if (item.lot == HOOD_PROXY) proxyID = item.id;
	ASSERT_NE(proxyID, LWOOBJID_EMPTY);

	RakNet::BitStream construction;
	inventory->Serialize(construction, true);
	RakNet::BitStream expected;
	expected.Write1();
	expected.Write<uint32_t>(3);
	WriteItem(expected, shirt->GetId(), PLAIN_SHIRT, 1, 0, false); // "chest", "clavicle", "hair"
	WriteItem(expected, proxyID, HOOD_PROXY, 0, 4 /* TempEquip */, true);
	WriteItem(expected, hood->GetId(), HOOD, 0, 0, true);
	expected.Write1();
	expected.Write<uint32_t>(0);
	ExpectSameBits(construction, expected);

	player.SetCharacter(nullptr);
}
