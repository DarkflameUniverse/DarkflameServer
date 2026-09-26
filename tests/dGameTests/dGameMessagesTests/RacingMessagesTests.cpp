#include "RacingMessages.h"
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"
#include "Legacy/RacingMessagesLegacy.h"

#include <cstdio>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

// eRacingClientNotificationType goes on the wire; pin its values.
static_assert(static_cast<int32_t>(eRacingClientNotificationType::INVALID) == 0);
static_assert(static_cast<int32_t>(eRacingClientNotificationType::ACTIVITY_START) == 1);
static_assert(static_cast<int32_t>(eRacingClientNotificationType::REWARD_PLAYER) == 2);
static_assert(static_cast<int32_t>(eRacingClientNotificationType::EXIT) == 3);
static_assert(static_cast<int32_t>(eRacingClientNotificationType::REPLAY) == 4);
static_assert(static_cast<int32_t>(eRacingClientNotificationType::REMOVE_PLAYER) == 5);
static_assert(static_cast<int32_t>(eRacingClientNotificationType::LEADERBOARD_UPDATED) == 6);

namespace {
	const std::vector<std::u16string> g_Strings = { u"", u"a", u"1:8092;2:4880;3:7503;", std::u16string(300, u'é') };
	const std::vector<NiPoint3> g_Points = { NiPoint3Constant::ZERO, NiPoint3(1.0f, -2.5f, 1e6f), NiPoint3(0.0f, 0.0f, -0.0f) };
}

class RacingMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(RacingMessagesTests, NoPayloadMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		GameMessages::VehicleAddPassiveBoostAction add;
		add.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendVehicleAddPassiveBoostAction(target, a); }, add);

		GameMessages::VehicleRemovePassiveBoostAction remove;
		remove.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendVehicleRemovePassiveBoostAction(target, a); }, remove);

		GameMessages::VehicleNotifyFinishedRace finished;
		finished.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendVehicleNotifyFinishedRace(target, a); }, finished);
	}
}

TEST_F(RacingMessagesTests, ModuleAssemblyDBDataForClientMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto assemblyID : g_Targets) {
			for (const auto& blob : g_Strings) {
				GameMessages::ModuleAssemblyDBDataForClient msg;
				msg.target = target;
				msg.assemblyID = assemblyID;
				msg.blob = blob;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendModuleAssemblyDBDataForClient(target, assemblyID, blob, a); }, msg);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.assemblyID, assemblyID);
				EXPECT_EQ(copy.blob, blob);
			}
		}
	}
}

TEST_F(RacingMessagesTests, NotifyVehicleOfRacingObjectMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto racingObjectID : g_Targets) {
			GameMessages::NotifyVehicleOfRacingObject msg;
			msg.target = target;
			msg.racingObjectID = racingObjectID;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendNotifyVehicleOfRacingObject(target, racingObjectID, a); }, msg);
			EXPECT_EQ(RoundTrip(msg).racingObjectID, racingObjectID);
		}
	}
}

TEST_F(RacingMessagesTests, RacingPlayerLoadedMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto playerID : g_Targets) {
			for (const auto vehicleID : g_Targets) {
				GameMessages::RacingPlayerLoaded msg;
				msg.target = target;
				msg.playerID = playerID;
				msg.vehicleID = vehicleID;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendRacingPlayerLoaded(target, playerID, vehicleID, a); }, msg);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.playerID, playerID);
				EXPECT_EQ(copy.vehicleID, vehicleID);
			}
		}
	}
}

TEST_F(RacingMessagesTests, VehicleUnlockInputMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const bool bLockWheels : { false, true }) {
			GameMessages::VehicleUnlockInput msg;
			msg.target = target;
			msg.bLockWheels = bLockWheels;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendVehicleUnlockInput(target, bLockWheels, a); }, msg);
			EXPECT_EQ(RoundTrip(msg).bLockWheels, bLockWheels);
		}
	}
}

TEST_F(RacingMessagesTests, VehicleSetWheelLockStateMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const bool bExtraFriction : { false, true }) {
			for (const bool bLocked : { false, true }) {
				GameMessages::VehicleSetWheelLockState msg;
				msg.target = target;
				msg.bExtraFriction = bExtraFriction;
				msg.bLocked = bLocked;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendVehicleSetWheelLockState(target, bExtraFriction, bLocked, a); }, msg);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.bExtraFriction, bExtraFriction);
				EXPECT_EQ(copy.bLocked, bLocked);

				// Inbound: same read as the old handler.
				RakNet::BitStream wire;
				msg.Serialize(wire);
				RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
				const auto [legacyFriction, legacyLocked] = LegacyGameMessages::ReadVehicleSetWheelLockState(legacyStream);
				EXPECT_EQ(legacyFriction, bExtraFriction);
				EXPECT_EQ(legacyLocked, bLocked);
			}
		}
	}
}

TEST_F(RacingMessagesTests, RacingSetPlayerResetInfoMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const int32_t currentLap : { 0, 3, -1, std::numeric_limits<int32_t>::max() }) {
			for (const uint32_t furthestResetPlane : { 0u, 7u, std::numeric_limits<uint32_t>::max() }) {
				for (const auto& respawnPos : g_Points) {
					const LWOOBJID playerID = 0x1000000000000001LL;
					const uint32_t upcomingPlane = furthestResetPlane + 1;
					GameMessages::RacingSetPlayerResetInfo msg;
					msg.target = target;
					msg.currentLap = currentLap;
					msg.furthestResetPlane = furthestResetPlane;
					msg.playerID = playerID;
					msg.respawnPos = respawnPos;
					msg.upcomingPlane = upcomingPlane;
					ExpectSameAsLegacy([&](const SystemAddress& a) {
						LegacyGameMessages::SendRacingSetPlayerResetInfo(target, currentLap, furthestResetPlane, playerID, respawnPos, upcomingPlane, a);
						}, msg);
					const auto copy = RoundTrip(msg);
					EXPECT_EQ(copy.currentLap, currentLap);
					EXPECT_EQ(copy.furthestResetPlane, furthestResetPlane);
					EXPECT_EQ(copy.respawnPos, respawnPos);
					EXPECT_EQ(copy.upcomingPlane, upcomingPlane);
				}
			}
		}
	}
}

TEST_F(RacingMessagesTests, RacingResetPlayerToLastResetMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto playerID : g_Targets) {
			GameMessages::RacingResetPlayerToLastReset msg;
			msg.target = target;
			msg.playerID = playerID;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendRacingResetPlayerToLastReset(target, playerID, a); }, msg);
			EXPECT_EQ(RoundTrip(msg).playerID, playerID);
		}
	}
}

TEST_F(RacingMessagesTests, VehicleStopBoostMatchesLegacy) {
	// The legacy function always broadcast, whatever address it was given; callers now pass UNASSIGNED explicitly.
	for (const auto target : g_Targets) {
		for (const bool bAffectPassive : { false, true }) {
			Entity vehicle(target, info);
			GameMessages::VehicleStopBoost msg;
			msg.target = target;
			msg.bAffectPassive = bAffectPassive;

			const auto legacyPackets = Capture([&] { LegacyGameMessages::SendVehicleStopBoost(&vehicle, ClientAddress(), bAffectPassive); });
			const auto newPackets = Capture([&] { msg.Send(UNASSIGNED_SYSTEM_ADDRESS); });
			ASSERT_EQ(legacyPackets.size(), 1);
			ASSERT_EQ(newPackets.size(), 1);
			EXPECT_TRUE(legacyPackets[0].broadcast);
			EXPECT_TRUE(newPackets[0].broadcast);
			EXPECT_PACKET_EQ(FromCapture(legacyPackets[0]), FromCapture(newPackets[0]));
			EXPECT_EQ(RoundTrip(msg).bAffectPassive, bAffectPassive);
		}
	}
}

TEST_F(RacingMessagesTests, NotifyRacingClientMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const int32_t eventType : { 0, 1, 2, 3, 6, -5 }) {
			for (const int32_t param1 : { 0, 1, -1 }) {
				for (const auto paramObj : g_Targets) {
					for (const auto& paramStr : g_Strings) {
						const LWOOBJID singleClient = paramObj == LWOOBJID_EMPTY ? 0x1000000000000001LL : LWOOBJID_EMPTY;
						GameMessages::NotifyRacingClient msg;
						msg.target = target;
						msg.eventType = static_cast<eRacingClientNotificationType>(eventType);
						msg.param1 = param1;
						msg.paramObj = paramObj;
						msg.paramStr = paramStr;
						msg.singleClient = singleClient;
						ExpectSameAsLegacy([&](const SystemAddress& a) {
							LegacyGameMessages::SendNotifyRacingClient(target, eventType, param1, paramObj, paramStr, singleClient, a);
							}, msg);
						const auto copy = RoundTrip(msg);
						EXPECT_EQ(copy.eventType, msg.eventType);
						EXPECT_EQ(copy.param1, param1);
						EXPECT_EQ(copy.paramObj, paramObj);
						EXPECT_EQ(copy.paramStr, paramStr);
						EXPECT_EQ(copy.singleClient, singleClient);
					}
				}
			}
		}
	}
}

// Independent of the legacy code: hand computed payloads (RakNet writes MSB first).
TEST_F(RacingMessagesTests, GoldenBytes) {
	auto payload = [](const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		return FromBitStream(bitStream);
	};

	GameMessages::NotifyVehicleOfRacingObject notifyVehicle;
	notifyVehicle.racingObjectID = 0x0102030405060708LL;
	EXPECT_PACKET_EQ(FromHex("84 03 83 02 82 01 81 00 80", 65), payload(notifyVehicle));
	notifyVehicle.racingObjectID = LWOOBJID_EMPTY;
	EXPECT_PACKET_EQ(FromHex("00", 1), payload(notifyVehicle));

	GameMessages::NotifyRacingClient notify;
	notify.eventType = eRacingClientNotificationType::EXIT;
	notify.param1 = 5;
	notify.paramObj = 0x11;
	notify.paramStr = u"ab";
	notify.singleClient = 0x22;
	EXPECT_PACKET_EQ(FromHex("81 80 00 00 02 80 00 00 08 80 00 00 00 00 00 00 01 00 00 00 30 80 31 00 11 00 00 00 00 00 00 00 00", 257), payload(notify));
	GameMessages::NotifyRacingClient notifyDefault;
	EXPECT_PACKET_EQ(FromHex("00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00", 193), payload(notifyDefault));

	GameMessages::RacingSetPlayerResetInfo reset;
	reset.currentLap = 2;
	reset.furthestResetPlane = 7;
	reset.playerID = 0x0102030405060708LL;
	reset.respawnPos = NiPoint3(1.0f, 2.0f, -3.0f);
	reset.upcomingPlane = 8;
	EXPECT_PACKET_EQ(FromHex("02 00 00 00 07 00 00 00 08 07 06 05 04 03 02 01 00 00 80 3f 00 00 00 40 00 00 40 c0 08 00 00 00"), payload(reset));

	// Full packet: 53 | CLIENT | GAME_MSG | 00 | target | msgId | payload
	GameMessages::RacingPlayerLoaded loaded;
	loaded.target = 0x0102030405060708LL;
	loaded.playerID = 0x11;
	loaded.vehicleID = 0x22;
	const auto msgId = static_cast<uint16_t>(MessageType::Game::RACING_PLAYER_LOADED);
	char idHex[8];
	std::snprintf(idHex, sizeof(idHex), "%02x %02x", msgId & 0xff, msgId >> 8);
	EXPECT_PACKET_EQ(FromHex(std::string("53 05 00 0c 00 00 00 00 08 07 06 05 04 03 02 01 ") + idHex + " 11 00 00 00 00 00 00 00 22 00 00 00 00 00 00 00"), StructPacket(loaded));
}

TEST_F(RacingMessagesTests, InboundReadsLikeLegacy) {
	for (const auto id : g_Targets) {
		{
			GameMessages::ModularAssemblyNIFCompleted msg;
			msg.objectID = id;
			RakNet::BitStream wire;
			msg.Serialize(wire);
			RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
			EXPECT_EQ(LegacyGameMessages::ReadModularAssemblyNIFCompleted(legacyStream), id);
			EXPECT_EQ(RoundTrip(msg).objectID, id);
			ExpectTruncatedFails(msg);
		}
		{
			GameMessages::RacingClientReady ready;
			ready.playerID = id;
			GameMessages::RacingPlayerInfoResetFinished finished;
			finished.playerID = id;
			for (const GameMessages::NetGameMsg* msg : std::initializer_list<const GameMessages::NetGameMsg*>{ &ready, &finished }) {
				RakNet::BitStream wire;
				msg->Serialize(wire);
				RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
				EXPECT_EQ(LegacyGameMessages::ReadRacingPlayerID(legacyStream), id);
			}
			EXPECT_EQ(RoundTrip(ready).playerID, id);
			EXPECT_EQ(RoundTrip(finished).playerID, id);
			ExpectTruncatedFails(ready);
			ExpectTruncatedFails(finished);
		}
	}

	// VehicleNotifyHitImaginationServer: every combination of present/absent optional fields.
	for (int mask = 0; mask < 16; mask++) {
		GameMessages::VehicleNotifyHitImaginationServer msg;
		if (mask & 1) msg.pickupObjID = 0x0102030405060708LL;
		if (mask & 2) msg.pickupSpawnerID = 0x1000000000000001LL;
		if (mask & 4) msg.pickupSpawnerIndex = 12;
		if (mask & 8) msg.vehiclePosition = NiPoint3(1.0f, 2.0f, 3.0f);

		RakNet::BitStream wire;
		msg.Serialize(wire);
		RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
		const auto legacy = LegacyGameMessages::ReadVehicleNotifyHitImaginationServer(legacyStream);
		EXPECT_EQ(legacyStream.GetReadOffset(), wire.GetNumberOfBitsUsed()); // both read exactly the same bits

		const auto copy = RoundTrip(msg);
		EXPECT_EQ(copy.pickupObjID, legacy.pickupObjID);
		EXPECT_EQ(copy.pickupSpawnerID, legacy.pickupSpawnerID);
		EXPECT_EQ(copy.pickupSpawnerIndex, legacy.pickupSpawnerIndex);
		EXPECT_EQ(copy.vehiclePosition, legacy.vehiclePosition);
		ExpectTruncatedFails(msg);
	}
}
