// Which missions a mission giver lists for a player and offers, as the 1.10.64 client works it out
// (LWOMissionOfferComponent 0x00c605f0 / 0x00bebf30, LWOMissionComponent::getMissionState 0x00cd7400).
#include "GameDependencies.h"
#include "PacketTestUtils.h"

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDMissionNPCComponentTable.h"
#include "CDMissionsTable.h"
#include "Entity.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "MissionOfferComponent.h"
#include "eMissionState.h"

#include "tinyxml2.h"

#include <ctime>
#include <gtest/gtest.h>

namespace {
	// Mission IDs only these tests use (the prerequisite cache is static)
	constexpr uint32_t NEW = 991001;       // no prerequisites
	constexpr uint32_t ACTIVE = 991002;
	constexpr uint32_t TURN_IN = 991003;   // ready to complete, accepted here
	constexpr uint32_t ELSEWHERE = 991004; // ready to complete, only offered here
	constexpr uint32_t DONE = 991005;      // completed, not repeatable
	constexpr uint32_t DAILY = 991006;     // completed, repeatable, 60 minute cooldown
	constexpr uint32_t LOCKED = 991007;    // needs DONE_LATER
	constexpr uint32_t DONE_LATER = 991008;
	constexpr uint32_t NEXT = 991009;      // needs DONE
	constexpr int32_t GIVER = 99100;

	CDMissions Row(const uint32_t id, const bool repeatable = false, const std::string& prereq = "", const int64_t cooldown = -1) {
		CDMissions row{};
		row.id = id;
		row.isMission = true;
		row.repeatable = repeatable;
		row.prereqMissionID = prereq;
		row.cooldownTime = cooldown;
		return row;
	}

	int32_t State(const eMissionState state) { return static_cast<int32_t>(state); }
}

class MissionOfferTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:"); // MissionComponent counts the achievements
		CDClientDatabase::ExecuteDML("CREATE TABLE Missions (id INTEGER, isMission INTEGER);");
		auto& missions = CDClientManager::GetEntriesMutable<CDMissionsTable>();
		for (const auto id : { NEW, ACTIVE, TURN_IN, ELSEWHERE, DONE, DONE_LATER }) missions.push_back(Row(id));
		missions.push_back(Row(DAILY, true, "", 60));
		missions.push_back(Row(LOCKED, false, std::to_string(DONE_LATER)));
		missions.push_back(Row(NEXT, false, std::to_string(DONE)));
	}

	void TearDown() override {
		CDClientManager::GetEntriesMutable<CDMissionsTable>().clear();
		CDClientManager::GetEntriesMutable<CDMissionNPCComponentTable>().clear();
		TearDownDependencies();
	}

	static void Load(MissionComponent& missions, const std::string& done, const std::string& cur) {
		tinyxml2::XMLDocument doc;
		const auto xml = R"(<obj v="1"><mis><done>)" + done + "</done><cur>" + cur + "</cur></mis></obj>";
		ASSERT_EQ(doc.Parse(xml.c_str()), tinyxml2::XML_SUCCESS);
		missions.LoadFromSaveData(doc);
	}

	static std::string Done(const uint32_t id, const uint32_t state, const int64_t at) {
		return "<m id=\"" + std::to_string(id) + "\" state=\"" + std::to_string(state) + "\" cct=\"1\" cts=\"" + std::to_string(at) + "\"/>";
	}

	static std::string Cur(const uint32_t id, const uint32_t state) {
		return "<m id=\"" + std::to_string(id) + "\" state=\"" + std::to_string(state) + "\"/>";
	}

	static void Giver(const std::vector<CDMissionNPCComponent>& rows) {
		auto& table = CDClientManager::GetEntriesMutable<CDMissionNPCComponentTable>();
		table = rows;
	}

	static std::vector<uint32_t> OfferedIDs(const std::vector<CapturedPacket>& packets) {
		std::vector<uint32_t> ids;
		for (const auto& packet : packets) {
			// 8 byte header, the target, the message ID, then the mission ID
			if (packet.bytes.size() < 22) continue;
			const uint16_t messageID = packet.bytes[16] | (packet.bytes[17] << 8);
			if (messageID != 248) continue;
			ids.push_back(packet.bytes[18] | (packet.bytes[19] << 8) | (packet.bytes[20] << 16) | (static_cast<uint32_t>(packet.bytes[21]) << 24));
		}
		return ids;
	}
};

TEST_F(MissionOfferTests, EntryPrioritiesAsTheClient) {
	using C = MissionOfferComponent;
	const auto ready = State(eMissionState::READY_TO_COMPLETE);
	// Ready to turn in: 5 where offered and accepted, 0 (a turn in) where only accepted, nothing where only offered
	EXPECT_EQ(C::GetOfferEntry(1, ready, false, true, true, false), (MissionOfferEntry{ 5, 1, ready, false }));
	EXPECT_EQ(C::GetOfferEntry(1, ready, false, false, true, false), (MissionOfferEntry{ 0, 1, ready, true }));
	EXPECT_FALSE(C::GetOfferEntry(1, ready, false, true, false, false));
	EXPECT_EQ(C::GetOfferEntry(1, State(eMissionState::COMPLETE_READY_TO_COMPLETE), false, false, true, false)->priority, 0);
	// In progress: every giver that has it
	EXPECT_EQ(C::GetOfferEntry(1, State(eMissionState::ACTIVE), false, false, false, false)->priority, 5);
	EXPECT_EQ(C::GetOfferEntry(1, State(eMissionState::COMPLETE_ACTIVE), false, false, false, false)->priority, 5);
	// Available: only where offered and off cooldown; a repeat after a new one
	EXPECT_EQ(C::GetOfferEntry(1, State(eMissionState::AVAILABLE), false, true, false, false)->priority, 10);
	EXPECT_EQ(C::GetOfferEntry(1, State(eMissionState::COMPLETE_AVAILABLE), false, true, false, false)->priority, 15);
	EXPECT_FALSE(C::GetOfferEntry(1, State(eMissionState::AVAILABLE), false, false, true, false));
	EXPECT_FALSE(C::GetOfferEntry(1, State(eMissionState::COMPLETE_AVAILABLE), true, true, false, false));
	// Completed: only when asked for
	EXPECT_FALSE(C::GetOfferEntry(1, State(eMissionState::COMPLETE), false, true, true, false));
	EXPECT_EQ(C::GetOfferEntry(1, State(eMissionState::COMPLETE), false, false, true, true), (MissionOfferEntry{ 20, 1, 8, true }));
	EXPECT_FALSE(C::GetOfferEntry(1, 0, false, true, true, true));
}

TEST_F(MissionOfferTests, SamePriorityKeepsInsertionOrder) {
	std::vector<MissionOfferEntry> entries;
	MissionOfferComponent::InsertByPriority(entries, { 10, 1 });
	MissionOfferComponent::InsertByPriority(entries, { 5, 2 });
	MissionOfferComponent::InsertByPriority(entries, { 10, 3 });
	MissionOfferComponent::InsertByPriority(entries, { 0, 4 });
	MissionOfferComponent::InsertByPriority(entries, { 5, 5 });
	std::vector<uint32_t> ids;
	for (const auto& entry : entries) ids.push_back(entry.missionID);
	EXPECT_EQ(ids, (std::vector<uint32_t>{ 4, 2, 5, 1, 3 }));
}

TEST_F(MissionOfferTests, CooldownInWholeMinutes) {
	constexpr int64_t at = 1000000;
	EXPECT_TRUE(MissionComponent::IsCooldownRunning(at, 60, at + 59 * 60 + 59));
	EXPECT_FALSE(MissionComponent::IsCooldownRunning(at, 60, at + 60 * 60));
	EXPECT_FALSE(MissionComponent::IsCooldownRunning(at, 0, at)); // no cooldown
	EXPECT_FALSE(MissionComponent::IsCooldownRunning(at, -1, at));
	EXPECT_TRUE(MissionComponent::IsCooldownRunning(at, 1, at - 120)); // completed in the future
}

TEST_F(MissionOfferTests, ClientMissionStateIsABitMask) {
	Entity player(42, info);
	auto* const missions = player.AddComponent<MissionComponent>(-1);
	const auto now = static_cast<int64_t>(std::time(nullptr));
	Load(*missions, Done(DONE, 8, now) + Done(DAILY, 8, now), Cur(ACTIVE, 2) + Cur(TURN_IN, 4));

	EXPECT_EQ(missions->GetClientMissionState(NEW), State(eMissionState::AVAILABLE));
	EXPECT_EQ(missions->GetClientMissionState(ACTIVE), State(eMissionState::ACTIVE));
	EXPECT_EQ(missions->GetClientMissionState(TURN_IN), State(eMissionState::READY_TO_COMPLETE));
	EXPECT_EQ(missions->GetClientMissionState(DONE), State(eMissionState::COMPLETE));
	EXPECT_EQ(missions->GetClientMissionState(DAILY), State(eMissionState::COMPLETE_AVAILABLE)); // even on cooldown
	EXPECT_EQ(missions->GetClientMissionState(LOCKED), 0); // prerequisite not met
	EXPECT_EQ(missions->GetClientMissionState(NEXT), State(eMissionState::AVAILABLE));
	EXPECT_EQ(missions->GetClientMissionState(66666666), 0); // no such mission
	EXPECT_EQ(missions->GetClientMissionState(0), 0);
	EXPECT_TRUE(missions->IsCooldownRunning(DAILY));
	EXPECT_FALSE(missions->IsCooldownRunning(DONE)); // no cooldown
	EXPECT_FALSE(missions->IsCooldownRunning(NEW));  // never completed

	// The message answers with the same mask (DLU answered -1 for a mission that cannot be taken)
	GameMessages::GetMissionState locked;
	locked.missionID = LOCKED;
	player.HandleMsg(locked);
	EXPECT_EQ(locked.missionState, static_cast<eMissionState>(0));

	GameMessages::GetMissionState daily;
	daily.missionID = DAILY;
	daily.cooldownInfoRequested = true;
	player.HandleMsg(daily);
	EXPECT_EQ(daily.missionState, eMissionState::COMPLETE_AVAILABLE);
	EXPECT_TRUE(daily.cooldownFinished); // the client only checks the cooldown of a repeat in progress
}

TEST_F(MissionOfferTests, GiverListsMissionsInTheClientsOrder) {
	Giver({
		{ GIVER, NEW, true, false, "" },
		{ GIVER, ACTIVE, true, false, "" },
		{ GIVER, TURN_IN, false, true, "" },
		{ GIVER, ELSEWHERE, true, false, "" },
		{ GIVER, DONE, true, true, "" },
		{ GIVER, DAILY, true, true, "" },
		{ GIVER, LOCKED, true, false, "" },
	});
	Entity npc(43, info);
	auto* const giver = npc.AddComponent<MissionOfferComponent>(GIVER);
	giver->LoadDataFromTemplate();

	Entity player(44, info);
	auto* const missions = player.AddComponent<MissionComponent>(-1);
	const auto now = static_cast<int64_t>(std::time(nullptr));
	Load(*missions, Done(DONE, 8, now) + Done(DAILY, 8, now), Cur(ACTIVE, 2) + Cur(TURN_IN, 4) + Cur(ELSEWHERE, 4));

	std::vector<std::pair<uint32_t, uint8_t>> listed;
	for (const auto& entry : giver->GetMissionsForPlayer(*missions, false)) listed.emplace_back(entry.missionID, entry.priority);
	// The turn in first, then the mission in progress, then the new one; not the one ready to turn in elsewhere, not
	// the daily on cooldown, not the done or locked ones
	EXPECT_EQ(listed, (std::vector<std::pair<uint32_t, uint8_t>>{ { TURN_IN, 0 }, { ACTIVE, 5 }, { NEW, 10 } }));

	// A use offers only the first one (live sent one OfferMission pair per use)
	const auto sent = PacketTestUtils::Capture([&] { giver->OfferMissions(&player); });
	EXPECT_EQ(OfferedIDs(sent), (std::vector<uint32_t>{ TURN_IN, TURN_IN }));

	// A pick from the multi interaction menu offers that one, if it is listed
	EXPECT_EQ(OfferedIDs(PacketTestUtils::Capture([&] { giver->OfferMissions(&player, NEW); })), (std::vector<uint32_t>{ NEW, NEW }));
	EXPECT_TRUE(OfferedIDs(PacketTestUtils::Capture([&] { giver->OfferMissions(&player, LOCKED); })).empty());
}

TEST_F(MissionOfferTests, LinkedMissionOnlyAfterATurnIn) {
	const auto active = State(eMissionState::ACTIVE);
	const auto available = State(eMissionState::AVAILABLE);
	// Just accepted: the list starts with that mission; nothing else new
	EXPECT_FALSE(MissionOfferComponent::GetLinkedMission({ { 5, 697, active } }, 697));
	// Just turned in: the next one
	EXPECT_EQ(MissionOfferComponent::GetLinkedMission({ { 10, 698, available } }, 697), 698u);
	// Not a repeat, and not a mission in progress
	EXPECT_FALSE(MissionOfferComponent::GetLinkedMission({ { 15, 698, State(eMissionState::COMPLETE_AVAILABLE) } }, 697));
	EXPECT_FALSE(MissionOfferComponent::GetLinkedMission({ { 5, 700, active }, { 10, 698, available } }, 697));
	EXPECT_FALSE(MissionOfferComponent::GetLinkedMission({}, 697));
}
