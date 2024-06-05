#include "GameDependencies.h"
#include "Character.h"
#include "Entity.h"
#include "tinyxml2.h"

class SavingTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;
	std::unique_ptr<Character> character;
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLPrinter printer{0, true, 0};

	void SetUp() override {
		SetUpDependencies();
		Game::zoneManager->LoadZone(LWOZONEID(1800, 2, 0));
		GameDependenciesTest::info.lot = 1;
		entity = std::make_unique<Entity>(1, GameDependenciesTest::info);
		character = std::make_unique<Character>(1, nullptr);
		doc.LoadFile("./test_xml_data.xml");
		entity->SetCharacter(character.get());
		character->SetEntity(entity.get());
		doc.Print(&printer);
		character->_setXmlData(printer.CStr());
		doc.Clear();
		character->_doQuickXMLDataParse();
	}

	void TearDown() override {
		entity->SetCharacter(nullptr);
		entity.reset();
		character.reset();
		TearDownDependencies();
	}
};

TEST_F(SavingTest, EntityLevelTest) {
	doc.Print(&printer);
	std::string xmlDataOriginal(printer.CStr());
	doc.Clear();
	character->SaveXMLToDatabase();
	doc.Print(&printer);
	std::string xmlDataModified(printer.CStr());
	doc.Clear();
	std::ofstream oldXml("./test_xml_data_original.xml");
	std::ofstream newXml("./test_xml_data_new.xml");
	oldXml << xmlDataOriginal;
	newXml << xmlDataModified;
	LOG("Component count: %i Same: %i", entity->GetComponents().size(), xmlDataOriginal == xmlDataModified);
}
