#include "../GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "DestroyableComponent.h"
#include "Entity.h"

class DestroyableTest : public GameDependenciesTest {
	protected:
		Entity* baseEntity;
		DestroyableComponent* destroyableComponent;
		CBITSTREAM
		uint32_t flags = 0;
		void SetUp() override {
			SetUpDependencies();
			baseEntity = new Entity(15, GameDependenciesTest::info);
			destroyableComponent = new DestroyableComponent(baseEntity);
			baseEntity->AddComponent(COMPONENT_TYPE_DESTROYABLE, destroyableComponent);
			// Initialize some values to be not default
			destroyableComponent->SetMaxHealth(12345.0f);
			destroyableComponent->SetHealth(23);
			destroyableComponent->SetMaxArmor(14.0f);
			destroyableComponent->SetArmor(7);
			destroyableComponent->SetMaxImagination(14000.0f);
			destroyableComponent->SetImagination(6000);
			destroyableComponent->SetIsSmashable(true);
			destroyableComponent->SetExplodeFactor(1.1f);
		}

		void TearDown() override {
			Game::logger->Flush();
			delete baseEntity;
		}
};

TEST_F(DestroyableTest, DestroyableComponentSerializeConstructionTest) {
	destroyableComponent->Serialize(&bitStream, true, flags);
	// Assert that the full number of bits are present
	EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 396);
	{
		// Now read in the full serialized construction BitStream
		bool optionStatusImmunityInfo{}; // Values under this option are unused.
		bool optionStatsInfo{};
		uint32_t currentHealth{};
		float maxHealth{};
		uint32_t currentArmor{};
		float maxArmor{};
		uint32_t currentImagination{};
		float maxImagination{};
		uint32_t damageAbsorptionPoints{};
		bool hasImmunity{};
		bool isGmImmune{};
		bool isShielded{};
		float actualMaxHealth{};
		float actualMaxArmor{};
		float actualMaxImagination{};
		uint32_t factionsSize{};
		std::vector<int32_t> factions{};
		bool isSmashable{};
		bool isDead{};
		bool isSmashed{};
		bool isModuleAssembly{};
		bool optionExplodeFactor{};
		float explodeFactor{};
		bool optionIsOnThreatList{};
		bool isThreatened{};
		bitStream.Read(optionStatusImmunityInfo);

		bitStream.Read(optionStatsInfo);
			bitStream.Read(currentHealth);
			bitStream.Read(maxHealth);
			bitStream.Read(currentArmor);
			bitStream.Read(maxArmor);
			bitStream.Read(currentImagination);
			bitStream.Read(maxImagination);
			bitStream.Read(damageAbsorptionPoints);
			bitStream.Read(hasImmunity);
			bitStream.Read(isGmImmune);
			bitStream.Read(isShielded);
			bitStream.Read(actualMaxHealth);
			bitStream.Read(actualMaxArmor);
			bitStream.Read(actualMaxImagination);
			bitStream.Read(factionsSize);
			bitStream.Read(isSmashable); // This is an option later and also a flag at this spot
			bitStream.Read(isDead);
			bitStream.Read(isSmashed);
			// if IsSmashable is true, read the next bits.
				bitStream.Read(isModuleAssembly);
				bitStream.Read(optionExplodeFactor);
					bitStream.Read(explodeFactor);

		bitStream.Read(optionIsOnThreatList);
			bitStream.Read(isThreatened);
		EXPECT_EQ(optionStatusImmunityInfo, false);

		EXPECT_EQ(optionStatsInfo, true);
			EXPECT_EQ(currentHealth, 23);
			EXPECT_EQ(maxHealth, 12345.0f);
			EXPECT_EQ(currentArmor, 7);
			EXPECT_EQ(maxArmor, 14.0f);
			EXPECT_EQ(currentImagination, 6000);
			EXPECT_EQ(maxImagination, 14000.0f);
			EXPECT_EQ(damageAbsorptionPoints, 0.0f);
			EXPECT_EQ(hasImmunity, false);
			EXPECT_EQ(isGmImmune, false);
			EXPECT_EQ(isShielded, false);
			EXPECT_EQ(actualMaxHealth, 12345.0f);
			EXPECT_EQ(actualMaxArmor, 14.0f);
			EXPECT_EQ(actualMaxImagination, 14000.0f);
			EXPECT_EQ(factionsSize, 0);
			EXPECT_EQ(isSmashable, true);
			EXPECT_EQ(isDead, false);
			EXPECT_EQ(isSmashed, false);
			EXPECT_EQ(isSmashable, true); // For the sake of readability with the struct viewers, we will test this twice since its used as an option here, but as a bool above.
				EXPECT_EQ(isModuleAssembly, false);
				EXPECT_EQ(optionExplodeFactor, true);
					EXPECT_EQ(explodeFactor, 1.1f);

		EXPECT_EQ(optionIsOnThreatList, true);
			EXPECT_EQ(isThreatened, false);
	}
	bitStream.Reset();
}

TEST_F(DestroyableTest, DestroyableComponentSerializeTest) {
	bitStream.Reset();
	// Initialize some values to be not default so we can test a full serialization
	destroyableComponent->SetMaxHealth(1233.0f);

	// Now we test a serialization for correctness.
	destroyableComponent->Serialize(&bitStream, false, flags);
	{
		// Now read in the full serialized BitStream
		bool optionStatsInfo{};
		uint32_t currentHealth{};
		float maxHealth{};
		uint32_t currentArmor{};
		float maxArmor{};
		uint32_t currentImagination{};
		float maxImagination{};
		uint32_t damageAbsorptionPoints{};
		bool hasImmunity{};
		bool isGmImmune{};
		bool isShielded{};
		float actualMaxHealth{};
		float actualMaxArmor{};
		float actualMaxImagination{};
		uint32_t factionsSize{};
		std::vector<int32_t> factions{};
		bool isSmashable{};
		bool optionIsOnThreatList{};
		bitStream.Read(optionStatsInfo);
			bitStream.Read(currentHealth);
			bitStream.Read(maxHealth);
			bitStream.Read(currentArmor);
			bitStream.Read(maxArmor);
			bitStream.Read(currentImagination);
			bitStream.Read(maxImagination);
			bitStream.Read(damageAbsorptionPoints);
			bitStream.Read(hasImmunity);
			bitStream.Read(isGmImmune);
			bitStream.Read(isShielded);
			bitStream.Read(actualMaxHealth);
			bitStream.Read(actualMaxArmor);
			bitStream.Read(actualMaxImagination);
			bitStream.Read(factionsSize);
			bitStream.Read(isSmashable);

		bitStream.Read(optionIsOnThreatList);

		EXPECT_EQ(optionStatsInfo, true);
			EXPECT_EQ(currentHealth, 23);
			EXPECT_EQ(maxHealth, 1233.0f);
			EXPECT_EQ(currentArmor, 7);
			EXPECT_EQ(maxArmor, 14.0f);
			EXPECT_EQ(currentImagination, 6000);
			EXPECT_EQ(maxImagination, 14000.0f);
			EXPECT_EQ(damageAbsorptionPoints, 0.0f);
			EXPECT_EQ(hasImmunity, false);
			EXPECT_EQ(isGmImmune, false);
			EXPECT_EQ(isShielded, false);
			EXPECT_EQ(actualMaxHealth, 1233.0f);
			EXPECT_EQ(actualMaxArmor, 14.0f);
			EXPECT_EQ(actualMaxImagination, 14000.0f);
			EXPECT_EQ(factionsSize, 0);
			EXPECT_EQ(isSmashable, true);

		EXPECT_EQ(optionIsOnThreatList, false); // Always zero for now on serialization
	}
}

TEST_F(DestroyableTest, DestroyableComponentDamageTest) {
	// Do some actions
	destroyableComponent->SetMaxHealth(100.0f);
	destroyableComponent->SetHealth(100);
	destroyableComponent->SetMaxArmor(0.0f);
	destroyableComponent->Damage(10, LWOOBJID_EMPTY);
	// Check that we take damage
	ASSERT_EQ(destroyableComponent->GetHealth(), 90);
	// Check that if we have armor, we take the correct amount of damage
	destroyableComponent->SetMaxArmor(10.0f);
	destroyableComponent->SetArmor(5.0f);
	destroyableComponent->Damage(10, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 85);
	// Check that if we have damage absorption we take the correct damage
	destroyableComponent->SetDamageToAbsorb(10);
	destroyableComponent->Damage(9, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 85);
	destroyableComponent->Damage(6, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 80);
	// Check that we take the correct reduced damage if we take reduced damage
	destroyableComponent->SetDamageReduction(2);
	destroyableComponent->Damage(7, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 75);
	destroyableComponent->Damage(2, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 74);
	destroyableComponent->SetDamageReduction(0);
	// Check that blocking works
	destroyableComponent->SetAttacksToBlock(1);
	destroyableComponent->Damage(UINT32_MAX, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 74);
	destroyableComponent->Damage(4, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 70);
	// Check that immunity works
	destroyableComponent->SetIsImmune(true);
	destroyableComponent->Damage(UINT32_MAX, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 70);
	destroyableComponent->SetIsImmune(false);
	// Finally deal enough damage to kill the Entity
	destroyableComponent->Damage(71, LWOOBJID_EMPTY);
	EXPECT_EQ(destroyableComponent->GetHealth(), 0);
}
