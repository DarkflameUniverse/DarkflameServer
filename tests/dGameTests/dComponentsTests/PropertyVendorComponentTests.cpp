// The property guard's mission: LWOPropertyVendorComponent::onCheckUseRequirements (0x00c2e210) lets a player use the
// guard only with the level's property_required_mission finished.
#include "GameDependencies.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDMissionsTable.h"
#include "Entity.h"
#include "MissionComponent.h"
#include "PropertyMessages.h"
#include "PropertyVendorComponent.h"

#include "tinyxml2.h"

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	// Mission IDs only these tests use
	constexpr uint32_t REQUIRED = 993001;
	constexpr uint32_t REPEATED = 993002;
}

class PropertyVendorComponentTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:"); // MissionComponent counts the achievements
		CDClientDatabase::ExecuteDML("CREATE TABLE Missions (id INTEGER, isMission INTEGER);");
		auto& missions = CDClientManager::GetEntriesMutable<CDMissionsTable>();
		for (const auto id : { REQUIRED, REPEATED }) {
			CDMissions row{};
			row.id = id;
			row.isMission = true;
			row.repeatable = id == REPEATED;
			missions.push_back(row);
		}
		guard = std::make_unique<Entity>(GUARD, info);
		vendor = guard->AddComponent<PropertyVendorComponent>(-1);
		player = std::make_unique<Entity>(PLAYER, info);
		missions_ = player->AddComponent<MissionComponent>(-1);
	}

	void TearDown() override {
		player.reset();
		guard.reset();
		CDClientManager::GetEntriesMutable<CDMissionsTable>().clear();
		TearDownDependencies();
	}

	void Load(const std::string& done, const std::string& cur) {
		tinyxml2::XMLDocument doc;
		const auto xml = R"(<obj v="1"><mis><done>)" + done + "</done><cur>" + cur + "</cur></mis></obj>";
		ASSERT_EQ(doc.Parse(xml.c_str()), tinyxml2::XML_SUCCESS);
		missions_->LoadFromSaveData(doc);
	}

	static constexpr LWOOBJID GUARD = 0x0000400000112233LL;
	static constexpr LWOOBJID PLAYER = 0x1000000000000001LL;
	std::unique_ptr<Entity> guard;
	std::unique_ptr<Entity> player;
	PropertyVendorComponent* vendor = nullptr;
	MissionComponent* missions_ = nullptr;
};

TEST_F(PropertyVendorComponentTests, AGuardWithoutAMissionAsksForNone) {
	EXPECT_EQ(vendor->GetRequiredMission(), 0u);
	EXPECT_TRUE(vendor->MayRent(*player));
}

TEST_F(PropertyVendorComponentTests, TheMissionMustBeFinished) {
	guard->SetVar<uint32_t>(u"property_required_mission", REQUIRED);
	EXPECT_EQ(vendor->GetRequiredMission(), REQUIRED);
	EXPECT_FALSE(vendor->MayRent(*player)); // never taken

	Load("", R"(<m id="993001" state="2"/>)");
	EXPECT_FALSE(vendor->MayRent(*player)); // in progress
}

TEST_F(PropertyVendorComponentTests, AFinishedMissionRents) {
	guard->SetVar<uint32_t>(u"property_required_mission", REQUIRED);
	Load(R"(<m id="993001" cct="1" cts="1000"/>)", "");
	EXPECT_TRUE(vendor->MayRent(*player));
}

// The client's list of finished missions keeps a mission that is being repeated
TEST_F(PropertyVendorComponentTests, AFinishedMissionThatIsRepeatedRents) {
	guard->SetVar<uint32_t>(u"property_required_mission", REPEATED);
	Load(R"(<m id="993002" cct="1" cts="1000"/>)", R"(<m id="993002" state="10"/>)");
	EXPECT_TRUE(vendor->MayRent(*player));
}

// A rent request without the mission is answered with code 9, which the client shows as "you failed to rent" and
// UGG_PROPRENT_Dont_have_achievement (LWOPropertyVendorComponent::onPropertyRentalResponse 0x00c56d70):
// clone u32 0, code u32 9, property u64 0, rent due i64 0
TEST_F(PropertyVendorComponentTests, RentingWithoutTheMissionIsRefused) {
	guard->SetVar<uint32_t>(u"property_required_mission", REQUIRED);
	const auto packets = Capture([&] { vendor->OnBuyFromVendor(player.get(), true, 0, 1); });
	ASSERT_EQ(packets.size(), 1u);
	EXPECT_PACKET_EQ(FromHex(
		"53 05 00 0c 00 00 00 00 33 22 11 00 00 40 00 00 ee 02 "
		"00 00 00 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00", 336), FromCapture(packets[0]));
}

// With the mission, the request goes on to the property (none in this test: nothing is sent, nothing refused)
TEST_F(PropertyVendorComponentTests, RentingWithTheMissionIsNotRefused) {
	guard->SetVar<uint32_t>(u"property_required_mission", REQUIRED);
	Load(R"(<m id="993001" cct="1" cts="1000"/>)", "");
	const auto packets = Capture([&] { vendor->OnBuyFromVendor(player.get(), true, 0, 1); });
	EXPECT_TRUE(packets.empty());
}
