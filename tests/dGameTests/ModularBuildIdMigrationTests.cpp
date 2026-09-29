#include <gtest/gtest.h>

#include <map>

#include "tinyxml2.h"

#include "ModularBuildIdMigration.h"

// Cars and rockets saved without a subkey get the next id as their subkey and a build with their modules; everything
// else is left as it is
TEST(ModularBuildIdMigrationTests, GivesOldCarsAndRocketsAnId) {
	const char* xml =
		"<obj v=\"1\"><inv><items>"
		"<in t=\"5\">"
		"<i l=\"8092\" id=\"11\" s=\"0\" c=\"1\" sk=\"0\"><x ma=\"0:1:8129+1:8130+1:9332\"/></i>"
		"<i l=\"6416\" id=\"12\" s=\"1\" c=\"1\" sk=\"777\"><x ma=\"0:1:4713+1:4714+1:4715\"/></i>"
		"<i l=\"6662\" id=\"13\" s=\"2\" c=\"1\"><x bp=\"9:555\"/></i>"
		"<i l=\"4714\" id=\"14\" s=\"3\" c=\"1\"/>"
		"</in>"
		"<in t=\"14\"><i l=\"6416\" id=\"15\" s=\"0\" c=\"1\"><x ma=\"0:1:4713+1:4714+1:4715\"/></i></in>"
		"</items></inv></obj>";
	tinyxml2::XMLDocument document;
	ASSERT_EQ(document.Parse(xml), tinyxml2::XML_SUCCESS);
	LWOOBJID next = 1000;
	const auto builds = ModularBuildIdMigration::AssignIds(document, [&next] { return next++; });

	ASSERT_EQ(builds.size(), 2u);
	EXPECT_EQ(builds[0].id, 1000);
	EXPECT_EQ(builds[0].modules, "1:8129+1:8130+1:9332");
	EXPECT_EQ(builds[1].id, 1001);
	EXPECT_EQ(builds[1].modules, "1:4713+1:4714+1:4715");

	std::map<int64_t, int64_t> subkeys;
	for (auto* bag = document.FirstChildElement("obj")->FirstChildElement("inv")->FirstChildElement("items")->FirstChildElement("in"); bag; bag = bag->NextSiblingElement("in")) {
		for (auto* item = bag->FirstChildElement("i"); item; item = item->NextSiblingElement("i")) subkeys[item->Int64Attribute("id")] = item->Int64Attribute("sk", 0);
	}
	EXPECT_EQ(subkeys[11], 1000);
	EXPECT_EQ(subkeys[12], 777);
	EXPECT_EQ(subkeys[13], 0);
	EXPECT_EQ(subkeys[14], 0);
	EXPECT_EQ(subkeys[15], 1001);

	// Run again: nothing left to give an id
	EXPECT_TRUE(ModularBuildIdMigration::AssignIds(document, [&next] { return next++; }).empty());
}
