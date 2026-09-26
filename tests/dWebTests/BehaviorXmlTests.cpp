#include <gtest/gtest.h>

#include "BehaviorXml.h"

TEST(BehaviorXmlTests, ParsesStatesStripsAndActions) {
	const auto behavior = BehaviorXml::Parse(R"(<Behavior id="49904" name="Guard" isLocked="false"><State id="0"><Strip><Position x="158.3" y="47"/>)"
		R"(<Action Type="OnInteract"/><Action Type="MoveRight" ValueParameterName="Distance" Value="5"/><Action Type="ChangeStateCircle"/></Strip></State>)"
		R"(<State id="1"><Strip><Position x="1" y="2"/><Action Type="OnChat" ValueParameterName="Message" Value="hi"/><Action Type="Smash"/></Strip></State></Behavior>)");
	ASSERT_FALSE(behavior.is_null());
	EXPECT_EQ(behavior["name"], "Guard");
	ASSERT_EQ(behavior["states"].size(), 2u);
	EXPECT_EQ(behavior["states"][0]["name"], "Home");
	EXPECT_EQ(behavior["states"][1]["name"], "Circle");
	const auto& actions = behavior["states"][0]["strips"][0]["actions"];
	ASSERT_EQ(actions.size(), 3u);
	EXPECT_EQ(actions[0]["type"], "OnInteract");
	EXPECT_FALSE(actions[0].contains("value"));
	EXPECT_EQ(actions[1]["type"], "MoveRight");
	EXPECT_EQ(actions[1]["parameter"], "Distance");
	EXPECT_EQ(actions[1]["value"], "5");
	EXPECT_EQ(behavior["states"][1]["strips"][0]["actions"][0]["value"], "hi");
	EXPECT_DOUBLE_EQ(behavior["states"][0]["strips"][0]["x"].get<double>(), 158.3);
}

TEST(BehaviorXmlTests, RejectsJunk) {
	EXPECT_TRUE(BehaviorXml::Parse("").is_null());
	EXPECT_TRUE(BehaviorXml::Parse("<not xml").is_null());
	EXPECT_TRUE(BehaviorXml::Parse("<Other/>").is_null());
}

TEST(BehaviorXmlTests, StatesAreNamedFromTheEnum) {
	EXPECT_EQ(BehaviorXml::StateName(0), "Home");
	EXPECT_EQ(BehaviorXml::StateName(static_cast<int>(BehaviorState::STAR_STATE)), "Star");
	EXPECT_EQ(BehaviorXml::StateName(99), "State");
}

// The viewer's behavior player gets the server's own block table
TEST(BehaviorXmlTests, RulesComeFromTheServerTable) {
	const auto rules = BehaviorXml::Rules([](int32_t lot) { return "LOT " + std::to_string(lot); });
	EXPECT_EQ(rules["triggers"].size(), PropertyBehaviorActions::TRIGGERS.size());
	EXPECT_EQ(rules["moves"]["MoveLeft"], nlohmann::json::array({ "x", -1.0f }));
	EXPECT_EQ(rules["moves"]["FlyUp"][0], "y");
	EXPECT_EQ(rules["spawns"]["SpawnPirate"]["lot"], 10497);
	EXPECT_EQ(rules["drops"]["DropArmor"]["name"], "LOT 6431");
	EXPECT_EQ(rules["stateChanges"]["ChangeStateDiamond"], 3);
	EXPECT_EQ(rules["states"]["5"], "Star");
	EXPECT_EQ(rules["defaultSpeed"], PropertyBehaviorActions::DEFAULT_SPEED);
	EXPECT_NE(std::find(rules["others"].begin(), rules["others"].end(), "UnSmash"), rules["others"].end());
}
