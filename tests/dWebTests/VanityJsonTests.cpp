#include <gtest/gtest.h>

#include <fstream>

#include "VanityJson.h"

namespace {
	std::string ReadSource(const std::string& relative) {
		std::ifstream in(std::string(DLU_SOURCE_DIR) + "/" + relative, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(in), {});
	}
}

TEST(VanityJsonTests, PageInputIsChecked) {
	std::string error;
	const nlohmann::json good{ {"name", "Guide"}, {"lot", 2279}, {"equipment", {6802}}, {"phrases", {"Hi!", ""}},
		{"config", {"custom_script_client=0:scripts\\ai\\SPEC\\MISSION_MINIGAME_CLIENT.lua"}},
		{"locations", {{ {"zone", 1200}, {"x", 1}, {"y", 2}, {"z", 3}, {"rw", 1}, {"rx", 0}, {"ry", 0}, {"rz", 0}, {"chance", 0.5} }}} };
	const auto object = VanityJson::FromJson(good, error);
	ASSERT_TRUE(object) << error;
	EXPECT_EQ(object->phrases.size(), 1u); // empty lines dropped
	EXPECT_EQ(object->locations.front().chance, 0.5f);

	auto bad = good; bad["config"] = { "not ldf" };
	EXPECT_FALSE(VanityJson::FromJson(bad, error));
	bad = good; bad["locations"] = nlohmann::json::array();
	EXPECT_FALSE(VanityJson::FromJson(bad, error));
	bad = good; bad["lot"] = 0;
	EXPECT_FALSE(VanityJson::FromJson(bad, error));
	bad = good; bad["locations"][0]["chance"] = 2;
	EXPECT_FALSE(VanityJson::FromJson(bad, error));
}

// The shipped files are what the game reads: nothing in them may be refused when saved back
TEST(VanityJsonTests, ShippedNpcsPassTheSaveChecks) {
	for (const auto* file : { "vanity/dev-tribute.xml", "vanity/atm.xml", "vanity/demo.xml" }) {
		std::string error;
		const auto doc = VanityXml::Read(ReadSource(file), error);
		ASSERT_TRUE(doc) << file << ": " << error;
		for (const auto& object : doc->objects) {
			error.clear();
			EXPECT_TRUE(VanityJson::FromJson(VanityJson::ToJson(object), error)) << file << " " << object.name << ": " << error;
		}
	}
}
