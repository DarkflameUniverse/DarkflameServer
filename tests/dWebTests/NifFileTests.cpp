#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
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
	// A node under the root: 90 degrees about y (x goes to -z), then scaled by 2 and moved 10 along x
	const std::array<float, 9> yaw{ 0, 0, 1, 0, 1, 0, -1, 0, 0 };
	const auto root = nif.Add("NiNode", {});
	const auto node = nif.Add("NiNode", {});
	const auto shape = nif.Add("NiTriShape", {});
	const auto data = nif.Add("NiTriShapeData", TriShapeData());
	nif.Set(root, Node(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), { node }));
	nif.Set(node, Node(Av(0, { 10, 0, 0 }, yaw, 2.0f, {}), { shape }));
	nif.Set(shape, Geometry(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), data));
	const auto file = nif.Build();
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

TEST(NifFileTests, LeavesOutTheRootsRotationAndTranslation) {
	// The client sets the object's own position and rotation on the root node it loads, so the root's stored ones
	// never show; its scale does
	NifBuilder nif;
	const std::array<float, 9> yaw{ 0, 0, 1, 0, 1, 0, -1, 0, 0 };
	const auto file = OneTriangle(nif, Av(0, { 10, 0, 0 }, yaw, 2.0f, {}));
	std::string error;
	const auto model = NifFile::Parse(file, 0, error);
	ASSERT_TRUE(model) << error;
	EXPECT_EQ(model->meshes.at(0).positions, (std::vector<float>{ 0, 0, 0, 2, 0, 0, 0, 2, 0 }));
	EXPECT_EQ(model->meshes[0].normals[2], 1.0f);
}

TEST(NifFileTests, StandsUpAModelWhoseRootIsTurned) {
	// Laid out as LU Toolbox's .nif (niftools from Blender, Z up): the root turned 90 degrees about x, the NiLODNode
	// turned back and the shape turned again, so without the root's turn the vertices stay as stored (Y up)
	NifBuilder nif;
	const std::array<float, 9> up{ 1, 0, 0, 0, 0, -1, 0, 1, 0 };   // +90 about x: y goes to z, z to -y
	const std::array<float, 9> down{ 1, 0, 0, 0, 0, 1, 0, -1, 0 }; // -90 about x
	const auto root = nif.Add("NiNode", {});
	const auto lodNode = nif.Add("NiLODNode", {});
	const auto level = nif.Add("NiNode", {});
	const auto shape = nif.Add("NiTriShape", {});
	const auto data = nif.Add("NiTriShapeData", TriShapeData());
	const auto ranges = nif.Add("NiRangeLODData", Bytes{}.Floats({ 0, 0, 0 }).Put<uint32_t>(1).Floats({ 0, 100 }));
	nif.Set(root, Node(Av(0, { 0, 0, 0 }, down, 1.0f, {}), { lodNode }));
	nif.Set(lodNode, Node(Av(0, { 0, 0, 0 }, up, 1.0f, {}), { level }).Put<uint16_t>(3).Put<uint32_t>(0).Put(ranges));
	nif.Set(level, Node(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), { shape }));
	nif.Set(shape, Geometry(Av(0, { 0, 0, 0 }, down, 1.0f, {}), data));
	std::string error;
	const auto model = NifFile::Parse(nif.Build(), 0, error);
	ASSERT_TRUE(model) << error;
	const auto& p = model->meshes.at(0).positions;
	// The vertex (0, 1, 0) still points up (with the root's turn it would lie along -z)
	EXPECT_NEAR(p[6], 0.0f, 1e-6);
	EXPECT_NEAR(p[7], 1.0f, 1e-6);
	EXPECT_NEAR(p[8], 0.0f, 1e-6);
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

// A material whose NiAlphaController animates its alpha (flickering effects that rest at 0 in the file) is drawn at
// its highest key; one without keeps its own alpha
TEST(NifFileTests, DrawsAnAnimatedAlphaAtItsHighest) {
	for (const bool animated : { true, false }) {
		NifBuilder nif;
		const auto root = nif.Add("NiNode", {});
		const auto material = nif.Add("NiMaterialProperty", {});
		const auto controller = nif.Add("NiAlphaController", {});
		const auto interpolator = nif.Add("NiFloatInterpolator", {});
		const auto keys = nif.Add("NiFloatData", Bytes().Put<uint32_t>(3).Put<uint32_t>(1).Floats({ 0.0f, 0.0f, 0.5f, 0.8f, 1.0f, 0.0f }));
		// NiObjectNET with the controller, ambient, diffuse, specular, emissive, glossiness, alpha 0
		Bytes body;
		body.Put<uint32_t>(0xFFFFFFFF).Put<uint32_t>(0).Put<int32_t>(animated ? controller : -1);
		body.Floats({ 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 10.0f, 0.0f });
		nif.Set(material, body);
		// NiTimeController: next, flags, frequency, phase, start, stop, target; the interpolator
		nif.Set(controller, Bytes().Put<int32_t>(-1).Put<uint16_t>(8).Floats({ 1, 0, 0, 1 }).Put<int32_t>(material).Put<int32_t>(interpolator));
		nif.Set(interpolator, Bytes().Put(0.0f).Put<int32_t>(keys));
		const auto shape = nif.Add("NiTriShape", {});
		const auto data = nif.Add("NiTriShapeData", TriShapeData());
		nif.Set(root, Node(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, { material }), { shape }));
		nif.Set(shape, Geometry(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), data));
		std::string error;
		const auto model = NifFile::Parse(nif.Build({ root }), 0, error);
		ASSERT_TRUE(model) << error;
		ASSERT_EQ(model->meshes.size(), 1u);
		EXPECT_FLOAT_EQ(model->meshes[0].material.alpha, animated ? 0.8f : 0.0f);
	}
}

// Two layer shaders use NiTexturingProperty's dark texture too, each texture on the UV set its flags name
TEST(NifFileTests, ReadsTheDarkTextureAndEachTexturesUvSet) {
	NifBuilder nif;
	const auto snow = nif.String("snow.dds"), rock = nif.String("rock.dds");
	const auto source = [&nif](int32_t name) {
		return nif.Add("NiSourceTexture", Net().Put<uint8_t>(1).Put(name).Put<int32_t>(-1).Floats({ 0, 0, 0 }).Put<uint8_t>(1).Put<uint8_t>(1).Put<uint8_t>(0));
	};
	const auto base = source(snow), dark = source(rock);
	// Base on UV set 1 with a texture transform (32 bytes to skip), dark on UV set 0
	auto texturing = Net().Put<uint16_t>(0).Put<uint32_t>(9).Put<uint8_t>(1).Put(base).Put<uint16_t>(0x3201).Put<uint8_t>(1);
	texturing.Raw(std::string(32, '\0'));
	texturing.Put<uint8_t>(1).Put(dark).Put<uint16_t>(0x3200).Put<uint8_t>(0);
	for (int slot = 2; slot < 9; slot++) texturing.Put<uint8_t>(0);
	texturing.Put<uint32_t>(0);
	const auto property = nif.Add("NiTexturingProperty", texturing);
	// A triangle with two UV sets: set 0 all (0.25, 0.75), set 1 all (0.5, 0.5)
	Bytes data;
	data.Put<int32_t>(0).Put<uint16_t>(3).Put<uint8_t>(0).Put<uint8_t>(0).Put<uint8_t>(1);
	for (int i = 0; i < 3; i++) data.Floats({ static_cast<float>(i == 1), static_cast<float>(i == 2), 0.0f });
	data.Put<uint16_t>(2).Put<uint8_t>(0).Floats({ 0, 0, 0, 1 }).Put<uint8_t>(0);
	for (int i = 0; i < 3; i++) data.Floats({ 0.25f, 0.75f });
	for (int i = 0; i < 3; i++) data.Floats({ 0.5f, 0.5f });
	data.Put<uint16_t>(0).Put<int32_t>(-1).Put<uint16_t>(1).Put<uint32_t>(3).Put<uint8_t>(1).Put<uint16_t>(0).Put<uint16_t>(1).Put<uint16_t>(2).Put<uint16_t>(0);
	const auto root = nif.Add("NiNode", {});
	const auto shape = nif.Add("NiTriShape", {});
	const auto shapeData = nif.Add("NiTriShapeData", data);
	nif.Set(root, Node(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, {}), { shape }));
	nif.Set(shape, Geometry(Av(0, { 0, 0, 0 }, IDENTITY, 1.0f, { property }), shapeData));
	std::string error;
	const auto model = NifFile::Parse(nif.Build({ root }), 0, error);
	ASSERT_TRUE(model) << error;
	ASSERT_EQ(model->meshes.size(), 1u);
	const auto& mesh = model->meshes[0];
	EXPECT_EQ(mesh.material.texture, "snow.dds");
	EXPECT_EQ(mesh.material.darkTexture, "rock.dds");
	ASSERT_EQ(mesh.uvs.size(), 6u);
	EXPECT_FLOAT_EQ(mesh.uvs[0], 0.5f);   // the base texture's set 1
	ASSERT_EQ(mesh.uvs2.size(), 6u);
	EXPECT_FLOAT_EQ(mesh.uvs2[1], 0.75f); // the dark texture's set 0

	// The browser gets the dark texture and its UVs
	const auto encoded = NifFile::Encode(*model, { "mesh/snow.dds" }, { "mesh/rock.dds" });
	uint32_t length{};
	std::memcpy(&length, encoded.data(), 4);
	const auto header = nlohmann::json::parse(encoded.substr(4, length));
	EXPECT_EQ(header["textures"], nlohmann::json::array({ "mesh/snow.dds", "mesh/rock.dds" }));
	EXPECT_EQ(header["meshes"][0]["darkTexture"], 1);
	EXPECT_EQ(header["meshes"][0]["uv2"], true);
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

namespace {
	// A scene file (`version`) with one environment chunk whose lighting is `lighting`
	std::string SceneWithLighting(uint32_t version, const Bytes& lighting) {
		Bytes lvl;
		// File info chunk (data at 0x20: version), then the environment chunk (header at 0x34, data at 0x54) whose
		// lighting starts at 0x60
		lvl.Raw("CHNK").Put<uint32_t>(1000).Put<uint16_t>(1).Put<uint16_t>(1).Put<uint32_t>(0x34).Put<uint32_t>(0x20);
		lvl.Raw(std::string(0x20 - lvl.data.size(), '\xCD'));
		lvl.Put<uint32_t>(version).Put<uint32_t>(0).Put<uint32_t>(0x34).Put<uint32_t>(0).Put<uint32_t>(0);
		lvl.Raw("CHNK").Put<uint32_t>(2000).Put<uint16_t>(1).Put<uint16_t>(2).Put<uint32_t>(0x60 + static_cast<uint32_t>(lighting.data.size()) - 0x34).Put<uint32_t>(0x54);
		lvl.Raw(std::string(0x54 - lvl.data.size(), '\xCD'));
		lvl.Put<uint32_t>(0x60).Put<uint32_t>(0).Put<uint32_t>(0);
		lvl.Raw(lighting.data);
		return lvl.data;
	}

	Bytes& Floats(Bytes& bytes, std::initializer_list<float> values) {
		for (const auto value : values) bytes.Put<float>(value);
		return bytes;
	}
}

// As the client's level_read_lighting_info: version 48 has a blend time, two draw distance settings and cull groups
TEST(WorldSceneTests, ReadsTheLightingOfASceneFile) {
	Bytes lighting;
	Floats(lighting, { 10.0f });                               // blend time
	Floats(lighting, { 0.42f, 0.62f, 0.75f });                 // ambient
	Floats(lighting, { 1, 1, 1 });                             // specular
	Floats(lighting, { 1, 0.7f, 0.5f });                       // upper hemisphere
	Floats(lighting, { 0, -3, -4 });                           // the way the sun shines
	Floats(lighting, { 100, 300, 50, 50, 8000, 8000 });        // lowest draw distances
	Floats(lighting, { 250, 350, 100, 100, 8000, 8000 });      // highest
	lighting.Put<uint32_t>(2).Put<uint32_t>(7);
	Floats(lighting, { 1, 2 });
	lighting.Put<uint32_t>(8);
	Floats(lighting, { 3, 4 });                                // cull groups
	Floats(lighting, { 0.5f, 0.8f, 0.9f });                    // fog color
	Floats(lighting, { 1, 0.9f, 0.8f });                       // sun color
	const auto read = WorldScene::ReadLighting(SceneWithLighting(48, lighting));
	ASSERT_TRUE(read);
	EXPECT_FLOAT_EQ(read->ambient[1], 0.62f);
	EXPECT_FLOAT_EQ(read->upperHemi[2], 0.5f);
	EXPECT_FLOAT_EQ(read->lightVec[0], 0.0f); // toward the sun: the stored direction turned around, unit length
	EXPECT_FLOAT_EQ(read->lightVec[1], 0.6f);
	EXPECT_FLOAT_EQ(read->lightVec[2], 0.8f);
	EXPECT_FLOAT_EQ(read->fogNear, 250.0f);
	EXPECT_FLOAT_EQ(read->fogFar, 350.0f);
	EXPECT_FLOAT_EQ(read->fogColor[2], 0.9f);
	EXPECT_FLOAT_EQ(read->light[1], 0.9f);

	// Version 35: no blend time, one fog range, no sun color
	Bytes old;
	Floats(old, { 0.5f, 0.5f, 0.5f, 1, 1, 1, 1, 1, 1, 0, -1, 0, 20, 90, 0.1f, 0.2f, 0.3f });
	const auto older = WorldScene::ReadLighting(SceneWithLighting(35, old));
	ASSERT_TRUE(older);
	EXPECT_FLOAT_EQ(older->ambient[0], 0.5f);
	EXPECT_FLOAT_EQ(older->lightVec[1], 1.0f);
	EXPECT_FLOAT_EQ(older->fogFar, 90.0f);
	EXPECT_FLOAT_EQ(older->fogColor[1], 0.2f);
	EXPECT_FLOAT_EQ(older->light[0], 0.0f);

	// Cut short, or no environment chunk
	const auto whole = SceneWithLighting(48, lighting);
	EXPECT_FALSE(WorldScene::ReadLighting(whole.substr(0, whole.size() - 8)));
	EXPECT_FALSE(WorldScene::ReadLighting(""));
}

TEST(WorldSceneTests, LightsAZoneAsMostOfItsObjectsAre) {
	WorldScene::Lighting day, dusk;
	day.ambient = { 1, 1, 1 };
	dusk.ambient = { 0.2f, 0.2f, 0.4f };
	EXPECT_FALSE(WorldScene::ZoneLighting({}));
	// Two scenes lit like dusk hold more objects than the day one
	EXPECT_EQ(WorldScene::ZoneLighting({ { day, 50 }, { dusk, 30 }, { dusk, 25 } }), dusk);
	EXPECT_EQ(WorldScene::ZoneLighting({ { day, 10 }, { dusk, 10 } }), day); // a tie goes to the first
}

TEST(NifFileTests, KnowsWhatEachShaderLeavesOut) {
	EXPECT_EQ(NifFile::ShaderLookFor(38), 0);                   // Basic VC: lit, textured, vertex colors
	EXPECT_EQ(NifFile::ShaderLookFor(94), 0);                   // "Basic" draws with vertex colors too (Nimbus Station's pines)
	EXPECT_EQ(NifFile::ShaderLookFor(84), NifFile::UNLIT);      // Opaque NL VC NoFog
	EXPECT_EQ(NifFile::ShaderLookFor(NifFile::LEGO_SHADER), 0);
	EXPECT_EQ(NifFile::ShaderLookFor(-1), 0);                   // fixed function is lit by Gamebryo
	EXPECT_EQ(NifFile::ShaderLookFor(33), NifFile::UNLIT | NifFile::NO_TEXTURE); // Basic NL VC NT
	EXPECT_EQ(NifFile::ShaderLookFor(37), NifFile::NO_TEXTURE); // Basic VC NT
	EXPECT_EQ(NifFile::ShaderLookFor(70), NifFile::UNLIT);      // ScrollingUV_NoLight_AnimAlpha
	EXPECT_EQ(NifFile::ShaderLookFor(32), NifFile::UNLIT | NifFile::NO_VERTEX_COLORS | NifFile::MATERIAL_COLOR); // Basic NL Material
	// The UGC server's metal and glow groups: Polished Metal, Brushed Steel, LEGO-Emissive
	EXPECT_EQ(NifFile::ShaderLookFor(98), NifFile::REFLECTIVE);
	EXPECT_EQ(NifFile::ShaderLookFor(99), NifFile::REFLECTIVE | NifFile::BRUSHED);
	EXPECT_EQ(NifFile::ShaderLookFor(53), NifFile::EMISSIVE);
	// Through a multishader tag's gameValue (S88 -> 98), as a player model's parts are drawn
	EXPECT_EQ(NifFile::ShaderLookFor(NifFile::MultishaderPart(98)), NifFile::REFLECTIVE);
	EXPECT_EQ(NifFile::ShaderLookFor(NifFile::MultishaderPart(std::nullopt)), 0);
}

TEST(NifFileTests, EncodesEachMeshsLook) {
	NifFile::Model model;
	model.meshes.resize(2);
	for (auto& mesh : model.meshes) {
		mesh.positions = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
		mesh.indices = { 0, 1, 2 };
	}
	const auto header = [](const std::string& encoded) {
		uint32_t length = 0;
		std::memcpy(&length, encoded.data(), 4);
		return nlohmann::json::parse(encoded.substr(4, length));
	};
	const auto with = header(NifFile::Encode(model, { "", "" }, {}, { NifFile::REFLECTIVE, NifFile::EMISSIVE }));
	EXPECT_EQ(with["meshes"][0]["look"], NifFile::REFLECTIVE);
	EXPECT_EQ(with["meshes"][1]["look"], NifFile::EMISSIVE);
	EXPECT_FALSE(header(NifFile::Encode(model, { "", "" }))["meshes"][0].contains("look"));
}

// The game client's own meshes, when a client is configured (DLU_CLIENT_RES, else client_location in the build's
// sharedconfig.ini): the first 300 .nif files under res/mesh/env read, and most have something to draw
namespace {
	// The game client's res folder (DLU_CLIENT_RES, else the build's sharedconfig.ini), empty when there is none
	std::filesystem::path ClientRes() {
		std::filesystem::path res;
		if (const char* env = std::getenv("DLU_CLIENT_RES")) res = env;
		else {
			std::ifstream config(std::filesystem::path(DLU_SOURCE_DIR) / "build" / "sharedconfig.ini");
			for (std::string line; std::getline(config, line);) {
				if (line.starts_with("client_location=")) res = std::filesystem::path(line.substr(16)) / "res";
			}
		}
		return res;
	}
}

TEST(NifFileTests, ReadsAClientModelWhereItStands) {
	// A game model (the pirate raft reward) reads where it stood before roots' turns were left out: upright on y 0
	const auto path = ClientRes() / "mesh" / "reward" / "rew_pirate-raft.nif";
	std::ifstream file(path, std::ios::binary);
	if (!file) GTEST_SKIP() << "No game client configured";
	const std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	std::string error;
	const auto model = NifFile::Parse(data, 0, error);
	ASSERT_TRUE(model) << error;
	ASSERT_EQ(model->meshes.size(), 1u);
	EXPECT_NEAR(model->min[0], -1.6f, 1e-4);
	EXPECT_NEAR(model->min[1], 0.0f, 1e-4);
	EXPECT_NEAR(model->min[2], -3.32814f, 1e-4);
	EXPECT_NEAR(model->max[0], 1.6f, 1e-4);
	EXPECT_NEAR(model->max[1], 5.12f, 1e-4);
	EXPECT_NEAR(model->max[2], 4.04143f, 1e-4);
}

TEST(NifFileTests, ReadsTheClientsMeshes) {
	const auto res = ClientRes();
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

TEST(NifFileTests, KnowsEachShadersTechniqueFamily) {
	using NifFile::eShaderFamily;
	const auto family = [](int32_t shader) { return NifFile::TechniqueFor(shader).family; };
	const auto flags = [](int32_t shader) { return NifFile::TechniqueFor(shader).flags; };
	EXPECT_EQ(family(-1), eShaderFamily::FIXED_FUNCTION);
	// The ones most of the zones' objects use: LEGO, Basic VC, "Basic", VertColor_Alpha, LEGO NoAmbient
	EXPECT_EQ(family(NifFile::LEGO_SHADER), eShaderFamily::LEGO);
	EXPECT_EQ(family(38), eShaderFamily::BASIC);
	EXPECT_EQ(family(94), eShaderFamily::BASIC);
	EXPECT_EQ(family(7), eShaderFamily::BASIC);
	EXPECT_TRUE(flags(7) & NifFile::DOUBLE_SIDED);     // AlphaAsAlpha: Cullmode none
	EXPECT_EQ(family(88), eShaderFamily::LEGO);
	EXPECT_TRUE(flags(88) & NifFile::NO_AMBIENT);
	EXPECT_EQ(family(3), eShaderFamily::TERRAIN);
	EXPECT_TRUE(flags(3) & NifFile::RIM_LIGHT);
	// Moving textures, water, metal, glass, darklings
	EXPECT_TRUE(flags(30) & NifFile::UV_ANIM);          // LEGO-AnimUV
	EXPECT_TRUE(flags(70) & NifFile::UV_ANIM);          // ScrollingUV_NoLight_AnimAlpha
	EXPECT_FALSE(flags(38) & NifFile::UV_ANIM);
	EXPECT_EQ(family(69), eShaderFamily::OCEAN);
	EXPECT_TRUE(flags(90) & NifFile::OCEAN_FX);
	EXPECT_EQ(family(98), eShaderFamily::METAL);
	EXPECT_EQ(family(99), eShaderFamily::METAL);
	EXPECT_EQ(family(6), eShaderFamily::CLEAR_PLASTIC);
	EXPECT_TRUE(flags(6) & NifFile::BLEND);
	EXPECT_EQ(family(75), eShaderFamily::DARKLING);
	EXPECT_TRUE(flags(76) & NifFile::SPECULAR);
	EXPECT_TRUE(flags(22) & NifFile::SUPER_EMISSIVE);
	EXPECT_TRUE(flags(87) & NifFile::ADDITIVE);
	EXPECT_TRUE(flags(74) & NifFile::NOT_DRAWN);        // Drop Shadow
	// A value the table lacks is the LEGO shader, as the client falls back to it
	EXPECT_EQ(family(4242), eShaderFamily::LEGO);
	EXPECT_EQ(NifFile::TextureAlphaFor(4242), NifFile::eTextureAlpha::DECAL);
}

TEST(NifFileTests, WritesTechniquesForTheManifest) {
	const auto json = nlohmann::json::parse(NifFile::TechniquesJson({ -1, 5, 99, 87 }));
	ASSERT_EQ(json.size(), 4u);
	EXPECT_EQ(json["-1"]["family"], "fixed");
	EXPECT_EQ(json["5"]["family"], "lego");
	EXPECT_EQ(json["5"]["alpha"], "decal");
	EXPECT_EQ(json["99"]["family"], "metal");
	EXPECT_EQ(json["99"]["look"], NifFile::REFLECTIVE | NifFile::BRUSHED);
	EXPECT_EQ(json["87"]["flags"], NifFile::ADDITIVE);
	EXPECT_EQ(json["87"]["look"], NifFile::UNLIT);
	for (const auto family : { "fixed", "lego", "basic", "metal", "clearPlastic", "ocean", "flatSurf", "brickWater", "darkling", "terrain" }) {
		bool named = false;
		for (int i = 0; i <= static_cast<int>(NifFile::eShaderFamily::TERRAIN); i++) named |= std::string(NifFile::FamilyName(static_cast<NifFile::eShaderFamily>(i))) == family;
		EXPECT_TRUE(named) << family;
	}
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
