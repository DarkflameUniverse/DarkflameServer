#include <gtest/gtest.h>

#include "CharacterXml.h"

namespace {
	const std::string XML = R"(<obj v="1"><char acct="91" cc="449" ls="110" /><dest hm="4" /><inv csl="-1"><bag><b t="0" m="3" /><b t="5" m="240" /></bag><items>)"
		R"(<in t="0"><i l="4057" id="100" s="0" c="1" b="true" eq="true" sk="0" parent="0" /><i l="2508" id="101" s="2" c="5" b="false" eq="false" sk="0" parent="0" /></in>)"
		R"(<in t="5"><i l="6416" id="102" s="1" c="1" b="true" eq="false" sk="0" parent="0"><x ma="0:1:14444" /></i></in></items></inv><lvl l="2" cv="1" sb="500" />)"
		R"(<mis><done><m state="8" id="1727" /><m state="8" id="308" /></done></mis></obj>)";

	int64_t g_Next = 500;
	int64_t NextId() { return g_Next++; }

	std::optional<std::string> Apply(const nlohmann::json& edit, std::string& error) {
		return CharacterXml::Apply(XML, edit, NextId, error);
	}
}

TEST(CharacterXmlTests, Summary) {
	const auto s = CharacterXml::Summary(XML);
	ASSERT_FALSE(s.is_null());
	EXPECT_EQ(s["coins"], 449);
	EXPECT_EQ(s["uscore"], 110);
	EXPECT_EQ(s["level"], 2);
	EXPECT_EQ(s["missions_done"], 2);
	ASSERT_EQ(s["inventories"].size(), 2u);
	EXPECT_EQ(s["inventories"][0]["size"], 3);
	EXPECT_EQ(s["inventories"][0]["items"][1]["count"], 5);
	EXPECT_EQ(s["inventories"][0]["items"][0]["equipped"], true);
	EXPECT_EQ(s["inventories"][1]["items"][0]["id"], "102");
	EXPECT_TRUE(CharacterXml::Summary("not xml").is_null());
}

TEST(CharacterXmlTests, EditsNumbersAndItems) {
	std::string error;
	const auto xml = Apply({ {"coins", 5000}, {"level", 10}, {"counts", { {"101", 20} }}, {"remove", { "100" }} }, error);
	ASSERT_TRUE(xml) << error;
	const auto s = CharacterXml::Summary(*xml);
	EXPECT_EQ(s["coins"], 5000);
	EXPECT_EQ(s["uscore"], 110); // untouched
	EXPECT_EQ(s["level"], 10);
	ASSERT_EQ(s["inventories"][0]["items"].size(), 1u);
	EXPECT_EQ(s["inventories"][0]["items"][0]["count"], 20);
	// Everything else in the file survives
	EXPECT_NE(xml->find(R"(<x ma="0:1:14444"/>)"), std::string::npos);
	EXPECT_NE(xml->find("<mis>"), std::string::npos);
}

TEST(CharacterXmlTests, AddsIntoTheFirstFreeSlot) {
	std::string error;
	const auto xml = Apply({ {"add", { { {"lot", 1727}, {"count", 3}, {"inventory", 0} } }} }, error);
	ASSERT_TRUE(xml) << error;
	const auto summary = CharacterXml::Summary(*xml);
	const auto& items = summary["inventories"][0]["items"];
	ASSERT_EQ(items.size(), 3u);
	EXPECT_EQ(items[2]["lot"], 1727);
	EXPECT_EQ(items[2]["slot"], 1); // 0 and 2 are taken
	EXPECT_EQ(items[2]["count"], 3);
	EXPECT_EQ(items[2]["bound"], false);
	EXPECT_EQ(std::stoll(items[2]["id"].get<std::string>()), g_Next - 1);
}

TEST(CharacterXmlTests, RefusesBadEdits) {
	std::string error;
	// The bag has 3 slots and 2 are used: a second new item doesn't fit
	EXPECT_FALSE(Apply({ {"add", { { {"lot", 1}, {"count", 1} }, { {"lot", 2}, {"count", 1} } }} }, error));
	EXPECT_EQ(error, "That inventory is full");
	EXPECT_FALSE(Apply({ {"remove", { "999" }} }, error));
	EXPECT_NE(error.find("no item 999"), std::string::npos);
	EXPECT_FALSE(Apply({ {"coins", -1} }, error));
	EXPECT_FALSE(Apply({ {"level", 46} }, error));
	EXPECT_FALSE(Apply({ {"counts", { {"101", 0} }} }, error));
	EXPECT_FALSE(Apply({ {"add", { { {"lot", 5}, {"inventory", 7} } }} }, error));
}

TEST(CharacterXmlTests, PetIdsMatchPetNames) {
	// Saved before eCharacterVersion::PET_IDS: the game clears bit 32 at the next login, and so does the dashboard
	const std::string oldSave = R"(<obj v="1"><lvl l="10" cv="5" /><pet><p id="1152921509111054336" l="3195" m="2" n="Trike" /><p id="0" l="1" /></pet></obj>)";
	const auto oldPets = CharacterXml::Pets(oldSave);
	ASSERT_EQ(oldPets.size(), 1u);
	EXPECT_EQ(oldPets[0].id, 1152921504816087040LL); // 1152921509111054336 without bit 32
	EXPECT_EQ(oldPets[0].lot, 3195);
	EXPECT_EQ(oldPets[0].name, "Trike");

	// Up to date: ids are used as saved
	const std::string newSave = R"(<obj v="1"><lvl l="10" cv="9" /><pet><p id="1152921509111054336" l="3195" n="" /></pet></obj>)";
	const auto newPets = CharacterXml::Pets(newSave);
	ASSERT_EQ(newPets.size(), 1u);
	EXPECT_EQ(newPets[0].id, 1152921509111054336LL);

	EXPECT_TRUE(CharacterXml::Pets("<obj/>").empty());
	EXPECT_TRUE(CharacterXml::Pets("not xml").empty());
}
