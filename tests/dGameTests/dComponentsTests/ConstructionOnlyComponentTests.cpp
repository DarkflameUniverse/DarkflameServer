// What the client reads for a chest, a choice build and a roller: LWOChestComponent::Deserialize (0x00c22010),
// LWOChoiceBuildComponent::Deserialize (0x00bf26d0) and LWORollerComponent::Deserialize (0x00c15920). Where they are
// read among an object's components is in ReplicaComponentOrderTests.
#include "GameDependencies.h"

#include "BitStream.h"
#include "ChestComponent.h"
#include "ChoiceBuildComponent.h"
#include "Entity.h"
#include "RollerComponent.h"

#include <gtest/gtest.h>
#include <vector>

class ConstructionOnlyComponentTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }

	template <typename Component>
	static std::vector<uint8_t> Bytes(Component& component, const bool construction, uint32_t* const bits = nullptr) {
		RakNet::BitStream stream;
		component.Serialize(stream, construction);
		if (bits) *bits = stream.GetNumberOfBitsUsed();
		return { stream.GetData(), stream.GetData() + stream.GetNumberOfBytesUsed() };
	}
};

// Construction: the player (u64) and the loot offer count (u32); nothing in an update
TEST_F(ConstructionOnlyComponentTests, Chest) {
	Entity entity(15, info);
	auto* const chest = entity.AddComponent<ChestComponent>(0);
	uint32_t bits{};
	EXPECT_EQ(Bytes(*chest, true, &bits), (std::vector<uint8_t>{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }));
	EXPECT_EQ(bits, 96u);

	chest->SetPlayerID(0x1000000000000001LL);
	chest->SetLootOfferCount(3);
	EXPECT_EQ(Bytes(*chest, true, &bits), (std::vector<uint8_t>{ 0x01, 0, 0, 0, 0, 0, 0, 0x10, 0x03, 0, 0, 0 }));
	EXPECT_EQ(bits, 96u);

	EXPECT_TRUE(Bytes(*chest, false, &bits).empty());
	EXPECT_EQ(bits, 0u);
}

// A flag, then the state (u32), the built object (u64) and the user (u64): always in a construction, in an update
// after a change only
TEST_F(ConstructionOnlyComponentTests, ChoiceBuild) {
	Entity entity(15, info);
	auto* const choiceBuild = entity.AddComponent<ChoiceBuildComponent>(1);
	uint32_t bits{};
	// 1 flag bit, then 160 zero bits: waiting, nothing built, nobody using it (the client's own start values)
	std::vector<uint8_t> waiting(21, 0);
	waiting[0] = 0x80;
	EXPECT_EQ(Bytes(*choiceBuild, true, &bits), waiting);
	EXPECT_EQ(bits, 161u);

	EXPECT_EQ(Bytes(*choiceBuild, false, &bits), (std::vector<uint8_t>{ 0x00 }));
	EXPECT_EQ(bits, 1u);

	choiceBuild->SetState(ChoiceBuildComponent::eState::Building, 0x0000400000000002LL, 0x1000000000000001LL);
	// The same fields shifted by the flag bit: state 1, object 0x0000400000000002, user 0x1000000000000001
	RakNet::BitStream expected;
	expected.Write1();
	expected.Write<uint32_t>(1);
	expected.Write<LWOOBJID>(0x0000400000000002LL);
	expected.Write<LWOOBJID>(0x1000000000000001LL);
	const std::vector<uint8_t> building{ expected.GetData(), expected.GetData() + expected.GetNumberOfBytesUsed() };
	EXPECT_EQ(building[0], 0x80); // the flag, then the low byte of the state (1) one bit on
	EXPECT_EQ(building[1], 0x80);
	EXPECT_EQ(Bytes(*choiceBuild, false, &bits), building);
	EXPECT_EQ(bits, 161u);
	// Sent once
	EXPECT_EQ(Bytes(*choiceBuild, false, &bits), (std::vector<uint8_t>{ 0x00 }));
	// A later construction carries the state as it is
	EXPECT_EQ(Bytes(*choiceBuild, true, &bits), building);
}

// Construction: camera position and look-at (3 floats each), the projectile skill (u32, the client starts at 1348),
// then the flag of the user block, which is never set. An update is that flag alone.
TEST_F(ConstructionOnlyComponentTests, Roller) {
	Entity entity(15, info);
	auto* const roller = entity.AddComponent<RollerComponent>(13951);
	uint32_t bits{};
	std::vector<uint8_t> expected(29, 0);
	expected[24] = 0x44; // 1348 = 0x544
	expected[25] = 0x05;
	EXPECT_EQ(Bytes(*roller, true, &bits), expected);
	EXPECT_EQ(bits, 225u);

	EXPECT_EQ(Bytes(*roller, false, &bits), (std::vector<uint8_t>{ 0x00 }));
	EXPECT_EQ(bits, 1u);
}

// The level's activity_camera_pos / activity_camera_lookat_pos: three numbers with character 31 between them
TEST_F(ConstructionOnlyComponentTests, RollerCameraComesFromTheLevel) {
	Entity entity(15, info);
	entity.SetVar<std::u16string>(u"activity_camera_pos", u"-327.861\x1f" u"256.91\x1f" u".64822");
	entity.SetVar<std::u16string>(u"activity_camera_lookat_pos", u"1\x1f" u"2\x1f" u"-0.5");
	auto* const roller = entity.AddComponent<RollerComponent>(13951); // loads the level's keys
	EXPECT_EQ(roller->GetCameraPosition(), NiPoint3(-327.861f, 256.91f, 0.64822f));
	EXPECT_EQ(roller->GetCameraLookAt(), NiPoint3(1.0f, 2.0f, -0.5f));

	RakNet::BitStream expected;
	for (const float value : { -327.861f, 256.91f, 0.64822f, 1.0f, 2.0f, -0.5f }) expected.Write(value);
	expected.Write<uint32_t>(1348);
	expected.Write0();
	uint32_t bits{};
	EXPECT_EQ(Bytes(*roller, true, &bits), (std::vector<uint8_t>{ expected.GetData(), expected.GetData() + expected.GetNumberOfBytesUsed() }));
	EXPECT_EQ(bits, 225u);
	// 1.0f little endian, the first look-at number
	EXPECT_EQ(Bytes(*roller, true)[12 + 3], 0x3f);
}
