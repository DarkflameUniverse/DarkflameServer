#include <gtest/gtest.h>

#include <fstream>

#include "VanityXml.h"

namespace {
	std::string ReadSource(const std::string& relative) {
		std::ifstream in(std::string(DLU_SOURCE_DIR) + "/" + relative, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(in), {});
	}
}

// The shipped files survive a read and write with nothing lost
TEST(VanityXmlTests, ShippedFilesRoundTrip) {
	for (const auto* file : { "vanity/dev-tribute.xml", "vanity/atm.xml", "vanity/demo.xml", "vanity/root.xml" }) {
		std::string error;
		const auto first = VanityXml::Read(ReadSource(file), error);
		ASSERT_TRUE(first) << file << ": " << error;
		const auto again = VanityXml::Read(VanityXml::Write(*first), error);
		ASSERT_TRUE(again) << file << ": " << error;
		ASSERT_EQ(first->objects.size(), again->objects.size()) << file;
		ASSERT_EQ(first->files.size(), again->files.size()) << file;
		for (size_t i = 0; i < first->objects.size(); i++) {
			const auto& a = first->objects[i];
			const auto& b = again->objects[i];
			EXPECT_EQ(a.name, b.name);
			EXPECT_EQ(a.lot, b.lot);
			EXPECT_EQ(a.equipment, b.equipment);
			EXPECT_EQ(a.phrases, b.phrases);
			EXPECT_EQ(a.config, b.config);
			ASSERT_EQ(a.locations.size(), b.locations.size());
			for (size_t l = 0; l < a.locations.size(); l++) {
				EXPECT_EQ(a.locations[l].zone, b.locations[l].zone);
				EXPECT_FLOAT_EQ(a.locations[l].x, b.locations[l].x);
				EXPECT_FLOAT_EQ(a.locations[l].ry, b.locations[l].ry);
				EXPECT_EQ(a.locations[l].chance, b.locations[l].chance);
			}
		}
	}
}

TEST(VanityXmlTests, DevTributeHasItsNpcs) {
	std::string error;
	const auto doc = VanityXml::Read(ReadSource("vanity/dev-tribute.xml"), error);
	ASSERT_TRUE(doc) << error;
	ASSERT_FALSE(doc->objects.empty());
	EXPECT_EQ(doc->objects.front().name, "Wincent - Developer");
	EXPECT_EQ(doc->objects.front().lot, 2279);
	EXPECT_EQ(doc->objects.front().equipment.size(), 4u);
	EXPECT_EQ(doc->objects.front().locations.front().zone, 1200u);
}

// What the world server skips, and says so
TEST(VanityXmlTests, SkipsIncompleteParts) {
	std::string error;
	const auto doc = VanityXml::Read(R"(<files><file name="a.xml" enabled="1"/><file name="b.xml"/><file enabled="1"/></files>)"
		R"(<objects><object name="NoLot" lot="x"><equipment>1, 2 ,x</equipment><locations><location zone="1200" x="1" y="2"/>)"
		R"(<location zone="1200" x="1" y="2" z="3" rw="1" rx="0" ry="0" rz="0" chance="0.25"/></locations></object>)"
		R"(<object name="Nowhere" lot="5"/></objects>)", error);
	ASSERT_TRUE(doc) << error;
	ASSERT_EQ(doc->files.size(), 2u);
	EXPECT_TRUE(doc->files[0].enabled);
	EXPECT_FALSE(doc->files[1].enabled);
	ASSERT_EQ(doc->objects.size(), 2u);
	EXPECT_EQ(doc->objects[0].lot, -1);
	EXPECT_EQ(doc->objects[0].equipment, (std::vector<int32_t>{ 1, 2 }));
	ASSERT_EQ(doc->objects[0].locations.size(), 1u);
	EXPECT_EQ(doc->objects[0].locations[0].chance, 0.25f);
	EXPECT_FALSE(doc->objects[0].locations[0].scale);
	EXPECT_TRUE(doc->objects[1].locations.empty());
	EXPECT_EQ(doc->warnings.size(), 2u); // the incomplete location, the object without locations
	EXPECT_FALSE(VanityXml::Read("<objects>", error));
}
