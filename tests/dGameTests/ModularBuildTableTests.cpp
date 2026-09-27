#include "CDModularBuildComponentTable.h"

#include <gtest/gtest.h>

// ModularBuildComponent's xml, laid out as the cdclient has it (a car: root part 0; a rocket: root part 2)
namespace {
	constexpr std::string_view CAR_XML = R"(<ModularBuild name="Car">
  <Assembly LOT="8092" PhysicsType="30" PhysicsID="13" />
     <topology>
       <numberOfParts value="7"/>
       <rootPart value="0" />
       <connection name="wheels" myPartid="0" myLocation="CHASSIS" connectingPart="1" optional="1" />
     </topology>
       <Module name="chassis" >
         <PartCode value="0" />
         <ExamplePartLOT value="8129" />
         <InitialGhost templateID="7716" textureName="2"/>
       </Module>
       <Module name="wheels" >
         <PartCode value="1" />
         <ExamplePartLOT value="8130" />
       </Module>
</ModularBuild>)";

	constexpr std::string_view ROCKET_XML = "<ModularBuild>\r\n    <Assembly LOT=\"6416\" PhysicsType=\"3\" PhysicsID=\"2692\" />\r\n"
		"    <topology>\r\n\t\t<numberOfParts value=\"3\" />\r\n\t\t<rootPart value=\"2\" />\r\n    </topology>\r\n"
		"\t\t<Module name=\"nose\">\r\n\t\t\t <PartCode value=\"0\" />\r\n\t\t\t <ExamplePartLOT value=\"4713\" />\r\n\t\t</Module>\r\n"
		"\t\t<Module name=\"engine\">\r\n\t\t\t <PartCode value=\"2\" />\r\n\t\t\t <ExamplePartLOT value=\"4715\" />\r\n\t\t</Module>\r\n</ModularBuild>";
}

TEST(ModularBuildTableTests, ReadsPartsAndRootPartFromTheXml) {
	CDModularBuildComponent car;
	CDModularBuildComponentTable::ParseXml(CAR_XML, car);
	EXPECT_EQ(car.numberOfParts, 7u);
	EXPECT_EQ(car.rootPartExampleLOT, 8129);

	CDModularBuildComponent rocket;
	CDModularBuildComponentTable::ParseXml(ROCKET_XML, rocket);
	EXPECT_EQ(rocket.numberOfParts, 3u);
	EXPECT_EQ(rocket.rootPartExampleLOT, 4715);

	CDModularBuildComponent empty;
	CDModularBuildComponentTable::ParseXml("", empty);
	EXPECT_EQ(empty.numberOfParts, 0u);
	EXPECT_EQ(empty.rootPartExampleLOT, LOT_NULL);
}

TEST(ModularBuildTableTests, FindsTheBuildByItsNumberOfParts) {
	const std::vector<CDModularBuildComponent> builds = {
		{ .id = 5, .buildType = 6, .createdLOT = 8092, .numberOfParts = 7, .rootPartExampleLOT = 8129 },
		{ .id = 6, .buildType = 3, .createdLOT = 6416, .numberOfParts = 3, .rootPartExampleLOT = 4715 },
		{ .id = 3, .buildType = 3, .createdLOT = 6416, .numberOfParts = 3, .rootPartExampleLOT = 4715 },
	};
	EXPECT_EQ(CDModularBuildComponentTable::FindByNumberOfParts(builds, 7)->createdLOT, 8092);
	EXPECT_EQ(CDModularBuildComponentTable::FindByNumberOfParts(builds, 3)->createdLOT, 6416);
	for (const uint32_t parts : { 0u, 4u, 5u, 6u, 8u }) {
		EXPECT_FALSE(CDModularBuildComponentTable::FindByNumberOfParts(builds, parts).has_value());
	}
}
