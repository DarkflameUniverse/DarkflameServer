#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "CDLootMatrixTable.h"
#include "CDPackageComponentTable.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "CombatMessages.h"
#include "ControllablePhysicsComponent.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "InventoryMessages.h"
#include "Item.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"

#include "dGameMessagesTests/GameMessageTestUtils.h"

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

// Opening a package: the client plays the item's use sound on a successful UseItemResult and a failure sound
// otherwise; live sent true for packages that opened (183 of the 187 in the captures).
class UseItemResultTests : public GameDependenciesTest {
protected:
	static constexpr LOT PACKAGE = 2998;
	static constexpr uint32_t PACKAGE_COMPONENT = 97000;
	static constexpr uint32_t LOOT_MATRIX = 97001;

	std::unique_ptr<Entity> entity;
	InventoryComponent* inventoryComponent{};

	void SetUp() override {
		SetUpDependencies();
		const auto itemComponent = static_cast<uint32_t>(96000 + PACKAGE);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		registry.insert_or_assign(static_cast<uint64_t>(PACKAGE), itemComponent);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(PACKAGE), itemComponent);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::PACKAGE) << 32 | static_cast<uint64_t>(PACKAGE), PACKAGE_COMPONENT);
		CDItemComponent component{};
		component.id = itemComponent;
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(itemComponent, component);
		CDClientManager::GetEntriesMutable<CDPackageComponentTable>().push_back({ PACKAGE_COMPONENT, LOOT_MATRIX, 0 });
		CDClientManager::GetEntriesMutable<CDLootMatrixTable>()[LOOT_MATRIX] = {}; // an empty package

		entity = std::make_unique<Entity>(1, info);
		inventoryComponent = entity->AddComponent<InventoryComponent>(-1);
	}

	void TearDown() override {
		entity.reset();
		CDClientManager::GetEntriesMutable<CDPackageComponentTable>().clear();
		CDClientManager::GetEntriesMutable<CDLootMatrixTable>().erase(LOOT_MATRIX);
		TearDownDependencies();
	}
};

TEST_F(UseItemResultTests, AnOpenedPackageReportsSuccess) {
	auto* const items = inventoryComponent->GetInventory(eInventoryType::ITEMS);
	auto* const package = new Item(0x7000, PACKAGE, items, 0, 2, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);

	const auto results = SentGameMessages<GameMessages::UseItemResult>(Capture([&] { package->UseNonEquip(package); }));

	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results[0].itemTemplateID, PACKAGE);
	EXPECT_TRUE(results[0].useItemResult);
	EXPECT_EQ(package->GetCount(), 1u);
}

// LWOInventoryComponent_Common::msgUseNonEquipmentItem (0x00cdeba0) uses nothing while GetStunned says the player can't
// use items (LWOControllablePhysComponent::msgGetStunned 0x00be0df0)
class UseItemStunTests : public UseItemResultTests {
protected:
	std::unique_ptr<Character> character;
	ControllablePhysicsComponent* physics{};
	Item* package{};

	void SetUp() override {
		UseItemResultTests::SetUp();
		// Again as a player
		entity.reset();
		info.lot = 1;
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		entity = std::make_unique<Entity>(1, info);
		inventoryComponent = entity->AddComponent<InventoryComponent>(-1);
		character = std::make_unique<Character>(1, nullptr);
		entity->SetCharacter(character.get());
		entity->AddComponent<CharacterComponent>(-1, character.get(), ClientAddress())->InitializeStatisticsFromString("");
		physics = entity->AddComponent<ControllablePhysicsComponent>(-1);
		auto* const items = inventoryComponent->GetInventory(eInventoryType::ITEMS);
		package = new Item(0x7000, PACKAGE, items, 0, 5, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
	}

	void TearDown() override {
		entity->SetCharacter(nullptr);
		entity.reset();
		character.reset();
		UseItemResultTests::TearDown();
	}

	void Use() {
		GameMessages::UseNonEquipmentItem msg;
		msg.itemToUse = package->GetId();
		inventoryComponent->OnUseNonEquipmentItem(msg);
	}

	void Stun(const eStateChangeType type, const bool cantUseItem = true) {
		GameMessages::SetStunned stun;
		stun.target = entity->GetObjectID();
		stun.StateChangeType = type;
		stun.bCantUseItem = cantUseItem;
		physics->ApplyStun(stun);
	}
};

TEST_F(UseItemStunTests, NothingIsUsedWhileStunnedFromUsingItems) {
	ASSERT_TRUE(entity->IsPlayer());
	Stun(eStateChangeType::PUSH);
	Stun(eStateChangeType::PUSH);
	Use();
	EXPECT_EQ(package->GetCount(), 5u);

	Stun(eStateChangeType::POP);
	Use();
	EXPECT_EQ(package->GetCount(), 5u);

	Stun(eStateChangeType::POP);
	Use();
	EXPECT_EQ(package->GetCount(), 4u);

	// Other stuns don't stop it, and a pop with nothing pushed stays at 0
	Stun(eStateChangeType::PUSH, false);
	Stun(eStateChangeType::POP);
	Use();
	EXPECT_EQ(package->GetCount(), 3u);
}

TEST_F(UseItemStunTests, ImmunityKeepsTheStunOff) {
	physics->SetStunImmunity(eStateChangeType::PUSH, LWOOBJID_EMPTY, false, false, false, false, false, false, true);
	GameMessages::SetStunned stun;
	stun.StateChangeType = eStateChangeType::PUSH;
	stun.bCantUseItem = true;
	stun.bIgnoreImmunity = false;
	physics->ApplyStun(stun);
	EXPECT_FALSE(physics->GetStunnedFromUsingItems());
	stun.bIgnoreImmunity = true;
	physics->ApplyStun(stun);
	EXPECT_TRUE(physics->GetStunnedFromUsingItems());
}

TEST_F(UseItemStunTests, NothingIsUsedWhileDead) {
	entity->AddComponent<DestroyableComponent>(-1)->SetIsDead(true);
	Use();
	EXPECT_EQ(package->GetCount(), 5u);
}
