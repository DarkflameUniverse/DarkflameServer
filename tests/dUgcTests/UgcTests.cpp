#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>

#include <glm/gtc/matrix_transform.hpp>

#include "Game.h"
#include "NifFile.h"
#include "UgcBricks.h"
#include "UgcFormats.h"
#include "UgcModel.h"
#include "UgcModular.h"
#include "UgcRender.h"
#include "UgcStorage.h"
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
		auto path = std::filesystem::temp_directory_path() / ("dlu_ugc_test_" + name + "_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
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
	const auto model = UgcModel::Build(UgcModel::ParseLxfml(LXFML5, error), library);
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
	const auto result = UgcRender::Optimize(model, UgcRender::OptimizeOptions{ 256, true, true, 0.6f });
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
	EXPECT_FALSE(storage.File(UgcStorage::Kind::MODEL, 1001, "../../etc/passwd"));
	EXPECT_EQ(storage.List().size(), 2u);
	std::filesystem::last_write_time(storage.Folder(UgcStorage::Kind::MODULAR, 2002), std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));
	const auto removed = storage.Evict(60);
	ASSERT_EQ(removed.size(), 1u);
	EXPECT_EQ(removed[0].id, 2002);
	EXPECT_TRUE(storage.File(UgcStorage::Kind::MODEL, 1001, "icon.png"));
	std::filesystem::remove_all(storage.GetRoot());
}
