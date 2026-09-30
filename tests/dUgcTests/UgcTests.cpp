#include <gtest/gtest.h>

#include <algorithm>
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
#include "UgcGlitter.h"
#include "UgcModel.h"
#include "UgcJobs.h"
#include "IUgc.h"
#include "UgcIconParams.h"
#include "UgcIconPose.h"
#include "UgcKeys.h"
#include "UgcModular.h"
#include "UgcPalette.h"
#include "UgcRays.h"
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
	const auto result = UgcHsr::RemoveHiddenFaces(model, UgcHsr::Options{ .resolution = 256 });
	EXPECT_EQ(result.trianglesRemoved, 12u);
	EXPECT_EQ(model.opaque.TriangleCount(), 12u);
	EXPECT_EQ(model.opaque.positions.size(), 8u);

	const auto icon = UgcRender::RenderIcon(model, UgcRender::IconOptions{ 32, 2 });
	ASSERT_EQ(icon.rgba.size(), 32u * 32u * 4u);
	EXPECT_EQ(icon.rgba[3], 0);                        // a corner is background
	EXPECT_EQ(icon.rgba[(16 * 32 + 16) * 4 + 3], 255); // the middle is the box
	EXPECT_GT(icon.rgba[(16 * 32 + 16) * 4], icon.rgba[(16 * 32 + 16) * 4 + 1]); // red
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
	EXPECT_EQ(model->meshes[0].positions, (std::vector<float>{ 0, 0, 0, 1, 0, 0, 0, 1, 0 })); // where they were, upright
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
	EXPECT_TRUE(UgcPalette::Linear(309).has_value()); // LU Toolbox's metallic colors are colors, not looks
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
	EXPECT_EQ(lod0->meshes[0].positions, (std::vector<float>{ 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0 }));
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
	EXPECT_EQ(UgcFormats::Md5Hex(nif), "7a7176731afd837da83450d260ed6832");
	EXPECT_EQ(UgcFormats::Md5Hex(*ZCompression::Gunzip(outcome.files.at("model.noao.nif.gz"))), "19d8a2f748015e1f9d680a60fb7565d5");
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
	EXPECT_EQ(UgcModel::LookOf(183, plastic, rules), UgcModel::eLook::PLASTIC);    // LU Toolbox's metallic, shinyPlastic in Materials.xml
	EXPECT_EQ(UgcModel::LookOf(131, plastic, rules), UgcModel::eLook::PLASTIC);    // the same (a grey that is often a whole baseplate)
	EXPECT_EQ(UgcModel::LookOf(329, plastic, rules), UgcModel::eLook::GLOW);       // LU Toolbox's glow colors
	EXPECT_EQ(UgcModel::LookOf(50, plastic, rules), UgcModel::eLook::GLOW);
	EXPECT_EQ(UgcModel::LookOf(9016, plastic, rules), UgcModel::eLook::GLOW);
	UgcModel::LookRules named;
	named.colors[298] = UgcModel::eLook::BRUSHED;
	named.colors[329] = UgcModel::eLook::BRUSHED;
	EXPECT_EQ(UgcModel::LookOf(298, steel, named), UgcModel::eLook::BRUSHED);    // a named color wins over its type
	EXPECT_EQ(UgcModel::LookOf(329, plastic, named), UgcModel::eLook::BRUSHED);  // and over the glow colors
	EXPECT_EQ(UgcModel::LookOf(150, steel, named), UgcModel::eLook::METAL);
	UgcModel::LookRules none;
	none.materialTypes.clear();
	EXPECT_EQ(UgcModel::LookOf(5000, steel, none), UgcModel::eLook::PLASTIC);
	EXPECT_EQ(UgcModel::LookOf(150, steel, none), UgcModel::eLook::PLASTIC);

	// Built: opaque vertices get their color's look, transparent bricks none (their glow stays with them)
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	library.SetMaterials({ { 150, steel }, { 5000, brushed } });
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
	library.SetMaterials({ { 150, { 152, 155, 153, 255, "shinySteel" } } }); // the client's Materials.xml entry
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

// color_brightness scales the models' vertex colors (not icons'); transparent_colors makes a color Materials.xml has
// opaque transparent, at transparent_opacity
TEST(UgcModel, BrightnessAndTransparentColors) {
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	constexpr uint32_t ADDED = 50001;
	library.SetMaterials({ { ADDED, { 0, 200, 100, 255 } } });
	std::string error;
	const auto brick = UgcModel::ParseLxfml("<LXFML versionMajor=\"5\"><Bricks><Brick><Part designID=\"3001\" materials=\"" + std::to_string(ADDED) +
		"\"><Bone transformation=\"1,0,0,0,1,0,0,0,1,0,0,0\"/></Part></Brick></Bricks></LXFML>", error);
	UgcModel::BuildOptions options;
	options.colorVariation = 0.0f;
	const auto normal = UgcModel::Build(brick, library, options);
	ASSERT_FALSE(normal.opaque.colors.empty());
	EXPECT_TRUE(normal.transparent.colors.empty());

	options.brightness = 50.0f;
	const auto darker = UgcModel::Build(brick, library, options);
	ASSERT_FALSE(darker.opaque.colors.empty());
	const auto linear = UgcPalette::SrgbToLinear(glm::vec3(normal.opaque.colors[0])) * 0.5f;
	EXPECT_NEAR(darker.opaque.colors[0].g, UgcPalette::LinearToSrgb(linear).g, 1e-4f);
	options.icon = true;
	EXPECT_EQ(UgcModel::Build(brick, library, options).opaque.colors[0], UgcModel::Build(brick, library, [&] { auto o = options; o.brightness = 100.0f; return o; }()).opaque.colors[0]);

	options = {};
	options.colorVariation = 0.0f;
	options.transparentColors.insert(ADDED);
	const auto seeThrough = UgcModel::Build(brick, library, options);
	EXPECT_TRUE(seeThrough.opaque.colors.empty());
	ASSERT_FALSE(seeThrough.transparent.colors.empty());
	EXPECT_NEAR(seeThrough.transparent.colors[0].a, 0.5882f, 1e-4f);
}

// The glitter texture: the same every time, tiling (flecks wrap around the edges), mipmapped down to 1x1; flecks of
// the size asked for (the texture grows to keep them 3 pixels wide), most dimmer than the brightest
TEST(UgcGlitter, TextureIsTheSameEveryTimeAndMipmapped) {
	UgcGlitter::Params params;
	EXPECT_EQ(params.TextureSize(), 128);
	const auto alpha = UgcGlitter::FleckAlpha(params);
	ASSERT_EQ(alpha.size(), 128u * 128u);
	EXPECT_EQ(alpha, UgcGlitter::FleckAlpha(params));
	const auto lit = std::count_if(alpha.begin(), alpha.end(), [](uint8_t a) { return a > 0; });
	EXPECT_GT(lit, 80 * 4);
	EXPECT_LT(lit, static_cast<long>(alpha.size() / 10)); // sparse
	// Flat flecks up to the opacity (80%: 204), most of them dimmer
	EXPECT_LE(*std::max_element(alpha.begin(), alpha.end()), 204);
	EXPECT_GE(*std::max_element(alpha.begin(), alpha.end()), 190);
	EXPECT_GT(std::count_if(alpha.begin(), alpha.end(), [](uint8_t a) { return a > 0 && a < 120; }), std::count_if(alpha.begin(), alpha.end(), [](uint8_t a) { return a >= 160; }));
	// Bigger flecks cover more; small ones get a bigger texture
	auto big = params;
	big.fleckSize = 0.1f;
	const auto bigAlpha = UgcGlitter::FleckAlpha(big);
	EXPECT_GT(std::count_if(bigAlpha.begin(), bigAlpha.end(), [](uint8_t a) { return a > 0; }), lit * 2);
	auto small = params;
	small.fleckSize = 0.02f;
	EXPECT_EQ(small.TextureSize(), 256);
	EXPECT_EQ(UgcGlitter::FleckAlpha(small).size(), 256u * 256u);
	auto none = params, dense = params;
	none.flecks = 0;
	dense.flecks = 300;
	const auto noneAlpha = UgcGlitter::FleckAlpha(none), denseAlpha = UgcGlitter::FleckAlpha(dense);
	EXPECT_EQ(std::count_if(noneAlpha.begin(), noneAlpha.end(), [](uint8_t a) { return a > 0; }), 0);
	EXPECT_GT(std::count_if(denseAlpha.begin(), denseAlpha.end(), [](uint8_t a) { return a > 0; }), lit);
	const auto mips = UgcGlitter::Mipmaps(alpha);
	ASSERT_EQ(mips.size(), 8u); // 128 .. 1
	EXPECT_EQ(mips.back().size(), 1u);
	double mean = 0;
	for (const auto a : alpha) mean += a;
	EXPECT_NEAR(mips.back()[0], mean / alpha.size(), 2.0);

	// UVs: the axis plane the normal faces most, in tiles; the same density on every side
	EXPECT_EQ(UgcGlitter::Uv({ 1.6f, 3.2f, 0.8f }, { 0, 0, 1 }, 1.6f), glm::vec2(1.0f, 2.0f));
	EXPECT_EQ(UgcGlitter::Uv({ 1.6f, 3.2f, 0.8f }, { 0, -1, 0 }, 1.6f), glm::vec2(1.0f, 0.5f));
	EXPECT_EQ(UgcGlitter::Uv({ 1.6f, 3.2f, 0.8f }, { 1, 0.2f, 0 }, 1.6f), glm::vec2(0.5f, 2.0f));
	// Sampling wraps
	EXPECT_FLOAT_EQ(UgcGlitter::Sample(alpha, { 0.3f, 0.7f }), UgcGlitter::Sample(alpha, { 2.3f, -0.3f }));
}

namespace {
	// A quad in the XY plane, 4 by 4 units, colored
	UgcModel::Mesh Quad(const glm::vec4& color) {
		UgcModel::Mesh mesh;
		mesh.positions = { { -2, -2, 0 }, { 2, -2, 0 }, { -2, 2, 0 }, { 2, 2, 0 } };
		mesh.normals.assign(4, { 0, 0, 1 });
		mesh.colors.assign(4, color);
		mesh.indices = { 0, 1, 2, 1, 3, 2 };
		return mesh;
	}
}

// A glitter group: UVs, the fleck texture stored in the file, and the two texture transform controllers the client
// animates it with; read back as NifFile sees it
namespace {
	// Each block's type and its NiAVObject flags (the u16 after the name, extra data count and controller), from a
	// NIF 20.3.0.9 as UgcFormats writes it (no extra data)
	std::vector<std::pair<std::string, uint16_t>> BlockFlags(const std::string& nif) {
		size_t at = nif.find('\n') + 1;
		const auto u32 = [&]() { uint32_t v = 0; std::memcpy(&v, nif.data() + at, 4); at += 4; return v; };
		const auto u16 = [&]() { uint16_t v = 0; std::memcpy(&v, nif.data() + at, 2); at += 2; return v; };
		at += 4 + 1 + 4; // version, endian, user version
		const auto blocks = u32();
		const auto typeCount = u16();
		std::vector<std::string> types;
		for (uint16_t i = 0; i < typeCount; i++) {
			const auto length = u32();
			types.emplace_back(nif.substr(at, length));
			at += length;
		}
		std::vector<uint16_t> blockTypes;
		for (uint32_t i = 0; i < blocks; i++) blockTypes.push_back(u16() & 0x7fff);
		std::vector<uint32_t> sizes;
		for (uint32_t i = 0; i < blocks; i++) sizes.push_back(u32());
		const auto strings = u32();
		u32(); // max length
		for (uint32_t i = 0; i < strings; i++) at += u32();
		const auto groups = u32();
		at += groups * 4;
		std::vector<std::pair<std::string, uint16_t>> out;
		for (uint32_t i = 0; i < blocks; i++) {
			uint16_t flags = 0;
			const auto& type = types[blockTypes[i]];
			if (type == "NiNode" || type == "NiLODNode" || type == "NiTriShape") std::memcpy(&flags, nif.data() + at + 12, 2);
			out.emplace_back(type, flags);
			at += sizes[i];
		}
		return out;
	}
}

// Nothing in a placed player model's .nif can move (the client never updates it: LWOSkinnedRenderComponent::Run with
// animation off for modelType 2), so a glitter .nif has no controllers and every node and shape keeps the game's brick
// model flags
TEST(UgcFormats, GlitterNifIsStatic) {
	const auto mesh = Quad({ 0.2f, 0.4f, 0.8f, 0.6f });
	const UgcGlitter::Params glitter;
	const auto nif = UgcFormats::WriteLodNif("SceneNode_Model", {
		{ "S01_Opaque_Model", false, { { 0.0f, 100.0f, "LOD_0", { &mesh } } } },
		{ "S21_Glitter_Model", false, { { 0.0f, 100.0f, "LOD_0", { &mesh } } }, 0.0f, &glitter },
		{ "S79_GlitterSparkle_Model", false, { { 0.0f, 100.0f, "LOD_0", { &mesh } } }, 0.0f, &glitter, true } });
	for (const auto* type : { "NiTextureTransformController", "NiFloatInterpolator", "NiFloatData" }) EXPECT_EQ(nif.find(type), std::string::npos) << type;
	const auto blocks = BlockFlags(nif);
	ASSERT_EQ(blocks[0].first, "NiNode");
	for (const auto& [type, flags] : blocks) {
		if (type == "NiNode" || type == "NiLODNode") EXPECT_EQ(flags, 0x110) << type;
		if (type == "NiTriShape") EXPECT_EQ(flags, 0x10) << type;
	}
}

TEST(UgcFormats, GlitterNifReadsBack) {
	const auto mesh = Quad({ 0.2f, 0.4f, 0.8f, 0.6f });
	const UgcGlitter::Params glitter;
	const auto nif = UgcFormats::WriteLodNif("SceneNode_Model", { { "S21_GlitterAlpha_Model", true, { { 0.0f, 100.0f, "LOD_0", { &mesh, &mesh } } }, 0.0f, &glitter } });
	std::string error;
	const auto read = NifFile::Parse(nif, 0, error);
	ASSERT_TRUE(read) << error;
	ASSERT_EQ(read->meshes.size(), 2u);
	for (const auto& shape : read->meshes) {
		EXPECT_EQ(shape.material.shaderTag, 21);
		ASSERT_EQ(shape.uvs.size(), 8u);
		for (size_t v = 0; v < 4; v++) {
			const auto uv = UgcGlitter::Uv(mesh.positions[v], mesh.normals[v], 1.6f);
			EXPECT_FLOAT_EQ(shape.uvs[v * 2], uv.x);
			EXPECT_FLOAT_EQ(shape.uvs[v * 2 + 1], uv.y);
		}
		EXPECT_TRUE(shape.material.texture.empty());
		ASSERT_GE(shape.material.embeddedTexture, 0);
		EXPECT_FALSE(shape.material.clampU);
		EXPECT_FALSE(shape.material.clampV);
		EXPECT_TRUE(shape.material.alphaBlend);
		EXPECT_FALSE(shape.material.alphaTest);
		EXPECT_EQ(shape.material.uvScroll, (std::array<float, 2>{})); // still
		// Vertex colors and the white material as the other groups
		EXPECT_EQ(shape.colors[3], 153);
		EXPECT_EQ(shape.material.diffuse, (std::array<float, 3>{ 1.0f, 1.0f, 1.0f }));
	}
	// One texturing property and one texture for every glitter shape; every block is read
	EXPECT_EQ(read->meshes[0].material.embeddedTexture, read->meshes[1].material.embeddedTexture);
	EXPECT_TRUE(read->skipped.empty()) << read->skipped.begin()->first;
	// The texture: 128 square, 32-bit, 8 mipmaps, white with the flecks in its alpha
	const auto dds = NifFile::EmbeddedTexture(nif, read->meshes[0].material.embeddedTexture);
	ASSERT_TRUE(dds);
	uint32_t header[31];
	std::memcpy(header, dds->data() + 4, sizeof(header));
	EXPECT_EQ(header[2], 128u);
	EXPECT_EQ(header[3], 128u);
	EXPECT_EQ(header[6], 8u);
	EXPECT_EQ(header[21], 32u);
	const auto alpha = UgcGlitter::FleckAlpha(glitter);
	for (size_t i = 0; i < alpha.size(); i++) {
		ASSERT_EQ(static_cast<uint8_t>((*dds)[128 + i * 4]), 255);
		ASSERT_EQ(static_cast<uint8_t>((*dds)[128 + i * 4 + 3]), alpha[i]) << i;
	}
	for (const auto* type : { "NiTexturingProperty", "NiSourceTexture", "NiPersistentSrcTextureRendererData" }) EXPECT_NE(nif.find(type), std::string::npos) << type;

	// The dashboard's encoding carries the UVs
	const auto encoded = NifFile::Encode(*read, { "glitter", "glitter" });
	uint32_t length = 0;
	std::memcpy(&length, encoded.data(), 4);
	const auto header2 = nlohmann::json::parse(encoded.substr(4, length));
	EXPECT_TRUE(header2["meshes"][0]["uv"].get<bool>());
}

// The sparkle texture: the same every time, flat sparkles at SPARKLE_ALPHA covering about the amount asked for, a
// sparkle 3 pixels wide; its first mipmaps keep the sparkles' alpha. The tile (how fast the client's fixed layer motion
// crosses sparkles) grows with the speed, the texture with it.
TEST(UgcGlitter, SparkleTexture) {
	const UgcGlitter::Params params;
	EXPECT_FLOAT_EQ(params.SparkleTile(), 7.5f);
	EXPECT_EQ(params.SparkleTextureSize(), 256);
	const auto alpha = UgcGlitter::SparkleAlpha(params);
	ASSERT_EQ(alpha.size(), 256u * 256u);
	EXPECT_EQ(alpha, UgcGlitter::SparkleAlpha(params));
	EXPECT_EQ(*std::max_element(alpha.begin(), alpha.end()), UgcGlitter::SPARKLE_ALPHA);
	double covered = 0;
	for (const auto a : alpha) covered += a / static_cast<double>(UgcGlitter::SPARKLE_ALPHA);
	EXPECT_NEAR(covered / alpha.size(), 0.05, 0.015); // overlaps make it a little less
	// One sparkle alone stays under the client's alpha test (GREATEREQUAL 127) with 2 or 3 layers averaged, two meet it
	EXPECT_LT(UgcGlitter::SPARKLE_ALPHA / 2, 127);
	EXPECT_GE(UgcGlitter::SPARKLE_ALPHA * 2 / 3, 127);
	EXPECT_LT(UgcGlitter::SPARKLE_ALPHA / 3, 127);
	const auto mips = UgcGlitter::Mipmaps(alpha, 2);
	ASSERT_EQ(mips.size(), 9u); // 256 .. 1
	EXPECT_EQ(*std::max_element(mips[1].begin(), mips[1].end()), UgcGlitter::SPARKLE_ALPHA);
	EXPECT_EQ(*std::max_element(mips[2].begin(), mips[2].end()), UgcGlitter::SPARKLE_ALPHA);
	EXPECT_LT(*std::max_element(mips[8].begin(), mips[8].end()), 127);
	// Faster: a bigger tile and texture; more: more covered
	UgcGlitter::Params fast = params;
	fast.speed = 2.0f;
	EXPECT_FLOAT_EQ(fast.SparkleTile(), 15.0f);
	EXPECT_EQ(fast.SparkleTextureSize(), 512);
	UgcGlitter::Params more = params;
	more.sparkleAmount = 10.0f;
	const auto moreAlpha = UgcGlitter::SparkleAlpha(more);
	EXPECT_GT(std::count(moreAlpha.begin(), moreAlpha.end(), UgcGlitter::SPARKLE_ALPHA), std::count(alpha.begin(), alpha.end(), UgcGlitter::SPARKLE_ALPHA));
	// Colors: white taking the tint of the brick's color, at the brightness
	EXPECT_EQ(UgcGlitter::SparkleColor({ 0.0f, 0.5f, 1.0f, 0.4f }, params), glm::vec4(0.7f, 0.85f, 1.0f, 1.0f));
	UgcGlitter::Params dim = params;
	dim.sparkleTint = 0.0f;
	dim.sparkleBrightness = 50.0f;
	EXPECT_EQ(UgcGlitter::SparkleColor({ 0.0f, 0.5f, 1.0f, 0.4f }, dim), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
}

// The sparkle group as the client's own Distortion Directional shapes (S79__pond_ripplesShape): the glitter bricks'
// triangles lifted off them along their normals, the sparkles' vertex colors, UVs on the sparkle tile placed per
// brick apart from the flecks, the sparkle texture stored in the file (no transform), alpha tested
TEST(UgcFormats, SparkleNifReadsBack) {
	auto mesh = Quad({ 0.0f, 0.5f, 1.0f, 0.6f });
	mesh.brickSeeds.assign(4, 99);
	const UgcGlitter::Params glitter;
	const auto nif = UgcFormats::WriteLodNif("SceneNode_Model", {
		{ "S21_GlitterAlpha_Model", true, { { 0.0f, 100.0f, "LOD_0", { &mesh } } }, 0.0f, &glitter },
		{ "S79_GlitterSparkle_Model", false, { { 0.0f, 100.0f, "LOD_0", { &mesh } } }, 0.0f, &glitter, true } });
	std::string error;
	const auto read = NifFile::Parse(nif, 0, error);
	ASSERT_TRUE(read) << error;
	ASSERT_EQ(read->meshes.size(), 2u);
	EXPECT_TRUE(read->skipped.empty());
	const auto& flecks = read->meshes[0];
	const auto& sparkles = read->meshes[1];
	EXPECT_EQ(sparkles.material.shaderTag, 79);
	EXPECT_TRUE(sparkles.material.alphaTest);
	EXPECT_EQ(sparkles.material.alphaThreshold, 127);
	EXPECT_FALSE(sparkles.material.alphaBlend);
	EXPECT_FLOAT_EQ(sparkles.material.alpha, 1.0f);
	ASSERT_GE(sparkles.material.embeddedTexture, 0);
	EXPECT_NE(sparkles.material.embeddedTexture, flecks.material.embeddedTexture);
	ASSERT_EQ(sparkles.positions.size(), 12u);
	for (size_t v = 0; v < 4; v++) {
		EXPECT_FLOAT_EQ(sparkles.positions[v * 3 + 2], UgcGlitter::SPARKLE_LIFT); // off the quad, along its normal
		const auto uv = UgcGlitter::Uv(mesh.positions[v], mesh.normals[v], glitter.SparkleTile(), 99, UgcGlitter::eLayer::SPARKLES);
		EXPECT_FLOAT_EQ(sparkles.uvs[v * 2], uv.x);
		EXPECT_FLOAT_EQ(sparkles.uvs[v * 2 + 1], uv.y);
		EXPECT_NE(sparkles.uvs[v * 2], flecks.uvs[v * 2]);
		// White taking 30% of the brick's color, opaque
		EXPECT_EQ(sparkles.colors[v * 4], 179);
		EXPECT_EQ(sparkles.colors[v * 4 + 2], 255);
		EXPECT_EQ(sparkles.colors[v * 4 + 3], 255);
	}
	const auto dds = NifFile::EmbeddedTexture(nif, sparkles.material.embeddedTexture);
	ASSERT_TRUE(dds);
	uint32_t header[31];
	std::memcpy(header, dds->data() + 4, sizeof(header));
	EXPECT_EQ(header[2], 256u);
	EXPECT_EQ(header[6], 9u);
	// The icon leaves the sparkles out
	EXPECT_EQ(UgcModel::FromNif(*read, {}, { 79 }).transparent.TriangleCount() + UgcModel::FromNif(*read, {}, { 79 }).opaque.TriangleCount(), 2u);
	EXPECT_EQ(UgcModel::FromNif(*read).opaque.TriangleCount() + UgcModel::FromNif(*read).transparent.TriangleCount(), 4u);
}

// Glitter colors (a Materials.xml glitter type or glitter_colors) get groups of their own, opaque and transparent,
// with every level; off (shader_glitter 0) they stay plastic and nothing changes
TEST(UgcShaders, GlitterGroups) {
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	library.SetMaterials({ { 5001, { 67, 84, 147, 255, "glitter" } }, { 5002, { 240, 143, 28, 150, "glitter" } }, { 21, { 200, 0, 0, 255, "shinyPlastic" } },
		{ 40, { 238, 238, 238, 150, "shinyPlastic" } } });
	const std::string lxfml = R"(<LXFML versionMajor="5"><Bricks>
		<Brick><Part designID="3001" materials="5001"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="5002"><Bone transformation="1,0,0,0,1,0,0,0,1,3,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="5002"><Bone transformation="1,0,0,0,1,0,0,0,1,6,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,9,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="40"><Bone transformation="1,0,0,0,1,0,0,0,1,12,0,0"/></Part></Brick>
		</Bricks></LXFML>)";
	auto settings = SmallSettings();
	settings.build.colorVariation = 0.0f;
	settings.shaders.glitter = 21;
	settings.shaders.sparkle = 79;
	const auto outcome = UgcJobs::ProcessModel(lxfml, library, settings, 7);
	ASSERT_TRUE(outcome.ok) << outcome.error;
	const auto nif = *ZCompression::Gunzip(outcome.files.at("model.nif.gz"));
	std::string error;
	for (const uint32_t level : { 0u, 1u }) {
		const auto read = NifFile::Parse(nif, level, error);
		ASSERT_TRUE(read) << error;
		for (const auto* name : { "S01_Opaque_Model", "S21_Glitter_Model", "S01_Alpha_Model", "S21_GlitterAlpha_Model", "S79_GlitterSparkle_Model" }) EXPECT_TRUE(read->nodes.contains(name)) << name;
		std::map<std::pair<int32_t, bool>, size_t> triangles; // (tag, transparent) -> triangles
		for (const auto& mesh : read->meshes) {
			bool seeThrough = false;
			for (size_t i = 3; i < mesh.colors.size(); i += 4) seeThrough = seeThrough || mesh.colors[i] < 250;
			triangles[{ mesh.material.shaderTag, seeThrough }] += mesh.indices.size() / 3;
			// Only the glitter and sparkle shapes are textured, only the sparkles alpha tested
			EXPECT_EQ(mesh.material.embeddedTexture >= 0, mesh.material.shaderTag == 21 || mesh.material.shaderTag == 79);
			EXPECT_EQ(!mesh.uvs.empty(), mesh.material.shaderTag == 21 || mesh.material.shaderTag == 79);
			EXPECT_EQ(mesh.material.alphaTest, mesh.material.shaderTag == 79);
		}
		// The sparkles: over every glitter brick, opaque and transparent, one shape per piece
		EXPECT_EQ((triangles[{ 79, false }]), 36u);
		EXPECT_EQ((triangles[{ 21, false }]), 12u);
		EXPECT_EQ((triangles[{ 21, true }]), 24u); // one shape per brick, as the other transparent bricks
		EXPECT_EQ((triangles[{ 1, false }]), 12u);
		EXPECT_EQ((triangles[{ 1, true }]), 12u);
	}
	EXPECT_NE(outcome.stats.find("\"S21_Glitter_Model\":12"), std::string::npos) << outcome.stats;
	EXPECT_NE(outcome.stats.find("\"S21_GlitterAlpha_Model\":24"), std::string::npos) << outcome.stats;
	EXPECT_NE(outcome.stats.find("\"S01_Alpha_Model\":12"), std::string::npos) << outcome.stats;
	EXPECT_NE(outcome.stats.find("\"S79_GlitterSparkle_Model\":36"), std::string::npos) << outcome.stats;

	// The icon reads the glitter back by the tag (transparent too)
	const auto read = NifFile::Parse(nif, 0, error);
	const auto back = UgcModel::FromNif(*read, settings.shaders.TagLooks(), settings.shaders.OverlayTags());
	EXPECT_EQ(std::count(back.opaque.looks.begin(), back.opaque.looks.end(), UgcModel::eLook::GLITTER), 8);
	EXPECT_EQ(back.opaque.TriangleCount() + back.transparent.TriangleCount(), 60u); // no sparkles
	// No sparkles (shader_glitter_sparkle 0): the glitter groups alone
	settings.shaders.sparkle = 0;
	const auto noSparkles = UgcJobs::ProcessModel(lxfml, library, settings, 7);
	ASSERT_TRUE(noSparkles.ok);
	const auto noSparklesRead = NifFile::Parse(*ZCompression::Gunzip(noSparkles.files.at("model.nif.gz")), 0, error);
	ASSERT_TRUE(noSparklesRead);
	EXPECT_FALSE(noSparklesRead->nodes.contains("S79_GlitterSparkle_Model"));
	EXPECT_EQ(noSparkles.files.at("icon.png"), outcome.files.at("icon.png"));
	settings.shaders.sparkle = 79;
	EXPECT_EQ(std::count(back.transparent.looks.begin(), back.transparent.looks.end(), UgcModel::eLook::GLITTER), 16);

	// Combined transparent bricks: one glitter shape
	settings.combineTransparent = true;
	const auto combined = UgcJobs::ProcessModel(lxfml, library, settings, 7);
	ASSERT_TRUE(combined.ok);
	const auto combinedRead = NifFile::Parse(*ZCompression::Gunzip(combined.files.at("model.nif.gz")), 0, error);
	ASSERT_TRUE(combinedRead);
	size_t transparentGlitterShapes = 0;
	for (const auto& mesh : combinedRead->meshes) transparentGlitterShapes += mesh.material.shaderTag == 21 && mesh.colors[3] < 250;
	EXPECT_EQ(transparentGlitterShapes, 1u);

	// Off: the glitter colors are plastic, in S01, and the files are the same as without glitter rules at all
	settings.combineTransparent = false;
	settings.shaders.glitter = 0;
	const auto off = UgcJobs::ProcessModel(lxfml, library, settings, 7);
	settings.build.looks.materialTypes.erase("glitter");
	settings.shaders.glitterParams.tile = 3.0f;
	settings.shaders.glitterParams.flecks = 7;
	settings.shaders.glitterParams.speed = 3.0f;
	settings.icon.glitter = settings.shaders.glitterParams;
	const auto noRules = UgcJobs::ProcessModel(lxfml, library, settings, 7);
	ASSERT_TRUE(off.ok && noRules.ok);
	for (const auto* name : { "model.nif.checksum", "model.noao.nif.gz", "icon.png" }) EXPECT_EQ(off.files.at(name), noRules.files.at(name)) << name;
	const auto offRead = NifFile::Parse(*ZCompression::Gunzip(off.files.at("model.nif.gz")), 0, error);
	ASSERT_TRUE(offRead);
	for (const auto& mesh : offRead->meshes) EXPECT_EQ(mesh.material.shaderTag, 1);
	EXPECT_EQ(off.stats.find("groups"), std::string::npos);
}

// Each glitter brick gets its own fleck pattern: bricks a whole number of tiles apart (whose projected UVs are the same
// but for whole tiles) get different ones, the same model made again the same ones, another model others;
// glitter_random 0 puts the same pattern on every brick as before
TEST(UgcShaders, GlitterIsPlacedPerBrick) {
	// Brick seeds: never 0, different bricks and models different, the same brick the same
	EXPECT_NE(UgcGlitter::BrickSeed(7, 0), 0u);
	EXPECT_EQ(UgcGlitter::BrickSeed(7, 3), UgcGlitter::BrickSeed(7, 3));
	EXPECT_NE(UgcGlitter::BrickSeed(7, 3), UgcGlitter::BrickSeed(7, 4));
	EXPECT_NE(UgcGlitter::BrickSeed(7, 3), UgcGlitter::BrickSeed(8, 3));
	// Seed 0 is the plain projection; a seed turns and moves it
	const glm::vec3 p(0.4f, 0.8f, 0.2f), q(1.2f, 0.8f, 0.2f);
	EXPECT_EQ(UgcGlitter::Uv(p, { 0, 0, 1 }, 1.6f, 0), UgcGlitter::Uv(p, { 0, 0, 1 }, 1.6f));
	const auto a = UgcGlitter::Uv(p, { 0, 0, 1 }, 1.6f, 12345), b = UgcGlitter::Uv(q, { 0, 0, 1 }, 1.6f, 12345);
	EXPECT_NE(a, UgcGlitter::Uv(p, { 0, 0, 1 }, 1.6f));
	EXPECT_NEAR(glm::length(b - a), 0.5f, 1e-5f); // turned and moved, not stretched: still 0.8 / 1.6 tiles apart
	EXPECT_NE(UgcGlitter::Uv(p, { 0, 0, 1 }, 1.6f, 12345), UgcGlitter::Uv(p, { 0, 0, 1 }, 1.6f, 54321));

	UgcBricks::BrickLibrary library(MakeRes(), 0);
	library.SetMaterials({ { 5001, { 67, 84, 147, 150, "glitter" } } });
	// Three transparent glitter bricks 3.2 apart (two tiles of 1.6)
	const std::string lxfml = R"(<LXFML versionMajor="5"><Bricks>
		<Brick><Part designID="3001" materials="5001"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="5001"><Bone transformation="1,0,0,0,1,0,0,0,1,3.2,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="5001"><Bone transformation="1,0,0,0,1,0,0,0,1,6.4,0,0"/></Part></Brick>
		</Bricks></LXFML>)";
	auto settings = SmallSettings();
	settings.shaders.glitter = 21;
	// Each glitter shape's UVs' fractions (the texture wraps), LOD 0
	const auto patterns = [&](const UgcJobs::Outcome& outcome) {
		std::string error;
		const auto read = NifFile::Parse(*ZCompression::Gunzip(outcome.files.at("model.nif.gz")), 0, error);
		EXPECT_TRUE(read) << error;
		std::vector<std::vector<float>> out;
		for (const auto& mesh : read->meshes) {
			if (mesh.material.shaderTag != 21) continue;
			std::vector<float> fractions;
			for (const auto uv : mesh.uvs) fractions.push_back(std::round((uv - std::floor(uv)) * 1000.0f) / 1000.0f);
			out.push_back(fractions);
		}
		return out;
	};
	const auto random = UgcJobs::ProcessModel(lxfml, library, settings, 7);
	ASSERT_TRUE(random.ok) << random.error;
	const auto perBrick = patterns(random);
	ASSERT_EQ(perBrick.size(), 3u); // one shape per transparent brick
	EXPECT_NE(perBrick[0], perBrick[1]);
	EXPECT_NE(perBrick[1], perBrick[2]);
	EXPECT_NE(perBrick[0], perBrick[2]);
	// The same model made again: the same file; another model (seed) with the same bricks: other patterns
	EXPECT_EQ(random.files.at("model.nif.checksum"), UgcJobs::ProcessModel(lxfml, library, settings, 7).files.at("model.nif.checksum"));
	EXPECT_NE(patterns(UgcJobs::ProcessModel(lxfml, library, settings, 8)), perBrick);
	// Off: the same pattern on every brick, as before
	settings.shaders.glitterParams.random = false;
	const auto off = UgcJobs::ProcessModel(lxfml, library, settings, 7);
	const auto same = patterns(off);
	ASSERT_EQ(same.size(), 3u);
	EXPECT_EQ(same[0], same[1]);
	EXPECT_EQ(same[1], same[2]);

	// The icon draws the flecks where the .nif has them (its UVs, read back), so it changes with the placement
	std::string error;
	const auto read = NifFile::Parse(*ZCompression::Gunzip(random.files.at("model.nif.gz")), 0, error);
	ASSERT_TRUE(read) << error;
	const auto back = UgcModel::FromNif(*read, settings.shaders.TagLooks());
	EXPECT_EQ(back.transparent.uvs.size(), back.transparent.positions.size());
	EXPECT_NE(random.files.at("icon.png"), off.files.at("icon.png"));
}

// Glitter in the icon: the texture's flecks over the color before the light, where they are at the start
TEST(UgcShaders, IconsDrawGlitterFlecks) {
	UgcModel::Model model;
	model.opaque = Quad({ 0.2f, 0.2f, 0.6f, 1.0f });
	UgcRender::IconOptions options;
	options.size = 64;
	options.supersample = 1;
	options.yawDegrees = 0.0f;
	options.pitchDegrees = 0.0f;
	options.shadows = 0.0f;
	options.glitter.tile = 0.5f;
	options.glitter.flecks = 60;
	options.glitter.fleckSize = 0.0156f; // as big against the tile as the defaults
	const auto plain = UgcRender::RenderIcon(model, options);
	model.opaque.looks.assign(4, UgcModel::eLook::GLITTER);
	const auto glitter = UgcRender::RenderIcon(model, options);
	ASSERT_EQ(plain.rgba.size(), glitter.rgba.size());
	size_t brighter = 0, same = 0;
	for (size_t i = 0; i < plain.rgba.size(); i += 4) {
		if (plain.rgba[i + 3] == 0) continue;
		if (glitter.rgba[i] > plain.rgba[i] + 20) brighter++;
		else if (glitter.rgba[i] == plain.rgba[i]) same++;
	}
	EXPECT_GT(brighter, 10u); // flecks
	EXPECT_GT(same, brighter * 5); // on plain plastic
	// Transparent glitter too, and it stays see-through
	UgcModel::Model clear;
	clear.transparent = Quad({ 0.2f, 0.2f, 0.6f, 0.5f });
	clear.transparent.looks.assign(4, UgcModel::eLook::GLITTER);
	const auto clearIcon = UgcRender::RenderIcon(clear, options);
	clear.transparent.looks.clear();
	const auto clearPlain = UgcRender::RenderIcon(clear, options);
	EXPECT_NE(clearIcon.rgba, clearPlain.rgba);
	for (size_t i = 3; i < clearIcon.rgba.size(); i += 4) EXPECT_EQ(clearIcon.rgba[i], clearPlain.rgba[i]);
}

// Satin colors: transparent at satin_opacity instead of the transparent opacity, and milky; the others as they were
TEST(UgcModel, SatinColors) {
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	library.SetMaterials({ { 360, { 252, 252, 252, 150 } }, { 367, { 35, 120, 65, 150 } }, { 43, { 0, 50, 200, 150 } } });
	std::string error;
	const auto parts = UgcModel::ParseLxfml(R"(<LXFML versionMajor="5"><Bricks>
		<Brick><Part designID="3001" materials="367"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="43"><Bone transformation="1,0,0,0,1,0,0,0,1,3,0,0"/></Part></Brick>
		</Bricks></LXFML>)", error);
	UgcModel::BuildOptions options;
	options.colorVariation = 0.0f;
	const auto before = UgcModel::Build(parts, library, options);
	options.satinColors = { 360, 367 };
	options.satinOpacity = 80.0f;
	options.satinWhiten = 25.0f;
	const auto satin = UgcModel::Build(parts, library, options);
	ASSERT_EQ(satin.transparent.colors.size(), 16u);
	EXPECT_NEAR(before.transparent.colors[0].a, 0.5882f, 1e-4f);
	EXPECT_NEAR(satin.transparent.colors[0].a, 0.8f, 1e-6f);
	const auto linear = UgcPalette::SrgbToLinear(glm::vec3(before.transparent.colors[0]));
	const auto milky = UgcPalette::LinearToSrgb(glm::mix(linear, glm::vec3(1.0f), 0.25f));
	for (int c = 0; c < 3; c++) EXPECT_NEAR(satin.transparent.colors[0][c], milky[c], 1e-5f);
	// The other transparent brick as before
	EXPECT_EQ(satin.transparent.colors[8], before.transparent.colors[8]);
	// Satin's own group is the transparent one: no look
	EXPECT_TRUE(satin.transparent.looks.empty());
}

TEST(UgcHsr, OffIsByteIdenticalToBefore) {
	// remove_hidden_faces=0 makes the same files whatever removes hidden faces. The hashes were taken with GCC on
	// x86-64 Linux.
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	auto settings = SmallSettings();
	settings.hsr.enabled = false;
	const auto outcome = UgcJobs::ProcessModel(LOOKS_LXFML, library, settings, 7);
	ASSERT_TRUE(outcome.ok) << outcome.error;
	const auto other = UgcJobs::ProcessModel(LXFML5, library, settings, 99);
	ASSERT_TRUE(other.ok) << other.error;
#if defined(__linux__) && defined(__x86_64__) && defined(__GNUC__) && !defined(__clang__)
	EXPECT_EQ(UgcFormats::Md5Hex(*ZCompression::Gunzip(outcome.files.at("model.nif.gz"))), "a8f6b7022a7c087d1659d67a16e9791f");
	EXPECT_EQ(UgcFormats::Md5Hex(*ZCompression::Gunzip(outcome.files.at("model.noao.nif.gz"))), "2da0810cbd2470732e4732a8128da2ab");
	EXPECT_EQ(UgcFormats::Md5Hex(outcome.files.at("icon.png")), "032ff7df236a636a4c609071d9b46181");
	EXPECT_EQ(UgcFormats::Md5Hex(*ZCompression::Gunzip(other.files.at("model.nif.gz"))), "289088a76248d35203cd961f2772ae21");
#endif
}

namespace {
	// A quad a b c d (in order around it) facing `normal`, as two triangles wound to face it
	void AddQuad(UgcModel::Mesh& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 normal) {
		const auto base = static_cast<uint32_t>(mesh.positions.size());
		for (const auto& p : { a, b, c, d }) {
			mesh.positions.push_back(p);
			mesh.normals.push_back(normal);
			mesh.colors.push_back(glm::vec4(1.0f));
		}
		if (glm::dot(glm::cross(b - a, c - a), normal) >= 0.0f) mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
		else mesh.indices.insert(mesh.indices.end(), { base, base + 2, base + 1, base, base + 3, base + 2 });
	}

	// An axis aligned box [min, max], faces outwards (or inwards)
	void AddBox(UgcModel::Mesh& mesh, glm::vec3 lo, glm::vec3 hi, bool inwards = false) {
		const float s = inwards ? -1.0f : 1.0f;
		AddQuad(mesh, { lo.x, lo.y, lo.z }, { lo.x, hi.y, lo.z }, { lo.x, hi.y, hi.z }, { lo.x, lo.y, hi.z }, { -s, 0, 0 });
		AddQuad(mesh, { hi.x, lo.y, lo.z }, { hi.x, hi.y, lo.z }, { hi.x, hi.y, hi.z }, { hi.x, lo.y, hi.z }, { s, 0, 0 });
		AddQuad(mesh, { lo.x, lo.y, lo.z }, { hi.x, lo.y, lo.z }, { hi.x, lo.y, hi.z }, { lo.x, lo.y, hi.z }, { 0, -s, 0 });
		AddQuad(mesh, { lo.x, hi.y, lo.z }, { hi.x, hi.y, lo.z }, { hi.x, hi.y, hi.z }, { lo.x, hi.y, hi.z }, { 0, s, 0 });
		AddQuad(mesh, { lo.x, lo.y, lo.z }, { hi.x, lo.y, lo.z }, { hi.x, hi.y, lo.z }, { lo.x, hi.y, lo.z }, { 0, 0, -s });
		AddQuad(mesh, { lo.x, lo.y, hi.z }, { hi.x, lo.y, hi.z }, { hi.x, hi.y, hi.z }, { lo.x, hi.y, hi.z }, { 0, 0, s });
	}

	// A room [-2, 2]^3 seen from inside (its walls face inwards), with a doorway in its +Z wall unless `closed`, and a
	// small box in the middle of it (triangles from 0 to 11, the room's after)
	UgcModel::Mesh Room(bool closed) {
		UgcModel::Mesh mesh;
		AddBox(mesh, glm::vec3(-0.3f), glm::vec3(0.3f));
		const float w = 2.0f;
		const glm::vec3 in(0, 0, -1);
		AddQuad(mesh, { -w, -w, -w }, { w, -w, -w }, { w, w, -w }, { -w, w, -w }, { 0, 0, 1 });
		AddQuad(mesh, { -w, -w, -w }, { -w, w, -w }, { -w, w, w }, { -w, -w, w }, { 1, 0, 0 });
		AddQuad(mesh, { w, -w, -w }, { w, w, -w }, { w, w, w }, { w, -w, w }, { -1, 0, 0 });
		AddQuad(mesh, { -w, -w, -w }, { w, -w, -w }, { w, -w, w }, { -w, -w, w }, { 0, 1, 0 });
		AddQuad(mesh, { -w, w, -w }, { w, w, -w }, { w, w, w }, { -w, w, w }, { 0, -1, 0 });
		// The +Z wall around a doorway x -0.5..0.5, y -2..0
		AddQuad(mesh, { -w, -w, w }, { -0.5f, -w, w }, { -0.5f, w, w }, { -w, w, w }, in);
		AddQuad(mesh, { 0.5f, -w, w }, { w, -w, w }, { w, w, w }, { 0.5f, w, w }, in);
		AddQuad(mesh, { -0.5f, 0, w }, { 0.5f, 0, w }, { 0.5f, w, w }, { -0.5f, w, w }, in);
		if (closed) AddQuad(mesh, { -0.5f, -w, w }, { 0.5f, -w, w }, { 0.5f, 0, w }, { -0.5f, 0, w }, in);
		return mesh;
	}
}

TEST(UgcHsr, GroundPlaneHidesTheUnderside) {
	// An upside down box open at the bottom: its ceiling (the slab's underside, triangles 4 and 5) is only seen from
	// below
	UgcModel::Model model;
	AddBox(model.opaque, glm::vec3(-1.0f, 1.0f, -1.0f), glm::vec3(1.0f, 1.2f, 1.0f));
	AddBox(model.opaque, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(-0.8f, 1.0f, 1.0f));
	AddBox(model.opaque, glm::vec3(0.8f, 0.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
	AddBox(model.opaque, glm::vec3(-0.8f, 0.0f, -1.0f), glm::vec3(0.8f, 1.0f, -0.8f));
	AddBox(model.opaque, glm::vec3(-0.8f, 0.0f, 0.8f), glm::vec3(0.8f, 1.0f, 1.0f));
	UgcHsr::Options options;
	options.resolution = 256;
	auto without = model;
	const auto seen = UgcHsr::RemoveHiddenFaces(without, options);
	EXPECT_TRUE(seen.kept[4] && seen.kept[5]);
	options.groundPlane = true;
	const auto with = UgcHsr::RemoveHiddenFaces(model, options);
	EXPECT_FALSE(with.kept[4] || with.kept[5]);
	EXPECT_TRUE(with.kept[6] && with.kept[7]); // the roof
}

// Stopping the server cancels the jobs being made: Checkpoint throws until the cancel is cleared
TEST(UgcThrottle, CancelStopsJobsAtTheirNextCheckpoint) {
	UgcThrottle::Cancel(true);
	EXPECT_TRUE(UgcThrottle::IsCancelled());
	EXPECT_THROW(UgcThrottle::Checkpoint(), UgcThrottle::Cancelled);
	UgcThrottle::Cancel(false);
	EXPECT_NO_THROW(UgcThrottle::Checkpoint());
}

// Transparent shapes get a material alpha of 0.9999 as the game's own brick models' S01_Alpha shapes: the client only
// blends a shape whose material alpha is under 0.99999 (ShaderCommon::GetAlphaFlags); opaque shapes keep 1.0
TEST(UgcFormats, TransparentShapesHaveTheGamesAlphaMaterial) {
	const auto mesh = Quad({ 0.2f, 0.4f, 0.8f, 0.6f });
	const auto alphaOf = [](const std::string& nif) {
		std::vector<float> alphas;
		// Every NiMaterialProperty block ends with glossiness 4.0 and the alpha: find glossiness then read the alpha
		for (size_t i = 0; i + 8 <= nif.size(); i++) {
			float gloss = 0.0f;
			std::memcpy(&gloss, nif.data() + i, 4);
			if (gloss != 4.0f) continue;
			float alpha = 0.0f;
			std::memcpy(&alpha, nif.data() + i + 4, 4);
			if (alpha > 0.9f && alpha <= 1.0f) alphas.push_back(alpha);
		}
		return alphas;
	};
	const auto mixed = UgcFormats::WriteLodNif("SceneNode_Model", {
		{ "S01_Opaque_Model", false, { { 0.0f, 100.0f, "LOD_0", { &mesh } } } },
		{ "S01_Alpha_Model", true, { { 0.0f, 100.0f, "LOD_0", { &mesh } } } } });
	const auto alphas = alphaOf(mixed);
	EXPECT_NE(std::find(alphas.begin(), alphas.end(), 1.0f), alphas.end());
	EXPECT_NE(std::find(alphas.begin(), alphas.end(), 0.9999f), alphas.end());
	const auto opaque = UgcFormats::WriteLodNif("SceneNode_Model", { { "S01_Opaque_Model", false, { { 0.0f, 100.0f, "LOD_0", { &mesh } } } } });
	const auto opaqueAlphas = alphaOf(opaque);
	EXPECT_EQ(std::find(opaqueAlphas.begin(), opaqueAlphas.end(), 0.9999f), opaqueAlphas.end());
}

// An icon drawn again keeps the make's time with its icon's: ms.icon is the new one's, ms.total changes by the difference
TEST(UgcJobs, IconDrawnAgainChangesTheMakesTime) {
	double change = 0.0;
	const auto updated = UgcJobs::WithIconTime(R"({"bricks":3,"ms":{"build":100,"icon":40,"total":500}})", 70.4, change);
	ASSERT_TRUE(updated);
	const auto stats = nlohmann::json::parse(*updated);
	EXPECT_EQ(stats["ms"]["icon"], 70);
	EXPECT_EQ(stats["ms"]["total"], 530);
	EXPECT_EQ(stats["ms"]["build"], 100);
	EXPECT_EQ(stats["bricks"], 3);
	EXPECT_NEAR(change, 30.4, 1e-9);
	// Stats from before the icon was timed: all of the new icon's time is added
	const auto old = UgcJobs::WithIconTime(R"({"ms":{"total":500}})", 20.0, change);
	ASSERT_TRUE(old);
	EXPECT_EQ(nlohmann::json::parse(*old)["ms"]["total"], 520);
	EXPECT_FALSE(UgcJobs::WithIconTime("not json", 20.0, change));
}

namespace {
	// A cluttered scene for the ray backends: the room with its doorway and box, a stack of boxes that touch and
	// overlap, and a staircase of thin slabs
	UgcModel::Mesh Clutter() {
		auto mesh = Room(false);
		for (int i = 0; i < 4; i++) AddBox(mesh, glm::vec3(-1.5f + i * 0.4f, -2.0f + i * 0.3f, -1.5f), glm::vec3(-1.0f + i * 0.4f, -1.6f + i * 0.3f, -0.8f));
		for (int i = 0; i < 6; i++) AddBox(mesh, glm::vec3(0.5f, -2.0f + i * 0.2f, -1.8f + i * 0.25f), glm::vec3(1.8f, -1.95f + i * 0.2f, -1.4f + i * 0.25f));
		return mesh;
	}

	// The backends other than embree that this build and machine can use (the GPUs')
	std::vector<UgcRays::eBackend> OtherBackends() {
		std::vector<UgcRays::eBackend> backends;
		for (const auto backend : { UgcRays::eBackend::HIPRT, UgcRays::eBackend::EMBREE_GPU }) {
			if (UgcRays::Available(backend)) backends.push_back(backend);
		}
		return backends;
	}
}

TEST(UgcHsr, RendersFromAround) {
	EXPECT_EQ(UgcRender::SphereDirections().size(), 42u);

	// A small box inside the big one: its faces can't be seen
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	std::string error;
	const auto parts = UgcModel::ParseLxfml(R"(<LXFML versionMajor="5"><Bricks>
		<Brick><Part designID="3002" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
		<Brick><Part designID="3001" materials="21"><Bone transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
		</Bricks></LXFML>)", error);
	auto model = UgcModel::Build(parts, library);
	UgcHsr::Options options;
	options.resolution = 256;
	const auto result = UgcHsr::RemoveHiddenFaces(model, options);
	EXPECT_EQ(result.trianglesRemoved, 12u);
	EXPECT_EQ(model.opaque.TriangleCount(), 12u);

	// A small box in a closed chamber whose only opening is a narrow chimney at the other end: nothing outside sees it
	// straight, so it is removed (it would only be seen by light bounced in through the chimney)
	UgcModel::Model chamber;
	auto& mesh = chamber.opaque;
	AddBox(mesh, glm::vec3(-0.9f, 0.0f, -0.15f), glm::vec3(-0.6f, 0.2f, 0.15f)); // the small box
	AddBox(mesh, glm::vec3(-1.2f, -0.2f, -1.2f), glm::vec3(1.2f, 0.0f, 1.2f));   // floor
	AddBox(mesh, glm::vec3(-1.2f, 0.0f, -1.2f), glm::vec3(-1.0f, 0.5f, 1.2f));   // walls
	AddBox(mesh, glm::vec3(1.0f, 0.0f, -1.2f), glm::vec3(1.2f, 0.5f, 1.2f));
	AddBox(mesh, glm::vec3(-1.0f, 0.0f, -1.2f), glm::vec3(1.0f, 0.5f, -1.0f));
	AddBox(mesh, glm::vec3(-1.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.5f, 1.2f));
	AddBox(mesh, glm::vec3(-1.2f, 0.5f, -1.2f), glm::vec3(0.6f, 0.7f, 1.2f));    // roof around the chimney's hole
	AddBox(mesh, glm::vec3(0.9f, 0.5f, -1.2f), glm::vec3(1.2f, 0.7f, 1.2f));
	AddBox(mesh, glm::vec3(0.6f, 0.5f, -1.2f), glm::vec3(0.9f, 0.7f, -0.15f));
	AddBox(mesh, glm::vec3(0.6f, 0.5f, 0.15f), glm::vec3(0.9f, 0.7f, 1.2f));
	AddBox(mesh, glm::vec3(0.5f, 0.7f, -0.25f), glm::vec3(0.6f, 2.2f, 0.25f));   // the chimney
	AddBox(mesh, glm::vec3(0.9f, 0.7f, -0.25f), glm::vec3(1.0f, 2.2f, 0.25f));
	AddBox(mesh, glm::vec3(0.6f, 0.7f, -0.25f), glm::vec3(0.9f, 2.2f, -0.15f));
	AddBox(mesh, glm::vec3(0.6f, 0.7f, 0.15f), glm::vec3(0.9f, 2.2f, 0.25f));
	const auto seen = UgcRender::VisibleFromAround(chamber, 512, false);
	ASSERT_EQ(seen.size(), mesh.TriangleCount());
	for (size_t t = 0; t < 12; t++) EXPECT_FALSE(seen[t]) << t;

	// Its files are the same every time
	auto settings = SmallSettings();
	const auto first = UgcJobs::ProcessModel(LXFML5, library, settings, 7);
	ASSERT_TRUE(first.ok) << first.error;
	EXPECT_EQ(first.files.at("model.nif.checksum"), UgcJobs::ProcessModel(LXFML5, library, settings, 7).files.at("model.nif.checksum"));
#if defined(__linux__) && defined(__x86_64__) && defined(__GNUC__) && !defined(__clang__)
	EXPECT_EQ(UgcFormats::Md5Hex(*ZCompression::Gunzip(first.files.at("model.nif.gz"))), "4bd664412c88333431842eb82a0abfac");
#endif
}

TEST(UgcProcessOptions, ParseApplyAndRecord) {
	UgcProcessOptions::Choice choice;
	ASSERT_TRUE(UgcProcessOptions::Parse("oidn  embree", choice));
	EXPECT_EQ(choice.rays, "embree");
	EXPECT_EQ(choice.denoise, "oidn");
	EXPECT_EQ(UgcProcessOptions::ToString(choice), "embree oidn");
	ASSERT_TRUE(UgcProcessOptions::Parse("default - oidn", choice));
	EXPECT_EQ(UgcProcessOptions::ToString(choice), "oidn");
	ASSERT_TRUE(UgcProcessOptions::Parse("", choice));
	EXPECT_TRUE(choice.Empty());
	EXPECT_FALSE(UgcProcessOptions::Parse("embree hiprt", choice)); // two backends
	EXPECT_FALSE(UgcProcessOptions::Parse("optix", choice));
	// Options stored by earlier versions named a hidden-face method: still read, the method ignored
	ASSERT_TRUE(UgcProcessOptions::Parse("embree toolbox off", choice));
	EXPECT_EQ(UgcProcessOptions::ToString(choice), "embree off");
	ASSERT_TRUE(UgcProcessOptions::Parse("fast", choice));
	EXPECT_TRUE(choice.Empty());

	// The shared names are the UGC server's
	for (const auto name : UgcProcessOptions::RAYS) EXPECT_EQ(UgcRays::Name(*UgcRays::Parse(name)), name);
	for (const auto name : UgcProcessOptions::DENOISE) EXPECT_EQ(UgcRender::Name(*UgcRender::ParseDenoise(name)), name);

	// Applied over the settings; what made a model is recorded as it was used
	UgcJobs::Settings settings;
	EXPECT_EQ(UgcProcessOptions::ToString(UgcJobs::MadeWith(settings)), "embree off");
	// builtin, the ray backend Embree replaced, is embree
	ASSERT_TRUE(UgcProcessOptions::Parse("builtin toolbox off", choice));
	EXPECT_EQ(UgcProcessOptions::ToString(choice), "embree off");
	ASSERT_TRUE(UgcProcessOptions::Parse("embree", choice));
	UgcJobs::ApplyOptions(settings, choice);
	EXPECT_EQ(settings.ao.rays, UgcRays::eBackend::EMBREE);
	EXPECT_EQ(settings.icon.ao.rays, UgcRays::eBackend::EMBREE);
	EXPECT_EQ(UgcProcessOptions::ToString(UgcJobs::MadeWith(settings)), "embree off");
	ASSERT_TRUE(UgcProcessOptions::Parse("hiprt oidn", choice));
	UgcJobs::ApplyOptions(settings, choice);
	const auto made = UgcJobs::MadeWith(settings);
	EXPECT_EQ(made.rays, UgcRays::Available(UgcRays::eBackend::HIPRT) ? "hiprt" : "embree");
	EXPECT_EQ(made.denoise, UgcRender::Available(UgcRender::eDenoise::OIDN) ? "oidn" : "off");

	// The make records it, in its stats too
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	auto small = SmallSettings();
	UgcJobs::ApplyOptions(small, choice);
	const auto outcome = UgcJobs::ProcessModel(LXFML5, library, small, 7);
	ASSERT_TRUE(outcome.ok) << outcome.error;
	EXPECT_EQ(outcome.options, UgcProcessOptions::ToString(UgcJobs::MadeWith(small)));
	const auto stats = nlohmann::json::parse(outcome.stats);
	EXPECT_EQ(stats["settings"]["rays"], made.rays);
	EXPECT_EQ(stats["settings"]["denoise"], made.denoise);
	EXPECT_FALSE(stats["settings"].contains("hsrMethod"));
}

TEST(UgcRender, DenoisedIconsTraceTheOcclusionPerPixel) {
	// A box standing on a floor, grey: denoised, the icon is drawn from the model before its bake with the occlusion
	// traced per pixel, so the floor is darker by the box than away from it; with few rays it is about what many give
	UgcModel::Model model;
	AddBox(model.opaque, glm::vec3(-4.0f, -0.2f, -4.0f), glm::vec3(4.0f, 0.0f, 4.0f));
	AddBox(model.opaque, glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.5f, 1.0f, 0.5f));
	UgcRender::IconOptions options;
	options.size = 48;
	options.supersample = 2;
	options.pitchDegrees = 60.0f;
	options.shadows = 0.0f;
	options.ao.distance = 2.0f;
	const auto normal = UgcRender::RenderIcon(model, options);
	options.denoise = UgcRender::eDenoise::OIDN;
	options.denoiseSamples = 2;
	const auto few = UgcRender::RenderIcon(model, options, nullptr, &model);
	if (!UgcRender::Available(UgcRender::eDenoise::OIDN)) {
		EXPECT_EQ(few.rgba, normal.rgba); // without it the option does nothing
		GTEST_SKIP() << "built without Open Image Denoise (DLU_OIDN)";
	}
	options.denoiseSamples = 64;
	const auto many = UgcRender::RenderIcon(model, options, nullptr, &model);
	ASSERT_EQ(few.rgba.size(), normal.rgba.size());
	double difference = 0.0, count = 0.0;
	for (size_t i = 0; i < few.rgba.size(); i += 4) {
		ASSERT_EQ(few.rgba[i + 3], normal.rgba[i + 3]) << i; // the same outline
		if (few.rgba[i + 3] != 255) continue;
		difference += std::abs(static_cast<double>(few.rgba[i]) - many.rgba[i]);
		count++;
	}
	EXPECT_LT(difference / count, 4.0);
	// Darker than the icon without occlusion only by the box (near the middle), not out at the floor's edges
	size_t darkened = 0, darkenedFar = 0;
	for (int y = 0; y < 48; y++) {
		for (int x = 0; x < 48; x++) {
			const size_t i = (static_cast<size_t>(y) * 48 + x) * 4;
			if (few.rgba[i + 3] != 255 || few.rgba[i] + 30 > normal.rgba[i]) continue;
			darkened++;
			if (std::abs(x - 24) > 14 || std::abs(y - 24) > 14) darkenedFar++;
		}
	}
	EXPECT_GT(darkened, 20u);
	EXPECT_EQ(darkenedFar, 0u);
}

TEST(UgcRays, NamesAndFallback) {
	for (const auto backend : { UgcRays::eBackend::EMBREE, UgcRays::eBackend::HIPRT, UgcRays::eBackend::EMBREE_GPU }) {
		EXPECT_EQ(UgcRays::Parse(UgcRays::Name(backend)), backend);
	}
	EXPECT_EQ(UgcRays::Parse("embree-gpu"), UgcRays::eBackend::EMBREE_GPU);
	EXPECT_EQ(UgcRays::Parse("builtin"), UgcRays::eBackend::EMBREE); // the backend Embree replaced
	EXPECT_FALSE(UgcRays::Parse("optix"));
	EXPECT_TRUE(UgcRays::Available(UgcRays::eBackend::EMBREE));
	// A backend this build or machine can't use falls back to embree, and says why
	for (const auto backend : { UgcRays::eBackend::HIPRT, UgcRays::eBackend::EMBREE_GPU }) {
		const bool available = UgcRays::Available(backend);
		EXPECT_EQ(UgcRays::Resolve(backend), available ? backend : UgcRays::eBackend::EMBREE);
		EXPECT_EQ(UgcRays::Problem(backend).empty(), available) << UgcRays::Problem(backend);
		// Made anyway: an Embree scene that works
		const auto scene = UgcRays::Make(backend, Clutter());
		EXPECT_NEAR(scene->Closest({ 0.3f, 0.1f, 0.1f }, { 1, 0, 0 }).t, 1.7f, 1e-5f) << UgcRays::Name(backend);
	}
	EXPECT_TRUE(UgcRays::Problem(UgcRays::eBackend::EMBREE).empty());
	EXPECT_EQ(UgcRays::Resolve(UgcRays::eBackend::EMBREE), UgcRays::eBackend::EMBREE);
	// An empty mesh is hit by nothing
	for (const auto backend : { UgcRays::eBackend::EMBREE, UgcRays::eBackend::HIPRT, UgcRays::eBackend::EMBREE_GPU }) {
		const auto scene = UgcRays::Make(backend, UgcModel::Mesh{});
		EXPECT_EQ(scene->Closest(glm::vec3(0.0f), glm::vec3(0, 1, 0)).triangle, UgcRays::NONE);
		EXPECT_FALSE(scene->Occluded(glm::vec3(0.0f), glm::vec3(0, 1, 0), 0.0f, 10.0f));
	}
}

TEST(UgcRays, FindsTheExpectedHits) {
	// The room of the clutter is [-2, 2]^3 (its walls face inwards; triangles 12 on), the box in it [-0.3, 0.3]^3
	// (triangles 0 to 11): rays whose hits are known
	const auto mesh = Clutter();
	for (const auto backend : { UgcRays::eBackend::EMBREE, UgcRays::eBackend::HIPRT, UgcRays::eBackend::EMBREE_GPU }) {
		const auto scene = UgcRays::Make(backend, mesh);
		const auto name = std::string(UgcRays::Name(UgcRays::Resolve(backend)));
		// From the box's +X face out along +X: the room's +X wall (triangles 16 and 17) 1.7 away
		auto hit = scene->Closest({ 0.3f, 0.1f, 0.1f }, { 1, 0, 0 });
		EXPECT_NEAR(hit.t, 1.7f, 1e-5f) << name;
		EXPECT_TRUE(hit.triangle == 16 || hit.triangle == 17) << name << " " << hit.triangle;
		// The same ray from the wall back: the box's +X face (triangles 2 and 3), not the wall it leaves
		hit = scene->Closest({ 2.0f, 0.1f, 0.1f }, { -1, 0, 0 }, hit.triangle);
		EXPECT_NEAR(hit.t, 1.7f, 1e-5f) << name;
		EXPECT_TRUE(hit.triangle == 2 || hit.triangle == 3) << name << " " << hit.triangle;
		// Nothing nearer than maxT: no hit, t = maxT
		hit = scene->Closest({ 0.3f, 0.1f, 0.1f }, { 1, 0, 0 }, UgcRays::NONE, 1.5f);
		EXPECT_EQ(hit.triangle, UgcRays::NONE) << name;
		EXPECT_EQ(hit.t, 1.5f) << name;
		// Out through the doorway (x -0.5..0.5, y -2..0 in the +Z wall): nothing
		EXPECT_EQ(scene->Closest({ 0.0f, -1.0f, 1.0f }, { 0, 0, 1 }).triangle, UgcRays::NONE) << name;
		// Occluded counts hits between minT and maxT only
		EXPECT_TRUE(scene->Occluded({ 0.3f, 0.1f, 0.1f }, { 1, 0, 0 }, 1e-4f, 1.8f)) << name;
		EXPECT_FALSE(scene->Occluded({ 0.3f, 0.1f, 0.1f }, { 1, 0, 0 }, 1e-4f, 1.6f)) << name;
		EXPECT_FALSE(scene->Occluded({ 0.3f, 0.1f, 0.1f }, { 1, 0, 0 }, 1.75f, 3.0f)) << name;
		// Batches answer as single rays do
		std::vector<UgcRays::Ray> rays{ { { 0.3f, 0.1f, 0.1f }, 1e-4f, { 1, 0, 0 }, 1.8f }, { { 0.3f, 0.1f, 0.1f }, 1e-4f, { 1, 0, 0 }, 1.6f } };
		std::vector<uint8_t> occluded(2);
		scene->Occluded(rays.data(), occluded.data(), rays.size());
		EXPECT_EQ(occluded, (std::vector<uint8_t>{ 1, 0 })) << name;
		std::vector<UgcRays::Hit> hits(2);
		scene->Closest(rays.data(), hits.data(), rays.size());
		EXPECT_NEAR(hits[0].t, 1.7f, 1e-5f) << name;
		EXPECT_EQ(hits[1].triangle, UgcRays::NONE) << name;
	}
}

TEST(UgcRays, BackendsFindTheSameHits) {
	// Rays in every direction from points around the clutter: the other backends find the nearest triangle Embree
	// finds at the same distance, and agree on what blocks. A ray through an edge two triangles share may hit either,
	// at the same distance.
	const auto mesh = Clutter();
	const auto embree = UgcRays::Make(UgcRays::eBackend::EMBREE, mesh);
	for (const auto backend : OtherBackends()) {
		const auto other = UgcRays::Make(backend, mesh);
		uint64_t state = 12345;
		const auto next = [&state]() {
			state = state * 6364136223846793005ull + 1442695040888963407ull;
			return static_cast<float>(state >> 40) / 16777216.0f;
		};
		size_t hits = 0, sameTriangle = 0;
		for (int i = 0; i < 20000; i++) {
			const glm::vec3 origin(next() * 3.8f - 1.9f, next() * 3.8f - 1.9f, next() * 3.8f - 1.9f);
			const auto direction = glm::normalize(glm::vec3(next() - 0.5f, next() - 0.5f, next() - 0.5f) + glm::vec3(1e-4f));
			const auto skip = static_cast<uint32_t>(next() * static_cast<float>(mesh.TriangleCount()));
			const auto a = embree->Closest(origin, direction, skip);
			const auto b = other->Closest(origin, direction, skip);
			ASSERT_EQ(a.triangle == UgcRays::NONE, b.triangle == UgcRays::NONE) << UgcRays::Name(backend) << " ray " << i;
			if (a.triangle == UgcRays::NONE) continue;
			hits++;
			EXPECT_NEAR(a.t, b.t, 1e-4f * std::max(1.0f, a.t)) << UgcRays::Name(backend) << " ray " << i;
			EXPECT_NE(b.triangle, skip);
			if (a.triangle == b.triangle) {
				sameTriangle++;
				EXPECT_NEAR(a.u, b.u, 1e-3f);
				EXPECT_NEAR(a.v, b.v, 1e-3f);
			}
			const float maxT = next() * 4.0f;
			EXPECT_EQ(embree->Occluded(origin, direction, 1e-4f, maxT), other->Occluded(origin, direction, 1e-4f, maxT)) << UgcRays::Name(backend) << " ray " << i;
			// Limited: the nearest hit before maxT, or none
			const auto limited = other->Closest(origin, direction, skip, maxT);
			EXPECT_EQ(limited.triangle != UgcRays::NONE, b.t < maxT) << UgcRays::Name(backend) << " ray " << i;
		}
		EXPECT_GT(hits, 15000u);
		EXPECT_GE(sameTriangle, hits - hits / 1000) << UgcRays::Name(backend);
	}
}

TEST(UgcRays, OtherBackendsMakeTheSameOcclusion) {
	// The clutter's occlusion is what the UGC server's own hierarchy (which Embree replaced) worked out: 296 vertices,
	// their occlusion summing to 114.5, 26 of them fully open and 183 darker than a half. Embree's and the other
	// backends' are within rounding of that; the small test model's files are the same with every backend.
	const auto mesh = Clutter();
	const auto expected = UgcRender::AmbientOcclusion(mesh, mesh, 2.0f, 64);
	ASSERT_EQ(expected.size(), 296u);
	double sum = 0.0;
	size_t open = 0, dark = 0;
	for (const float value : expected) {
		sum += value;
		open += value == 1.0f ? 1 : 0;
		dark += value < 0.5f ? 1 : 0;
	}
	EXPECT_NEAR(sum, 114.5, 0.5);
	EXPECT_NEAR(static_cast<double>(open), 26.0, 2.0);
	EXPECT_NEAR(static_cast<double>(dark), 183.0, 3.0);
	UgcBricks::BrickLibrary library(MakeRes(), 0);
	const auto embreeModel = UgcJobs::ProcessModel(LOOKS_LXFML, library, SmallSettings(), 7);
	ASSERT_TRUE(embreeModel.ok) << embreeModel.error;
	for (const auto backend : OtherBackends()) {
		const auto ao = UgcRender::AmbientOcclusion(mesh, mesh, 2.0f, 64, backend);
		ASSERT_EQ(ao.size(), expected.size());
		double total = 0.0;
		for (size_t v = 0; v < ao.size(); v++) {
			EXPECT_NEAR(ao[v], expected[v], 0.05f) << UgcRays::Name(backend) << " vertex " << v;
			total += std::abs(ao[v] - expected[v]);
		}
		EXPECT_LT(total / static_cast<double>(ao.size()), 0.002) << UgcRays::Name(backend);

		auto settings = SmallSettings();
		settings.ao.rays = backend;
		const auto made = UgcJobs::ProcessModel(LOOKS_LXFML, library, settings, 7);
		ASSERT_TRUE(made.ok) << made.error;
		EXPECT_EQ(made.files.at("model.nif.checksum"), embreeModel.files.at("model.nif.checksum")) << UgcRays::Name(backend);
		EXPECT_EQ(made.files.at("icon.png"), embreeModel.files.at("icon.png")) << UgcRays::Name(backend);
	}
}
