#include "MissionMessages.h"
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"
#include "Legacy/MissionMessagesLegacy.h"

#include "eMissionState.h"

#include <functional>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	const std::vector<int32_t> g_Ints = { 0, 1, -1, 1727, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };

	PacketBytes Payload(const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		return FromBitStream(bitStream);
	}

	// Serializes msg, then reads it with the legacy read sequence; both must consume exactly the same bits.
	template<typename Result>
	Result ReadWithLegacy(const GameMessages::NetGameMsg& msg, const std::function<Result(RakNet::BitStream&)>& read) {
		RakNet::BitStream wire;
		msg.Serialize(wire);
		RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
		auto result = read(legacyStream);
		EXPECT_EQ(legacyStream.GetReadOffset(), wire.GetNumberOfBitsUsed());
		return result;
	}
}

class MissionMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(MissionMessagesTests, OfferMissionMatchesLegacy) {
	// The legacy function sent the message twice, first targeting the offerer and then the player.
	for (const auto player : g_Targets) {
		for (const auto offerer : g_Targets) {
			for (const auto missionID : g_Ints) {
				for (const auto& address : g_Addresses) {
					const auto legacy = Capture([&] { LegacyGameMessages::SendOfferMission(player, address, missionID, offerer); });
					const auto ours = Capture([&] {
						GameMessages::OfferMission msg;
						msg.missionID = missionID;
						msg.offerer = offerer;
						msg.target = offerer;
						msg.SendToClient(address);
						msg.target = player;
						msg.SendToClient(address);
						});
					ASSERT_EQ(legacy.size(), 2);
					ASSERT_EQ(ours.size(), 2);
					for (size_t i = 0; i < 2; i++) {
						EXPECT_PACKET_EQ(FromCapture(legacy[i]), FromCapture(ours[i]));
						EXPECT_EQ(legacy[i].broadcast, ours[i].broadcast);
						EXPECT_EQ(legacy[i].sysAddr, ours[i].sysAddr);
					}
				}
				GameMessages::OfferMission msg;
				msg.missionID = missionID;
				msg.offerer = offerer;
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.missionID, missionID);
				EXPECT_EQ(copy.offerer, offerer);
			}
		}
	}
}

TEST_F(MissionMessagesTests, NotifyMissionMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto missionID : g_Ints) {
			for (const int missionState : { 0, 1, 2, 4, 8, 12, -1 }) {
				for (const bool sendingRewards : { false, true }) {
					GameMessages::NotifyMission msg;
					msg.target = target;
					msg.missionID = missionID;
					msg.missionState = missionState;
					msg.sendingRewards = sendingRewards;
					ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendNotifyMission(&entity, a, missionID, missionState, sendingRewards); }, msg, SendMode::SendToClient);
					const auto copy = RoundTrip(msg);
					EXPECT_EQ(copy.missionID, missionID);
					EXPECT_EQ(copy.missionState, missionState);
					EXPECT_EQ(copy.sendingRewards, sendingRewards);
				}
			}
		}
	}
}

TEST_F(MissionMessagesTests, NotifyMissionTaskMatchesLegacy) {
	std::vector<float> many(255);
	for (size_t i = 0; i < many.size(); i++) many[i] = static_cast<float>(i) * 0.5f;
	const std::vector<std::vector<float>> updateSets = { {}, { 1.0f }, { 0.0f, -1.0f, 3.5f }, many };
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto missionID : g_Ints) {
			for (const int taskMask : { 0, 2, 4, 1 << 30 }) {
				for (const auto& updates : updateSets) {
					GameMessages::NotifyMissionTask msg;
					msg.target = target;
					msg.missionID = missionID;
					msg.taskMask = taskMask;
					msg.updates = updates;
					ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendNotifyMissionTask(&entity, a, missionID, taskMask, updates); }, msg, SendMode::SendToClient);
					const auto copy = RoundTrip(msg);
					EXPECT_EQ(copy.missionID, missionID);
					EXPECT_EQ(copy.taskMask, taskMask);
					EXPECT_EQ(copy.updates, updates);
				}
			}
		}
	}
}

TEST_F(MissionMessagesTests, ResetMissionsMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto missionID : g_Ints) {
			GameMessages::ResetMissions msg;
			msg.target = target;
			msg.missionID = missionID;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendResetMissions(&entity, a, missionID); }, msg, SendMode::SendToClient);
			EXPECT_EQ(RoundTrip(msg).missionID, missionID);
		}
	}
}

TEST_F(MissionMessagesTests, NotifyClientFlagChangeMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const uint32_t flagID : { 0u, 42u, 1110u, std::numeric_limits<uint32_t>::max() }) {
			for (const bool bFlag : { false, true }) {
				GameMessages::NotifyClientFlagChange msg;
				msg.target = target;
				msg.iFlagID = flagID;
				msg.bFlag = bFlag;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendNotifyClientFlagChange(target, flagID, bFlag, a); }, msg, SendMode::SendToClient);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.iFlagID, flagID);
				EXPECT_EQ(copy.bFlag, bFlag);
			}
		}
	}
}

TEST_F(MissionMessagesTests, NotifyLevelRewardsMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto level : g_Ints) {
			for (const bool sendingRewards : { false, true }) {
				GameMessages::NotifyLevelRewards msg;
				msg.target = target;
				msg.level = level;
				msg.sendingRewards = sendingRewards;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::NotifyLevelRewards(target, a, level, sendingRewards); }, msg, SendMode::SendToClient);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.level, level);
				EXPECT_EQ(copy.sendingRewards, sendingRewards);
			}
		}
	}
}

TEST_F(MissionMessagesTests, InboundReadsLikeLegacy) {
	for (const auto id : g_Targets) {
		for (const auto value : g_Ints) {
			for (const bool flag : { false, true }) {
				GameMessages::SetFlag setFlag;
				setFlag.bFlag = flag;
				setFlag.iFlagID = value;
				const auto legacySetFlag = ReadWithLegacy<LegacyGameMessages::LegacySetFlag>(setFlag, LegacyGameMessages::ReadSetFlag);
				const auto setFlagCopy = RoundTrip(setFlag);
				EXPECT_EQ(setFlagCopy.bFlag, legacySetFlag.bFlag);
				EXPECT_EQ(setFlagCopy.iFlagID, legacySetFlag.iFlagID);
				ExpectTruncatedFails(setFlag);

				for (const LOT reward : { LOT_NULL, LOT{ 1727 }, LOT{ 0 } }) {
					GameMessages::RespondToMission respond;
					respond.missionID = value;
					respond.playerID = id;
					respond.receiver = 0x0102030405060708LL;
					respond.rewardItem = reward;
					const auto legacyRespond = ReadWithLegacy<LegacyGameMessages::LegacyRespondToMission>(respond, LegacyGameMessages::ReadRespondToMission);
					const auto respondCopy = RoundTrip(respond);
					EXPECT_EQ(respondCopy.missionID, legacyRespond.missionID);
					EXPECT_EQ(respondCopy.playerID, legacyRespond.playerID);
					EXPECT_EQ(respondCopy.receiver, legacyRespond.receiverID);
					EXPECT_EQ(respondCopy.rewardItem, legacyRespond.reward);
					ExpectTruncatedFails(respond);
				}

				for (const auto state : { eMissionState::AVAILABLE, eMissionState::READY_TO_COMPLETE, eMissionState::COMPLETE_AVAILABLE, static_cast<eMissionState>(-1) }) {
					GameMessages::MissionDialogueOK ok;
					ok.bIsComplete = flag;
					ok.iMissionState = state;
					ok.missionID = value;
					ok.responder = id;
					const auto legacyOk = ReadWithLegacy<LegacyGameMessages::LegacyMissionDialogOK>(ok, LegacyGameMessages::ReadMissionDialogOK);
					const auto okCopy = RoundTrip(ok);
					EXPECT_EQ(okCopy.bIsComplete, legacyOk.bIsComplete);
					EXPECT_EQ(okCopy.iMissionState, legacyOk.iMissionState);
					EXPECT_EQ(okCopy.missionID, legacyOk.missionID);
					EXPECT_EQ(okCopy.responder, legacyOk.responder);
					ExpectTruncatedFails(ok);
				}

				GameMessages::RequestLinkedMission linked;
				linked.playerID = id;
				linked.missionID = value;
				linked.bMissionOffered = flag;
				const auto legacyLinked = ReadWithLegacy<LegacyGameMessages::LegacyRequestLinkedMission>(linked, LegacyGameMessages::ReadRequestLinkedMission);
				const auto linkedCopy = RoundTrip(linked);
				EXPECT_EQ(linkedCopy.playerID, legacyLinked.playerId);
				EXPECT_EQ(linkedCopy.missionID, legacyLinked.missionId);
				EXPECT_EQ(linkedCopy.bMissionOffered, legacyLinked.bMissionOffered);
				ExpectTruncatedFails(linked);
			}
		}

		GameMessages::HasBeenCollected collected;
		collected.playerID = id;
		EXPECT_EQ(ReadWithLegacy<LWOOBJID>(collected, LegacyGameMessages::ReadHasBeenCollected), id);
		EXPECT_EQ(RoundTrip(collected).playerID, id);
		ExpectTruncatedFails(collected);
	}
}

// Independent of the legacy code: hand computed payloads (RakNet writes MSB first).
TEST_F(MissionMessagesTests, GoldenBytes) {
	GameMessages::ResetMissions reset;
	EXPECT_PACKET_EQ(FromHex("00", 1), Payload(reset));
	reset.missionID = 5;
	EXPECT_PACKET_EQ(FromHex("82 80 00 00 00", 33), Payload(reset));

	GameMessages::RespondToMission respond;
	respond.missionID = 7;
	respond.playerID = 0x11;
	respond.receiver = 0x22;
	EXPECT_PACKET_EQ(FromHex("07 00 00 00 11 00 00 00 00 00 00 00 22 00 00 00 00 00 00 00 00", 161), Payload(respond));
	respond.rewardItem = 1727;
	EXPECT_PACKET_EQ(FromHex("07 00 00 00 11 00 00 00 00 00 00 00 22 00 00 00 00 00 00 00 df 83 00 00 00", 193), Payload(respond));

	GameMessages::MissionDialogueOK ok;
	ok.bIsComplete = true;
	ok.iMissionState = static_cast<eMissionState>(2);
	ok.missionID = 7;
	ok.responder = 0x33;
	EXPECT_PACKET_EQ(FromHex("81 00 00 00 03 80 00 00 19 80 00 00 00 00 00 00 00", 129), Payload(ok));

	GameMessages::NotifyClientFlagChange flag;
	flag.bFlag = true;
	flag.iFlagID = 42;
	EXPECT_PACKET_EQ(FromHex("95 00 00 00 00", 33), Payload(flag));

	GameMessages::NotifyMissionTask task;
	task.missionID = 1;
	task.taskMask = 4;
	task.updates = { 1.5f, 2.0f };
	EXPECT_PACKET_EQ(FromHex("01 00 00 00 04 00 00 00 02 00 00 c0 3f 00 00 00 40"), Payload(task));
}

// Behaviour fix: a MissionDialogueOK naming a responder that does not exist used to dereference a null player.
TEST_F(MissionMessagesTests, MissionDialogueOKWithUnknownResponderDoesNotCrash) {
	Entity missionGiver(15, info);
	GameMessages::MissionDialogueOK ok;
	ok.bIsComplete = false;
	ok.iMissionState = eMissionState::AVAILABLE;
	ok.missionID = 1727;
	ok.responder = 0x7777; // no such entity
	ok.Handle(missionGiver, UNASSIGNED_SYSTEM_ADDRESS);
	SUCCEED();
}
