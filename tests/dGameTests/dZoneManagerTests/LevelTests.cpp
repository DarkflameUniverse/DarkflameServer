#include "Level.h"

#include <gtest/gtest.h>

#include <cstring>
#include <sstream>

#include "GameDependencies.h"

namespace {
	template<typename T>
	void Write(std::ostream& stream, const T& value) {
		stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	void WriteFloats(std::ostream& stream, std::initializer_list<float> values) {
		for (const float value : values) Write(stream, value);
	}

	// A u32 length, then that many bytes
	void WriteString32(std::ostream& stream, const std::string& value) {
		Write<uint32_t>(stream, static_cast<uint32_t>(value.size()));
		stream.write(value.data(), value.size());
	}

	// A u32 length, then that many UTF-16 characters (ASCII only)
	void WriteWString32(std::ostream& stream, const std::string& value) {
		Write<uint32_t>(stream, static_cast<uint32_t>(value.size()));
		for (const char c : value) Write<uint16_t>(stream, static_cast<uint16_t>(c));
	}

	void WriteName64(std::ostream& stream, const std::string& name) {
		std::string bytes(64, '\0');
		bytes.replace(0, name.size(), name);
		stream.write(bytes.data(), bytes.size());
	}

	// The light environment as the given version has it
	void WriteLighting(std::ostream& stream, uint32_t version) {
		if (version >= 45) Write(stream, 2.5f); // blend time
		WriteFloats(stream, { 0.1f, 0.2f, 0.3f }); // ambient
		WriteFloats(stream, { 0.4f, 0.5f, 0.6f }); // specular
		WriteFloats(stream, { 0.7f, 0.8f, 0.9f }); // upper hemisphere
		WriteFloats(stream, { 10.0f, 20.0f, 30.0f }); // position
		if (version >= 31) {
			if (version < 39) {
				WriteFloats(stream, { 50.0f, 60.0f }); // fog near and far
			} else {
				WriteFloats(stream, { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f }); // min draw distances
				WriteFloats(stream, { 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f }); // max draw distances
				if (version >= 40) {
					Write<uint32_t>(stream, 1);
					Write<uint32_t>(stream, 3); // group
					WriteFloats(stream, { 100.0f, 200.0f });
				}
			}
			WriteFloats(stream, { 0.25f, 0.5f, 0.75f }); // fog colour
		}
		if (version >= 36) WriteFloats(stream, { 1.0f, 0.5f, 0.0f }); // directional
		if (version < 42) {
			WriteFloats(stream, { 5.0f, 6.0f, 7.0f }); // spawn position
			if (version >= 33) WriteFloats(stream, { 0.0f, 1.0f, 0.0f, 0.0f }); // spawn rotation, w first
		}
	}

	void WriteSkydome(std::ostream& stream, uint32_t version) {
		WriteString32(stream, "mesh/env/sky.nif");
		if (version >= 34) {
			for (const auto* name : { "a.nif", "b.nif", "c.nif", "d.nif", "e.nif" }) WriteString32(stream, name);
		}
	}

	void WriteEditorSettings(std::ostream& stream) {
		Write<uint32_t>(stream, 4 + 12 * 2); // size
		Write<uint32_t>(stream, 2);
		WriteFloats(stream, { 1.0f, 0.0f, 0.0f });
		WriteFloats(stream, { 0.0f, 1.0f, 0.0f });
	}

	void WriteObject(std::ostream& stream, uint32_t version, LWOOBJID id, LOT lot, const std::string& config, uint32_t techniques = 0) {
		Write(stream, id);
		Write(stream, lot);
		if (version >= 38) Write<uint32_t>(stream, 12); // node type out of range: the client uses 1
		if (version >= 32) Write<uint32_t>(stream, 4); // glom ID
		WriteFloats(stream, { 1.0f, 2.0f, 3.0f }); // position
		WriteFloats(stream, { 1.0f, 0.0f, 0.0f, 0.0f }); // rotation, w first
		Write(stream, 2.0f); // scale
		WriteWString32(stream, config);
		if (version >= 7) {
			Write(stream, techniques);
			if (techniques != 0) {
				WriteName64(stream, "techniques");
				for (uint32_t i = 0; i < techniques; ++i) {
					WriteName64(stream, "technique" + std::to_string(i));
					Write<uint32_t>(stream, 4);
					Write<uint8_t>(stream, 1);
					for (uint32_t j = 0; j < 16; ++j) Write(stream, static_cast<float>(j));
				}
			}
		}
	}

	void WriteParticle(std::ostream& stream, uint32_t version, const std::string& names, const std::string& config) {
		if (version >= 43) Write<int16_t>(stream, -2); // priority
		WriteFloats(stream, { 4.0f, 5.0f, 6.0f }); // position
		WriteFloats(stream, { 1.0f, 0.0f, 0.0f, 0.0f }); // rotation, w first
		WriteWString32(stream, names);
		if (version < 46) Write<uint16_t>(stream, 9);
		else WriteWString32(stream, config);
	}

	void WriteChunkHeader(std::ostream& stream, uint32_t id, uint32_t size, uint32_t dataStart) {
		stream.write("CHNK", 4);
		Write(stream, id);
		Write<uint16_t>(stream, 1);
		Write<uint16_t>(stream, 1);
		Write(stream, size);
		Write(stream, dataStart);
	}

	// Pads to the next 16 bytes with 0xcd, as the client's files are
	void Pad(std::ostringstream& stream) {
		while (stream.tellp() % 16 != 0) Write<uint8_t>(stream, 0xcd);
	}

	// A scene file from before chunks
	std::string WriteFlat(uint32_t version, bool withParticles = true) {
		std::ostringstream stream;
		Write<uint16_t>(stream, static_cast<uint16_t>(version));
		Write<uint16_t>(stream, 0);
		if (version >= 32) Write<uint8_t>(stream, 0); // no editor settings flag
		if (version > 36) Write<uint32_t>(stream, 77); // revision
		WriteLighting(stream, version);
		WriteSkydome(stream, version);
		if (version >= 37) WriteEditorSettings(stream);
		if (version >= 3) {
			Write<uint32_t>(stream, 1);
			WriteObject(stream, version, 1000, 2000, "respawn=5:20000\ncustom=0:a");
		}
		if (version >= 5) {
			Write<uint32_t>(stream, 2);
			WriteFloats(stream, { 8.0f, 9.0f, 10.0f });
			if (version < 35) {
				std::u16string name(260, u'\0');
				name.replace(0, 5, u"birds");
				stream.write(reinterpret_cast<const char*>(name.data()), name.size() * sizeof(char16_t));
			} else {
				WriteWString32(stream, "soundName=0:birds");
			}
			WriteFloats(stream, { 11.0f, 12.0f, 13.0f });
			if (version < 35) stream.write(std::string(520, '\0').data(), 520);
			else WriteWString32(stream, "");
			if (withParticles) {
				Write<uint32_t>(stream, 1);
				WriteParticle(stream, version, "fx.psb", "");
			}
		}
		return stream.str();
	}

	// A chunked scene file: file info, environment, objects and particles chunks, each padded as the client's are
	std::string WriteChunked(uint32_t version) {
		std::ostringstream stream;
		WriteChunkHeader(stream, 1000, 64, 32);
		Pad(stream);
		Write(stream, version);
		Write<uint32_t>(stream, 9); // revision
		Write<uint32_t>(stream, 64); // environment
		Write<uint32_t>(stream, 0); // objects, set below
		Write<uint32_t>(stream, 0); // particles, set below
		Pad(stream);

		// Environment: its header, the three offsets, then each part
		WriteChunkHeader(stream, 2000, 0, 96);
		Pad(stream);
		const auto offsets = stream.tellp();
		WriteFloats(stream, { 0.0f, 0.0f, 0.0f });
		Pad(stream);
		const uint32_t lighting = static_cast<uint32_t>(stream.tellp());
		WriteLighting(stream, version);
		Pad(stream);
		const uint32_t skydome = static_cast<uint32_t>(stream.tellp());
		WriteSkydome(stream, version);
		Pad(stream);
		const uint32_t editor = static_cast<uint32_t>(stream.tellp());
		WriteEditorSettings(stream);
		Pad(stream);

		const uint32_t objects = static_cast<uint32_t>(stream.tellp());
		WriteChunkHeader(stream, 2001, 0, objects + 32);
		Pad(stream);
		Write<uint32_t>(stream, 3);
		WriteObject(stream, version, 1, 176, "spawntemplate=1:14,subkey=9:55\nrespawn=3:250");
		WriteObject(stream, version, 2, 1, "");
		WriteObject(stream, version, 3, 4000, "sceneIDOverrideEnabled=7:1,novalue", 2);
		Pad(stream);

		const uint32_t particles = static_cast<uint32_t>(stream.tellp());
		WriteChunkHeader(stream, 2002, 0, particles + 32);
		Pad(stream);
		Write<uint32_t>(stream, 3);
		WriteParticle(stream, version, "fx1.psb;fx2.psb", "fxconfig=0:1");
		WriteParticle(stream, version, "fx3.psb", "");
		WriteParticle(stream, version, "", "");

		auto data = stream.str();
		const auto patch = [&data](size_t at, uint32_t value) { std::memcpy(data.data() + at, &value, sizeof(value)); };
		patch(32 + 12, objects);
		patch(32 + 16, particles);
		patch(static_cast<size_t>(offsets), lighting);
		patch(static_cast<size_t>(offsets) + 4, skydome);
		patch(static_cast<size_t>(offsets) + 8, editor);
		return data;
	}

	Level Read(const std::string& data) {
		std::istringstream stream(data);
		return Level(stream);
	}
}

class LevelTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(LevelTests, ReadsEveryPartOfAChunkedFile) {
	const auto level = Read(WriteChunked(46));
	EXPECT_EQ(level.GetVersion(), 46);
	EXPECT_EQ(level.GetRevision(), 9);
	ASSERT_EQ(level.m_ChunkHeaders.size(), 4);
	EXPECT_EQ(level.m_ChunkHeaders.at(1000).fileInfo.revision, 9);

	ASSERT_TRUE(level.HasLighting());
	EXPECT_FLOAT_EQ(level.GetLighting().blendTime, 2.5f);
	EXPECT_FLOAT_EQ(level.GetLighting().ambient.m_Blue, 0.3f);
	EXPECT_FLOAT_EQ(level.GetLighting().upperHemi.m_Red, 0.7f);
	EXPECT_FLOAT_EQ(level.GetLighting().position.z, 30.0f);
	EXPECT_FLOAT_EQ(level.GetLighting().minDrawDistances.dynamicObjectDistance, 6.0f);
	EXPECT_FLOAT_EQ(level.GetLighting().maxDrawDistances.fogNear, 7.0f);
	ASSERT_EQ(level.GetLighting().cullSettings.size(), 1);
	EXPECT_EQ(level.GetLighting().cullSettings[0].group, 3);
	EXPECT_FLOAT_EQ(level.GetLighting().cullSettings[0].maxDistance, 200.0f);
	EXPECT_FLOAT_EQ(level.GetLighting().fogColor.m_Green, 0.5f);
	EXPECT_FLOAT_EQ(level.GetLighting().directional.m_Red, 1.0f);
	EXPECT_FALSE(level.GetLighting().hasSpawnPoint);

	EXPECT_EQ(level.GetSkydome().filename, "mesh/env/sky.nif");
	EXPECT_EQ(level.GetSkydome().extraFilenames[4], "e.nif");
	ASSERT_EQ(level.GetEditorColors().size(), 2);
	EXPECT_FLOAT_EQ(level.GetEditorColors()[1].m_Green, 1.0f);

	ASSERT_EQ(level.GetObjects().size(), 3);
	const auto& technique = level.GetObjects()[2];
	EXPECT_EQ(technique.nodeType, 1);
	EXPECT_EQ(technique.glomId, 4);
	EXPECT_FLOAT_EQ(technique.scale, 2.0f);
	EXPECT_EQ(technique.renderTechniquesName, "techniques");
	ASSERT_EQ(technique.renderTechniques.size(), 2);
	EXPECT_EQ(technique.renderTechniques[1].name, "technique1");
	EXPECT_EQ(technique.renderTechniques[1].valueCount, 4);
	EXPECT_EQ(technique.renderTechniques[1].isColor, 1);
	EXPECT_FLOAT_EQ(technique.renderTechniques[1].values[15], 15.0f);

	ASSERT_EQ(level.GetParticles().size(), 2); // the client stops at the one without an effect
	EXPECT_EQ(level.GetParticles()[0].priority, -2);
	EXPECT_EQ(level.GetParticles()[0].effectNames, "fx1.psb;fx2.psb");
	EXPECT_EQ(level.GetParticles()[0].config, "fxconfig=0:1");
	EXPECT_FLOAT_EQ(level.GetParticles()[1].position.y, 5.0f);
	EXPECT_TRUE(level.GetSounds().empty());
}

// LwoNameValue::FromString splits at commas and line breaks and leaves out entries without a '='; ReadLvlObjectData
// adjusts some values as it loads the object
TEST_F(LevelTests, ObjectConfigIsReadAndAdjustedAsTheClientDoes) {
	const auto level = Read(WriteChunked(43));
	ASSERT_EQ(level.GetObjects().size(), 3);

	const auto& spawner = level.GetObjects()[0].settings;
	ASSERT_NE(spawner.find(u"blueprintid"), spawner.end());
	EXPECT_EQ(spawner.find(u"blueprintid")->second->GetValueAsString(), "55");
	EXPECT_EQ(spawner.find(u"DisableModelBehaviors")->second->GetValueAsString(), "1");
	EXPECT_EQ(spawner.find(u"preventRenderWrapping")->second->GetValueAsString(), "1");
	// A float respawn over 100 was milliseconds
	EXPECT_EQ(spawner.find(u"respawn")->second->GetValueType(), LDF_TYPE_FLOAT);
	EXPECT_FLOAT_EQ(static_cast<const LDFData<float>*>(spawner.find(u"respawn")->second.get())->GetValue(), 250.0f * 0.001f);

	const auto& overridden = level.GetObjects()[2].settings;
	EXPECT_EQ(overridden.values.size(), 2); // the entry without a '=' is left out
	ASSERT_NE(overridden.find(u"sceneLayerIDOverride"), overridden.end());
	EXPECT_EQ(overridden.find(u"sceneLayerIDOverride")->second->GetValueAsString(), "0");

	// From version 47 a respawn is in seconds already, and from 44 the scene override stays
	const auto newer = Read(WriteChunked(47));
	EXPECT_FLOAT_EQ(static_cast<const LDFData<float>*>(newer.GetObjects()[0].settings.find(u"respawn")->second.get())->GetValue(), 250.0f);
	EXPECT_EQ(newer.GetObjects()[2].settings.find(u"sceneLayerIDOverride"), newer.GetObjects()[2].settings.end());
}

TEST_F(LevelTests, ReadsAFileFromBeforeChunks) {
	const auto level = Read(WriteFlat(40));
	EXPECT_EQ(level.GetVersion(), 40);
	EXPECT_EQ(level.GetRevision(), 77);
	ASSERT_EQ(level.m_ChunkHeaders.size(), 1);
	EXPECT_EQ(level.m_ChunkHeaders.at(1000).fileInfo.revision, 77);
	EXPECT_FLOAT_EQ(level.GetLighting().blendTime, 10.0f); // the client's default before version 45
	ASSERT_EQ(level.GetLighting().cullSettings.size(), 1);
	ASSERT_TRUE(level.GetLighting().hasSpawnPoint);
	EXPECT_FLOAT_EQ(level.GetLighting().spawnPosition.y, 6.0f);
	EXPECT_FLOAT_EQ(level.GetLighting().spawnRotation.x, 1.0f);
	EXPECT_EQ(level.GetEditorColors().size(), 2);

	ASSERT_EQ(level.GetObjects().size(), 1);
	// Before version 47 a u32 respawn is milliseconds; it becomes seconds
	const auto& respawn = level.GetObjects()[0].settings.find(u"respawn")->second;
	EXPECT_EQ(respawn->GetValueType(), LDF_TYPE_FLOAT);
	EXPECT_FLOAT_EQ(static_cast<const LDFData<float>*>(respawn.get())->GetValue(), 20.0f);

	ASSERT_EQ(level.GetSounds().size(), 2);
	EXPECT_FLOAT_EQ(level.GetSounds()[1].position.z, 13.0f);
	EXPECT_EQ(level.GetSounds()[0].config.find(u"soundName")->second->GetValueAsString(), "birds");
	ASSERT_EQ(level.GetParticles().size(), 1);
	EXPECT_EQ(level.GetParticles()[0].priority, 0); // only from version 43
	EXPECT_EQ(level.GetParticles()[0].unknown, 9);
}

// Older lighting: fog distances only before 39, no directional colour before 36, and before 33 a fixed spawn point
TEST_F(LevelTests, ReadsOlderLightingAndSounds) {
	const auto v32 = Read(WriteFlat(32));
	EXPECT_EQ(v32.GetRevision(), 0);
	EXPECT_FLOAT_EQ(v32.GetLighting().minDrawDistances.fogFar, 60.0f);
	EXPECT_FLOAT_EQ(v32.GetLighting().maxDrawDistances.fogNear, 50.0f);
	EXPECT_FLOAT_EQ(v32.GetLighting().maxDrawDistances.staticObjectDistance, -1.0f);
	EXPECT_FLOAT_EQ(v32.GetLighting().directional.m_Red, 0.0f);
	EXPECT_FLOAT_EQ(v32.GetLighting().filePosition.x, 5.0f);
	EXPECT_FLOAT_EQ(v32.GetLighting().spawnPosition.x, 347.0f);
	EXPECT_TRUE(v32.GetSkydome().extraFilenames[0].empty());
	EXPECT_TRUE(v32.GetEditorColors().empty());
	ASSERT_EQ(v32.GetSounds().size(), 2);
	EXPECT_EQ(v32.GetSounds()[0].config.find(u"soundName")->second->GetValueAsString(), "birds");
	ASSERT_EQ(v32.GetObjects().size(), 1);

	const auto v30 = Read(WriteFlat(30));
	EXPECT_FLOAT_EQ(v30.GetLighting().fogColor.m_Red, 0.75f); // the client's default
	EXPECT_FLOAT_EQ(v30.GetLighting().spawnRotation.y, 0.23979919f);
	EXPECT_EQ(v30.GetObjects().size(), 1);
	EXPECT_EQ(v30.GetParticles().size(), 1);
}

// Before version 44 a file may end before its particles; a file older than 3 has no objects, a newer one than 48
// isn't read
TEST_F(LevelTests, ReadsWhatTheClientReadsOfOtherFiles) {
	const auto noParticles = Read(WriteFlat(43, false));
	EXPECT_EQ(noParticles.GetObjects().size(), 1);
	EXPECT_EQ(noParticles.GetSounds().size(), 2);
	EXPECT_TRUE(noParticles.GetParticles().empty());

	const auto old = Read(WriteFlat(2));
	EXPECT_TRUE(old.HasLighting());
	EXPECT_TRUE(old.GetObjects().empty());

	std::ostringstream newer;
	Write<uint16_t>(newer, 49);
	Write<uint16_t>(newer, 0);
	Write<uint8_t>(newer, 0);
	Write<uint32_t>(newer, 1);
	const auto refused = Read(newer.str());
	EXPECT_EQ(refused.GetVersion(), 49);
	EXPECT_FALSE(refused.HasLighting());
}

// A chunked file without lighting has none of the environment's parts read
TEST_F(LevelTests, NoEnvironmentWithoutLighting) {
	auto data = WriteChunked(48);
	std::memset(data.data() + 96, 0, 4); // lighting offset
	const auto level = Read(data);
	EXPECT_FALSE(level.HasLighting());
	EXPECT_TRUE(level.GetSkydome().filename.empty());
	EXPECT_TRUE(level.GetEditorColors().empty());
	EXPECT_EQ(level.GetObjects().size(), 3);
}
