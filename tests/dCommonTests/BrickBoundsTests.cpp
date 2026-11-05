#include "gtest/gtest.h"

#include "AssetManager.h"
#include "BrickBounds.h"
#include "ZCompression.h"
#include "Zip.h"
#include "dCommonDependencies.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
	std::string Primitive(const std::string& aliases, const std::string& aabb) {
		return R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?><LEGOPrimitive versionMajor="1" versionMinor="0">)"
			R"(<Annotations><Annotation designname="TEST"/><Annotation aliases=")" + aliases + R"("/></Annotations>)"
			R"(<Bounding>)" + aabb + R"(</Bounding>)"
			R"(<GeometryBounding><AABB minX="-9" minY="-9" minZ="-9" maxX="9" maxY="9" maxZ="9"/></GeometryBounding></LEGOPrimitive>)";
	}

	const std::string BRICK_2X2_AABB = R"(<AABB minX="-0.4" minY="0" minZ="-1.2" maxX="1.2" maxY="1.138" maxZ="0.4"/>)";

	struct Entry {
		std::string name;
		std::string data;
		bool deflate;
	};

	void Write16(std::string& out, uint16_t value) {
		out += static_cast<char>(value & 0xFF);
		out += static_cast<char>(value >> 8);
	}

	void Write32(std::string& out, uint32_t value) {
		Write16(out, static_cast<uint16_t>(value & 0xFFFF));
		Write16(out, static_cast<uint16_t>(value >> 16));
	}

	// Raw deflate data: zlib's stream without its 2 byte header and 4 byte checksum
	std::string Deflate(const std::string& data) {
		std::string out(ZCompression::GetMaxCompressedLength(static_cast<uint32_t>(data.size())), '\0');
		const auto size = ZCompression::Compress(reinterpret_cast<const uint8_t*>(data.data()), static_cast<uint32_t>(data.size()), reinterpret_cast<uint8_t*>(out.data()), static_cast<uint32_t>(out.size()));
		return out.substr(2, size - 6);
	}

	// A zip archive (crc left at 0: it isn't checked)
	std::string MakeZip(const std::vector<Entry>& entries) {
		std::string zip;
		std::string directory;
		for (const auto& entry : entries) {
			const auto data = entry.deflate ? Deflate(entry.data) : entry.data;
			const auto method = static_cast<uint16_t>(entry.deflate ? 8 : 0);
			const auto offset = static_cast<uint32_t>(zip.size());
			Write32(zip, 0x04034b50); Write16(zip, 20); Write16(zip, 0); Write16(zip, method); Write32(zip, 0); Write32(zip, 0);
			Write32(zip, static_cast<uint32_t>(data.size())); Write32(zip, static_cast<uint32_t>(entry.data.size()));
			Write16(zip, static_cast<uint16_t>(entry.name.size())); Write16(zip, 0);
			zip += entry.name + data;

			Write32(directory, 0x02014b50); Write16(directory, 20); Write16(directory, 20); Write16(directory, 0); Write16(directory, method);
			Write32(directory, 0); Write32(directory, 0); Write32(directory, static_cast<uint32_t>(data.size())); Write32(directory, static_cast<uint32_t>(entry.data.size()));
			Write16(directory, static_cast<uint16_t>(entry.name.size())); Write16(directory, 0); Write16(directory, 0); Write16(directory, 0); Write16(directory, 0);
			Write32(directory, 0); Write32(directory, offset);
			directory += entry.name;
		}
		const auto directoryOffset = static_cast<uint32_t>(zip.size());
		zip += directory;
		Write32(zip, 0x06054b50); Write16(zip, 0); Write16(zip, 0);
		Write16(zip, static_cast<uint16_t>(entries.size())); Write16(zip, static_cast<uint16_t>(entries.size()));
		Write32(zip, static_cast<uint32_t>(directory.size())); Write32(zip, directoryOffset); Write16(zip, 0);
		return zip;
	}

	const std::string BRICKDB = MakeZip({
		{ "info.xml", R"(<DB version="1"><Bricks version="457"/></DB>)", false },
		{ "Primitives/3003.xml", Primitive("6223;3003", BRICK_2X2_AABB), true },
		{ "Primitives/3001.xml", Primitive("3001;3003", R"(<AABB minX="-0.4" minY="0" minZ="-2" maxX="2.8" maxY="1.138" maxZ="0.4"/>)"), false },
		{ "Primitives/4000.xml", Primitive("4000", ""), true },
		{ "Primitives/notanumber.xml", Primitive("", BRICK_2X2_AABB), false },
		{ "Assemblies/5000.lxfml", "<LXFML/>", false },
	});

	class BrickBoundsTests : public dCommonDependenciesTest {
	protected:
		void SetUp() override { SetUpDependencies(); }
		void TearDown() override {
			TearDownDependencies();
			Game::logger = nullptr;
		}
	};
}

TEST(BrickBoundsParseTests, ReadsTheBoundingBoxNotTheGeometryBox) {
	const auto bounds = BrickBounds::ParsePrimitive(Primitive("3003", BRICK_2X2_AABB));
	ASSERT_TRUE(bounds);
	EXPECT_EQ(bounds->min, NiPoint3(-0.4f, 0.0f, -1.2f));
	EXPECT_EQ(bounds->max, NiPoint3(1.2f, 1.138f, 0.4f));
}

TEST(BrickBoundsParseTests, APrimitiveWithoutABoxHasNoBounds) {
	EXPECT_FALSE(BrickBounds::ParsePrimitive(Primitive("3003", "")));
	EXPECT_FALSE(BrickBounds::ParsePrimitive(Primitive("3003", R"(<AABB minX="-0.4" minY="0" minZ="-1.2" maxX="1.2" maxY="1.138"/>)")));
	EXPECT_FALSE(BrickBounds::ParsePrimitive("not xml"));
}

TEST(BrickBoundsParseTests, ZipEntriesAreReadInOrder) {
	std::vector<std::string> names;
	std::vector<std::string> contents;
	EXPECT_TRUE(Zip::ForEachEntry(BRICKDB, [](std::string_view name) { return name != "Assemblies/5000.lxfml"; },
		[&](std::string_view name, std::string_view data) { names.emplace_back(name); contents.emplace_back(data); }));
	EXPECT_EQ(names, (std::vector<std::string>{ "info.xml", "Primitives/3003.xml", "Primitives/3001.xml", "Primitives/4000.xml", "Primitives/notanumber.xml" }));
	// Stored and deflated entries both come out as written
	EXPECT_EQ(contents[0], R"(<DB version="1"><Bricks version="457"/></DB>)");
	EXPECT_EQ(contents[1], Primitive("6223;3003", BRICK_2X2_AABB));
}

TEST(BrickBoundsParseTests, NotAZip) {
	EXPECT_FALSE(Zip::ForEachEntry("", [](std::string_view) { return true; }, [](std::string_view, std::string_view) {}));
	EXPECT_FALSE(Zip::ForEachEntry(std::string(100, 'x'), [](std::string_view) { return true; }, [](std::string_view, std::string_view) {}));
	EXPECT_TRUE(BrickBounds::ParseBrickDb("not a zip").empty());
}

TEST(BrickBoundsParseTests, BrickDbHasEveryPrimitiveWithABox) {
	const auto bounds = BrickBounds::ParseBrickDb(BRICKDB);
	ASSERT_EQ(bounds.size(), 3);
	EXPECT_EQ(bounds.at(3003).min, NiPoint3(-0.4f, 0.0f, -1.2f));
	EXPECT_EQ(bounds.at(3003).max, NiPoint3(1.2f, 1.138f, 0.4f));
	EXPECT_EQ(bounds.at(3001).max, NiPoint3(2.8f, 1.138f, 0.4f));
	// 6223 is only an alias of 3003; 3003 being listed as an alias of 3001 doesn't replace its own box
	EXPECT_EQ(bounds.at(6223).max, NiPoint3(1.2f, 1.138f, 0.4f));
	EXPECT_FALSE(bounds.contains(4000));
}

TEST_F(BrickBoundsTests, LoadsTheClientsBrickDb) {
	const auto root = std::filesystem::temp_directory_path() / ("dlu_brickbounds_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root / "res");
	std::ofstream(root / "res" / "CDServer.sqlite").put('\0');
	{
		AssetManager assets(root);
		EXPECT_TRUE(BrickBounds::Load(assets).empty());
	}
	std::ofstream(root / "res" / "brickdb.zip", std::ios::binary) << BRICKDB;
	AssetManager assets(root);
	const auto loaded = BrickBounds::Load(assets);
	EXPECT_EQ(loaded.size(), 3);

	// Read once, then kept
	const auto& kept = BrickBounds::Get(&assets);
	EXPECT_EQ(kept.size(), 3);
	std::filesystem::remove_all(root);
	EXPECT_EQ(&BrickBounds::Get(&assets), &kept);
	EXPECT_EQ(BrickBounds::Get(nullptr).size(), 3);
}
