#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>

#include "NifFile.h"
#include "WorldScene.h"
#include "json.hpp"

namespace {
	// Little-endian bytes
	struct Bytes {
		std::string data;
		template<typename T>
		Bytes& Put(T value) {
			data.append(reinterpret_cast<const char*>(&value), sizeof(T));
			return *this;
		}
		Bytes& Floats(std::initializer_list<float> values) {
			for (const auto value : values) Put(value);
			return *this;
		}
		Bytes& Raw(const std::string& bytes) {
			data += bytes;
			return *this;
		}
	};

	// Builds a .nif the way the client's 20.3.0.9 files are laid out (nif.xml)
	class NifBuilder {
	public:
		uint32_t String(const std::string& text) {
			m_Strings.push_back(text);
			return static_cast<uint32_t>(m_Strings.size() - 1);
		}

		int32_t Add(const std::string& type, const Bytes& body) {
			auto it = std::find(m_Types.begin(), m_Types.end(), type);
			if (it == m_Types.end()) it = m_Types.insert(m_Types.end(), type);
			m_Blocks.push_back({ static_cast<uint16_t>(it - m_Types.begin()), body.data });
			return static_cast<int32_t>(m_Blocks.size() - 1);
		}

		// Blocks are added before they're known to be referenced, so a slot can be filled in later
		void Set(int32_t index, const Bytes& body) { m_Blocks[index].second = body.data; }

		std::string Build(std::vector<int32_t> roots = { 0 }) const {
			Bytes out;
			out.Raw("Gamebryo File Format, Version 20.3.0.9\n");
			out.Put<uint32_t>(0x14030009).Put<uint8_t>(1).Put<uint32_t>(0).Put<uint32_t>(static_cast<uint32_t>(m_Blocks.size()));
			out.Put<uint16_t>(static_cast<uint16_t>(m_Types.size()));
			for (const auto& type : m_Types) out.Put<uint32_t>(static_cast<uint32_t>(type.size())).Raw(type);
			for (const auto& block : m_Blocks) out.Put<uint16_t>(block.first);
			for (const auto& block : m_Blocks) out.Put<uint32_t>(static_cast<uint32_t>(block.second.size()));
			size_t longest = 0;
			for (const auto& text : m_Strings) longest = std::max(longest, text.size());
			out.Put<uint32_t>(static_cast<uint32_t>(m_Strings.size())).Put<uint32_t>(static_cast<uint32_t>(longest));
			for (const auto& text : m_Strings) out.Put<uint32_t>(static_cast<uint32_t>(text.size())).Raw(text);
			out.Put<uint32_t>(0); // groups
			for (const auto& block : m_Blocks) out.Raw(block.second);
			out.Put<uint32_t>(static_cast<uint32_t>(roots.size()));
			for (const auto root : roots) out.Put<int32_t>(root);
			return out.data;
		}

	private:
		std::vector<std::string> m_Types;
		std::vector<std::string> m_Strings;
		std::vector<std::pair<uint16_t, std::string>> m_Blocks;
	};

	Bytes Net(Bytes bytes = {}) {
		return bytes.Put<uint32_t>(0xFFFFFFFF).Put<uint32_t>(0).Put<int32_t>(-1);
	}

	// NiAVObject; `rotation` row-major (for column vectors), written the way the file stores it (column by column)
	Bytes Av(uint16_t flags, std::array<float, 3> translation, std::array<float, 9> rotation, float scale, std::vector<int32_t> properties) {
		auto bytes = Net();
		bytes.Put(flags).Floats({ translation[0], translation[1], translation[2] });
		for (int col = 0; col < 3; col++) for (int row = 0; row < 3; row++) bytes.Put(rotation[row * 3 + col]);
		bytes.Put(scale).Put<uint32_t>(static_cast<uint32_t>(properties.size()));
		for (const auto p : properties) bytes.Put(p);
		return bytes.Put<int32_t>(-1); // collision
	}

	constexpr std::array<float, 9> IDENTITY{ 1, 0, 0, 0, 1, 0, 0, 0, 1 };

	Bytes Node(Bytes av, std::vector<int32_t> children) {
		av.Put<uint32_t>(static_cast<uint32_t>(children.size()));
		for (const auto c : children) av.Put(c);
		return av.Put<uint32_t>(0); // effects
	}

	Bytes Geometry(Bytes av, int32_t data) {
		return av.Put(data).Put<int32_t>(-1).Put<uint32_t>(0).Put<int32_t>(-1).Put<uint8_t>(0); // skin, materials, active, needs update
	}

	// NiGeometryData for a unit triangle with normals, colors and one UV set
	Bytes GeometryData(uint16_t vertices = 3) {
		Bytes bytes;
		bytes.Put<int32_t>(0).Put(vertices).Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(1);
		for (uint16_t i = 0; i < vertices; i++) bytes.Floats({ static_cast<float>(i == 1), static_cast<float>(i == 2), 0.0f });
		bytes.Put<uint16_t>(1).Put<uint8_t>(1); // one UV set, has normals
		for (uint16_t i = 0; i < vertices; i++) bytes.Floats({ 0.0f, 0.0f, 1.0f });
		bytes.Floats({ 0, 0, 0, 1 }).Put<uint8_t>(1); // bounds, has colors
		for (uint16_t i = 0; i < vertices; i++) bytes.Floats({ 1.0f, 0.5f, 0.0f, 1.0f });
		for (uint16_t i = 0; i < vertices; i++) bytes.Floats({ 0.25f, 0.75f });
		return bytes.Put<uint16_t>(0).Put<int32_t>(-1); // consistency, additional data
	}

	Bytes TriShapeData() {
		auto bytes = GeometryData();
		return bytes.Put<uint16_t>(1).Put<uint32_t>(3).Put<uint8_t>(1).Put<uint16_t>(0).Put<uint16_t>(1).Put<uint16_t>(2).Put<uint16_t>(0);
	}

	// A root node holding one triangle; `rootAv` sets the root's transform and properties
	std::string OneTriangle(NifBuilder& nif, Bytes rootAv, std::vector<int32_t> shapeProperties = {}) {
		const auto root = nif.Add("NiNode", {});
		const auto shape = nif.Add("NiTriShape", {});
		const auto data = nif.Add("NiTriShapeData", TriShapeData());
		nif.Set(root, Node(rootAv, { shape }));
		nif.Set(shape, Geometry(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, shapeProperties), data));
		return nif.Build();
	}
}

TEST(NifFileTests, ReadsATriangleWithItsVertexData) {
	NifBuilder nif;
	const auto file = OneTriangle(nif, Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}));
	std::string error;
	const auto model = NifFile::Parse(file, 0, error);
	ASSERT_TRUE(model) << error;
	EXPECT_EQ(model->version, 0x14030009u);
	ASSERT_EQ(model->meshes.size(), 1u);
	const auto& mesh = model->meshes[0];
	EXPECT_EQ(mesh.positions, (std::vector<float>{ 0, 0, 0, 1, 0, 0, 0, 1, 0 }));
	EXPECT_EQ(mesh.indices, (std::vector<uint16_t>{ 0, 1, 2 }));
	ASSERT_EQ(mesh.normals.size(), 9u);
	EXPECT_FLOAT_EQ(mesh.normals[2], 1.0f);
	ASSERT_EQ(mesh.colors.size(), 12u);
	EXPECT_EQ(mesh.colors[0], 255);
	EXPECT_EQ(mesh.colors[1], 128);
	ASSERT_EQ(mesh.uvs.size(), 6u);
	EXPECT_FLOAT_EQ(mesh.uvs[1], 0.75f);
	EXPECT_FLOAT_EQ(model->max[0], 1.0f);
	EXPECT_TRUE(model->skipped.empty());
}

TEST(NifFileTests, BakesNodeTransformsIntoVertices) {
	NifBuilder nif;
	// 90 degrees about y (x goes to -z), then scaled by 2 and moved 10 along x
	const std::array<float, 9> yaw{ 0, 0, 1, 0, 1, 0, -1, 0, 0 };
	const auto file = OneTriangle(nif, Av(0, { 10, 0, 0 }, yaw, 2.0f, {}));
	std::string error;
	const auto model = NifFile::Parse(file, 0, error);
	ASSERT_TRUE(model) << error;
	const auto& p = model->meshes.at(0).positions;
	EXPECT_NEAR(p[3], 10.0f, 1e-5); // vertex (1, 0, 0)
	EXPECT_NEAR(p[4], 0.0f, 1e-5);
	EXPECT_NEAR(p[5], -2.0f, 1e-5);
	// Normals turn with the node but stay unit length
	const auto& n = model->meshes[0].normals;
	EXPECT_NEAR(n[0], 1.0f, 1e-5);
	EXPECT_NEAR(n[2], 0.0f, 1e-5);
}

TEST(NifFileTests, SkipsHiddenSubtrees) {
	NifBuilder nif;
	const auto file = OneTriangle(nif, Av(1, { 0, 0, 0 }, IDENTITY, 1.0f, {}));
	std::string error;
	const auto model = NifFile::Parse(file, 0, error);
	ASSERT_TRUE(model) << error;
	EXPECT_TRUE(model->meshes.empty());
}

TEST(NifFileTests, PassesPropertiesDownTheTree) {
	NifBuilder nif;
	const auto texName = nif.String("rock.dds");
	const auto material = nif.Add("NiMaterialProperty", Net().Floats({ 1, 1, 1, 0.5f, 0.25f, 0.125f, 1, 1, 1, 0.1f, 0.2f, 0.3f, 10.0f, 0.5f }));
	const auto alpha = nif.Add("NiAlphaProperty", Net().Put<uint16_t>(0x0201).Put<uint8_t>(64));
	const auto source = nif.Add("NiSourceTexture", Net().Put<uint8_t>(1).Put(texName).Put<int32_t>(-1).Floats({ 0, 0, 0 }).Put<uint8_t>(1).Put<uint8_t>(1).Put<uint8_t>(0));
	// Clamp mode 0 (clamp both) in the flags' top nibble
	const auto texturing = nif.Add("NiTexturingProperty", Net().Put<uint16_t>(0).Put<uint32_t>(7).Put<uint8_t>(1).Put(source).Put<uint16_t>(0x0200).Put<uint8_t>(0)
		.Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(0).Put<uint32_t>(0));
	const auto vertexColors = nif.Add("NiVertexColorProperty", Net().Put<uint16_t>(1 << 4)); // emissive
	const auto stencil = nif.Add("NiStencilProperty", Net().Put<uint16_t>(3 << 10).Put<uint32_t>(0).Put<uint32_t>(0xFFFFFFFF));
	const auto root = nif.Add("NiNode", {});
	const auto shape = nif.Add("NiTriShape", {});
	const auto data = nif.Add("NiTriShapeData", TriShapeData());
	// The root's material and texture, the shape's own alpha, vertex color and stencil properties
	nif.Set(root, Node(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, { material, texturing }), { shape }));
	nif.Set(shape, Geometry(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, { alpha, vertexColors, stencil }), data));
	std::string error;
	const auto model = NifFile::Parse(nif.Build({ root }), 0, error);
	ASSERT_TRUE(model) << error;
	ASSERT_EQ(model->meshes.size(), 1u);
	const auto& m = model->meshes[0].material;
	EXPECT_FLOAT_EQ(m.diffuse[0], 0.5f);
	EXPECT_FLOAT_EQ(m.diffuse[2], 0.125f);
	EXPECT_FLOAT_EQ(m.emissive[1], 0.2f);
	EXPECT_FLOAT_EQ(m.alpha, 0.5f);
	EXPECT_TRUE(m.alphaBlend);
	EXPECT_TRUE(m.alphaTest);
	EXPECT_EQ(m.alphaThreshold, 64);
	EXPECT_EQ(m.texture, "rock.dds");
	EXPECT_TRUE(m.clampU);
	EXPECT_TRUE(m.clampV);
	EXPECT_EQ(m.vertexColorMode, 1);
	EXPECT_TRUE(m.doubleSided);
}

TEST(NifFileTests, TurnsStripsIntoTriangles) {
	NifBuilder nif;
	const auto root = nif.Add("NiNode", {});
	const auto shape = nif.Add("NiTriStrips", {});
	auto data = GeometryData(4);
	// One strip 0 1 2 3: triangles (0 1 2) and (1 3 2), keeping the winding
	data.Put<uint16_t>(2).Put<uint16_t>(1).Put<uint16_t>(4).Put<uint8_t>(1).Put<uint16_t>(0).Put<uint16_t>(1).Put<uint16_t>(2).Put<uint16_t>(3);
	const auto strips = nif.Add("NiTriStripsData", data);
	nif.Set(root, Node(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), { shape }));
	nif.Set(shape, Geometry(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), strips));
	std::string error;
	const auto model = NifFile::Parse(nif.Build(), 0, error);
	ASSERT_TRUE(model) << error;
	ASSERT_EQ(model->meshes.size(), 1u);
	EXPECT_EQ(model->meshes[0].indices, (std::vector<uint16_t>{ 0, 1, 2, 1, 3, 2 }));
}

TEST(NifFileTests, PicksLevelsOfDetailByRange) {
	NifBuilder nif;
	const auto root = nif.Add("NiLODNode", {});
	const auto farChild = nif.Add("NiNode", {});
	const auto nearChild = nif.Add("NiNode", {});
	const auto farShape = nif.Add("NiTriShape", {});
	const auto nearShape = nif.Add("NiTriShape", {});
	const auto data = nif.Add("NiTriShapeData", TriShapeData());
	// Children listed far first: the ranges decide which is the detailed one
	const auto ranges = nif.Add("NiRangeLODData", Bytes{}.Floats({ 0, 0, 0 }).Put<uint32_t>(2).Floats({ 50, 1000, 0, 50 }));
	nif.Set(root, Node(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), { farChild, nearChild }).Put<uint16_t>(3).Put<uint32_t>(0).Put(ranges));
	nif.Set(farChild, Node(Av(0, { 100, 0, 0 }, IDENTITY, 1.0f, {}), { farShape }));
	nif.Set(nearChild, Node(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), { nearShape }));
	nif.Set(farShape, Geometry(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), data));
	nif.Set(nearShape, Geometry(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), data));
	const auto file = nif.Build();
	std::string error;
	for (const auto [lod, x] : std::vector<std::pair<uint32_t, float>>{ { 0, 0.0f }, { 1, 100.0f }, { 7, 100.0f } }) {
		const auto model = NifFile::Parse(file, lod, error);
		ASSERT_TRUE(model) << error;
		ASSERT_EQ(model->meshes.size(), 1u) << "lod " << lod;
		EXPECT_FLOAT_EQ(model->meshes[0].positions[0], x) << "lod " << lod;
	}
}

TEST(NifFileTests, CountsBlocksItDoesNotDraw) {
	NifBuilder nif;
	const auto root = nif.Add("NiNode", {});
	const auto light = nif.Add("NiAmbientLight", Bytes{}.Put<uint32_t>(0x12345678));
	nif.Set(root, Node(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), { light }));
	std::string error;
	const auto model = NifFile::Parse(nif.Build(), 0, error);
	ASSERT_TRUE(model) << error;
	EXPECT_EQ(model->skipped.at("NiAmbientLight"), 1u);
}

TEST(NifFileTests, RefusesDamagedAndForeignFiles) {
	std::string error;
	EXPECT_FALSE(NifFile::Parse("not a nif at all", 0, error));
	EXPECT_FALSE(error.empty());

	NifBuilder nif;
	const auto file = OneTriangle(nif, Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}));
	// A header that claims more than the file holds
	EXPECT_FALSE(NifFile::Parse(file.substr(0, 60), 0, error));
	// Every cut through the blocks reads without crashing: a block that doesn't fit is refused
	for (size_t size = 60; size < file.size(); size += 7) NifFile::Parse(file.substr(0, size), 0, error);

	auto older = file;
	const uint32_t version = 0x0A000100; // 10.0.1.0
	std::memcpy(older.data() + older.find('\n') + 1, &version, 4);
	EXPECT_FALSE(NifFile::Parse(older, 0, error));
}

TEST(NifFileTests, WrapsEmbeddedTexturesAsDds) {
	NifBuilder nif;
	Bytes pixels;
	pixels.Put<uint32_t>(4).Put<uint8_t>(0).Put<uint32_t>(0).Put<uint32_t>(0).Put<uint8_t>(0).Put<uint32_t>(0).Put<uint8_t>(0); // DXT1, untiled, not sRGB
	for (int i = 0; i < 10; i++) pixels.Put<uint32_t>(0);                                                                        // channels
	pixels.Put<int32_t>(-1).Put<uint32_t>(2).Put<uint32_t>(0).Put<uint32_t>(8).Put<uint32_t>(4).Put<uint32_t>(0).Put<uint32_t>(4).Put<uint32_t>(4).Put<uint32_t>(8);
	pixels.Put<uint32_t>(16).Put<uint32_t>(1).Raw(std::string(16, '\x5A'));
	const auto block = nif.Add("NiPixelData", pixels);
	const auto dds = NifFile::EmbeddedTexture(nif.Build({ block }), block);
	ASSERT_TRUE(dds);
	ASSERT_EQ(dds->size(), 128u + 16u);
	EXPECT_EQ(dds->substr(0, 4), "DDS ");
	EXPECT_EQ(dds->substr(84, 4), "DXT1");
	uint32_t width{}, height{}, mips{};
	std::memcpy(&height, dds->data() + 12, 4);
	std::memcpy(&width, dds->data() + 16, 4);
	std::memcpy(&mips, dds->data() + 28, 4);
	EXPECT_EQ(width, 8u);
	EXPECT_EQ(height, 4u);
	EXPECT_EQ(mips, 2u);
	EXPECT_FALSE(NifFile::EmbeddedTexture(nif.Build({ block }), 5));
}

TEST(NifFileTests, EncodesMeshesForTheBrowser) {
	NifBuilder nif;
	const auto file = OneTriangle(nif, Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}));
	std::string error;
	auto model = NifFile::Parse(file, 0, error);
	ASSERT_TRUE(model) << error;
	model->meshes.push_back(model->meshes[0]);
	const auto encoded = NifFile::Encode(*model, { "mesh/env/rock.dds", "mesh/env/rock.dds" });
	uint32_t length{};
	std::memcpy(&length, encoded.data(), 4);
	ASSERT_EQ(length % 4, 0u);
	const auto header = nlohmann::json::parse(encoded.substr(4, length));
	EXPECT_EQ(header["textures"], nlohmann::json::array({ "mesh/env/rock.dds" }));
	ASSERT_EQ(header["meshes"].size(), 2u);
	const auto& first = header["meshes"][0];
	EXPECT_EQ(first["texture"], 0);
	EXPECT_EQ(header["meshes"][1]["texture"], 0);
	EXPECT_EQ(first["vertices"], 3);
	// positions 36 + normals 9 (padded to 12) + UVs 24 + colors 12 + indices 6 (padded to 8)
	EXPECT_EQ(header["meshes"][1]["offset"], 92);
	EXPECT_EQ(encoded.size(), 4 + length + 2 * 92);
	float x{};
	std::memcpy(&x, encoded.data() + 4 + length + 12, 4);
	EXPECT_FLOAT_EQ(x, 1.0f);
	EXPECT_EQ(static_cast<int8_t>(encoded[4 + length + 36 + 2]), 127); // the first normal's z
}

TEST(NifFileTests, ReadsTheModelOfAnAnimationSet) {
	Bytes kfm;
	const std::string path = "..\\..\\mesh\\minifig\\mf_main_noLOD.nif";
	kfm.Raw(";Gamebryo KFM File Version 2.2.0.0b\n").Put<uint8_t>(1).Put<uint32_t>(static_cast<uint32_t>(path.size())).Raw(path).Put<uint32_t>(0);
	EXPECT_EQ(NifFile::KfmModelPath(kfm.data), path);
	EXPECT_FALSE(NifFile::KfmModelPath("Gamebryo File Format, Version 20.3.0.9\n"));
}

TEST(WorldSceneTests, ReadsTheSkydomeOfASceneFile) {
	const std::string sky = "mesh\\env\\env_sky_won_ag_property.nif";
	Bytes lvl;
	// One environment chunk: its data (at 0x20) points at the lighting, skydome (0x2C) and editor settings
	lvl.Raw("CHNK").Put<uint32_t>(2000).Put<uint16_t>(1).Put<uint16_t>(2).Put<uint32_t>(0x2C + 4 + static_cast<uint32_t>(sky.size())).Put<uint32_t>(0x20);
	lvl.Raw(std::string(0x20 - lvl.data.size(), '\xCD'));
	lvl.Put<uint32_t>(0x2C).Put<uint32_t>(0x2C).Put<uint32_t>(0);
	lvl.Put<uint32_t>(static_cast<uint32_t>(sky.size())).Raw(sky);
	EXPECT_EQ(WorldScene::ReadSkydome(lvl.data), sky);
	EXPECT_EQ(WorldScene::ReadSkydome(lvl.data.substr(0, 0x30)), "");
	EXPECT_EQ(WorldScene::ReadSkydome(""), "");
}

// The game client's own meshes, when a client is configured (DLU_CLIENT_RES, else client_location in the build's
// sharedconfig.ini): the first 300 .nif files under res/mesh/env read, and most have something to draw
TEST(NifFileTests, ReadsTheClientsMeshes) {
	std::filesystem::path res;
	if (const char* env = std::getenv("DLU_CLIENT_RES")) res = env;
	else {
		std::ifstream config(std::filesystem::path(DLU_SOURCE_DIR) / "build" / "sharedconfig.ini");
		for (std::string line; std::getline(config, line);) {
			if (line.starts_with("client_location=")) res = std::filesystem::path(line.substr(16)) / "res";
		}
	}
	std::error_code ec;
	const auto folder = res / "mesh" / "env";
	if (res.empty() || !std::filesystem::is_directory(folder, ec)) GTEST_SKIP() << "No game client configured";

	size_t files = 0, read = 0, withMeshes = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(folder, ec)) {
		auto extension = entry.path().extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		if (extension != ".nif" || files >= 300) continue;
		std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
		std::string data(static_cast<size_t>(file.tellg()), '\0');
		file.seekg(0);
		file.read(data.data(), static_cast<std::streamsize>(data.size()));
		files++;
		std::string error;
		const auto model = NifFile::Parse(data, 0, error);
		if (!model) continue;
		read++;
		if (!model->meshes.empty()) withMeshes++;
		for (const auto& mesh : model->meshes) {
			const auto vertices = mesh.positions.size() / 3;
			EXPECT_TRUE(std::all_of(mesh.indices.begin(), mesh.indices.end(), [vertices](uint16_t i) { return i < vertices; })) << entry.path();
			EXPECT_TRUE(std::all_of(mesh.positions.begin(), mesh.positions.end(), [](float v) { return std::isfinite(v); })) << entry.path();
		}
	}
	ASSERT_GT(files, 0u);
	EXPECT_EQ(read, files);
	EXPECT_GT(withMeshes, files * 9 / 10);
}

TEST(NifFileTests, ReadsMultishaderTagsLikeTheClient) {
	EXPECT_EQ(NifFile::ShaderTag("S05__TRUNKS"), 5);
	EXPECT_EQ(NifFile::ShaderTag("S30__Rockwall_0"), 30);
	EXPECT_EQ(NifFile::ShaderTag("rock_S14"), 14);
	EXPECT_EQ(NifFile::ShaderTag("Shadow_S7_glow"), 7); // "S" not followed by a number: the "_S" tag counts
	EXPECT_EQ(NifFile::ShaderTag("rock_S"), -1);
	EXPECT_EQ(NifFile::ShaderTag("ROCK"), -1);
	EXPECT_EQ(NifFile::ShaderTag(""), -1);
	// The client draws a part with the LEGO shader when its tag names no usable shader
	EXPECT_EQ(NifFile::MultishaderPart(38), 38);
	EXPECT_EQ(NifFile::MultishaderPart(2), NifFile::LEGO_SHADER);
	EXPECT_EQ(NifFile::MultishaderPart(9999), NifFile::LEGO_SHADER);
	EXPECT_EQ(NifFile::MultishaderPart(std::nullopt), NifFile::LEGO_SHADER);
}

TEST(NifFileTests, KnowsWhichShadersUseTextureAlphaAsOpacity) {
	using NifFile::eTextureAlpha;
	EXPECT_EQ(NifFile::TextureAlphaFor(NifFile::LEGO_SHADER), eTextureAlpha::DECAL); // LEGOPPLighting: lerp over vertex colors
	EXPECT_EQ(NifFile::TextureAlphaFor(31), eTextureAlpha::IGNORED);  // LEGO-Item: alpha forced to 1
	EXPECT_EQ(NifFile::TextureAlphaFor(3), eTextureAlpha::IGNORED);   // Terrain Mesh Rim Light: alpha is the fade only
	EXPECT_EQ(NifFile::TextureAlphaFor(7), eTextureAlpha::OPACITY);   // VertColor_Alpha (AlphaAsAlpha)
	EXPECT_EQ(NifFile::TextureAlphaFor(38), eTextureAlpha::OPACITY);  // Basic VC
	EXPECT_EQ(NifFile::TextureAlphaFor(14), eTextureAlpha::OPACITY);  // LEGO Masked NonDecal: texture alpha is output
	EXPECT_EQ(NifFile::TextureAlphaFor(53), eTextureAlpha::OPACITY);  // LEGO-Emissive lets texture alpha through
	EXPECT_EQ(NifFile::TextureAlphaFor(-1), eTextureAlpha::OPACITY);  // fixed function: NiAlphaProperty as Gamebryo does
}

TEST(NifFileTests, PassesMultishaderTagsDownToMeshes) {
	NifBuilder nif;
	auto rootAv = Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {});
	const auto name = nif.String("S30__Rockwall_0");
	std::memcpy(rootAv.data.data(), &name, 4);
	const auto file = OneTriangle(nif, rootAv);
	std::string error;
	const auto model = NifFile::Parse(file, 0, error);
	ASSERT_TRUE(model) << error;
	ASSERT_EQ(model->meshes.size(), 1u);
	EXPECT_EQ(model->meshes[0].material.shaderTag, 30);
	const auto encoded = NifFile::Encode(*model, { "" });
	uint32_t length{};
	std::memcpy(&length, encoded.data(), 4);
	EXPECT_EQ(nlohmann::json::parse(encoded.substr(4, length))["meshes"][0]["shaderTag"], 30);
}
