#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "PetComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "ePetAbilityType.h"
#include "eStateChangeType.h"

// Table includes
#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"

class PetTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	PetComponent* petComponent;
	CBITSTREAM
		// uint32_t flags = 0;
	void SetUp() override {
		SetUpDependencies();

		// Set up entity and pet component
		baseEntity = new Entity(3050, GameDependenciesTest::info);
		//CDComponentsRegistryTable* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
		//uint32_t petComponentId = compRegistryTable->GetByIDAndType(baseEntity->GetLOT(), eReplicaComponentType::PET);
		petComponent = baseEntity->AddComponent<PetComponent>(1);

		// Initialize some values to be not default
		petComponent->SetFlag(PetFlag::NONE);
		petComponent->SetPetAiState(PetAiState::follow);
		petComponent->SetAbility(ePetAbilityType::Invalid);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

TEST_F(PetTest, PlacementNewAddComponentTest) {
	ASSERT_NE(petComponent, nullptr);
	baseEntity->AddComponent<PetComponent>();
	ASSERT_NE(baseEntity->GetComponent<PetComponent>(), nullptr);
	ASSERT_EQ(petComponent->GetParent()->GetObjectID(), 15);
	ASSERT_EQ(petComponent->HasFlag(), PetFlag::NONE); // UPDATE TO USE FLAGS
	ASSERT_EQ(petComponent->GetPetAiState(), PetAiState::follow);
	ASSERT_EQ(petComponent->GetAbility(), ePetAbilityType::Invalid);
}

/**
 * Test Construction of a PetComponent
 */
/*TEST_F(PetTest, PetComponent) {
	petComponent->Serialize(&bitStream, true);
	// Assert that the full number of bits are present
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 748);
	{
		// Now read in the full serialized construction BitStream
		bool optionStatusImmunityInfo{};
		uint32_t ImmuneToBasicAttackCount{};
		uint32_t ImmuneToDamageOverTimeCount{};
		uint32_t ImmuneToKnockbackCount{};
		uint32_t ImmuneToInterruptCount{};
		uint32_t ImmuneToSpeedCount{};
		uint32_t ImmuneToImaginationGainCount{};
		uint32_t ImmuneToImaginationLossCount{};
		uint32_t ImmuneToQuickbuildInterruptCount{};
		uint32_t ImmuneToPullToPointCount{};
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
		bitStream.Read(ImmuneToBasicAttackCount);
		bitStream.Read(ImmuneToDamageOverTimeCount);
		bitStream.Read(ImmuneToKnockbackCount);
		bitStream.Read(ImmuneToInterruptCount);
		bitStream.Read(ImmuneToSpeedCount);
		bitStream.Read(ImmuneToImaginationGainCount);
		bitStream.Read(ImmuneToImaginationLossCount);
		bitStream.Read(ImmuneToQuickbuildInterruptCount);
		bitStream.Read(ImmuneToPullToPointCount);
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
		for (uint32_t i = 0; i < factionsSize; i++) {
			int32_t factionID{};
			bitStream.Read(factionID);
			factions.push_back(factionID);
		}
		bitStream.Read(isSmashable); // This is an option later and also a flag at this spot
		bitStream.Read(isDead);
		bitStream.Read(isSmashed);
		// if IsSmashable is true, read the next bits.
		bitStream.Read(isModuleAssembly);
		bitStream.Read(optionExplodeFactor);
		bitStream.Read(explodeFactor);

		bitStream.Read(optionIsOnThreatList);
		bitStream.Read(isThreatened);
		EXPECT_EQ(optionStatusImmunityInfo, true);
		EXPECT_EQ(ImmuneToBasicAttackCount, 0);
		EXPECT_EQ(ImmuneToDamageOverTimeCount, 0);
		EXPECT_EQ(ImmuneToKnockbackCount, 0);
		EXPECT_EQ(ImmuneToInterruptCount, 0);
		EXPECT_EQ(ImmuneToSpeedCount, 0);
		EXPECT_EQ(ImmuneToImaginationGainCount, 0);
		EXPECT_EQ(ImmuneToImaginationLossCount, 0);
		EXPECT_EQ(ImmuneToQuickbuildInterruptCount, 0);
		EXPECT_EQ(ImmuneToPullToPointCount, 0);

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
		EXPECT_EQ(factionsSize, 2);
		EXPECT_NE(std::find(factions.begin(), factions.end(), -1), factions.end());
		EXPECT_NE(std::find(factions.begin(), factions.end(), 6), factions.end());
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
}*/

/**
 * Test serialization of a PetComponent
 */
/*TEST_F(PetTest, PEtComponentSerializeTest) {
	bitStream.Reset();
	// Initialize some values to be not default so we can test a full serialization
	destroyableComponent->SetMaxHealth(1233.0f);

	// Now we test a serialization for correctness.
	destroyableComponent->Serialize(&bitStream, false);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 422);
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
		for (uint32_t i = 0; i < factionsSize; i++) {
			int32_t factionID{};
			bitStream.Read(factionID);
			factions.push_back(factionID);
		}
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
		EXPECT_EQ(factionsSize, 2);
		EXPECT_NE(std::find(factions.begin(), factions.end(), -1), factions.end());
		EXPECT_NE(std::find(factions.begin(), factions.end(), 6), factions.end());
		EXPECT_EQ(isSmashable, true);

		EXPECT_EQ(optionIsOnThreatList, false); // Always zero for now on serialization
	}
}*/

/**
 * Test the Damage method of DestroyableComponent
 */
/*TEST_F(DestroyableTest, DestroyableComponentDamageTest) {
	// Do some actions
	destroyableComponent->SetMaxHealth(100.0f);
	destroyableComponent->SetHealth(100);
	destroyableComponent->SetMaxArmor(0.0f);
	destroyableComponent->Damage(10, LWOOBJID_EMPTY);
	// Check that we take damage
	ASSERT_EQ(destroyableComponent->GetHealth(), 90);
	// Check that if we have armor, we take the correct amount of damage
	destroyableComponent->SetMaxArmor(10.0f);
	destroyableComponent->SetArmor(5);
	destroyableComponent->Damage(10, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 85);
	// Check that if we have damage absorption we take the correct damage
	destroyableComponent->SetDamageToAbsorb(10);
	destroyableComponent->Damage(9, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 85);
	ASSERT_EQ(destroyableComponent->GetDamageToAbsorb(), 1);
	destroyableComponent->Damage(6, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 80);
	// Check that we take the correct reduced damage if we take reduced damage
	destroyableComponent->SetDamageReduction(2);
	destroyableComponent->Damage(7, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 75);
	destroyableComponent->Damage(2, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 74);
	ASSERT_EQ(destroyableComponent->GetDamageReduction(), 2);
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
	ASSERT_TRUE(destroyableComponent->IsImmune());
	destroyableComponent->SetIsImmune(false);
	destroyableComponent->SetIsGMImmune(true);
	destroyableComponent->Damage(UINT32_MAX, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 70);
	ASSERT_TRUE(destroyableComponent->IsImmune());
	destroyableComponent->SetIsGMImmune(false);
	// Check knockback immunity
	destroyableComponent->SetIsShielded(true);
	ASSERT_TRUE(destroyableComponent->IsKnockbackImmune());
	// Finally deal enough damage to kill the Entity
	destroyableComponent->Damage(71, LWOOBJID_EMPTY);
	ASSERT_EQ(destroyableComponent->GetHealth(), 0);
	// Now lets heal some stats
	destroyableComponent->Heal(15);
	ASSERT_EQ(destroyableComponent->GetHealth(), 15);
	destroyableComponent->Heal(15000);
	ASSERT_EQ(destroyableComponent->GetHealth(), 100);
	destroyableComponent->Repair(10);
	ASSERT_EQ(destroyableComponent->GetArmor(), 10);
	destroyableComponent->Repair(15000);
	ASSERT_EQ(destroyableComponent->GetArmor(), 10);
	destroyableComponent->SetMaxImagination(100.0f);
	destroyableComponent->SetImagination(0);
	destroyableComponent->Imagine(99);
	ASSERT_EQ(destroyableComponent->GetImagination(), 99);
	destroyableComponent->Imagine(4);
	ASSERT_EQ(destroyableComponent->GetImagination(), 100);
}

TEST_F(DestroyableTest, DestroyableComponentFactionTest) {
	ASSERT_TRUE(destroyableComponent->HasFaction(-1));
	ASSERT_TRUE(destroyableComponent->HasFaction(6));
}

TEST_F(DestroyableTest, DestroyableComponentValiditiyTest) {
	auto* enemyEntity = new Entity(19, info);
	enemyEntity->AddComponent<DestroyableComponent>()->AddFactionNoLookup(16);
	destroyableComponent->AddEnemyFaction(16);
	EXPECT_TRUE(destroyableComponent->IsEnemy(enemyEntity));
	EXPECT_FALSE(destroyableComponent->IsFriend(enemyEntity));
	delete enemyEntity;
}

TEST_F(DestroyableTest, DestroyableComponentImmunityTest) {
	// assert to show that they are empty
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	// set them all to true (count 1) and check
	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, true, true, true, true, true, true, true, true, true);
	ASSERT_TRUE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_TRUE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_TRUE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_TRUE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_TRUE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_TRUE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_TRUE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_TRUE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_TRUE(destroyableComponent->GetImmuneToPullToPoint());

	// remove them to check that they get removed properly
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, true, true, true, true, true, true, true, true, true);
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());

	// should not crash to remove them again
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, true, true, true, true, true, true, true, true, true);
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());

	// just do one
	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, true);
	ASSERT_TRUE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	// now stack it to 2 on basic attack
	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, true);
	ASSERT_TRUE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	// remove one and still shoudl be true
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, true);
	ASSERT_TRUE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	// go back to 0
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, true);
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	// check individual ones now
	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, true, false, false, false, false, false, false, false, false);
	ASSERT_TRUE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, true, false, false, false, false, false, false, false, false);

	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, false, true, false, false, false, false, false, false, false);
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_TRUE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, false, true, false, false, false, false, false, false, false);

	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, false, false, true, false, false, false, false, false, false);
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_TRUE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, false, false, true, false, false, false, false, false, false);

	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, false, false, false, true, false, false, false, false, false);
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_TRUE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, false, false, false, true, false, false, false, false, false);

	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, false, false, false, false, true, false, false, false, false);
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_TRUE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, false, false, false, false, true, false, false, false, false);

	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, false, false, false, false, false, true, false, false, false);
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_TRUE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, false, false, false, false, false, true, false, false, false);

	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, false, false, false, false, false, false, true, false, false);
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_TRUE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, false, false, false, false, false, false, true, false, false);


	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, false, false, false, false, false, false, false, true, false);
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_TRUE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToPullToPoint());
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, false, false, false, false, false, false, false, true, false);


	destroyableComponent->SetStatusImmunity(eStateChangeType::PUSH, false, false, false, false, false, false, false, false, true);
	ASSERT_FALSE(destroyableComponent->GetImmuneToBasicAttack());
	ASSERT_FALSE(destroyableComponent->GetImmuneToDamageOverTime());
	ASSERT_FALSE(destroyableComponent->GetImmuneToKnockback());
	ASSERT_FALSE(destroyableComponent->GetImmuneToInterrupt());
	ASSERT_FALSE(destroyableComponent->GetImmuneToSpeed());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationGain());
	ASSERT_FALSE(destroyableComponent->GetImmuneToImaginationLoss());
	ASSERT_FALSE(destroyableComponent->GetImmuneToQuickbuildInterrupt());
	ASSERT_TRUE(destroyableComponent->GetImmuneToPullToPoint());
	destroyableComponent->SetStatusImmunity(eStateChangeType::POP, false, false, false, false, false, false, false, false, true);

}*/

/**
 * Test the Damage cooldown timer of DestroyableComponent
 */
/*TEST_F(DestroyableTest, DestroyableComponentDamageCooldownTest) {
	// Test the damage immune timer state (anything above 0.0f)
	destroyableComponent->SetDamageCooldownTimer(1.0f);
	EXPECT_FLOAT_EQ(destroyableComponent->GetDamageCooldownTimer(), 1.0f);
	ASSERT_TRUE(destroyableComponent->IsCooldownImmune());

	// Test that the Update() function correctly decrements the damage cooldown timer
	destroyableComponent->Update(0.5f);
	EXPECT_FLOAT_EQ(destroyableComponent->GetDamageCooldownTimer(), 0.5f);
	ASSERT_TRUE(destroyableComponent->IsCooldownImmune());

	// Test the non damage immune timer state (anything below or equal to 0.0f)
	destroyableComponent->SetDamageCooldownTimer(0.0f);
	EXPECT_FLOAT_EQ(destroyableComponent->GetDamageCooldownTimer(), 0.0f);
	ASSERT_FALSE(destroyableComponent->IsCooldownImmune());
}*/

TEST_F(PetTest, PetComponentFlagTest) {
	// Test setting and reading single flags
	petComponent->SetFlag(PetFlag::NOT_WAITING);
	ASSERT_TRUE(petComponent->HasFlag(PetFlag::NOT_WAITING));
	ASSERT_FALSE(petComponent->HasFlag(PetFlag::NOT_WAITING));

	// Test
}
