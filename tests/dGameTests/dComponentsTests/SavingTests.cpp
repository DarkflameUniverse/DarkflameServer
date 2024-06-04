#include "GameDependencies.h"
#include "Character.h"
#include "Entity.h"
#include "tinyxml2.h"

class SavingTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;
	std::unique_ptr<Character> character;
	tinyxml2::XMLDocument doc;
	void SetUp() override {
		SetUpDependencies();
		entity = std::make_unique<Entity>(1, GameDependenciesTest::info);
		character = std::make_unique<Character>(1, nullptr);
		doc.LoadFile("./test_xml_data.xml");
		entity->SetCharacter(character.get());
		character->_setXmlDoc(doc);
		character->SetEntity(entity.get());
	}

	void TearDown() override {
		entity->SetCharacter(nullptr);
		entity.reset();
		character.reset();
		TearDownDependencies();
	}
};

TEST_F(SavingTest, EntityLevelTest) {
	character->SaveXMLToDatabase();
}
