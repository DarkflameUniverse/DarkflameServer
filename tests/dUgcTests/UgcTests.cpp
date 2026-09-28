#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

#include <glm/gtc/matrix_transform.hpp>

#include "Game.h"
#include "NifFile.h"
#include "UgcBricks.h"
#include "UgcFormats.h"
#include "UgcModel.h"
#include "UgcJobs.h"
#include "IUgc.h"
#include "UgcIconParams.h"
#include "UgcIconPose.h"
#include "UgcKeys.h"
#include "UgcModular.h"
#include "UgcPalette.h"
#include "UgcRender.h"
#include "UgcStorage.h"
#include "UgcThrottle.h"
#include "Sd0.h"
#include "ZCompression.h"
#include "json.hpp"

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
	const auto materials = UgcBricks::ParseMaterials(R"(<Materials><Material MatID="21" Red="222" Green="0" Blue="13" Alpha="255"/><Material MatID="40" Red="238" Green="238" Blue="238" Alpha="150" MaterialType="shinySteel"/></Materials>)");
	ASSERT_EQ(materials.size(), 2u);
	EXPECT_EQ(materials.at(21).r, 222);
	EXPECT_EQ(materials.at(21).type, "");
	EXPECT_EQ(materials.at(40).type, "shinySteel");
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

// A color LU Toolbox's palette doesn't have but the client's Materials.xml does (one added to the brick database) is
// drawn in its Materials.xml color; one neither knows is LU Toolbox's black
TEST(UgcModel, ColorsOnlyInMaterialsXmlAreNotBlack) {
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	constexpr uint32_t ADDED = 50001;
	ASSERT_FALSE(UgcPalette::Linear(ADDED));
	library.SetMaterials({ { ADDED, { 0, 200, 100, 255 } } });
	std::string error;
	UgcModel::BuildOptions options;
	options.palette = UgcModel::ePalette::LU_TOOLBOX;
	options.colorVariation = 0.0f;
	const auto brick = [&error](uint32_t material) {
		return UgcModel::ParseLxfml("<LXFML versionMajor=\"5\"><Bricks><Brick><Part designID=\"3001\" materials=\"" + std::to_string(material) +
			"\"><Bone transformation=\"1,0,0,0,1,0,0,0,1,0,0,0\"/></Part></Brick></Bricks></LXFML>", error);
	};
	const auto added = UgcModel::Build(brick(ADDED), library, options);
	ASSERT_FALSE(added.opaque.colors.empty());
	EXPECT_NEAR(added.opaque.colors[0].g, 200.0f / 255.0f, 1e-3f);
	EXPECT_NEAR(added.opaque.colors[0].r, 0.0f, 1e-3f);
	ASSERT_FALSE(UgcPalette::Linear(50002));
	const auto unknown = UgcModel::Build(brick(50002), library, options);
	ASSERT_FALSE(unknown.opaque.colors.empty());
	const auto black = UgcModel::Build(brick(UgcPalette::FALLBACK_ID), library, options);
	ASSERT_FALSE(black.opaque.colors.empty());
	EXPECT_EQ(unknown.opaque.colors[0], black.opaque.colors[0]);
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
	// Every shape blends by its vertex alpha, as the game's own brick models do
	EXPECT_TRUE(model->meshes[0].material.alphaBlend);
	EXPECT_EQ(model->meshes[0].colors[3], 255);
	EXPECT_TRUE(model->meshes[1].material.alphaBlend);
	EXPECT_EQ(UgcModel::FromNif(*model).transparent.TriangleCount(), 1u);
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
	ASSERT_EQ(dds.size(), 128u + 16u); // one DXT5 block
	EXPECT_TRUE(dds.starts_with("DDS "));
	EXPECT_EQ(UgcFormats::Md5Hex("abc"), "900150983cd24fb0d6963f7d28e17f72");
	EXPECT_NE(UgcFormats::ChecksumXml("abc").find("<Checksum><MD5>900150983cd24fb0d6963f7d28e17f72</MD5><Filesize>3</Filesize></Checksum>"), std::string::npos);
	std::string md5;
	uint32_t size{};
	ASSERT_TRUE(UgcFormats::ReadChecksumXml(UgcFormats::ChecksumXml("abc"), md5, size));
	EXPECT_EQ(md5, "900150983cd24fb0d6963f7d28e17f72");
	EXPECT_EQ(size, 3u);
	EXPECT_FALSE(UgcFormats::ReadChecksumXml("<Checksum><MD5>abc</MD5><Filesize>3</Filesize></Checksum>", md5, size));
	EXPECT_FALSE(UgcFormats::ReadChecksumXml("<Checksum><MD5>900150983cd24fb0d6963f7d28e17f72</MD5><Filesize>x</Filesize></Checksum>", md5, size));
}

namespace {
	uint32_t U32(const std::string& data, size_t at) {
		uint32_t v{};
		std::memcpy(&v, data.data() + at, 4);
		return v;
	}

	// A DXT5 block back to RGBA (the reference decoding, for the tests)
	std::array<uint8_t, 64> DecodeDxt5Block(const uint8_t* b) {
		std::array<uint8_t, 64> out{};
		std::array<int, 8> alpha{ b[0], b[1] };
		for (int i = 2; i < 8; i++) alpha[i] = b[0] > b[1] ? ((8 - i) * b[0] + (i - 1) * b[1]) / 7 : (i < 6 ? ((6 - i) * b[0] + (i - 1) * b[1]) / 5 : (i == 6 ? 0 : 255));
		uint64_t abits = 0;
		for (int i = 0; i < 6; i++) abits |= static_cast<uint64_t>(b[2 + i]) << (8 * i);
		const uint16_t c0 = b[8] | (b[9] << 8), c1 = b[10] | (b[11] << 8);
		const auto rgb = [](uint16_t v) { return std::array<int, 3>{ ((v >> 11) & 31) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31 }; };
		const auto a = rgb(c0), z = rgb(c1);
		std::array<std::array<int, 3>, 4> pal{ a, z };
		for (int c = 0; c < 3; c++) {
			pal[2][c] = c0 > c1 ? (2 * a[c] + z[c]) / 3 : (a[c] + z[c]) / 2;
			pal[3][c] = c0 > c1 ? (a[c] + 2 * z[c]) / 3 : 0;
		}
		const uint32_t bits = b[12] | (b[13] << 8) | (b[14] << 16) | (static_cast<uint32_t>(b[15]) << 24);
		for (int i = 0; i < 16; i++) {
			for (int c = 0; c < 3; c++) out[i * 4 + c] = static_cast<uint8_t>(pal[(bits >> (2 * i)) & 3][c]);
			out[i * 4 + 3] = static_cast<uint8_t>(alpha[(abits >> (3 * i)) & 7]);
		}
		return out;
	}
}

// Icons are written like the client's own 128x128 ones: DXT5, no mipmaps, flags 0x81007, the linear size, caps 0x1000
TEST(UgcFormats, DdsIsDxt5LikeTheClientsIcons) {
	UgcRender::Image image{ 128, 128, std::vector<uint8_t>(128 * 128 * 4, 0) };
	for (int y = 0; y < 128; y++) {
		for (int x = 0; x < 128; x++) {
			auto* p = &image.rgba[(y * 128 + x) * 4];
			const bool inside = x >= 32 && x < 96 && y >= 32 && y < 96;
			p[0] = static_cast<uint8_t>(x * 2);
			p[1] = static_cast<uint8_t>(y * 2);
			p[2] = 90;
			p[3] = inside ? 255 : 0;
		}
	}
	const auto dds = UgcFormats::EncodeDds(image);
	ASSERT_EQ(dds.size(), 128u + 32u * 32u * 16u);
	EXPECT_EQ(U32(dds, 4), 124u);
	EXPECT_EQ(U32(dds, 8), 0x81007u);
	EXPECT_EQ(U32(dds, 12), 128u);
	EXPECT_EQ(U32(dds, 16), 128u);
	EXPECT_EQ(U32(dds, 20), 16384u); // linear size
	EXPECT_EQ(U32(dds, 28), 0u);     // no mipmaps
	EXPECT_EQ(U32(dds, 80), 0x4u);   // four CC
	EXPECT_EQ(dds.substr(84, 4), "DXT5");
	EXPECT_EQ(U32(dds, 108), 0x1000u);

	// Decoded, the visible pixels are close to the source and the background stays transparent
	int worst = 0;
	for (int by = 0; by < 32; by++) {
		for (int bx = 0; bx < 32; bx++) {
			const auto block = DecodeDxt5Block(reinterpret_cast<const uint8_t*>(dds.data()) + 128 + (by * 32 + bx) * 16);
			for (int i = 0; i < 16; i++) {
				const auto* src = &image.rgba[((by * 4 + i / 4) * 128 + bx * 4 + i % 4) * 4];
				EXPECT_EQ(block[i * 4 + 3], src[3]);
				if (src[3] == 0) continue;
				for (int c = 0; c < 3; c++) worst = std::max(worst, std::abs(block[i * 4 + c] - src[c]));
			}
		}
	}
	EXPECT_LE(worst, 12);

	// A flat block is one color, however it's stored
	std::array<uint8_t, 64> flat{};
	for (int i = 0; i < 16; i++) flat[i * 4] = 200, flat[i * 4 + 1] = 40, flat[i * 4 + 2] = 10, flat[i * 4 + 3] = 128;
	const auto encoded = UgcFormats::EncodeDxt5Block(flat);
	const auto decoded = DecodeDxt5Block(encoded.data());
	for (int i = 0; i < 16; i++) {
		EXPECT_NEAR(decoded[i * 4], 200, 5);
		EXPECT_NEAR(decoded[i * 4 + 1], 40, 5);
		EXPECT_EQ(decoded[i * 4 + 3], 128);
	}
}

// A download is written for both of the client's modes: .gz and .checksum (3D services) and .sd0 (without), all
// holding the same file
TEST(UgcJobs, AddsTheDownloadForBothClientModes) {
	UgcStorage::Files files;
	UgcJobs::AddDownload(files, "icon.dds", "abc");
	EXPECT_EQ(ZCompression::Gunzip(files.at("icon.dds.gz")).value_or(""), "abc");
	std::istringstream sd0(files.at("icon.dds.sd0"));
	EXPECT_EQ(Sd0(sd0).GetAsStringUncompressed(), "abc");
	std::string md5;
	uint32_t size{};
	ASSERT_TRUE(UgcFormats::ReadChecksumXml(files.at("icon.dds.checksum"), md5, size));
	EXPECT_EQ(md5, "900150983cd24fb0d6963f7d28e17f72");
	EXPECT_EQ(size, 3u);
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

TEST(UgcJobs, ModelsWithoutBricksAreEmptyNotFailed) {
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	UgcJobs::Settings settings;
	const auto empty = UgcJobs::ProcessModel(R"(<?xml version="1.0"?><LXFML versionMajor="5"><Meta/><Bricks/></LXFML>)", library, settings);
	EXPECT_FALSE(empty.ok);
	EXPECT_TRUE(empty.empty);
	EXPECT_TRUE(UgcModel::HasNoBricks(R"(<LXFML versionMajor="5"><Bricks/></LXFML>)"));
	// Broken LXFML, or bricks without geometry, are failures
	EXPECT_FALSE(UgcJobs::ProcessModel("<LXFML><nope", library, settings).empty);
	const auto missing = UgcJobs::ProcessModel(R"(<LXFML versionMajor="5"><Bricks><Brick><Part designID="9999" materials="1">
		<Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick></Bricks></LXFML>)", library, settings);
	EXPECT_FALSE(missing.ok);
	EXPECT_FALSE(missing.empty);
	EXPECT_FALSE(UgcModel::HasNoBricks(R"(<LXFML versionMajor="5"><Bricks><Brick><Part designID="9999"/></Brick></Bricks></LXFML>)"));
}

TEST(UgcStates, NamesComeFromTheEnum) {
	EXPECT_EQ(IUgc::ProcessStateName(IUgc::eProcessState::EMPTY), "empty");
	EXPECT_EQ(IUgc::ProcessStateName(IUgc::eProcessState::FAILED), "failed");
	EXPECT_EQ(IUgc::ParseProcessState("empty"), IUgc::eProcessState::EMPTY);
	EXPECT_EQ(IUgc::ParseProcessState("pending"), IUgc::eProcessState::PENDING);
	EXPECT_FALSE(IUgc::ParseProcessState("nonsense").has_value());
	EXPECT_EQ(magic_enum::enum_count<IUgc::eProcessState>(), 4u);
}

TEST(UgcIconPose, AnglesRoundTrip) {
	// The camera's direction and back
	for (const float yaw : { -170.0f, -53.0f, 0.0f, 21.0f, 90.0f, 179.0f }) {
		for (const float pitch : { -80.0f, -10.0f, 0.0f, 19.54f, 60.0f }) {
			const auto direction = UgcIconPose::CameraDirection(yaw, pitch);
			EXPECT_NEAR(glm::length(direction), 1.0f, 1e-5f);
			const auto angles = UgcIconPose::DirectionAngles(direction * 3.0f);
			EXPECT_NEAR(angles.x, yaw, 1e-3f);
			EXPECT_NEAR(angles.y, pitch, 1e-3f);
		}
	}
	// Yaw 0 looks from +Z, yaw 90 from +X, pitch 90 from above
	EXPECT_NEAR(UgcIconPose::CameraDirection(0, 0).z, 1.0f, 1e-6f);
	EXPECT_NEAR(UgcIconPose::CameraDirection(90, 0).x, 1.0f, 1e-6f);
	EXPECT_NEAR(UgcIconPose::CameraDirection(0, 90).y, 1.0f, 1e-6f);

	// The model's rotation and back (Ry * Rx * Rz)
	for (const auto& angles : { glm::vec3(0), glm::vec3(30, 20, 10), glm::vec3(-120, -45, 170), glm::vec3(90, 89, -90), glm::vec3(179, 0, -179) }) {
		const auto rotation = UgcIconPose::ModelRotation(angles.x, angles.y, angles.z);
		const auto back = UgcIconPose::RotationAngles(rotation);
		EXPECT_NEAR(back.x, angles.x, 1e-2f);
		EXPECT_NEAR(back.y, angles.y, 1e-2f);
		EXPECT_NEAR(back.z, angles.z, 1e-2f);
		// Same matrix from the angles found
		const auto again = UgcIconPose::ModelRotation(back.x, back.y, back.z);
		for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) EXPECT_NEAR(again[c][r], rotation[c][r], 1e-4f);
	}
	// The order: yaw turns +X towards -Z, pitch turns +Y towards +Z, roll turns +X towards +Y, applied roll first
	const auto yawed = UgcIconPose::ModelRotation(90, 0, 0) * glm::vec4(1, 0, 0, 0);
	EXPECT_NEAR(yawed.z, -1.0f, 1e-5f);
	const auto pitched = UgcIconPose::ModelRotation(0, 90, 0) * glm::vec4(0, 1, 0, 0);
	EXPECT_NEAR(pitched.z, 1.0f, 1e-5f);
	const auto rolled = UgcIconPose::ModelRotation(0, 0, 90) * glm::vec4(1, 0, 0, 0);
	EXPECT_NEAR(rolled.y, 1.0f, 1e-5f);
	const auto both = UgcIconPose::ModelRotation(90, 0, 90) * glm::vec4(1, 0, 0, 0); // rolled to +Y, which the yaw leaves
	EXPECT_NEAR(both.y, 1.0f, 1e-5f);
	// Glm's own YXZ Euler matrix agrees
	const auto glmYxz = glm::rotate(glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(0, 1, 0)), glm::radians(20.0f), glm::vec3(1, 0, 0)), glm::radians(10.0f), glm::vec3(0, 0, 1));
	const auto ours = UgcIconPose::ModelRotation(30, 20, 10);
	for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) EXPECT_NEAR(ours[c][r], glmYxz[c][r], 1e-6f);
}

TEST(UgcIconPose, FramingFillsTheIcon) {
	const std::vector<glm::vec3> box = { { -1, 0, -2 }, { 3, 0, -2 }, { -1, 2, -2 }, { 3, 2, -2 }, { -1, 0, 1 }, { 3, 0, 1 }, { -1, 2, 1 }, { 3, 2, 1 } };
	UgcIconPose::Camera camera{ 53.36f, 19.54f, 39.6f, 1.0f, 0.0f, 0.0f };
	auto frame = UgcIconPose::Compute({ &box }, camera);
	ASSERT_TRUE(frame.ok);
	EXPECT_NEAR(frame.distance, frame.radius / std::sin(glm::radians(39.6f) * 0.5f), 1e-4f);
	float minX = 2, maxX = -2, minY = 2, maxY = -2;
	for (const auto& p : box) {
		const auto point = frame.IconPoint(p);
		minX = std::min(minX, point.x), maxX = std::max(maxX, point.x), minY = std::min(minY, point.y), maxY = std::max(maxY, point.y);
	}
	// Margin 1: the larger side spans the icon exactly, both centred
	EXPECT_NEAR(std::max(maxX - minX, maxY - minY), 1.0f, 1e-4f);
	EXPECT_NEAR((minX + maxX) * 0.5f, 0.5f, 1e-4f);
	EXPECT_NEAR((minY + maxY) * 0.5f, 0.5f, 1e-4f);

	// The icon's rectangle in NDC maps back onto the icon's corners, also shifted and with a border
	camera.margin = 1.5f;
	camera.offsetX = 0.2f;
	camera.offsetY = -0.1f;
	frame = UgcIconPose::Compute({ &box }, camera);
	const auto rect = frame.IconRect();
	const auto corner = [&](float ndcX, float ndcY) {
		// A point at that NDC place: through the inverse view-projection
		const auto world = glm::inverse(frame.viewProjection) * glm::vec4(ndcX, ndcY, 0.5f, 1.0f);
		return frame.IconPoint(glm::vec3(world) / world.w);
	};
	const auto topLeft = corner(rect.x, rect.w), bottomRight = corner(rect.z, rect.y);
	EXPECT_NEAR(topLeft.x, 0.0f, 1e-3f);
	EXPECT_NEAR(topLeft.y, 0.0f, 1e-3f);
	EXPECT_NEAR(bottomRight.x, 1.0f, 1e-3f);
	EXPECT_NEAR(bottomRight.y, 1.0f, 1e-3f);
	// The model's projected size is the icon's over the margin
	minX = 2, maxX = -2;
	for (const auto& p : box) minX = std::min(minX, frame.IconPoint(p).x), maxX = std::max(maxX, frame.IconPoint(p).x);
	float minY2 = 2, maxY2 = -2;
	for (const auto& p : box) minY2 = std::min(minY2, frame.IconPoint(p).y), maxY2 = std::max(maxY2, frame.IconPoint(p).y);
	EXPECT_NEAR(std::max(maxX - minX, maxY2 - minY2), 1.0f / 1.5f, 1e-4f);
	EXPECT_NEAR((minX + maxX) * 0.5f, 0.7f, 1e-4f);
	EXPECT_NEAR((minY2 + maxY2) * 0.5f, 0.6f, 1e-4f);
}

TEST(UgcIconPose, RendererHonoursTheModelRotation) {
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	library.SetMaterials({ { 21, { 222, 0, 13, 255 } } });
	std::string error;
	// A long bar along X: seen from the front (yaw 0) it is wide; turned 90 degrees it is narrow
	auto model = UgcModel::Build(UgcModel::ParseLxfml(R"(<LXFML versionMajor="5"><Bricks>
		<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,1,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,2,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,3,0,0"/></Part></Brick>
		</Bricks></LXFML>)", error), library);
	const auto coverage = [](const UgcRender::Image& image, bool columns) {
		int count = 0;
		for (int i = 0; i < image.width; i++) {
			bool any = false;
			for (int j = 0; j < image.height && !any; j++) any = image.rgba[((columns ? j : i) * image.width + (columns ? i : j)) * 4 + 3] > 0;
			count += any;
		}
		return count;
	};
	UgcRender::IconOptions options{ 64, 1 };
	options.yawDegrees = 0.0f;
	options.pitchDegrees = 0.0f;
	options.margin = 1.0f;
	const auto front = UgcRender::RenderIcon(model, options);
	EXPECT_GT(coverage(front, true), coverage(front, false) * 2); // wider than tall
	options.modelYawDegrees = 90.0f;
	const auto turned = UgcRender::RenderIcon(model, options);
	EXPECT_NEAR(coverage(turned, true), coverage(turned, false), 12); // end on: its square end, the rest behind it
	// Turning the model is the same as turning the camera the other way (the light turns with the camera here: none)
	UgcIconParams::Apply(options, { { "modelYaw", 0.0f }, { "modelRoll", 90.0f } });
	EXPECT_FLOAT_EQ(options.modelRollDegrees, 90.0f);
	const auto rolled = UgcRender::RenderIcon(model, options);
	EXPECT_GT(coverage(rolled, false), coverage(rolled, true) * 2); // standing up: taller than wide
}

TEST(UgcJobs, AssemblyNifIsTheIconsModel) {
	// Two modules, each a triangle; the second stands on the first's CP_A1 node
	const auto res = TempFolder("assembly");
	std::filesystem::create_directories(res / "mesh");
	UgcModel::Mesh triangle;
	triangle.positions = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
	triangle.normals = { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } };
	triangle.colors = { { 1, 0, 0, 1 }, { 1, 0, 0, 1 }, { 1, 0, 0, 1 } };
	triangle.indices = { 0, 1, 2 };
	std::ofstream(res / "mesh" / "a.nif", std::ios::binary) << UgcFormats::WriteNif("A", { { "A", &triangle, false } });
	std::ofstream(res / "mesh" / "b.nif", std::ios::binary) << UgcFormats::WriteNif("B", { { "B", &triangle, false } });
	UgcJobs::ModularInput input;
	input.buildXml = R"(<ModularBuild><topology><numberOfParts value="2" /><rootPart value="0" /><connection myPartid="0" myLocation="CP_A1" connectingPart="1" /></topology>
		<Placement><AdditionalModelRotation><Rotation w="0.70710678" x="0" y="0.70710678" z="0" /></AdditionalModelRotation></Placement></ModularBuild>)";
	input.modules = { { 1, 0, "mesh/a.nif", "" }, { 2, 1, "mesh/b.nif", R"(<ModuleInfo><connection name="CP_A1"><translation x="0" y="0" z="0" /></connection></ModuleInfo>)" } };
	input.key = "1-2";
	std::string error, note;
	glm::mat4 additional{ 1.0f };
	const auto model = UgcJobs::AssembleModular(input, res, additional, error, note);
	ASSERT_TRUE(model) << error;
	EXPECT_EQ(model->opaque.TriangleCount(), 2u);
	const auto nif = UgcJobs::AssemblyNif(input, res, error);
	ASSERT_TRUE(nif) << error;
	const auto read = NifFile::Parse(*nif, 0, error);
	ASSERT_TRUE(read) << error;
	// The .nif holds the model already turned by the build type's AdditionalModelRotation (90 degrees around Y: +X -> -Z)
	const auto fromNif = UgcModel::FromNif(*read);
	ASSERT_EQ(fromNif.opaque.positions.size(), 6u);
	EXPECT_NEAR(fromNif.opaque.positions[1].z, -1.0f, 1e-4f);
	EXPECT_NEAR(fromNif.opaque.positions[1].x, 0.0f, 1e-4f);
	// And drawing it with no further turn gives the same icon as the renderer's own path
	UgcRender::IconOptions options{ 32, 1 };
	auto turned = options;
	turned.modelRotation = additional;
	EXPECT_EQ(UgcRender::RenderIcon(fromNif, options).rgba, UgcRender::RenderIcon(*model, turned).rgba);
	// Nothing to draw
	input.modules.clear();
	EXPECT_FALSE(UgcJobs::AssemblyNif(input, res, error));
}

TEST(UgcIconPose, MatchesTheEditorsFixture) {
	// The same numbers the dashboard's editor math (ugc-pose-math.js) is checked against
	std::ifstream file(UGC_POSE_FIXTURE);
	const auto fixture = nlohmann::json::parse(file, nullptr, false);
	ASSERT_TRUE(fixture.is_object());
	std::vector<glm::vec3> positions;
	const auto& flat = fixture["positions"];
	for (size_t i = 0; i + 2 < flat.size(); i += 3) positions.emplace_back(flat[i].get<float>(), flat[i + 1].get<float>(), flat[i + 2].get<float>());
	for (const auto& c : fixture["cases"]) {
		const auto& pose = c["pose"];
		const auto rotation = UgcIconPose::ModelRotation(pose["modelYaw"].get<float>(), pose["modelPitch"].get<float>(), pose["modelRoll"].get<float>());
		std::vector<glm::vec3> turned;
		for (const auto& p : positions) turned.emplace_back(rotation * glm::vec4(p, 1.0f));
		const auto frame = UgcIconPose::Compute({ &turned }, { pose["yaw"].get<float>(), pose["pitch"].get<float>(), pose["fov"].get<float>(),
			pose["margin"].get<float>(), pose["offsetX"].get<float>(), pose["offsetY"].get<float>() });
		ASSERT_TRUE(frame.ok);
		for (int k = 0; k < 3; k++) EXPECT_NEAR(frame.center[k], c["center"][k].get<float>(), 1e-4f);
		for (int k = 0; k < 3; k++) EXPECT_NEAR(frame.eye[k], c["eye"][k].get<float>(), 1e-3f);
		EXPECT_NEAR(frame.scale, c["scale"].get<float>(), 1e-4f);
		for (size_t v = 0; v < turned.size(); v++) {
			const auto point = frame.IconPoint(turned[v]);
			EXPECT_NEAR(point.x, c["iconPoints"][v][0].get<float>(), 1e-4f) << v;
			EXPECT_NEAR(point.y, c["iconPoints"][v][1].get<float>(), 1e-4f) << v;
		}
	}
}

namespace {
	// Plastic (21), LU Toolbox metallic (150), glow (329, and 294 which LU Toolbox's palette has opaque) and
	// transparent (40)
	const char* LOOKS_LXFML = R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<LXFML versionMajor="5" versionMinor="0"><Bricks>
<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
<Brick><Part designID="3001" materials="150"><Bone transformation="1,0,0,0,1,0,0,0,1,3,0,0"/></Part></Brick>
<Brick><Part designID="3001" materials="329"><Bone transformation="1,0,0,0,1,0,0,0,1,6,0,0"/></Part></Brick>
<Brick><Part designID="3001" materials="294"><Bone transformation="1,0,0,0,1,0,0,0,1,9,0,0"/></Part></Brick>
<Brick><Part designID="3001" materials="40"><Bone transformation="1,0,0,0,1,0,0,0,1,12,0,0"/></Part></Brick>
</Bricks></LXFML>)";

	UgcJobs::Settings SmallSettings() {
		UgcJobs::Settings settings;
		settings.optimize.resolution = 128;
		settings.ao.samples = 8;
		settings.icon.size = 32;
		settings.icon.supersample = 1;
		settings.icon.ao.samples = 4;
		return settings;
	}
}

TEST(UgcShaders, OffIsByteIdenticalToBefore) {
	// With the shader settings off (the default) the files are exactly what the server made before they existed, so
	// nothing is made again needlessly. The hashes are of the files made before the settings were added.
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	const auto outcome = UgcJobs::ProcessModel(LOOKS_LXFML, library, SmallSettings(), 7);
	ASSERT_TRUE(outcome.ok) << outcome.error;
	const auto nif = *ZCompression::Gunzip(outcome.files.at("model.nif.gz"));
	std::string error;
	const auto read = NifFile::Parse(nif, 0, error);
	ASSERT_TRUE(read) << error;
	EXPECT_EQ(read->nodes.size(), 4u); // the root, S01_Opaque_Model, S01_Alpha_Model and LOD_0
	for (const auto& mesh : read->meshes) EXPECT_EQ(mesh.material.shaderTag, 1);
	// The floating point results (color variation, occlusion) are the same on one platform and compiler; the hashes
	// were taken with GCC on x86-64 Linux
#if defined(__linux__) && defined(__x86_64__) && defined(__GNUC__) && !defined(__clang__)
	EXPECT_EQ(UgcFormats::Md5Hex(nif), "b0fcb707d36ccdb62e951bf50593e633");
	EXPECT_EQ(UgcFormats::Md5Hex(*ZCompression::Gunzip(outcome.files.at("model.noao.nif.gz"))), "db55bd2c8567a862b2c96942aa5a2617");
	EXPECT_EQ(UgcFormats::Md5Hex(outcome.files.at("icon.png")), "032ff7df236a636a4c609071d9b46181");
#endif
}

TEST(UgcShaders, OnlyTheShaderIdsSwitchItOn) {
	// The other shader settings change nothing while the groups are off
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	auto settings = SmallSettings();
	const auto before = UgcJobs::ProcessModel(LOOKS_LXFML, library, settings, 7);
	settings.shaders.glowEmissive = 0.5f;
	settings.icon.glowEmissive = 0.5f;
	settings.build.looks.materialTypes.clear();
	const auto after = UgcJobs::ProcessModel(LOOKS_LXFML, library, settings, 7);
	ASSERT_TRUE(before.ok && after.ok);
	for (const auto* name : { "model.nif.checksum", "model.noao.nif.gz", "icon.png" }) EXPECT_EQ(before.files.at(name), after.files.at(name)) << name;
}

TEST(UgcShaders, NamesTheGroups) {
	UgcJobs::Settings settings;
	settings.shaders.metal = 88;
	settings.shaders.brushed = 89;
	settings.shaders.glow = 7;
	EXPECT_EQ(UgcJobs::ShapeName(settings, UgcModel::eLook::PLASTIC, false), "S01_Opaque_Model");
	EXPECT_EQ(UgcJobs::ShapeName(settings, UgcModel::eLook::PLASTIC, true), "S01_Alpha_Model");
	EXPECT_EQ(UgcJobs::ShapeName(settings, UgcModel::eLook::METAL, false), "S88_Metal_Model");
	EXPECT_EQ(UgcJobs::ShapeName(settings, UgcModel::eLook::BRUSHED, false), "S89_Brushed_Model");
	EXPECT_EQ(UgcJobs::ShapeName(settings, UgcModel::eLook::GLOW, false), "S07_Glow_Model");
	// The client reads the id back as the tag
	EXPECT_EQ(NifFile::ShaderTag(UgcJobs::ShapeName(settings, UgcModel::eLook::GLOW, false)), 7);
	EXPECT_EQ(NifFile::ShaderTag(UgcJobs::ShapeName(settings, UgcModel::eLook::METAL, false)), 88);
	// The client's own ids read back to their looks whatever the settings, the settings' own too
	const auto looks = settings.shaders.TagLooks();
	EXPECT_EQ(looks.at(88), UgcModel::eLook::METAL);
	EXPECT_EQ(looks.at(46), UgcModel::eLook::GLOW);
	EXPECT_EQ(looks.at(7), UgcModel::eLook::GLOW);
	EXPECT_FALSE(looks.contains(1));
}

TEST(UgcShaders, LooksComeFromTheColorData) {
	const UgcModel::LookRules rules;
	const UgcBricks::Material plastic{ 200, 0, 0, 255, "shinyPlastic" }, steel{ 150, 150, 150, 255, "shinySteel" }, brushed{ 150, 150, 150, 255, "brushedSteel" };
	EXPECT_EQ(UgcModel::LookOf(21, plastic, rules), UgcModel::eLook::PLASTIC);
	EXPECT_EQ(UgcModel::LookOf(5000, steel, rules), UgcModel::eLook::METAL);       // a Materials.xml shinySteel
	EXPECT_EQ(UgcModel::LookOf(5000, brushed, rules), UgcModel::eLook::BRUSHED);
	EXPECT_EQ(UgcModel::LookOf(183, plastic, rules), UgcModel::eLook::METAL);      // LU Toolbox's metallic, shinyPlastic in Materials.xml
	EXPECT_EQ(UgcModel::LookOf(329, plastic, rules), UgcModel::eLook::GLOW);       // LU Toolbox's glow colors
	EXPECT_EQ(UgcModel::LookOf(50, plastic, rules), UgcModel::eLook::GLOW);
	EXPECT_EQ(UgcModel::LookOf(9016, plastic, rules), UgcModel::eLook::GLOW);
	UgcModel::LookRules none;
	none.materialTypes.clear();
	none.paletteMetallic = false;
	EXPECT_EQ(UgcModel::LookOf(5000, steel, none), UgcModel::eLook::PLASTIC);
	EXPECT_EQ(UgcModel::LookOf(150, steel, none), UgcModel::eLook::PLASTIC);

	// Built: opaque vertices get their color's look, transparent bricks none (their glow stays with them)
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	library.SetMaterials({ { 5000, brushed } });
	std::string error;
	const auto model = UgcModel::Build(UgcModel::ParseLxfml(R"(<LXFML versionMajor="5"><Bricks>
		<Brick><Part designID="3001" materials="150"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="5000"><Bone transformation="1,0,0,0,1,0,0,0,1,3,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,6,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="40"><Bone transformation="1,0,0,0,1,0,0,0,1,9,0,0"/></Part></Brick>
		</Bricks></LXFML>)", error), library);
	ASSERT_EQ(model.opaque.looks.size(), 24u);
	EXPECT_EQ(model.opaque.looks[0], UgcModel::eLook::METAL);
	EXPECT_EQ(model.opaque.looks[8], UgcModel::eLook::BRUSHED);
	EXPECT_EQ(model.opaque.looks[16], UgcModel::eLook::PLASTIC);
	EXPECT_TRUE(model.transparent.looks.empty());

	// Split by the looks that have groups; the rest stay plastic; nothing to split: the mesh as it is
	const auto split = UgcModel::SplitLooks(model.opaque, { false, true, false, false });
	ASSERT_TRUE(split);
	EXPECT_EQ((*split)[0].TriangleCount(), 24u);
	EXPECT_EQ((*split)[1].TriangleCount(), 12u);
	EXPECT_TRUE((*split)[2].Empty());
	EXPECT_FALSE(UgcModel::SplitLooks(model.opaque, { false, false, false, true }));
}

TEST(UgcShaders, WritesAGroupPerLookWithEveryLevel) {
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	auto settings = SmallSettings();
	settings.build.colorVariation = 0.0f;
	settings.shaders.metal = 88;
	settings.shaders.brushed = 89;
	settings.shaders.glow = 46;
	settings.shaders.glowEmissive = 0.75f;
	const auto outcome = UgcJobs::ProcessModel(LOOKS_LXFML, library, settings, 7);
	ASSERT_TRUE(outcome.ok) << outcome.error;
	const auto nif = *ZCompression::Gunzip(outcome.files.at("model.nif.gz"));
	std::string error;
	for (const uint32_t level : { 0u, 1u }) {
		const auto read = NifFile::Parse(nif, level, error);
		ASSERT_TRUE(read) << error;
		// No brushed steel colors: no group for them. Every group has both levels.
		for (const auto* name : { "S01_Opaque_Model", "S88_Metal_Model", "S46_Glow_Model", "S01_Alpha_Model" }) EXPECT_TRUE(read->nodes.contains(name)) << name;
		EXPECT_FALSE(read->nodes.contains("S89_Brushed_Model"));
		EXPECT_TRUE(read->nodes.contains(level == 0 ? "LOD_0" : "LOD_2"));
		std::map<int32_t, size_t> triangles;
		for (const auto& mesh : read->meshes) triangles[mesh.material.shaderTag] += mesh.indices.size() / 3;
		EXPECT_EQ(triangles[1], 24u); // the plastic brick and the transparent one
		EXPECT_EQ(triangles[88], 12u);
		EXPECT_EQ(triangles[46], 24u); // 329 and 294
	}
	const auto read = NifFile::Parse(nif, 0, error);
	const auto noao = NifFile::Parse(*ZCompression::Gunzip(outcome.files.at("model.noao.nif.gz")), 0, error);
	ASSERT_TRUE(read && noao);
	size_t glowShapes = 0;
	for (const auto& mesh : read->meshes) {
		if (mesh.material.shaderTag == 46) {
			glowShapes++;
			// The emissive shader's material, the plain color (as before the lighting bake), opaque
			for (const auto value : mesh.material.emissive) EXPECT_FLOAT_EQ(value, 0.75f);
			const auto plain = std::find_if(noao->meshes.begin(), noao->meshes.end(), [&](const auto& other) { return other.material.shaderTag == 46 && other.positions == mesh.positions; });
			ASSERT_NE(plain, noao->meshes.end());
			EXPECT_EQ(mesh.colors, plain->colors);
			for (size_t i = 3; i < mesh.colors.size(); i += 4) EXPECT_EQ(mesh.colors[i], 255);
		} else {
			for (const auto value : mesh.material.emissive) EXPECT_EQ(value, 0.0f);
		}
	}
	EXPECT_EQ(glowShapes, 1u);
	EXPECT_NE(outcome.stats.find("\"S88_Metal_Model\":12"), std::string::npos) << outcome.stats;
}

TEST(UgcShaders, IconsDrawGlowUnlitAndMetalShiny) {
	// One quad facing the camera, lit from behind: plastic is dark, glow its full color, metal shows a reflection
	UgcModel::Model model;
	model.opaque.positions = { { -1, -1, 0 }, { 1, -1, 0 }, { -1, 1, 0 }, { 1, 1, 0 } };
	model.opaque.normals.assign(4, { 0, 0, 1 });
	model.opaque.colors.assign(4, { 0.8f, 0.4f, 0.2f, 1.0f });
	model.opaque.indices = { 0, 1, 2, 1, 3, 2 };
	UgcRender::IconOptions options;
	options.size = 16;
	options.supersample = 1;
	options.yawDegrees = 0.0f;
	options.pitchDegrees = 0.0f;
	options.sunYawDegrees = 180.0f;
	options.sunPitchDegrees = 0.0f;
	options.shadows = 0.0f;
	const auto centre = [&](UgcModel::eLook look) {
		auto copy = model;
		if (look != UgcModel::eLook::PLASTIC) copy.opaque.looks.assign(4, look);
		const auto image = UgcRender::RenderIcon(copy, options);
		const size_t at = (8 * 16 + 8) * 4;
		return glm::ivec3(image.rgba[at], image.rgba[at + 1], image.rgba[at + 2]);
	};
	const auto plastic = centre(UgcModel::eLook::PLASTIC), glow = centre(UgcModel::eLook::GLOW), metal = centre(UgcModel::eLook::METAL);
	EXPECT_NEAR(glow.r, 204, 2);
	EXPECT_NEAR(glow.g, 102, 2);
	EXPECT_NEAR(glow.b, 51, 2);
	EXPECT_LT(plastic.r, glow.r);
	EXPECT_NE(metal, plastic);
	EXPECT_GE(metal.r, metal.g); // tinted by its color
	options.glowEmissive = 0.0f;
	EXPECT_EQ(centre(UgcModel::eLook::GLOW), plastic);

	// Read back from a .nif by the groups' tags
	const UgcModel::Mesh mesh = model.opaque;
	const auto nif = UgcFormats::WriteLodNif("SceneNode_Model", { { "S46_Glow_Model", false, { { 0.0f, 100.0f, "LOD_0", { &mesh } } }, 1.0f } });
	std::string error;
	const auto read = NifFile::Parse(nif, 0, error);
	ASSERT_TRUE(read) << error;
	EXPECT_EQ(UgcModel::FromNif(*read, UgcJobs::Shaders{}.TagLooks()).opaque.looks, std::vector<UgcModel::eLook>(4, UgcModel::eLook::GLOW));
	EXPECT_TRUE(UgcModel::FromNif(*read).opaque.looks.empty());
}
