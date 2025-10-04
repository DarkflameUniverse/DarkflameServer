#include "GameDependencies.h"
#include "Character.h"
#include "Entity.h"
#include "tinyxml2.h"
#include "BuffComponent.h"
#include "CharacterComponent.h"

class SavingTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;
	std::unique_ptr<Character> character;
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLPrinter printer{ 0, true, 0 };

	void SetUp() override {
		SetUpDependencies();
		Game::zoneManager->LoadZone(LWOZONEID(1800, 2, 0));
		static_cast<dServerMock*>(Game::server)->SetZoneId(1800);

		entity = std::make_unique<Entity>(1, GameDependenciesTest::info);
		character = std::make_unique<Character>(1, nullptr);

		doc.LoadFile("./test_xml_data.xml");

		entity->SetCharacter(character.get());
		character->SetEntity(entity.get());

		doc.Print(&printer);
		character->_setXmlData(printer.CStr());
		printer.ClearBuffer();

		character->_doQuickXMLDataParse();
		character->LoadXmlRespawnCheckpoints();

		entity->AddComponent<CharacterComponent>(-1, character.get(), UNASSIGNED_SYSTEM_ADDRESS)->LoadFromXml(entity->GetCharacter()->GetXMLDoc());
	}

	void TearDown() override {
		entity->SetCharacter(nullptr);

		entity.reset();
		character.reset();

		TearDownDependencies();
	}
};

TEST_F(SavingTest, CharacterComponentTest) {
	// Print the original XML data
	// character->GetXMLDoc().Print(&printer);
	// std::string xmlDataOriginal(printer.CStr());
	// printer.ClearBuffer();
	// std::ofstream oldXml("./test_xml_data_original.xml");
	// oldXml << xmlDataOriginal;

	auto* characterComponent = entity->GetComponent<CharacterComponent>();

	// Update the xml document so its been run through the saver
	character->SaveXMLToDatabase();

	// Reload the component and character from the now updated xml data
	const auto prevTotalTime = characterComponent->GetTotalTimePlayed();
	character->_doQuickXMLDataParse();
	entity->AddComponent<CharacterComponent>(-1, character.get(), UNASSIGNED_SYSTEM_ADDRESS);
	characterComponent->LoadFromXml(entity->GetCharacter()->GetXMLDoc());

	// Check that the buff component is the same as before which means resaving data and loading it back in didn't change anything
	ASSERT_EQ("32114;69;343;13;163;2;181;2;388;252;146;24451;25;9022;41898;42186;42524;4404;0;0;0;0;0;0;0;0;0;", characterComponent->StatisticsToString());

	// need a variable because the macros do not support {}
	constexpr std::array<uint64_t, 4> correctCodes = { 1073741968, 0, 0, 0 };
	ASSERT_EQ(correctCodes, characterComponent->GetClaimCodes());
	ASSERT_EQ(1, characterComponent->m_Character->GetEyebrows());
	ASSERT_EQ(2, characterComponent->m_Character->GetEyes());
	ASSERT_EQ(9, characterComponent->m_Character->GetHairColor());
	ASSERT_EQ(8, characterComponent->m_Character->GetHairStyle());
	ASSERT_EQ(3, characterComponent->m_Character->GetPantsColor());
	ASSERT_EQ(27634704, characterComponent->m_Character->GetLeftHand());
	ASSERT_EQ(3, characterComponent->m_Character->GetMouth());
	ASSERT_EQ(27187396, characterComponent->m_Character->GetRightHand());
	ASSERT_EQ(13, characterComponent->m_Character->GetShirtColor());
	ASSERT_EQ(7510, characterComponent->GetUScore());
	ASSERT_EQ(300, characterComponent->GetReputation());
	ASSERT_EQ(u"0:1:4719+1:4720+1:4721", characterComponent->GetLastRocketConfig());
	ASSERT_EQ(prevTotalTime, characterComponent->GetTotalTimePlayed());

	const std::map<LWOMAPID, ZoneStatistics> correctZoneStats =
	{
	{ 1000, { .m_AchievementsCollected = 0, .m_BricksCollected = 0, .m_CoinsCollected = 4, .m_EnemiesSmashed = 0, .m_QuickBuildsCompleted = 0 } },
	{ 1100, { .m_AchievementsCollected = 1, .m_BricksCollected = 54, .m_CoinsCollected = 584, .m_EnemiesSmashed = 34, .m_QuickBuildsCompleted = 0 } },
	{ 1101, { .m_AchievementsCollected = 7, .m_BricksCollected = 0, .m_CoinsCollected = 750, .m_EnemiesSmashed = 100, .m_QuickBuildsCompleted = 7 } },
	{ 1200, { .m_AchievementsCollected = 51, .m_BricksCollected = 9, .m_CoinsCollected = 26724, .m_EnemiesSmashed = 0, .m_QuickBuildsCompleted = 3 } },
	{ 1250, { .m_AchievementsCollected = 1, .m_BricksCollected = 1, .m_CoinsCollected = 158, .m_EnemiesSmashed = 15, .m_QuickBuildsCompleted = 1 } },
	{ 1800, { .m_AchievementsCollected = 4, .m_BricksCollected = 5, .m_CoinsCollected = 3894, .m_EnemiesSmashed = 14, .m_QuickBuildsCompleted = 2 } },
	};

	ASSERT_EQ(correctZoneStats, characterComponent->GetZoneStatistics());

	// Fails currently due to not reading style from xml
	// Should the value be fixed, this test will fail and will match the above
	// only then will this comment be removed.
	ASSERT_NE(27, characterComponent->m_Character->GetShirtStyle());
}
