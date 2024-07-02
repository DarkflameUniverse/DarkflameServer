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

		entity->AddComponent<BuffComponent>()->LoadFromXml(entity->GetCharacter()->GetXMLDoc());
		entity->AddComponent<CharacterComponent>(character.get(), UNASSIGNED_SYSTEM_ADDRESS)->LoadFromXml(entity->GetCharacter()->GetXMLDoc());
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

	auto statsPrev = characterComponent->StatisticsToString();
	auto claimCodesPrev = characterComponent->GetClaimCodes();
	auto eyebrowsPrev = characterComponent->m_Character->GetEyebrows();
	auto eyesPrev = characterComponent->m_Character->GetEyes();
	auto hairColorPrev = characterComponent->m_Character->GetHairColor();
	auto hairStylePrev = characterComponent->m_Character->GetHairStyle();
	auto pantsColorPrev = characterComponent->m_Character->GetPantsColor();
	auto leftHandPrev = characterComponent->m_Character->GetLeftHand();
	auto mouthPrev = characterComponent->m_Character->GetMouth();
	auto rightHandPrev = characterComponent->m_Character->GetRightHand();
	auto shirtColorPrev = characterComponent->m_Character->GetShirtColor();

	// Update the xml document so its been run through the saver
	character->SaveXMLToDatabase();

	// Reload the component from the now updated xml data
	characterComponent = entity->AddComponent<CharacterComponent>(character.get(), UNASSIGNED_SYSTEM_ADDRESS);
	characterComponent->LoadFromXml(entity->GetCharacter()->GetXMLDoc());

	// Check that the buff component is the same as before
	ASSERT_EQ(statsPrev, characterComponent->StatisticsToString());
	ASSERT_EQ(claimCodesPrev, characterComponent->GetClaimCodes());
	ASSERT_EQ(eyebrowsPrev, characterComponent->m_Character->GetEyebrows());
	ASSERT_EQ(eyesPrev, characterComponent->m_Character->GetEyes());
	ASSERT_EQ(hairColorPrev, characterComponent->m_Character->GetHairColor());
	ASSERT_EQ(hairStylePrev, characterComponent->m_Character->GetHairStyle());
	ASSERT_EQ(pantsColorPrev, characterComponent->m_Character->GetPantsColor());
	ASSERT_EQ(leftHandPrev, characterComponent->m_Character->GetLeftHand());
	ASSERT_EQ(mouthPrev, characterComponent->m_Character->GetMouth());
	ASSERT_EQ(rightHandPrev, characterComponent->m_Character->GetRightHand());
	ASSERT_EQ(shirtColorPrev, characterComponent->m_Character->GetShirtColor());

	// character->GetXMLDoc().Print(&printer);
	// std::string xmlDataModified(printer.CStr());
	// printer.ClearBuffer();
	// std::ofstream newXml("./test_xml_data_new.xml");
	// newXml << xmlDataModified;
}
