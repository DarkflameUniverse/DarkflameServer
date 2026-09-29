#include "GameDependencies.h"
#include <gtest/gtest.h>

#include <ranges>

#include "CDClientDatabase.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "CDObjectSkillsTable.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "EffectsMessages.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "InventoryMessages.h"
#include "Item.h"
#include "SkillMessages.h"
#include "ZoneMessages.h"
#include "eReplicaComponentType.h"

#include "dGameMessagesTests/GameMessageTestUtils.h"

// Equipping and unequipping as live did (captures, e.g. equipping a head item over a ninja hood): ChangeObjectWorldState
// on the item and the equip-<slot> / unequip-<slot> effect on the wearer to everyone; the replaced item's proxies taken
// away (RemoveItemFromInventory, UnEquipInventory, ChangeObjectWorldState) and its equip skills uncast, to the player.

using namespace GameMessageTestUtils;

class EquipMessagesTests : public GameDependenciesTest {
protected:
	static constexpr LOT HOOD = 2641; // hair, with the proxy below and equip skills 362 and 371
	static constexpr LOT HOOD_PROXY = 10482; // clavicle, no equip animation
	static constexpr LOT HAT = 2632; // hair
	static constexpr LOT SWORD = 7000; // special_r
	static constexpr LOT ROCKET = 6416; // Extra_1: no effect

	std::unique_ptr<Entity> entity;
	std::unique_ptr<Character> character;
	InventoryComponent* inventory{};
	LWOOBJID nextId = 0x1000000000007000LL;

	void SetUp() override {
		SetUpDependencies();
		// Equipping looks the item's set up in the CDClient; none of these items is in one
		CDClientDatabase::Connect(":memory:");
		CDClientDatabase::ExecuteDML("CREATE TABLE ItemSets (setID INTEGER, itemIDs TEXT);");
		RegisterLot(HOOD, "hair", std::to_string(HOOD_PROXY));
		RegisterLot(HOOD_PROXY, "clavicle", "", true);
		RegisterLot(HAT, "hair");
		RegisterLot(SWORD, "special_r");
		RegisterLot(ROCKET, "Extra_1");
		auto& skills = CDClientManager::GetEntriesMutable<CDObjectSkillsTable>();
		skills.push_back({ HOOD, 362, 1, 0 });
		skills.push_back({ HOOD, 371, 1, 0 });
		skills.push_back({ HOOD_PROXY, 372, 1, 0 });

		info.lot = 1; // a player
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		entity = std::make_unique<Entity>(0x1000000000000001LL, info);
		inventory = entity->AddComponent<InventoryComponent>(-1); // before the character, so it doesn't load a save
		character = std::make_unique<Character>(1, nullptr);
		entity->SetCharacter(character.get());
		entity->AddComponent<CharacterComponent>(-1, character.get(), ClientAddress())->InitializeStatisticsFromString("");
	}

	void TearDown() override {
		entity->SetCharacter(nullptr);
		entity.reset();
		character.reset();
		TearDownDependencies();
	}

	static void RegisterLot(const LOT lot, const std::string& equipLocation, const std::string& subItems = "", const bool noEquipAnimation = false) {
		const auto componentID = static_cast<uint32_t>(96000 + lot);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(lot), componentID);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(lot), componentID);
		CDItemComponent component{};
		component.id = componentID;
		component.equipLocation = equipLocation;
		component.subItems = subItems;
		component.noEquipAnimation = noEquipAnimation;
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(componentID, component);
	}

	// The proxies (sub-items) equipped with an item
	std::vector<Item*> Proxies(const Item* item) {
		std::vector<Item*> proxies;
		for (auto* candidate : inventory->GetInventory(eInventoryType::ITEM_SETS)->GetItems() | std::views::values) {
			if (candidate->GetParent() == item->GetId()) proxies.push_back(candidate);
		}
		return proxies;
	}

	Item* Give(const LOT lot) {
		auto* const bag = inventory->GetInventory(eInventoryType::ITEMS);
		return new Item(nextId++, lot, bag, static_cast<uint32_t>(bag->GetItems().size()), 1, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
	}

	// Each game message sent: what it is, about which object, to whom, and the fields the tests look at
	struct Sent {
		MessageType::Game id{};
		LWOOBJID target{};
		bool toEveryone{};
		std::string detail;
		bool operator==(const Sent&) const = default;
	};

	std::vector<Sent> Messages(const std::vector<CapturedPacket>& packets) const {
		std::vector<Sent> out;
		for (const auto& packet : packets) {
			RakNet::BitStream bitStream(const_cast<uint8_t*>(packet.bytes.data()), packet.bytes.size(), false);
			Sent sent;
			if (!GameMessages::NetGameMsg::ReadPacketHeader(bitStream, sent.target, sent.id)) continue;
			sent.toEveryone = packet.broadcast && packet.sysAddr == UNASSIGNED_SYSTEM_ADDRESS;
			if (!sent.toEveryone) EXPECT_EQ(packet.sysAddr, ClientAddress());
			switch (sent.id) {
			case MessageType::Game::CHANGE_OBJECT_WORLD_STATE: {
				GameMessages::ChangeObjectWorldState msg;
				EXPECT_TRUE(msg.Deserialize(bitStream));
				sent.detail = msg.newState == eObjectWorldState::ATTACHED ? "ATTACHED" : "INVENTORY";
				break;
			}
			case MessageType::Game::PLAY_FX_EFFECT: {
				GameMessages::PlayFXEffect msg;
				EXPECT_TRUE(msg.Deserialize(bitStream));
				EXPECT_EQ(msg.effectID, -1);
				EXPECT_FLOAT_EQ(msg.priority, 1.07f);
				sent.detail = GeneralUtils::UTF16ToWTF8(msg.effectType);
				break;
			}
			case MessageType::Game::UN_EQUIP_INVENTORY: {
				GameMessages::UnEquipInventory msg;
				EXPECT_TRUE(msg.Deserialize(bitStream));
				EXPECT_TRUE(msg.bIgnoreCooldown);
				sent.detail = std::to_string(msg.itemToUnequip);
				break;
			}
			case MessageType::Game::REMOVE_ITEM_FROM_INVENTORY: {
				GameMessages::RemoveItemFromInventory msg;
				EXPECT_TRUE(msg.Deserialize(bitStream));
				sent.detail = std::to_string(msg.iObjID);
				break;
			}
			case MessageType::Game::UNCAST_SKILL: {
				GameMessages::UncastSkill msg;
				EXPECT_TRUE(msg.Deserialize(bitStream));
				sent.detail = std::to_string(msg.skillID);
				break;
			}
			case MessageType::Game::ADD_ITEM_TO_INVENTORY_CLIENT_SYNC:
				break;
			default:
				continue; // not part of equipping
			}
			out.push_back(sent);
		}
		return out;
	}
};

TEST_F(EquipMessagesTests, EquippingAndReplacingAsLiveDid) {
	using enum MessageType::Game;
	const auto player = entity->GetObjectID();
	auto* const hood = Give(HOOD);
	auto* const hat = Give(HAT);

	// The hood: attached with its effect, then its proxy added and attached (no effect: no equip animation)
	auto sent = Messages(Capture([&] { hood->Equip(); }));
	ASSERT_EQ(sent.size(), 4);
	EXPECT_EQ(sent[0], (Sent{ CHANGE_OBJECT_WORLD_STATE, hood->GetId(), true, "ATTACHED" }));
	EXPECT_EQ(sent[1], (Sent{ PLAY_FX_EFFECT, player, true, "equip-head" }));
	EXPECT_EQ(sent[2].id, ADD_ITEM_TO_INVENTORY_CLIENT_SYNC);
	const auto proxies = Proxies(hood);
	ASSERT_EQ(proxies.size(), 1);
	const auto proxy = proxies[0]->GetId();
	EXPECT_EQ(sent[3], (Sent{ CHANGE_OBJECT_WORLD_STATE, proxy, true, "ATTACHED" }));

	// The hat replaces it: the hat first, then the hood goes back with its effect, its proxy is taken away and its
	// equip skills are uncast (the proxy's aren't: they were never cast)
	sent = Messages(Capture([&] { hat->Equip(); }));
	const std::vector<Sent> expected = {
		{ CHANGE_OBJECT_WORLD_STATE, hat->GetId(), true, "ATTACHED" },
		{ PLAY_FX_EFFECT, player, true, "equip-head" },
		{ CHANGE_OBJECT_WORLD_STATE, hood->GetId(), true, "INVENTORY" },
		{ PLAY_FX_EFFECT, player, true, "unequip-head" },
		{ REMOVE_ITEM_FROM_INVENTORY, player, false, std::to_string(proxy) },
		{ UN_EQUIP_INVENTORY, player, false, std::to_string(proxy) },
		{ CHANGE_OBJECT_WORLD_STATE, proxy, true, "INVENTORY" },
		{ UNCAST_SKILL, player, false, "362" },
		{ UNCAST_SKILL, player, false, "371" },
	};
	EXPECT_EQ(sent, expected);
	EXPECT_TRUE(Proxies(hood).empty());
	EXPECT_TRUE(hat->IsEquipped());
	EXPECT_FALSE(hood->IsEquipped());
}

TEST_F(EquipMessagesTests, UnequippingAndSlotsWithoutAnEffect) {
	using enum MessageType::Game;
	const auto player = entity->GetObjectID();
	auto* const sword = Give(SWORD);
	auto* const rocket = Give(ROCKET);

	sword->Equip();
	auto sent = Messages(Capture([&] { sword->UnEquip(); }));
	EXPECT_EQ(sent, (std::vector<Sent>{ { CHANGE_OBJECT_WORLD_STATE, sword->GetId(), true, "INVENTORY" }, { PLAY_FX_EFFECT, player, true, "unequip-right" } }));

	// Extra_1 (a rocket) has no effect; the world state still goes out
	sent = Messages(Capture([&] { rocket->Equip(true); }));
	EXPECT_EQ(sent, (std::vector<Sent>{ { CHANGE_OBJECT_WORLD_STATE, rocket->GetId(), true, "ATTACHED" } }));
}
