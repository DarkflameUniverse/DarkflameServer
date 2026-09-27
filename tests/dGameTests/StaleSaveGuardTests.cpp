#include "GameDependencies.h"

#include "Character.h"
#include "Database.h"

namespace {
	// One character's xml and save generation, like the charxml row
	class SaveDatabase : public TestSQLDatabase {
	public:
		std::string xml = "<obj v=\"1\"><char cc=\"10\"/></obj>";
		uint64_t generation = 7;
		uint32_t saves = 0;
		std::vector<std::string> audits;

		std::optional<CharacterXml> ClaimCharacterXml(const LWOOBJID) override { return CharacterXml{ xml, ++generation }; }
		bool SaveCharacterXml(const LWOOBJID, const std::string_view lxfml, const uint64_t expected) override {
			if (expected != generation) return false;
			xml = lxfml;
			generation = expected + 1;
			saves++;
			return true;
		}
		uint64_t GetCharacterSaveGeneration(const LWOOBJID) override { return generation; }
		// A dashboard edit
		void UpdateCharacterXml(const LWOOBJID, const std::string_view lxfml) override { xml = lxfml; generation++; }
		void InsertAuditLog(uint32_t, const std::string_view, const std::string_view action, const std::string_view, uint32_t, LWOOBJID) override {
			audits.push_back(std::string(action));
		}
	};
}

class StaleSaveGuardTest : public GameDependenciesTest {
protected:
	SaveDatabase* db{};

	void SetUp() override {
		SetUpDependencies();
		db = new SaveDatabase();
		Database::_setDatabase(db);
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(StaleSaveGuardTest, LoadingClaimsTheCharacter) {
	Character character(1, nullptr);
	character.UpdateFromDatabase();
	EXPECT_EQ(character.GetSaveGeneration(), 8u);
	EXPECT_EQ(db->generation, 8u);
	EXPECT_EQ(character.GetXMLData(), db->xml);
}

TEST_F(StaleSaveGuardTest, SavesMoveTheGenerationAlong) {
	Character character(1, nullptr);
	character.UpdateFromDatabase();
	character.WriteToDatabase();
	character.WriteToDatabase();
	EXPECT_EQ(db->saves, 2u);
	EXPECT_EQ(character.GetSaveGeneration(), db->generation);
	EXPECT_FALSE(character.IsSaveRefused());
	EXPECT_TRUE(db->audits.empty());
}

TEST_F(StaleSaveGuardTest, AnotherWorldLoadingMakesTheOldOneStale) {
	Character oldWorld(1, nullptr);
	oldWorld.UpdateFromDatabase();
	oldWorld.WriteToDatabase();

	Character newWorld(1, nullptr);
	newWorld.UpdateFromDatabase();
	newWorld.WriteToDatabase();
	const auto newXml = db->xml;

	// The old world still has the player (a disconnect it noticed late) and saves: refused, the new data stays
	oldWorld.WriteToDatabase();
	EXPECT_TRUE(oldWorld.IsSaveRefused());
	EXPECT_EQ(db->xml, newXml);
	EXPECT_EQ(db->saves, 2u);
	ASSERT_EQ(db->audits.size(), 1u);
	EXPECT_EQ(db->audits[0], "stale_save_refused");

	// It doesn't try again (or audit again)
	oldWorld.WriteToDatabase();
	EXPECT_EQ(db->audits.size(), 1u);

	// The world that has the character keeps saving
	newWorld.WriteToDatabase();
	EXPECT_FALSE(newWorld.IsSaveRefused());
	EXPECT_EQ(db->saves, 3u);
}

TEST_F(StaleSaveGuardTest, DashboardEditWins) {
	Character character(1, nullptr);
	character.UpdateFromDatabase();
	Database::Get()->UpdateCharacterXml(1, "<obj v=\"1\"><char cc=\"999\"/></obj>");
	character.WriteToDatabase();
	EXPECT_TRUE(character.IsSaveRefused());
	EXPECT_EQ(db->xml, "<obj v=\"1\"><char cc=\"999\"/></obj>");

	// Loading again (the player logs back in) picks the edit up and saves normally
	character.UpdateFromDatabase();
	EXPECT_FALSE(character.IsSaveRefused());
	EXPECT_EQ(character.GetXMLData(), "<obj v=\"1\"><char cc=\"999\"/></obj>");
	character.WriteToDatabase();
	EXPECT_FALSE(character.IsSaveRefused());
}
