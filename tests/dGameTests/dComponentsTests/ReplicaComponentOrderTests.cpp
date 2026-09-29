#include "GameDependencies.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>

#include "BitStream.h"
#include "CDClientDatabase.h"
#include "Character.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eReplicaPacketType.h"
#include "tinyxml2.h"

#include "AchievementVendorComponent.h"
#include "BaseCombatAIComponent.h"
#include "BouncerComponent.h"
#include "BuffComponent.h"
#include "CharacterComponent.h"
#include "CollectibleComponent.h"
#include "ControllablePhysicsComponent.h"
#include "DestroyableComponent.h"
#include "DonationVendorComponent.h"
#include "HavokVehiclePhysicsComponent.h"
#include "InventoryComponent.h"
#include "ItemComponent.h"
#include "LevelProgressionComponent.h"
#include "LUPExhibitComponent.h"
#include "MiniGameControlComponent.h"
#include "ModelComponent.h"
#include "ModuleAssemblyComponent.h"
#include "MovingPlatformComponent.h"
#include "PetComponent.h"
#include "PhantomPhysicsComponent.h"
#include "PlayerForcedMovementComponent.h"
#include "PossessableComponent.h"
#include "PossessorComponent.h"
#include "QuickBuildComponent.h"
#include "RacingControlComponent.h"
#include "RacingSoundTriggerComponent.h"
#include "RenderComponent.h"
#include "RigidbodyPhantomPhysicsComponent.h"
#include "ScriptComponent.h"
#include "ScriptedActivityComponent.h"
#include "ShootingGalleryComponent.h"
#include "SimplePhysicsComponent.h"
#include "SkillComponent.h"
#include "SoundTriggerComponent.h"
#include "SwitchComponent.h"
#include "VendorComponent.h"

/**
 * Entity::WriteComponents writes the components in the client's order from a list of component types. Before, the
 * order was spelled out component by component, with flags for where the destroyable went. That code is kept below,
 * unchanged apart from looking components up by class, as the oracle: the bytes must not change.
 */
namespace {
	template<typename T>
	bool TryGet(const Entity& entity, T*& component) {
		component = entity.GetComponent<T>();
		return component != nullptr;
	}

	void OldWriteComponents(const Entity& entity, RakNet::BitStream& outBitStream, eReplicaPacketType packetType) {

		/**
		 * This has to be done in a specific order.
		 */

		bool destroyableSerialized = false;
		bool bIsInitialUpdate = packetType == eReplicaPacketType::CONSTRUCTION;

		PossessableComponent* possessableComponent;
		if (TryGet(entity, possessableComponent)) {
			possessableComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		ModuleAssemblyComponent* moduleAssemblyComponent;
		if (TryGet(entity, moduleAssemblyComponent)) {
			moduleAssemblyComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		ControllablePhysicsComponent* controllablePhysicsComponent;
		if (TryGet(entity, controllablePhysicsComponent)) {
			controllablePhysicsComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		SimplePhysicsComponent* simplePhysicsComponent;
		if (TryGet(entity, simplePhysicsComponent)) {
			simplePhysicsComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		RigidbodyPhantomPhysicsComponent* rigidbodyPhantomPhysics;
		if (TryGet(entity, rigidbodyPhantomPhysics)) {
			rigidbodyPhantomPhysics->Serialize(outBitStream, bIsInitialUpdate);
		}

		HavokVehiclePhysicsComponent* havokVehiclePhysicsComponent;
		if (TryGet(entity, havokVehiclePhysicsComponent)) {
			havokVehiclePhysicsComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		PhantomPhysicsComponent* phantomPhysicsComponent;
		if (TryGet(entity, phantomPhysicsComponent)) {
			phantomPhysicsComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		SoundTriggerComponent* soundTriggerComponent;
		if (TryGet(entity, soundTriggerComponent)) {
			soundTriggerComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		RacingSoundTriggerComponent* racingSoundTriggerComponent;
		if (TryGet(entity, racingSoundTriggerComponent)) {
			racingSoundTriggerComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		BuffComponent* buffComponent;
		if (TryGet(entity, buffComponent)) {
			buffComponent->Serialize(outBitStream, bIsInitialUpdate);

			DestroyableComponent* destroyableComponent;
			if (TryGet(entity, destroyableComponent)) {
				destroyableComponent->Serialize(outBitStream, bIsInitialUpdate);
			}
			destroyableSerialized = true;
		}

		CollectibleComponent* collectibleComponent;
		if (TryGet(entity, collectibleComponent)) {
			DestroyableComponent* destroyableComponent;
			if (TryGet(entity, destroyableComponent) && !destroyableSerialized) {
				destroyableComponent->Serialize(outBitStream, bIsInitialUpdate);
			}
			destroyableSerialized = true;
			collectibleComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		PetComponent* petComponent;
		if (TryGet(entity, petComponent)) {
			petComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		CharacterComponent* characterComponent;
		if (TryGet(entity, characterComponent)) {

			PossessorComponent* possessorComponent;
			if (TryGet(entity, possessorComponent)) {
				possessorComponent->Serialize(outBitStream, bIsInitialUpdate);
			} else {
				// Should never happen, but just to be safe
				outBitStream.Write0();
			}

			LevelProgressionComponent* levelProgressionComponent;
			if (TryGet(entity, levelProgressionComponent)) {
				levelProgressionComponent->Serialize(outBitStream, bIsInitialUpdate);
			} else {
				// Should never happen, but just to be safe
				outBitStream.Write0();
			}

			PlayerForcedMovementComponent* playerForcedMovementComponent;
			if (TryGet(entity, playerForcedMovementComponent)) {
				playerForcedMovementComponent->Serialize(outBitStream, bIsInitialUpdate);
			} else {
				// Should never happen, but just to be safe
				outBitStream.Write0();
			}

			characterComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		InventoryComponent* inventoryComponent;
		if (TryGet(entity, inventoryComponent)) {
			inventoryComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		ScriptComponent* scriptComponent;
		if (TryGet(entity, scriptComponent)) {
			scriptComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		SkillComponent* skillComponent;
		if (TryGet(entity, skillComponent)) {
			skillComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		BaseCombatAIComponent* baseCombatAiComponent;
		if (TryGet(entity, baseCombatAiComponent)) {
			baseCombatAiComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		// The item comes after the skill and combat AI components, as the client reads them (ComponentOrderVector::Initialize
		// 0x0101f8e0; live constructions of LOT 14535). It used to come before the inventory, which was harmless while
		// it wrote a single 0 bit.
		ItemComponent* itemComponent;
		if (TryGet(entity, itemComponent)) {
			itemComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		QuickBuildComponent* quickBuildComponent;
		if (TryGet(entity, quickBuildComponent)) {
			DestroyableComponent* destroyableComponent;
			if (TryGet(entity, destroyableComponent) && !destroyableSerialized) {
				destroyableComponent->Serialize(outBitStream, bIsInitialUpdate);
			}
			destroyableSerialized = true;
			quickBuildComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		MovingPlatformComponent* movingPlatformComponent;
		if (TryGet(entity, movingPlatformComponent)) {
			movingPlatformComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		SwitchComponent* switchComponent;
		if (TryGet(entity, switchComponent)) {
			switchComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		VendorComponent* vendorComponent;
		if (TryGet(entity, vendorComponent)) {
			vendorComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		DonationVendorComponent* donationVendorComponent;
		if (TryGet(entity, donationVendorComponent)) {
			donationVendorComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		AchievementVendorComponent* achievementVendorComponent;
		if (TryGet(entity, achievementVendorComponent)) {
			achievementVendorComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		BouncerComponent* bouncerComponent;
		if (TryGet(entity, bouncerComponent)) {
			bouncerComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		ScriptedActivityComponent* scriptedActivityComponent;
		if (TryGet(entity, scriptedActivityComponent)) {
			scriptedActivityComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		ShootingGalleryComponent* shootingGalleryComponent;
		if (TryGet(entity, shootingGalleryComponent)) {
			shootingGalleryComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		RacingControlComponent* racingControlComponent;
		if (TryGet(entity, racingControlComponent)) {
			racingControlComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		LUPExhibitComponent* lupExhibitComponent;
		if (TryGet(entity, lupExhibitComponent)) {
			lupExhibitComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		ModelComponent* modelComponent;
		if (TryGet(entity, modelComponent)) {
			modelComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		RenderComponent* renderComponent;
		if (TryGet(entity, renderComponent)) {
			renderComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		if (modelComponent || !destroyableSerialized) {
			DestroyableComponent* destroyableComponent;
			if (TryGet(entity, destroyableComponent) && !destroyableSerialized) {
				destroyableComponent->Serialize(outBitStream, bIsInitialUpdate);
				destroyableSerialized = true;
			}
		}

		MiniGameControlComponent* miniGameControlComponent;
		if (TryGet(entity, miniGameControlComponent)) {
			miniGameControlComponent->Serialize(outBitStream, bIsInitialUpdate);
		}

		// BBB Component, unused currently
		// Need to to write0 so that is serialized correctly
		// TODO: Implement BBB Component
		outBitStream.Write0();
	}


	// Some components read the CDClient in their constructors; give them an empty one
	void ConnectEmptyCDClient() {
		if (CDClientDatabase::isConnected) return;
		CDClientDatabase::Connect(":memory:");
		for (const auto* table : {
			"ComponentsRegistry (id INTEGER, component_type INTEGER, component_id INTEGER)",
			"BaseCombatAIComponent (id INTEGER, aggroRadius REAL, tetherSpeed REAL, pursuitSpeed REAL, softTetherRadius REAL, hardTetherRadius REAL, minRoundLength REAL, maxRoundLength REAL, combatRoundLength REAL)",
			"ItemSets (setID INTEGER, itemIDs TEXT)",
			"ObjectSkills (objectTemplate INTEGER, skillID INTEGER, castOnType INTEGER, AICombatWeight INTEGER)",
			"SkillBehavior (skillID INTEGER, behaviorID INTEGER)",
			"RenderComponent (id INTEGER)",
			"BuffParameters (BuffID INTEGER)",
			"PossessableComponent (id INTEGER, possessionType INTEGER, depossessOnHit INTEGER, skillSet INTEGER)",
		}) {
			CDClientDatabase::ExecuteDML(std::string("CREATE TABLE ") + table + ";");
		}
	}

	std::vector<uint8_t> Bytes(RakNet::BitStream& stream) {
		return { stream.GetData(), stream.GetData() + stream.GetNumberOfBytesUsed() };
	}
}

class ReplicaComponentOrderTest : public GameDependenciesTest {
protected:
	std::vector<std::unique_ptr<Character>> characters;

	void SetUp() override {
		SetUpDependencies();
		ConnectEmptyCDClient();
	}

	void TearDown() override {
		TearDownDependencies();
	}

	std::unique_ptr<Entity> MakePlayer() {
		auto entity = std::make_unique<Entity>(1, info);
		auto& character = characters.emplace_back(std::make_unique<Character>(1, nullptr));
		tinyxml2::XMLDocument doc;
		doc.LoadFile("./test_xml_data.xml");
		tinyxml2::XMLPrinter printer{ 0, true, 0 };
		doc.Print(&printer);
		character->_setXmlData(printer.CStr());
		character->_doQuickXMLDataParse();
		entity->SetCharacter(character.get());
		character->SetEntity(entity.get());
		entity->AddComponent<ControllablePhysicsComponent>(-1);
		entity->AddComponent<BuffComponent>(-1);
		entity->AddComponent<DestroyableComponent>(-1)->SetMaxHealth(4.0f);
		entity->AddComponent<PossessorComponent>(-1);
		entity->AddComponent<LevelProgressionComponent>(-1);
		entity->AddComponent<PlayerForcedMovementComponent>(-1);
		entity->AddComponent<CharacterComponent>(-1, character.get(), UNASSIGNED_SYSTEM_ADDRESS);
		entity->AddComponent<InventoryComponent>(-1);
		entity->AddComponent<SkillComponent>(-1);
		entity->AddComponent<RenderComponent>(-1);
		return entity;
	}

	// Serializes an entity built by `build` with the old and the new code (each on its own copy, as serializing
	// clears dirty flags) and expects the same bits, for the construction and a serialization.
	void ExpectSameBytes(const std::function<void(Entity&)>& build) {
		for (const auto packetType : { eReplicaPacketType::CONSTRUCTION, eReplicaPacketType::SERIALIZATION }) {
			Entity oldEntity(15, info);
			Entity newEntity(15, info);
			// Same random numbers for both (a vehicle picks a random end behavior)
			Game::randomEngine.seed(1);
			build(oldEntity);
			Game::randomEngine.seed(1);
			build(newEntity);
			RakNet::BitStream oldStream;
			RakNet::BitStream newStream;
			OldWriteComponents(oldEntity, oldStream, packetType);
			newEntity.WriteComponents(newStream, packetType);
			EXPECT_GT(newStream.GetNumberOfBitsUsed(), 1u);
			EXPECT_EQ(oldStream.GetNumberOfBitsUsed(), newStream.GetNumberOfBitsUsed()) << "packet type " << static_cast<int>(packetType);
			const auto oldBytes = Bytes(oldStream);
			const auto newBytes = Bytes(newStream);
			const auto mismatch = std::ranges::mismatch(oldBytes, newBytes);
			EXPECT_EQ(mismatch.in1, oldBytes.end()) << "packet type " << static_cast<int>(packetType) << ", first different byte " << (mismatch.in1 - oldBytes.begin());
		}
	}
};

TEST(ReplicaComponentTypeTest, MatchesComponentsRegistry) {
	// ComponentsRegistry.component_type values
	EXPECT_EQ(static_cast<uint32_t>(eReplicaComponentType::DESTROYABLE), 7u);
	EXPECT_EQ(static_cast<uint32_t>(eReplicaComponentType::BUFF), 98u);
	EXPECT_EQ(DestroyableComponent::ComponentType, eReplicaComponentType::DESTROYABLE);
	EXPECT_EQ(BuffComponent::ComponentType, eReplicaComponentType::BUFF);
}

TEST_F(ReplicaComponentOrderTest, Player) {
	for (const auto packetType : { eReplicaPacketType::CONSTRUCTION, eReplicaPacketType::SERIALIZATION }) {
		auto oldEntity = MakePlayer();
		auto newEntity = MakePlayer();
		RakNet::BitStream oldStream;
		RakNet::BitStream newStream;
		OldWriteComponents(*oldEntity, oldStream, packetType);
		newEntity->WriteComponents(newStream, packetType);
		EXPECT_EQ(oldStream.GetNumberOfBitsUsed(), newStream.GetNumberOfBitsUsed());
		EXPECT_EQ(Bytes(oldStream), Bytes(newStream));
		oldEntity->SetCharacter(nullptr);
		newEntity->SetCharacter(nullptr);
	}
}

TEST_F(ReplicaComponentOrderTest, Enemy) {
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<ControllablePhysicsComponent>(-1);
		entity.AddComponent<BuffComponent>(-1);
		auto* destroyable = entity.AddComponent<DestroyableComponent>(-1);
		destroyable->SetMaxHealth(8.0f);
		destroyable->SetHealth(8);
		destroyable->AddFactionNoLookup(4);
		entity.AddComponent<ScriptComponent>(-1, "", true);
		entity.AddComponent<SkillComponent>(-1);
		entity.AddComponent<BaseCombatAIComponent>(-1);
		entity.AddComponent<RenderComponent>(-1);
	});
}

TEST_F(ReplicaComponentOrderTest, SmashableWithDestructibleRow) {
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<SimplePhysicsComponent>(-1);
		entity.AddComponent<BuffComponent>(-1);
		entity.AddComponent<DestroyableComponent>(-1)->SetIsSmashable(true);
		entity.AddComponent<RenderComponent>(-1);
	});
}

TEST_F(ReplicaComponentOrderTest, IsSmashableWithoutRegistryEntry) {
	// Written after the render component
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<SimplePhysicsComponent>(-1);
		entity.AddComponent<ScriptComponent>(-1, "", true);
		auto* destroyable = entity.AddComponent<DestroyableComponent>(-1);
		destroyable->SetIsSmashable(true);
		destroyable->SetMaxHealth(1.0f);
		destroyable->SetHealth(1);
		entity.AddComponent<RenderComponent>(-1);
		entity.AddComponent<MiniGameControlComponent>(-1);
	});
}

TEST_F(ReplicaComponentOrderTest, QuickBuildWithoutDestroyable) {
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<SimplePhysicsComponent>(-1);
		entity.AddComponent<ScriptComponent>(-1, "", true);
		entity.AddComponent<QuickBuildComponent>(-1);
		entity.AddComponent<RenderComponent>(-1);
	});
}

TEST_F(ReplicaComponentOrderTest, QuickBuildWithoutRegistryEntry) {
	// Written right before the quick build, after the script
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<SimplePhysicsComponent>(-1);
		entity.AddComponent<DestroyableComponent>(-1)->SetIsSmashable(true);
		entity.AddComponent<ScriptComponent>(-1, "", true);
		entity.AddComponent<SkillComponent>(-1);
		entity.AddComponent<QuickBuildComponent>(-1);
		entity.AddComponent<MovingPlatformComponent>(-1, "");
		entity.AddComponent<RenderComponent>(-1);
	});
}

TEST_F(ReplicaComponentOrderTest, QuickBuildWithRegistryEntry) {
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<SimplePhysicsComponent>(-1);
		entity.AddComponent<BuffComponent>(-1);
		entity.AddComponent<DestroyableComponent>(-1)->SetMaxHealth(3.0f);
		entity.AddComponent<ScriptComponent>(-1, "", true);
		entity.AddComponent<QuickBuildComponent>(-1);
		entity.AddComponent<RenderComponent>(-1);
	});
}

TEST_F(ReplicaComponentOrderTest, CollectibleWithoutRegistryEntry) {
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<SimplePhysicsComponent>(-1);
		entity.AddComponent<DestroyableComponent>(-1);
		entity.AddComponent<CollectibleComponent>(-1, 12);
		entity.AddComponent<ScriptComponent>(-1, "", true);
		entity.AddComponent<RenderComponent>(-1);
	});
}

TEST_F(ReplicaComponentOrderTest, CollectibleWithRegistryEntry) {
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<SimplePhysicsComponent>(-1);
		entity.AddComponent<BuffComponent>(-1);
		entity.AddComponent<DestroyableComponent>(-1)->SetIsSmashable(true);
		entity.AddComponent<CollectibleComponent>(-1, 3);
		entity.AddComponent<RenderComponent>(-1);
	});
}

TEST_F(ReplicaComponentOrderTest, Npc) {
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<ControllablePhysicsComponent>(-1);
		entity.AddComponent<BuffComponent>(-1);
		entity.AddComponent<DestroyableComponent>(-1);
		entity.AddComponent<InventoryComponent>(-1);
		entity.AddComponent<ScriptComponent>(-1, "", true);
		entity.AddComponent<SkillComponent>(-1);
		entity.AddComponent<SwitchComponent>(-1);
		entity.AddComponent<BouncerComponent>(-1);
		entity.AddComponent<RenderComponent>(-1);
	});
}

TEST_F(ReplicaComponentOrderTest, Pet) {
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<ControllablePhysicsComponent>(-1);
		entity.AddComponent<BuffComponent>(-1);
		entity.AddComponent<DestroyableComponent>(-1);
		entity.AddComponent<PetComponent>(1);
		entity.AddComponent<SkillComponent>(-1);
		entity.AddComponent<RenderComponent>(-1);
	});
}

TEST_F(ReplicaComponentOrderTest, Vehicle) {
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<PossessableComponent>(-1);
		entity.AddComponent<ModuleAssemblyComponent>(-1);
		entity.AddComponent<HavokVehiclePhysicsComponent>(-1);
		entity.AddComponent<BuffComponent>(-1);
		entity.AddComponent<DestroyableComponent>(-1);
		entity.AddComponent<RenderComponent>(-1);
	});
}

TEST_F(ReplicaComponentOrderTest, Model) {
	// A model's destroyable is written after the render component
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<SimplePhysicsComponent>(-1);
		entity.AddComponent<ModelComponent>(-1);
		entity.AddComponent<RenderComponent>(-1);
		entity.AddComponent<DestroyableComponent>(-1)->SetIsSmashable(true);
	});
}

TEST_F(ReplicaComponentOrderTest, EveryListedComponent) {
	ExpectSameBytes([](Entity& entity) {
		entity.AddComponent<PossessableComponent>(-1);
		entity.AddComponent<ModuleAssemblyComponent>(-1);
		entity.AddComponent<ControllablePhysicsComponent>(-1);
		entity.AddComponent<SimplePhysicsComponent>(-1);
		entity.AddComponent<RigidbodyPhantomPhysicsComponent>(-1);
		entity.AddComponent<HavokVehiclePhysicsComponent>(-1);
		entity.AddComponent<PhantomPhysicsComponent>(-1);
		entity.AddComponent<SoundTriggerComponent>(-1);
		entity.AddComponent<BuffComponent>(-1);
		entity.AddComponent<DestroyableComponent>(-1);
		entity.AddComponent<CollectibleComponent>(-1, 1);
		entity.AddComponent<ItemComponent>(-1);
		entity.AddComponent<InventoryComponent>(-1);
		entity.AddComponent<ScriptComponent>(-1, "", true);
		entity.AddComponent<SkillComponent>(-1);
		entity.AddComponent<QuickBuildComponent>(-1);
		entity.AddComponent<SwitchComponent>(-1);
		entity.AddComponent<BouncerComponent>(-1);
		entity.AddComponent<ScriptedActivityComponent>(-1);
		entity.AddComponent<LUPExhibitComponent>(-1);
		entity.AddComponent<ModelComponent>(-1);
		entity.AddComponent<RenderComponent>(-1);
		entity.AddComponent<MiniGameControlComponent>(-1);
	});
}
