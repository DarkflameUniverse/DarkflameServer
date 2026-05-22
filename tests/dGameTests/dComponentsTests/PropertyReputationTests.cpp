#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "Character.h"
#include "CharacterComponent.h"
#include "Database.h"
#include "Entity.h"
#include "GameDatabase/TestSQL/TestSQLDatabase.h"

// ---------------------------------------------------------------------------
// CharacterComponent reputation tests
// ---------------------------------------------------------------------------
class CharacterReputationTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;
	std::unique_ptr<Character> character;
	CharacterComponent* characterComponent = nullptr;

	void SetUp() override {
		SetUpDependencies();
		entity = std::make_unique<Entity>(1, GameDependenciesTest::info);
		character = std::make_unique<Character>(1, nullptr);
		entity->SetCharacter(character.get());
		character->SetEntity(entity.get());
		characterComponent = entity->AddComponent<CharacterComponent>(-1, character.get(), UNASSIGNED_SYSTEM_ADDRESS);
	}

	void TearDown() override {
		entity->SetCharacter(nullptr);
		entity.reset();
		character.reset();
		TearDownDependencies();
	}
};

// SetReputation must persist using the character's raw DB id (Character::GetID),
// not the runtime object id (Character::GetObjectID with the CHARACTER bit set).
TEST_F(CharacterReputationTest, SetReputationUsesCharacterDBId) {
	constexpr int64_t repValue = 12345;
	characterComponent->SetReputation(repValue);

	// Reputation must be stored at the DB char id (GetID() == 1).
	EXPECT_EQ(Database::Get()->GetCharacterReputation(character->GetID()), repValue);
}

// SetReputation / GetReputation round-trip via in-memory TestSQLDatabase.
TEST_F(CharacterReputationTest, ReputationRoundTrip) {
	characterComponent->SetReputation(500);

	EXPECT_EQ(characterComponent->GetReputation(), 500);
	EXPECT_EQ(Database::Get()->GetCharacterReputation(character->GetID()), 500);
}

// The CharacterComponent constructor must load reputation from the DB using
// Character::GetID() as the lookup key.
TEST_F(CharacterReputationTest, LoadReputationFromDBOnConstruction) {
	Database::Get()->SetCharacterReputation(character->GetID(), 9876);

	// Re-construct the component; the constructor reads from DB.
	auto* freshComp = entity->AddComponent<CharacterComponent>(-1, character.get(), UNASSIGNED_SYSTEM_ADDRESS);
	EXPECT_EQ(freshComp->GetReputation(), 9876);
}

// In the test context, Character::GetObjectID() returns LWOOBJID_EMPTY (0) because
// UpdateInfoFromDatabase() is never called for test characters.  This mirrors the
// production scenario where GetObjectID() carries the CHARACTER bit and therefore
// differs from GetID().  Verify the two keys are treated independently.
TEST_F(CharacterReputationTest, DBIdAndObjectIdAreDistinctKeys) {
	// Precondition: the two IDs must differ so the test is meaningful.
	ASSERT_NE(character->GetID(), character->GetObjectID());

	characterComponent->SetReputation(42);

	// Reputation stored at GetID().
	EXPECT_EQ(Database::Get()->GetCharacterReputation(character->GetID()), 42);
	// No reputation stored at GetObjectID() (wrong key).
	EXPECT_EQ(Database::Get()->GetCharacterReputation(character->GetObjectID()), 0);
}

// ---------------------------------------------------------------------------
// TestSQLDatabase property reputation + contribution storage tests
// ---------------------------------------------------------------------------
class PropertyReputationDBTest : public GameDependenciesTest {
protected:
	TestSQLDatabase* testDB = nullptr;

	void SetUp() override {
		SetUpDependencies();
		testDB = dynamic_cast<TestSQLDatabase*>(Database::Get());
		ASSERT_NE(testDB, nullptr);
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

// UpdatePropertyReputation / GetPropertyReputation round-trip.
TEST_F(PropertyReputationDBTest, PropertyReputationRoundTrip) {
	constexpr LWOOBJID propertyId = 42;
	Database::Get()->UpdatePropertyReputation(propertyId, 100);
	EXPECT_EQ(testDB->GetPropertyReputation(propertyId), 100u);
}

// Overwriting property reputation replaces the previous value.
TEST_F(PropertyReputationDBTest, PropertyReputationOverwrite) {
	constexpr LWOOBJID propertyId = 42;
	Database::Get()->UpdatePropertyReputation(propertyId, 100);
	Database::Get()->UpdatePropertyReputation(propertyId, 250);
	EXPECT_EQ(testDB->GetPropertyReputation(propertyId), 250u);
}

// UpdatePropertyReputationContribution stores using the given player ID (expected
// to be the character DB id, not the runtime object id with bits set).
TEST_F(PropertyReputationDBTest, ContributionStoredByCharDBId) {
	constexpr LWOOBJID propertyId = 5;
	constexpr LWOOBJID charDbId = 1; // raw DB id, no object bits
	const std::string date = "2024-01-01";

	Database::Get()->UpdatePropertyReputationContribution(propertyId, charDbId, date, 10);

	const auto contributions = Database::Get()->GetPropertyReputationContributions(propertyId, date);
	ASSERT_EQ(contributions.size(), 1u);
	EXPECT_EQ(contributions[0].playerId, charDbId);
	EXPECT_EQ(contributions[0].reputationGained, 10u);
}

// A second UpdatePropertyReputationContribution for the same player must upsert
// (update the existing entry) rather than append a duplicate.
TEST_F(PropertyReputationDBTest, ContributionUpsertUpdatesExistingEntry) {
	constexpr LWOOBJID propertyId = 5;
	constexpr LWOOBJID charDbId = 1;
	const std::string date = "2024-01-01";

	Database::Get()->UpdatePropertyReputationContribution(propertyId, charDbId, date, 10);
	Database::Get()->UpdatePropertyReputationContribution(propertyId, charDbId, date, 30);

	const auto contributions = Database::Get()->GetPropertyReputationContributions(propertyId, date);
	ASSERT_EQ(contributions.size(), 1u);
	EXPECT_EQ(contributions[0].reputationGained, 30u);
}

// Contributions from different dates must not bleed into each other.
TEST_F(PropertyReputationDBTest, ContributionsAreIsolatedByDate) {
	constexpr LWOOBJID propertyId = 5;
	constexpr LWOOBJID charDbId = 1;

	Database::Get()->UpdatePropertyReputationContribution(propertyId, charDbId, "2024-01-01", 10);
	Database::Get()->UpdatePropertyReputationContribution(propertyId, charDbId, "2024-01-02", 20);

	const auto day1 = Database::Get()->GetPropertyReputationContributions(propertyId, "2024-01-01");
	const auto day2 = Database::Get()->GetPropertyReputationContributions(propertyId, "2024-01-02");

	ASSERT_EQ(day1.size(), 1u);
	EXPECT_EQ(day1[0].reputationGained, 10u);

	ASSERT_EQ(day2.size(), 1u);
	EXPECT_EQ(day2[0].reputationGained, 20u);
}

// Multiple distinct players can each have their own contribution entry per property/date.
TEST_F(PropertyReputationDBTest, MultiplePlayerContributionsForSameProperty) {
	constexpr LWOOBJID propertyId = 5;
	const std::string date = "2024-01-01";

	Database::Get()->UpdatePropertyReputationContribution(propertyId, 1, date, 10);
	Database::Get()->UpdatePropertyReputationContribution(propertyId, 2, date, 20);

	const auto contributions = Database::Get()->GetPropertyReputationContributions(propertyId, date);
	ASSERT_EQ(contributions.size(), 2u);

	const auto FindPlayerRep = [&](const LWOOBJID playerId) {
		for (const auto& c : contributions) {
			if (c.playerId == playerId) return c.reputationGained;
		}
		return 0u;
	};
	EXPECT_EQ(FindPlayerRep(1), 10u);
	EXPECT_EQ(FindPlayerRep(2), 20u);
}

// Querying contributions for a date with no entries returns an empty vector.
TEST_F(PropertyReputationDBTest, NoContributionsReturnsEmpty) {
	const auto contributions = Database::Get()->GetPropertyReputationContributions(99, "2024-01-01");
	EXPECT_TRUE(contributions.empty());
}
