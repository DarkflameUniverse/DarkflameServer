#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <unistd.h>

#include <glm/gtc/matrix_transform.hpp>

#include "Game.h"
#include "NifFile.h"
#include "UgcBricks.h"
#include "UgcFormats.h"
#include "UgcModel.h"
#include "UgcJobs.h"
#include "UgcIconParams.h"
#include "UgcKeys.h"
#include "UgcModular.h"
#include "UgcPalette.h"
#include "UgcRender.h"
#include "UgcStorage.h"
#include "UgcThrottle.h"
#include "ZCompression.h"

class Logger;
class dConfig;
namespace Game {
	Logger* logger = nullptr;
	dConfig* config = nullptr;
}

namespace {
	// A closed box as an LDD .g file: 8 corners, 12 triangles
	std::string BoxGeometry(glm::vec3 min, glm::vec3 max) {
		std::vector<float> positions, normals;
		for (int i = 0; i < 8; i++) {
			const glm::vec3 p((i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 4) ? max.z : min.z);
			const auto n = glm::normalize(p - (min + max) * 0.5f);
			positions.insert(positions.end(), { p.x, p.y, p.z });
			normals.insert(normals.end(), { n.x, n.y, n.z });
		}
		const std::vector<uint32_t> indices = { 0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6, 0, 1, 4, 1, 5, 4, 2, 6, 3, 3, 6, 7, 0, 4, 2, 2, 4, 6, 1, 3, 5, 3, 7, 5 };
		std::string out;
		const int32_t header[4] = { 0x42473031, 8, static_cast<int32_t>(indices.size()), 0 };
		out.append(reinterpret_cast<const char*>(header), sizeof(header));
		out.append(reinterpret_cast<const char*>(positions.data()), positions.size() * 4);
		out.append(reinterpret_cast<const char*>(normals.data()), normals.size() * 4);
		out.append(reinterpret_cast<const char*>(indices.data()), indices.size() * 4);
		return out;
	}

	std::filesystem::path TempFolder(const std::string& name) {
		// One folder per test and process: ctest runs the tests in parallel processes
		const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
		auto path = std::filesystem::temp_directory_path() / ("dlu_ugc_test_" + name + "_" + (test ? std::string(test->name()) : std::string()) + "_" + std::to_string(::getpid()));
		std::filesystem::remove_all(path);
		std::filesystem::create_directories(path);
		return path;
	}

	// A res folder with brick 3001 (a 1x1x1 box) and brick 3002 (a big box)
	std::filesystem::path MakeRes() {
		const auto res = TempFolder("res");
		std::filesystem::create_directories(res / "brickprimitives" / "lod0");
		std::ofstream(res / "brickprimitives" / "lod0" / "3001.g", std::ios::binary) << BoxGeometry(glm::vec3(0.0f), glm::vec3(1.0f));
		std::ofstream(res / "brickprimitives" / "lod0" / "3002.g", std::ios::binary) << BoxGeometry(glm::vec3(-4.0f), glm::vec3(4.0f));
		return res;
	}

	const char* LXFML5 = R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<LXFML versionMajor="5" versionMinor="0"><Bricks>
<Brick refID="0" designID="3001"><Part refID="0" designID="3001" materials="21,0"><Bone refID="0" transformation="1,0,0,0,1,0,0,0,1,10,0,0"/></Part></Brick>
<Brick refID="1" designID="3001"><Part refID="1" designID="3001" materials="40"><Bone refID="1" transformation="1,0,0,0,1,0,0,0,1,0,5,0"/></Part></Brick>
<Brick refID="2" designID="9999"><Part refID="2" designID="9999" materials="1"><Bone refID="2" transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
</Bricks></LXFML>)";
}

TEST(UgcCompression, GzipRoundTrip) {
	const std::string data(10000, 'x');
	const auto gz = ZCompression::Gzip(data);
	ASSERT_GE(gz.size(), 2u);
	EXPECT_EQ(static_cast<uint8_t>(gz[0]), 0x1f);
	EXPECT_EQ(static_cast<uint8_t>(gz[1]), 0x8b);
	EXPECT_EQ(ZCompression::Gunzip(gz), data);
	EXPECT_FALSE(ZCompression::Gunzip("not gzip"));
}

TEST(UgcBricks, ParsesGeometryAndRejectsBadData) {
	const auto geometry = UgcBricks::ParseGeometry(BoxGeometry(glm::vec3(0.0f), glm::vec3(1.0f)));
	ASSERT_TRUE(geometry);
	EXPECT_EQ(geometry->positions.size(), 24u);
	EXPECT_EQ(geometry->indices.size(), 36u);
	EXPECT_FALSE(UgcBricks::ParseGeometry("10GB"));
	auto broken = BoxGeometry(glm::vec3(0.0f), glm::vec3(1.0f));
	broken.resize(broken.size() - 4);
	EXPECT_FALSE(UgcBricks::ParseGeometry(broken));
}

TEST(UgcBricks, ParsesMaterials) {
	const auto materials = UgcBricks::ParseMaterials(R"(<Materials><Material MatID="21" Red="222" Green="0" Blue="13" Alpha="255"/><Material MatID="40" Red="238" Green="238" Blue="238" Alpha="150"/></Materials>)");
	ASSERT_EQ(materials.size(), 2u);
	EXPECT_EQ(materials.at(21).r, 222);
	EXPECT_FALSE(materials.at(21).Transparent());
	EXPECT_TRUE(materials.at(40).Transparent());
}

TEST(UgcBricks, ReadsStoredZipEntries) {
	// A zip with one stored file, "Materials.xml"
	const std::string name = "Materials.xml", content = "<Materials/>";
	std::string zip;
	const auto u16 = [&zip](uint16_t v) { zip.append(reinterpret_cast<const char*>(&v), 2); };
	const auto u32 = [&zip](uint32_t v) { zip.append(reinterpret_cast<const char*>(&v), 4); };
	u32(0x04034b50); u16(20); u16(0); u16(0); u16(0); u16(0); u32(0); u32(content.size()); u32(content.size()); u16(name.size()); u16(0);
	zip += name + content;
	const auto central = static_cast<uint32_t>(zip.size());
	u32(0x02014b50); u16(20); u16(20); u16(0); u16(0); u16(0); u16(0); u32(0); u32(content.size()); u32(content.size()); u16(name.size());
	u16(0); u16(0); u16(0); u16(0); u32(0); u32(0);
	zip += name;
	const auto centralSize = static_cast<uint32_t>(zip.size()) - central;
	u32(0x06054b50); u16(0); u16(0); u16(1); u16(1); u32(centralSize); u32(central); u16(0);
	EXPECT_EQ(UgcBricks::ReadZipEntry(zip, "materials.XML"), content);
	EXPECT_FALSE(UgcBricks::ReadZipEntry(zip, "Other.xml"));
}

TEST(UgcModel, ParsesLxfml5And4) {
	std::string error;
	const auto parts = UgcModel::ParseLxfml(LXFML5, error);
	ASSERT_EQ(parts.size(), 3u);
	EXPECT_EQ(parts[0].designId, 3001u);
	EXPECT_EQ(parts[0].materials, (std::vector<uint32_t>{ 21, 21 })); // 0: the part's first material
	EXPECT_FLOAT_EQ(parts[0].transform[3].x, 10.0f);

	const auto v4 = UgcModel::ParseLxfml(R"(<LXFML versionMajor="4"><Scene><Model><Group ax="0" ay="1" az="0" angle="90" tx="1" ty="0" tz="0">
		<Part designID="3001" materialID="21" ax="0" ay="1" az="0" angle="0" tx="0" ty="2" tz="0"/></Group></Model></Scene></LXFML>)", error);
	ASSERT_EQ(v4.size(), 1u);
	const auto origin = v4[0].transform * glm::vec4(0, 0, 0, 1);
	EXPECT_NEAR(origin.x, 1.0f, 1e-5f);
	EXPECT_NEAR(origin.y, 2.0f, 1e-5f);

	EXPECT_TRUE(UgcModel::ParseLxfml("<nope", error).empty());
	EXPECT_FALSE(error.empty());
}

TEST(UgcModel, BuildsOpaqueAndTransparentMeshes) {
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	library.SetMaterials({ { 21, { 222, 0, 13, 255 } }, { 40, { 238, 238, 238, 150 } } });
	std::string error;
	UgcModel::BuildOptions options;
	options.palette = UgcModel::ePalette::BRICKDB;
	options.colorVariation = 0.0f;
	const auto model = UgcModel::Build(UgcModel::ParseLxfml(LXFML5, error), library, options);
	EXPECT_EQ(model.bricks, 2u);
	EXPECT_EQ(model.missingDesigns, std::vector<uint32_t>{ 9999 });
	EXPECT_EQ(model.opaque.TriangleCount(), 12u);
	EXPECT_EQ(model.transparent.TriangleCount(), 12u);
	EXPECT_NEAR(model.opaque.colors[0].r, 222.0f / 255.0f, 1e-5f);
	EXPECT_NEAR(model.transparent.colors[0].a, 150.0f / 255.0f, 1e-5f);
	EXPECT_NEAR(model.opaque.positions[0].x, 10.0f, 1e-5f);
}

TEST(UgcModel, SplitsBigMeshes) {
	UgcModel::Mesh mesh;
	for (uint32_t i = 0; i < 30; i++) {
		mesh.positions.push_back(glm::vec3(static_cast<float>(i)));
		mesh.normals.push_back(glm::vec3(0, 1, 0));
		mesh.colors.push_back(glm::vec4(1.0f));
	}
	for (uint32_t i = 0; i + 2 < 30; i += 3) mesh.indices.insert(mesh.indices.end(), { i, i + 1, i + 2 });
	const auto pieces = UgcModel::Split(mesh, 9, 100);
	ASSERT_EQ(pieces.size(), 4u); // 10 triangles, 3 fit per piece
	size_t triangles = 0;
	for (const auto& piece : pieces) {
		EXPECT_LE(piece.positions.size(), 9u);
		triangles += piece.TriangleCount();
	}
	EXPECT_EQ(triangles, 10u);
}

TEST(UgcRender, RemovesWhatIsInsideAndDrawsIcons) {
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	library.SetMaterials({ { 21, { 222, 0, 13, 255 } } });
	std::string error;
	// A small box inside the big one: its faces can't be seen
	const auto parts = UgcModel::ParseLxfml(R"(<LXFML versionMajor="5"><Bricks>
		<Brick><Part designID="3002" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
		</Bricks></LXFML>)", error);
	auto model = UgcModel::Build(parts, library);
	ASSERT_EQ(model.opaque.TriangleCount(), 24u);
	const auto result = UgcRender::Optimize(model, UgcRender::OptimizeOptions{ 256, true });
	EXPECT_EQ(result.trianglesRemoved, 12u);
	EXPECT_EQ(model.opaque.TriangleCount(), 12u);
	EXPECT_EQ(model.opaque.positions.size(), 8u);

	const auto icon = UgcRender::RenderIcon(model, UgcRender::IconOptions{ 32, 2 });
	ASSERT_EQ(icon.rgba.size(), 32u * 32u * 4u);
	EXPECT_EQ(icon.rgba[3], 0);                        // a corner is background
	EXPECT_EQ(icon.rgba[(16 * 32 + 16) * 4 + 3], 255); // the middle is the box
	EXPECT_GT(icon.rgba[(16 * 32 + 16) * 4], icon.rgba[(16 * 32 + 16) * 4 + 1]); // red
	EXPECT_EQ(UgcRender::SphereDirections().size(), 42u);
}

TEST(UgcFormats, NifReadsBack) {
	UgcModel::Mesh opaque, transparent;
	opaque.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
	opaque.normals = { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } };
	opaque.colors = { { 1, 0, 0, 1 }, { 1, 0, 0, 1 }, { 1, 0, 0, 1 } };
	opaque.indices = { 0, 1, 2 };
	transparent = opaque;
	for (auto& color : transparent.colors) color.a = 0.5f;
	const auto nif = UgcFormats::WriteNif("SceneNode_Model", { { "S01_Opaque_Model", &opaque, false }, { "S01_Alpha_Model", &transparent, true } });
	ASSERT_TRUE(nif.starts_with("Gamebryo File Format, Version 20.3.0.9\n"));
	std::string error;
	const auto model = NifFile::Parse(nif, 0, error);
	ASSERT_TRUE(model) << error;
	EXPECT_TRUE(model->skipped.empty());
	ASSERT_EQ(model->meshes.size(), 2u);
	EXPECT_EQ(model->meshes[0].indices.size(), 3u);
	EXPECT_EQ(model->meshes[0].colors[0], 255);
	EXPECT_FALSE(model->meshes[0].material.alphaBlend);
	EXPECT_TRUE(model->meshes[1].material.alphaBlend);
	EXPECT_EQ(model->meshes[1].colors[3], 128);
	EXPECT_EQ(model->meshes[0].material.vertexColorMode, 2);
	EXPECT_TRUE(model->nodes.contains("SceneNode_Model"));
}

TEST(UgcFormats, ImagesAndChecksums) {
	UgcRender::Image image{ 2, 2, std::vector<uint8_t>(16, 0) };
	image.rgba[0] = 10; // red of the first pixel
	image.rgba[3] = 255;
	const auto png = UgcFormats::EncodePng(image);
	EXPECT_TRUE(png.starts_with("\x89PNG\r\n\x1a\n"));
	const auto dds = UgcFormats::EncodeDds(image);
	ASSERT_EQ(dds.size(), 128u + 16u);
	EXPECT_TRUE(dds.starts_with("DDS "));
	EXPECT_EQ(dds[128 + 2], 10); // stored BGRA
	EXPECT_EQ(UgcFormats::Md5Hex("abc"), "900150983cd24fb0d6963f7d28e17f72");
	EXPECT_NE(UgcFormats::ChecksumXml("abc").find("<Checksum><MD5>900150983cd24fb0d6963f7d28e17f72</MD5><Filesize>3</Filesize></Checksum>"), std::string::npos);
}

TEST(UgcModular, ParsesTheCdClientData) {
	EXPECT_EQ(UgcModular::ParseModuleLots("1:4713+1:4714+1:4715"), (std::vector<uint32_t>{ 4713, 4714, 4715 }));
	EXPECT_EQ(UgcModular::ParseModuleLots("1:8129;1:x;1:8130"), (std::vector<uint32_t>{ 8129, 8130 }));
	const auto build = UgcModular::ParseBuild(R"(<ModularBuild><topology><numberOfParts value="3" /><rootPart value="2" />
		<connection myPartid="2" myLocation="CP_A1" connectingPart="1" /><connection myPartid="1" myLocation="CP_B2" connectingPart="0"/></topology>
		<Placement><AdditionalModelRotation><Rotation w="0.707" x="0" y="-0.707" z="0" /></AdditionalModelRotation></Placement></ModularBuild>)");
	ASSERT_TRUE(build);
	EXPECT_EQ(build->rootPart, 2u);
	ASSERT_EQ(build->connections.size(), 2u);
	EXPECT_EQ(build->connections[1].location, "CP_B2");
	const auto connections = UgcModular::ParseModuleConnections(R"(<ModuleInfo moduleLOT="4714"><connection name="CP_B2"><translation x="0" y="0" z="5.2" /></connection></ModuleInfo>)");
	EXPECT_FLOAT_EQ(connections.at("CP_B2").z, 5.2f);
	EXPECT_FALSE(UgcModular::ParseBuild("<ModularBuild/>"));
}

TEST(UgcModular, PutsPartsOnTheirAttachPoints) {
	const auto triangle = [] {
		NifFile::Model nif;
		NifFile::Mesh mesh;
		mesh.positions = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
		mesh.indices = { 0, 1, 2 };
		nif.meshes.push_back(mesh);
		return nif;
	};
	UgcModular::BuildInfo build;
	build.rootPart = 2;
	build.connections = { { 2, "CP_A1", 1 }, { 1, "CP_B2", 0 } };
	std::vector<UgcModular::Module> modules(3);
	modules[0].partCode = 2; // bottom: node CP_A1 at y 5
	modules[0].nif = triangle();
	modules[0].nif.nodes["CP_A1"].translation = { 0, 5, 0 };
	modules[1].partCode = 1; // middle: no node, a connection offset of y 3 in the CDClient
	modules[1].nif = triangle();
	modules[1].connections["CP_B2"] = glm::vec3(0, 3, 0);
	modules[2].partCode = 0; // top: its own CP_B2 node at y 1 lines up with the middle's
	modules[2].nif = triangle();
	modules[2].nif.nodes["CP_B2"].translation = { 0, 1, 0 };
	std::string warnings;
	const auto model = UgcModular::Assemble(build, modules, warnings);
	EXPECT_TRUE(warnings.empty()) << warnings;
	ASSERT_EQ(model.opaque.positions.size(), 9u);
	EXPECT_FLOAT_EQ(model.opaque.positions[0].y, 0.0f);
	EXPECT_FLOAT_EQ(model.opaque.positions[3].y, 5.0f);
	EXPECT_FLOAT_EQ(model.opaque.positions[6].y, 7.0f); // 5 + 3 - 1
}

TEST(UgcStorage, WritesListsAndEvicts) {
	UgcStorage storage(TempFolder("storage"));
	std::string error;
	ASSERT_TRUE(storage.Write(UgcStorage::Kind::MODEL, 1001, { { "icon.png", std::string(100, 'a') } }, error)) << error;
	ASSERT_TRUE(storage.Write(UgcStorage::Kind::MODULAR, 2002, { { "icon.png", std::string(100, 'b') } }, error)) << error;
	ASSERT_TRUE(storage.Write(UgcStorage::Kind::MODEL, 1001, { { "icon.png", std::string(50, 'c') } }, error)) << error; // replaced
	EXPECT_TRUE(storage.File(UgcStorage::Kind::MODEL, 1001, "icon.png"));
	// The version before is kept to compare with
	const auto previous = storage.File(UgcStorage::Kind::MODEL, 1001, "previous.icon.png");
	ASSERT_TRUE(previous);
	EXPECT_EQ(std::filesystem::file_size(*previous), 100u);
	EXPECT_FALSE(storage.File(UgcStorage::Kind::MODEL, 1001, "../../etc/passwd"));
	EXPECT_EQ(storage.List().size(), 2u);
	std::filesystem::last_write_time(storage.Folder(UgcStorage::Kind::MODULAR, 2002), std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));
	const auto removed = storage.Evict(160);
	ASSERT_EQ(removed.size(), 1u);
	EXPECT_EQ(removed[0].id, 2002);
	EXPECT_TRUE(storage.File(UgcStorage::Kind::MODEL, 1001, "icon.png"));
	std::filesystem::remove_all(storage.GetRoot());
}

TEST(UgcPalette, ColorVariationMatchesLuToolbox) {
	const glm::vec3 red = *UgcPalette::Linear(21);
	// random 0.5 is the middle of the range: no change
	const auto same = UgcPalette::ApplyVariation(red, 7.0f, 0.5f);
	EXPECT_NEAR(same.r, red.r, 1e-5f);
	EXPECT_NEAR(same.b, red.b, 1e-5f);
	// The top of the range: value^(1/2.224) + variation/200, back to the power of 2.224; hue and saturation kept
	const auto brighter = UgcPalette::ApplyVariation(red, 7.0f, 1.0f);
	const float expected = std::pow(std::pow(red.r, 1.0f / 2.224f) + 0.035f, 2.224f);
	EXPECT_NEAR(brighter.r, expected, 1e-5f);
	EXPECT_NEAR(brighter.b / brighter.r, red.b / red.r, 1e-5f);
	const auto darker = UgcPalette::ApplyVariation(red, 7.0f, 0.0f);
	EXPECT_LT(darker.r, red.r);
	// Clamped to 0..1, and black turns grey rather than staying black
	EXPECT_LE(UgcPalette::ApplyVariation(glm::vec3(1.0f), 100.0f, 1.0f).r, 1.0f);
	EXPECT_GT(UgcPalette::ApplyVariation(glm::vec3(0.0f), 10.0f, 1.0f).g, 0.0f);
	EXPECT_FLOAT_EQ(UgcPalette::ApplyVariation(red, 0.0f, 1.0f).r, red.r);

	// Per color amounts, aliases, transparency, glow and the icon's corrections
	EXPECT_FLOAT_EQ(UgcPalette::VariationScale(26), 0.4f);
	EXPECT_FLOAT_EQ(UgcPalette::VariationScale(5), 1.0f);
	EXPECT_EQ(*UgcPalette::Linear(0), *UgcPalette::Linear(26));
	EXPECT_EQ(*UgcPalette::Linear(293), *UgcPalette::Linear(43));
	EXPECT_TRUE(UgcPalette::IsTransparent(40));
	EXPECT_FALSE(UgcPalette::IsTransparent(21));
	EXPECT_TRUE(UgcPalette::Glow(9013).has_value());
	EXPECT_FALSE(UgcPalette::Glow(21).has_value());
	EXPECT_TRUE(UgcPalette::IsMetallic(309));
	EXPECT_FALSE(UgcPalette::Linear(123456).has_value());
	EXPECT_NEAR(UgcPalette::LinearToSrgb(*UgcPalette::Linear(1, true)).r, 0.7f, 1e-5f);
	EXPECT_NEAR(UgcPalette::LinearToSrgb(red).r * 255.0f, 222.0f, 0.5f); // LDD's bright red
	EXPECT_NEAR(UgcPalette::SrgbToLinear(UgcPalette::LinearToSrgb(0.3f)), 0.3f, 1e-5f);
}

TEST(UgcPalette, BrickRandomIsStableAndSpread) {
	EXPECT_EQ(UgcPalette::BrickRandom(7, 3, 21), UgcPalette::BrickRandom(7, 3, 21));
	EXPECT_NE(UgcPalette::BrickRandom(7, 3, 21), UgcPalette::BrickRandom(7, 4, 21));
	EXPECT_NE(UgcPalette::BrickRandom(7, 3, 21), UgcPalette::BrickRandom(8, 3, 21));
	EXPECT_NE(UgcPalette::BrickRandom(7, 3, 21), UgcPalette::BrickRandom(7, 3, 23));
	double sum = 0.0;
	float low = 1.0f, high = 0.0f;
	for (uint32_t brick = 0; brick < 10000; brick++) {
		const float value = UgcPalette::BrickRandom(1, brick, 1);
		ASSERT_GE(value, 0.0f);
		ASSERT_LT(value, 1.0f);
		sum += value;
		low = std::min(low, value);
		high = std::max(high, value);
	}
	EXPECT_NEAR(sum / 10000.0, 0.5, 0.02); // uniform, like random.uniform
	EXPECT_LT(low, 0.01f);
	EXPECT_GT(high, 0.99f);
}

TEST(UgcModel, ColorsLikeLuToolbox) {
	const auto res = MakeRes();
	std::filesystem::create_directories(res / "brickprimitives" / "lod1");
	std::ofstream(res / "brickprimitives" / "lod1" / "3001.g", std::ios::binary) << BoxGeometry(glm::vec3(0.0f), glm::vec3(1.0f));
	UgcBricks::BrickLibrary library(res, 0);
	std::string error;
	// Three red bricks, a transparent one, a red and transparent one, an unknown color
	const auto parts = UgcModel::ParseLxfml(R"(<LXFML versionMajor="5"><Bricks>
		<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,2,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,4,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="40"><Bone transformation="1,0,0,0,1,0,0,0,1,6,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="21,40"><Bone transformation="1,0,0,0,1,0,0,0,1,8,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="987654"><Bone transformation="1,0,0,0,1,0,0,0,1,10,0,0"/></Part></Brick>
		</Bricks></LXFML>)", error);
	ASSERT_EQ(parts.size(), 6u);

	UgcModel::BuildOptions plain;
	plain.colorVariation = 0.0f;
	const auto flat = UgcModel::Build(parts, library, plain);
	EXPECT_EQ(flat.transparent.TriangleCount(), 12u); // only the all-transparent brick
	EXPECT_EQ(flat.transparentBricks, std::vector<size_t>{ 0 });
	EXPECT_EQ(flat.opaque.TriangleCount(), 60u);
	EXPECT_NEAR(flat.opaque.colors[0].r * 255.0f, 222.0f, 0.5f);
	EXPECT_FLOAT_EQ(flat.opaque.colors[0].a, 1.0f);
	EXPECT_NEAR(flat.transparent.colors[0].a, 0.5882f, 1e-4f);
	const auto black = UgcPalette::LinearToSrgb(*UgcPalette::Linear(26));
	EXPECT_NEAR(flat.opaque.colors[4 * 8].r, black.r, 1e-5f); // the unknown color is black
	EXPECT_TRUE(flat.opaque.glow.empty());

	UgcModel::BuildOptions varied;
	varied.seed = 42;
	const auto a = UgcModel::Build(parts, library, varied);
	const auto again = UgcModel::Build(parts, library, varied);
	EXPECT_EQ(a.opaque.colors, again.opaque.colors); // the same every time
	// Each brick has one shift for all its vertices, different between bricks of the same color
	EXPECT_EQ(a.opaque.colors[0], a.opaque.colors[7]);
	EXPECT_NE(a.opaque.colors[0], a.opaque.colors[8]);
	EXPECT_NE(a.opaque.colors[8], a.opaque.colors[16]);
	// Within 5% x 1.4 (red's own amount) of the plain color in LU Toolbox's gamma
	for (size_t brick = 0; brick < 3; brick++) {
		const float value = UgcPalette::SrgbToLinear(a.opaque.colors[brick * 8].r);
		const float base = UgcPalette::Linear(21)->r;
		EXPECT_LE(std::abs(std::pow(value, 1.0f / 2.224f) - std::pow(base, 1.0f / 2.224f)), 0.035f + 1e-4f);
	}
	// The same brick gets the same color in another LOD
	varied.lod = 1;
	const auto lod1 = UgcModel::Build(parts, library, varied);
	EXPECT_EQ(lod1.opaque.colors[8], a.opaque.colors[8]);
	// Another model (seed) gets other shifts
	varied.lod = 0;
	varied.seed = 43;
	EXPECT_NE(UgcModel::Build(parts, library, varied).opaque.colors[0], a.opaque.colors[0]);

	// The icon: its corrections, and no variation
	UgcModel::BuildOptions icon;
	icon.icon = true;
	icon.colorVariation = 0.0f;
	const auto white = UgcModel::Build(UgcModel::ParseLxfml(R"(<LXFML versionMajor="5"><Bricks><Brick><Part designID="3001" materials="1">
		<Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick></Bricks></LXFML>)", error), library, icon);
	EXPECT_NEAR(white.opaque.colors[0].r, 0.7f, 1e-5f);
}

TEST(UgcModel, LodRangesLikeLuToolbox) {
	const UgcModel::LodDistances d;
	using Ranges = std::vector<std::pair<float, float>>;
	EXPECT_EQ(UgcModel::LodRanges({ 0, 2 }, d), (Ranges{ { 0.0f, 100.0f }, { 100.0f, 10000.0f } }));
	EXPECT_EQ(UgcModel::LodRanges({ 0 }, d), (Ranges{ { 0.0f, 10000.0f } }));
	EXPECT_EQ(UgcModel::LodRanges({ 0, 1, 2 }, d), (Ranges{ { 0.0f, 50.0f }, { 50.0f, 100.0f }, { 100.0f, 10000.0f } }));
	EXPECT_EQ(UgcModel::LodRanges({ 0, 1 }, d), (Ranges{ { 0.0f, 50.0f }, { 50.0f, 10000.0f } }));
	EXPECT_EQ(UgcModel::LodRanges({ 0, 2, 3 }, d), (Ranges{ { 0.0f, 100.0f }, { 100.0f, 280.0f }, { 280.0f, 10000.0f } }));
}

TEST(UgcModel, DividesAlongTheLongestSide) {
	// Two separate strips of triangles far apart on x: divided between them, each kept whole
	UgcModel::Mesh mesh;
	for (int cluster = 0; cluster < 2; cluster++) {
		for (uint32_t i = 0; i < 40; i++) {
			mesh.positions.push_back(glm::vec3(cluster * 100.0f + static_cast<float>(i % 2), static_cast<float>(i / 2), 0.0f));
			mesh.normals.push_back(glm::vec3(0, 0, 1));
			mesh.colors.push_back(glm::vec4(1.0f));
		}
		const uint32_t base = cluster * 40;
		for (uint32_t i = 0; i + 2 < 40; i++) mesh.indices.insert(mesh.indices.end(), { base + i, base + i + 1, base + i + 2 });
	}
	const auto pieces = UgcModel::Divide(mesh, 50, 1000);
	ASSERT_EQ(pieces.size(), 2u);
	for (const auto& piece : pieces) {
		EXPECT_EQ(piece.positions.size(), 40u);
		EXPECT_EQ(piece.TriangleCount(), 38u);
	}
	EXPECT_EQ(UgcModel::Divide(mesh, 100, 1000).size(), 1u);
}

TEST(UgcModel, SplitsTransparentBricksApart) {
	// Two boxes in one mesh, one shape each (LU Toolbox leaves transparent bricks uncombined)
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	std::string error;
	const auto model = UgcModel::Build(UgcModel::ParseLxfml(R"(<LXFML versionMajor="5"><Bricks>
		<Brick><Part designID="3001" materials="40"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="43"><Bone transformation="1,0,0,0,1,0,0,0,1,5,0,0"/></Part></Brick>
		</Bricks></LXFML>)", error), library);
	ASSERT_EQ(model.transparentBricks.size(), 2u);
	const auto pieces = UgcModel::SplitAt(model.transparent, model.transparentBricks);
	ASSERT_EQ(pieces.size(), 2u);
	EXPECT_EQ(pieces[0].positions.size(), 8u);
	EXPECT_EQ(pieces[1].TriangleCount(), 12u);
	EXPECT_NEAR(pieces[1].positions[0].x, 5.0f, 1e-5f);
}

TEST(UgcFormats, LodNifReadsBack) {
	UgcModel::Mesh near, far;
	near.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 } };
	near.normals.assign(4, { 0, 0, 1 });
	near.colors.assign(4, { 1, 0, 0, 1 });
	near.indices = { 0, 1, 2, 1, 3, 2 };
	far = near;
	far.indices = { 0, 1, 2 };
	const auto nif = UgcFormats::WriteLodNif("SceneNode_Model", { { "S01_Opaque_Model", false, { { 0.0f, 100.0f, "LOD_0", { &near } }, { 100.0f, 10000.0f, "LOD_2", { &far } } } } });
	std::string error;
	const auto lod0 = NifFile::Parse(nif, 0, error);
	ASSERT_TRUE(lod0) << error;
	EXPECT_TRUE(lod0->skipped.empty());
	ASSERT_EQ(lod0->meshes.size(), 1u);
	EXPECT_EQ(lod0->meshes[0].indices.size(), 6u);
	EXPECT_TRUE(lod0->nodes.contains("S01_Opaque_Model"));
	EXPECT_TRUE(lod0->nodes.contains("LOD_0"));
	const auto lod1 = NifFile::Parse(nif, 1, error);
	ASSERT_TRUE(lod1) << error;
	ASSERT_EQ(lod1->meshes.size(), 1u);
	EXPECT_EQ(lod1->meshes[0].indices.size(), 3u);
}

TEST(UgcRender, AmbientOcclusionUnderARoof) {
	// A floor vertex under a low roof is dark, one out in the open is lit; nothing is hit past the distance
	UgcModel::Mesh points;
	points.positions = { { 0, 0, 0 }, { 50, 0, 0 } };
	points.normals = { { 0, 1, 0 }, { 0, 1, 0 } };
	UgcModel::Mesh roof;
	roof.positions = { { -10, 1, -10 }, { 10, 1, -10 }, { -10, 1, 10 }, { 10, 1, 10 } };
	roof.normals.assign(4, { 0, -1, 0 });
	roof.indices = { 0, 1, 2, 1, 3, 2 };
	const auto ao = UgcRender::AmbientOcclusion(points, roof, 5.0f, 64);
	ASSERT_EQ(ao.size(), 2u);
	EXPECT_LT(ao[0], 0.2f);
	EXPECT_FLOAT_EQ(ao[1], 1.0f);
	EXPECT_FLOAT_EQ(UgcRender::AmbientOcclusion(points, roof, 0.5f, 64)[0], 1.0f);

	// Baking darkens the colors of occluded vertices only, and glow lights them up again
	UgcModel::Model model;
	model.opaque = roof;
	model.opaque.colors.assign(4, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
	UgcModel::Mesh floor = roof;
	for (auto& p : floor.positions) p.y = 0.0f;
	floor.normals.assign(4, { 0, 1, 0 });
	floor.colors.assign(4, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
	model.opaque.Append(floor);
	UgcRender::BakeAo(model, UgcRender::AoOptions{});
	EXPECT_LT(model.opaque.colors[5].r, 0.8f);
	EXPECT_EQ(model.opaque.colors[5].a, 1.0f);
}

TEST(UgcThrottle, KeepsUnderTheBudget) {
	int from = -1, to = -1;
	EXPECT_TRUE(UgcThrottle::ParseHours("22-6", from, to));
	EXPECT_TRUE(UgcThrottle::InHours(23, from, to));
	EXPECT_TRUE(UgcThrottle::InHours(3, from, to));
	EXPECT_FALSE(UgcThrottle::InHours(12, from, to));
	EXPECT_FALSE(UgcThrottle::ParseHours("", from, to));
	EXPECT_FALSE(UgcThrottle::ParseHours("25-3", from, to));
	EXPECT_FALSE(UgcThrottle::InHours(3, -1, -1));

	// 0.6 s of CPU work at a quarter of a CPU takes at least (0.6 - the burst) / 0.25 s
	UgcThrottle::SetBudget(0.25);
	UgcThrottle::Begin();
	const auto start = std::chrono::steady_clock::now();
	const double cpuStart = UgcThrottle::ThreadCpuSeconds();
	volatile double sink = 0.0;
	while (UgcThrottle::ThreadCpuSeconds() - cpuStart < 0.6) {
		for (int i = 0; i < 10000; i++) sink = sink + std::sqrt(static_cast<double>(i));
		UgcThrottle::Checkpoint();
	}
	const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	UgcThrottle::SetBudget(0.0);
	EXPECT_GE(wall, 1.2);
	EXPECT_GT(UgcThrottle::GetStats().sleptMs, 0u);
}

TEST(UgcJobs, MakesLodsStatsAndIcons) {
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	UgcJobs::Settings settings;
	settings.optimize.resolution = 128;
	settings.ao.samples = 8;
	settings.icon.size = 32;
	settings.icon.supersample = 1;
	settings.icon.ao.samples = 4;
	const auto outcome = UgcJobs::ProcessModel(LXFML5, library, settings, 99);
	ASSERT_TRUE(outcome.ok) << outcome.error;
	for (const auto* name : { "model.nif.gz", "model.nif.checksum", "model.noao.nif.gz", "icon.png", "icon.dds.gz", "stats.json" }) {
		EXPECT_TRUE(outcome.files.contains(name)) << name;
	}
	EXPECT_NE(outcome.stats.find("\"lods\""), std::string::npos);
	EXPECT_NE(outcome.stats.find("\"opaqueAfter\""), std::string::npos);
	// Stored compressed only; the LXFML is served from the database
	EXPECT_FALSE(outcome.files.contains("model.nif"));
	EXPECT_FALSE(outcome.files.contains("model.lxfml.gz"));
	const auto nifBytes = *ZCompression::Gunzip(outcome.files.at("model.nif.gz"));
	std::string error;
	const auto nif = NifFile::Parse(nifBytes, 0, error);
	ASSERT_TRUE(nif) << error;
	EXPECT_TRUE(nif->nodes.contains("S01_Opaque_Model"));
	EXPECT_TRUE(nif->nodes.contains("S01_Alpha_Model"));
	EXPECT_TRUE(nif->nodes.contains("LOD_0"));
	const auto far = NifFile::Parse(nifBytes, 1, error);
	ASSERT_TRUE(far) << error;
	EXPECT_TRUE(far->nodes.contains("LOD_2"));
	// The icon is the .nif's LOD 0, drawn with the icon camera and no occlusion of its own
	auto iconOptions = settings.icon;
	iconOptions.ao.enabled = false;
	EXPECT_EQ(outcome.files.at("icon.png"), UgcFormats::EncodePng(UgcRender::RenderIcon(UgcModel::FromNif(*nif), iconOptions)));
	// The same colors when made again
	EXPECT_EQ(UgcJobs::ProcessModel(LXFML5, library, settings, 99).files.at("model.nif.checksum"), outcome.files.at("model.nif.checksum"));

	settings.maxBricks = 2;
	const auto tooBig = UgcJobs::ProcessModel(LXFML5, library, settings, 99);
	EXPECT_FALSE(tooBig.ok);
	EXPECT_NE(tooBig.error.find("max_model_bricks"), std::string::npos);

	EXPECT_EQ(UgcJobs::CountParts(LXFML5), 3u);
	EXPECT_GT(UgcJobs::EstimateMemory(1000, settings), UgcJobs::EstimateMemory(10, settings));
}

TEST(UgcModularKey, SameModulesSameKey) {
	// However the modules are written or ordered, the combination is the same; its files are stored once
	EXPECT_EQ(UgcModularKey::Normalize("1:4715+1:4713+1:4714"), "4713-4714-4715");
	EXPECT_EQ(UgcModularKey::Normalize("1:4713;1:4714,1:4715"), "4713-4714-4715");
	EXPECT_EQ(UgcModularKey::Normalize("4714+4715+4713+1:4713"), "4713-4714-4715");
	EXPECT_EQ(UgcModularKey::Normalize(""), "");
	EXPECT_EQ(UgcModularKey::Normalize("1:abc+"), "");
	EXPECT_NE(UgcModularKey::Normalize("1:4713+1:4714+1:4716"), UgcModularKey::Normalize("1:4713+1:4714+1:4715"));
	const auto id = UgcModularKey::StorageId("4713-4714-4715");
	EXPECT_GT(id, 0);
	EXPECT_EQ(id, UgcModularKey::StorageId(UgcModularKey::Normalize("1:4715+1:4714+1:4713")));
	EXPECT_NE(id, UgcModularKey::StorageId("4713-4714-4716"));

	// Two builds of the same modules find the one set of files
	UgcStorage storage(TempFolder("combo"));
	std::string error;
	ASSERT_TRUE(storage.Write(UgcStorage::Kind::MODULAR, id, { { "icon.png", "png" } }, error)) << error;
	EXPECT_TRUE(storage.File(UgcStorage::Kind::MODULAR, UgcModularKey::StorageId(UgcModularKey::Normalize("1:4713+1:4714+1:4715")), "icon.png"));
	EXPECT_TRUE(storage.File(UgcStorage::Kind::MODULAR, UgcModularKey::StorageId(UgcModularKey::Normalize("1:4714+1:4715+1:4713")), "icon.png"));
	std::filesystem::remove_all(storage.GetRoot());
}

TEST(UgcDebounce, WaitsForTheQuietPeriod) {
	EXPECT_EQ(UgcDebounce::ProcessAfter(1000, 120), 1120);
	EXPECT_EQ(UgcDebounce::ProcessAfter(1000, 0), 0);
	EXPECT_EQ(UgcDebounce::ProcessAfter(1000, -5), 0);
	EXPECT_FALSE(UgcDebounce::Due(1120, 1100, false)); // saved 100 s ago: still quiet
	EXPECT_TRUE(UgcDebounce::Due(1120, 1120, false));
	EXPECT_TRUE(UgcDebounce::Due(1120, 1100, true));   // a client asked for it
	EXPECT_TRUE(UgcDebounce::Due(0, 5, false));        // expedited or saved without a wait
	// A new save starts the wait again
	const auto first = UgcDebounce::ProcessAfter(1000, 120), second = UgcDebounce::ProcessAfter(1100, 120);
	EXPECT_FALSE(UgcDebounce::Due(std::max(first, second), 1150, false));
}

TEST(UgcIconParams, OneListDrivesEverything) {
	// Every parameter has a setting, a range holding its default, and something it changes
	for (const auto& param : UgcIconParams::List()) {
		EXPECT_TRUE(param.setting.starts_with("icon_")) << param.key;
		EXPECT_LE(param.min, param.defaultValue) << param.key;
		EXPECT_GE(param.max, param.defaultValue) << param.key;
		EXPECT_TRUE(param.apply) << param.key;
		EXPECT_EQ(UgcIconParams::Find(param.key), &param);
	}
	const auto values = UgcIconParams::Parse(R"({"yaw":10,"pitch":200,"margin":0,"offsetX":0.1,"unknown":1,"fov":"wide","exposure":1.5})");
	EXPECT_FLOAT_EQ(values.at("yaw"), 10.0f);
	EXPECT_FLOAT_EQ(values.at("pitch"), 89.0f); // clamped
	EXPECT_FLOAT_EQ(values.at("margin"), 0.5f);
	EXPECT_FALSE(values.contains("fov"));      // not a number
	EXPECT_FALSE(values.contains("unknown"));
	EXPECT_TRUE(UgcIconParams::Parse("not json").empty());
	EXPECT_EQ(UgcIconParams::Parse(UgcIconParams::ToJson(values)), values);

	// Settings, then values over them
	auto options = UgcIconParams::FromSettings([](const std::string& key) -> std::optional<std::string> {
		if (key == "icon_fov") return "33";
		if (key == "icon_ambient") return "nonsense";
		return std::nullopt;
	});
	EXPECT_FLOAT_EQ(options.fovDegrees, 33.0f);
	EXPECT_FLOAT_EQ(options.ambient, UgcIconParams::Find("ambient")->defaultValue);
	UgcIconParams::Apply(options, values);
	EXPECT_FLOAT_EQ(options.yawDegrees, 10.0f);
	EXPECT_FLOAT_EQ(options.fovDegrees, 33.0f); // not in the values: the setting stays
	EXPECT_FLOAT_EQ(options.exposure, 1.5f);
	EXPECT_FLOAT_EQ(options.offsetX, 0.1f);
	UgcIconParams::Apply(options, { { "aoStrength", 0.5f } });
	EXPECT_TRUE(options.ao.enabled);

	// A car or rocket: the settings with its preset and combination values
	UgcJobs::Settings settings;
	UgcJobs::ModularInput input;
	input.iconValues = { { "yaw", 5.0f } };
	EXPECT_FLOAT_EQ(UgcJobs::ModularIconOptions(input, settings).yawDegrees, 5.0f);
	EXPECT_EQ(UgcIconParams::KindTarget(UgcIconParams::BuildKind(6)), "kind:build6");
	EXPECT_EQ(UgcIconParams::ModelTarget(12), "model:12");
	EXPECT_EQ(UgcIconParams::CombinationTarget("1-2"), "combo:1-2");

	// Exposure brightens, contrast spreads
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	std::string error;
	const auto box = UgcModel::Build(UgcModel::ParseLxfml(R"(<LXFML versionMajor="5"><Bricks><Brick><Part designID="3001" materials="194">
		<Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick></Bricks></LXFML>)", error), library);
	const auto mean = [](const UgcRender::Image& image) {
		double sum = 0, count = 0;
		for (size_t i = 0; i < image.rgba.size(); i += 4) {
			if (image.rgba[i + 3] < 128) continue;
			sum += image.rgba[i];
			count++;
		}
		return sum / std::max(count, 1.0);
	};
	UgcRender::IconOptions dim{ 32, 1 }, bright{ 32, 1 };
	bright.exposure = 2.0f;
	EXPECT_GT(mean(UgcRender::RenderIcon(box, bright)), mean(UgcRender::RenderIcon(box, dim)) + 10.0);

	// An offset moves the drawn model by that share of the icon
	UgcModel::Model model;
	model = UgcModel::Build(UgcModel::ParseLxfml(R"(<LXFML versionMajor="5"><Bricks><Brick><Part designID="3001" materials="21">
		<Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick></Bricks></LXFML>)", error), library);
	const auto centroid = [](const UgcRender::Image& image) {
		double sum = 0, count = 0;
		for (int y = 0; y < image.height; y++) for (int x = 0; x < image.width; x++) {
			const auto a = image.rgba[(static_cast<size_t>(y) * image.width + x) * 4 + 3];
			sum += x * a;
			count += a;
		}
		return count > 0 ? sum / count : -1.0;
	};
	UgcRender::IconOptions plain{ 64, 1 };
	plain.margin = 2.0f;
	auto shifted = plain;
	shifted.offsetX = 0.25f;
	EXPECT_NEAR(centroid(UgcRender::RenderIcon(model, shifted)) - centroid(UgcRender::RenderIcon(model, plain)), 16.0, 1.0);
}
