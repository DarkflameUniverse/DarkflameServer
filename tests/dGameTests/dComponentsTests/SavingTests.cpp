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
	tinyxml2::XMLPrinter printer{0, true, 0};

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

TEST_F(SavingTest, EntityLevelTest) {
	// Print the original XML data
	character->GetXMLDoc().Print(&printer);
	std::string xmlDataOriginal(printer.CStr());
	printer.ClearBuffer();

	character->SaveXMLToDatabase();

	// Load the modified XML data
	character->GetXMLDoc().Print(&printer);
	std::string xmlDataModified(printer.CStr());
	printer.ClearBuffer();
	std::ofstream oldXml("./test_xml_data_original.xml");
	std::ofstream newXml("./test_xml_data_new.xml");
	oldXml << xmlDataOriginal;
	newXml << xmlDataModified;
	ASSERT_EQ(xmlDataOriginal, xmlDataModified);
}
