#include "SkillMessages.h"
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"
#include "Legacy/SkillMessagesLegacy.h"

#include "BehaviorSlot.h"

#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	const std::vector<std::string> g_Streams = { "", std::string("\x00", 1), std::string("\x01\x02\xff", 3), std::string(1000, '\x5a') };
	const std::vector<LWOOBJID> g_Ids = { LWOOBJID_EMPTY, 0x1000000000000001LL, 0x0102030405060708LL };
	const std::vector<NiPoint3> g_Points = { NiPoint3Constant::ZERO, NiPoint3(1.0f, -2.5f, 3.0f) };
	const std::vector<NiQuaternion> g_Rotations = { QuatUtils::IDENTITY, NiQuaternion(0.5f, 0.5f, -0.5f, 0.5f) };

	PacketBytes Payload(const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		return FromBitStream(bitStream);
	}

	// What the old senders put on the wire: the CLIENT/GAME_MSG header, the target, then the class's Serialize
	// (message ID and payload).
	template<typename Legacy>
	PacketBytes LegacyPacket(LWOOBJID target, Legacy& legacy) {
		RakNet::BitStream bitStream;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::GAME_MSG);
		bitStream.Write(target);
		legacy.Serialize(bitStream);
		return FromBitStream(bitStream);
	}

	// Serializes msg and reads it back with the legacy class's Deserialize; both must consume the same bits.
	template<typename Legacy>
	Legacy ReadWithLegacy(const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream wire;
		msg.Serialize(wire);
		RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
		Legacy legacy;
		EXPECT_TRUE(legacy.Deserialize(legacyStream));
		EXPECT_EQ(legacyStream.GetReadOffset(), wire.GetNumberOfBitsUsed());
		return legacy;
	}
}

class SkillMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(SkillMessagesTests, AddAndRemoveSkillMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const TSkillID skillID : { 0, 1, -1, 1727, std::numeric_limits<int32_t>::max() }) {
			for (const auto slot : { BehaviorSlot::Invalid, BehaviorSlot::Primary, BehaviorSlot::Offhand, BehaviorSlot::Neck, BehaviorSlot::Head, BehaviorSlot::Consumable }) {
				GameMessages::AddSkill msg;
				msg.target = target;
				msg.skillID = skillID;
				msg.slotID = slot;
				const auto legacy = Capture([&] { LegacyGameMessages::SendAddSkill(&entity, skillID, slot); });
				const auto ours = Capture([&] { msg.SendToClient(entity.GetSystemAddress()); });
				ASSERT_EQ(legacy.size(), 1);
				ASSERT_EQ(ours.size(), 1);
				EXPECT_PACKET_EQ(FromCapture(legacy[0]), FromCapture(ours[0]));
				EXPECT_EQ(legacy[0].broadcast, ours[0].broadcast);
				EXPECT_EQ(legacy[0].sysAddr, ours[0].sysAddr);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.skillID, skillID);
				EXPECT_EQ(copy.slotID, slot);
				EXPECT_TRUE(copy.temporary);
			}

			GameMessages::RemoveSkill remove;
			remove.target = target;
			remove.skillID = skillID;
			const auto legacy = Capture([&] { LegacyGameMessages::SendRemoveSkill(&entity, skillID); });
			const auto ours = Capture([&] { remove.SendToClient(entity.GetSystemAddress()); });
			ASSERT_EQ(legacy.size(), 1);
			ASSERT_EQ(ours.size(), 1);
			EXPECT_PACKET_EQ(FromCapture(legacy[0]), FromCapture(ours[0]));
			EXPECT_EQ(legacy[0].broadcast, ours[0].broadcast);
			EXPECT_EQ(RoundTrip(remove).skillID, skillID);
		}
	}

	// Every optional field of AddSkill set, which DLU never sends, still round trips.
	GameMessages::AddSkill full;
	full.AICombatWeight = 3;
	full.bFromSkillSet = true;
	full.castType = 2;
	full.fTimeSecs = 1.5f;
	full.iTimesCanCast = 4;
	full.skillID = 9;
	full.slotID = BehaviorSlot::Head;
	full.temporary = false;
	const auto copy = RoundTrip(full);
	EXPECT_EQ(copy.AICombatWeight, 3);
	EXPECT_EQ(copy.castType, 2);
	EXPECT_EQ(copy.fTimeSecs, 1.5f);
	EXPECT_EQ(copy.iTimesCanCast, 4);
	ExpectTruncatedFails(full);
}

TEST_F(SkillMessagesTests, StartSkillAndEchoMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const auto& sBitStream : g_Streams) {
			for (const auto optionalID : g_Ids) {
				for (const auto& point : g_Points) {
					for (const auto& rotation : g_Rotations) {
						for (const bool flag : { false, true }) {
							const float latency = flag ? 0.25f : 0.0f;
							const int32_t castType = flag ? 3 : 0;
							const uint32_t handle = flag ? 77u : 0u;
							const TSkillID skillID = flag ? 1727 : -1;

							GameMessages::EchoStartSkill echo;
							echo.target = target;
							echo.bUsedMouse = flag;
							echo.fCasterLatency = latency;
							echo.iCastType = castType;
							echo.lastClickedPosit = point;
							echo.optionalOriginatorID = target;
							echo.optionalTargetID = optionalID;
							echo.originatorRot = rotation;
							echo.sBitStream = sBitStream;
							echo.skillID = skillID;
							echo.uiSkillHandle = handle;
							LegacyGameMessages::EchoStartSkill legacyEcho(target, sBitStream, skillID, flag, latency, castType, point, optionalID, rotation, handle);
							EXPECT_PACKET_EQ(LegacyPacket(target, legacyEcho), StructPacket(echo));
							const auto echoRead = ReadWithLegacy<LegacyGameMessages::EchoStartSkill>(echo);
							EXPECT_EQ(echoRead.sBitStream, sBitStream);
							EXPECT_EQ(echoRead.optionalTargetID, optionalID);
							const auto echoCopy = RoundTrip(echo);
							EXPECT_EQ(echoCopy.sBitStream, sBitStream);
							EXPECT_EQ(echoCopy.originatorRot, rotation);
							EXPECT_EQ(echoCopy.lastClickedPosit, point);

							GameMessages::StartSkill start;
							start.bUsedMouse = flag;
							start.consumableItemID = optionalID;
							start.fCasterLatency = latency;
							start.iCastType = castType;
							start.lastClickedPosit = point;
							start.optionalOriginatorID = target;
							start.optionalTargetID = optionalID;
							start.originatorRot = rotation;
							start.sBitStream = sBitStream;
							start.skillID = skillID;
							start.uiSkillHandle = handle;
							LegacyGameMessages::StartSkill legacyStart(target, sBitStream, skillID, flag, optionalID, latency, castType, point, optionalID, rotation, handle);
							EXPECT_PACKET_EQ(LegacyPacket(target, legacyStart), [&] { auto copy = start; copy.target = target; return StructPacket(copy); }());
							const auto legacy = ReadWithLegacy<LegacyGameMessages::StartSkill>(start);
							EXPECT_EQ(legacy.bUsedMouse, start.bUsedMouse);
							EXPECT_EQ(legacy.consumableItemID, start.consumableItemID);
							EXPECT_EQ(legacy.fCasterLatency, start.fCasterLatency);
							EXPECT_EQ(legacy.iCastType, start.iCastType);
							EXPECT_EQ(legacy.lastClickedPosit, start.lastClickedPosit);
							EXPECT_EQ(legacy.optionalOriginatorID, start.optionalOriginatorID);
							EXPECT_EQ(legacy.optionalTargetID, start.optionalTargetID);
							EXPECT_EQ(legacy.originatorRot, start.originatorRot);
							EXPECT_EQ(legacy.sBitStream, start.sBitStream);
							EXPECT_EQ(legacy.skillID, start.skillID);
							EXPECT_EQ(legacy.uiSkillHandle, start.uiSkillHandle);
							const auto startCopy = RoundTrip(start);
							EXPECT_EQ(startCopy.sBitStream, sBitStream);
							EXPECT_EQ(startCopy.consumableItemID, optionalID);
							if (sBitStream.size() < 8) {
								ExpectTruncatedFails(start);
								ExpectTruncatedFails(echo);
							}
						}
					}
				}
			}
		}
	}
}

TEST_F(SkillMessagesTests, SyncSkillAndEchoMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const auto& sBitStream : g_Streams) {
			for (const bool bDone : { false, true }) {
				for (const uint32_t handle : { 0u, 1u, 0xFFFFFFFFu }) {
					GameMessages::EchoSyncSkill echo;
					echo.target = target;
					echo.bDone = bDone;
					echo.sBitStream = sBitStream;
					echo.uiBehaviorHandle = handle;
					echo.uiSkillHandle = handle ^ 0x5u;
					LegacyGameMessages::EchoSyncSkill legacyEcho(sBitStream, handle, handle ^ 0x5u, bDone);
					EXPECT_PACKET_EQ(LegacyPacket(target, legacyEcho), StructPacket(echo));
					EXPECT_EQ(RoundTrip(echo).sBitStream, sBitStream);

					GameMessages::SyncSkill sync;
					sync.bDone = bDone;
					sync.sBitStream = sBitStream;
					sync.uiBehaviorHandle = handle;
					sync.uiSkillHandle = handle ^ 0x5u;
					const auto legacy = ReadWithLegacy<LegacyGameMessages::SyncSkill>(sync);
					EXPECT_EQ(legacy.bDone, bDone);
					EXPECT_EQ(legacy.sBitStream, sBitStream);
					EXPECT_EQ(legacy.uiBehaviorHandle, handle);
					EXPECT_EQ(legacy.uiSkillHandle, handle ^ 0x5u);
					LegacyGameMessages::SyncSkill legacySync(sBitStream, handle, handle ^ 0x5u, bDone);
					EXPECT_PACKET_EQ(LegacyPacket(target, legacySync), [&] { auto copy = sync; copy.target = target; return StructPacket(copy); }());
					RoundTrip(sync);
					if (sBitStream.size() < 8) ExpectTruncatedFails(sync);
				}
			}
		}
	}
}

TEST_F(SkillMessagesTests, ProjectileImpactsMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const auto& sBitStream : g_Streams) {
			for (const auto a : g_Ids) {
				for (const auto b : g_Ids) {
					GameMessages::DoClientProjectileImpact impact;
					impact.target = target;
					impact.i64OrgID = a;
					impact.i64OwnerID = b;
					impact.i64TargetID = target;
					impact.sBitStream = sBitStream;
					LegacyGameMessages::DoClientProjectileImpact legacyImpact(sBitStream, a, b, target);
					EXPECT_PACKET_EQ(LegacyPacket(target, legacyImpact), StructPacket(impact));
					const auto impactCopy = RoundTrip(impact);
					EXPECT_EQ(impactCopy.i64OrgID, a);
					EXPECT_EQ(impactCopy.i64OwnerID, b);

					GameMessages::RequestServerProjectileImpact request;
					request.i64LocalID = a;
					request.i64TargetID = b;
					request.sBitStream = sBitStream;
					const auto legacy = ReadWithLegacy<LegacyGameMessages::RequestServerProjectileImpact>(request);
					EXPECT_EQ(legacy.i64LocalID, a);
					EXPECT_EQ(legacy.i64TargetID, b);
					EXPECT_EQ(legacy.sBitStream, sBitStream);
					LegacyGameMessages::RequestServerProjectileImpact legacyRequest(sBitStream, a, b);
					EXPECT_PACKET_EQ(LegacyPacket(target, legacyRequest), [&] { auto copy = request; copy.target = target; return StructPacket(copy); }());
					RoundTrip(request);
					if (sBitStream.size() < 8) ExpectTruncatedFails(request);
				}
			}
		}
	}
}

// The echoes went to everyone except the caster: a RakNet broadcast with the caster's address excluded.
TEST_F(SkillMessagesTests, EchoesBroadcastExceptTheCaster) {
	GameMessages::EchoSyncSkill echo;
	echo.target = 0x0102030405060708LL;
	echo.bDone = true;
	echo.sBitStream = "ab";
	const auto caster = ClientAddress();
	const auto legacy = Capture([&] {
		RakNet::BitStream bitStreamLocal;
		BitStreamUtils::WriteHeader(bitStreamLocal, ServiceType::CLIENT, MessageType::Client::GAME_MSG);
		bitStreamLocal.Write(echo.target);
		LegacyGameMessages::EchoSyncSkill legacyEcho("ab", 0, 0, true);
		legacyEcho.Serialize(bitStreamLocal);
		Game::server->Send(bitStreamLocal, caster, true);
		});
	const auto ours = Capture([&] { echo.BroadcastExcept(caster); });
	ASSERT_EQ(legacy.size(), 1);
	ASSERT_EQ(ours.size(), 1);
	EXPECT_PACKET_EQ(FromCapture(legacy[0]), FromCapture(ours[0]));
	EXPECT_TRUE(ours[0].broadcast);
	EXPECT_EQ(ours[0].sysAddr, caster);
}

TEST_F(SkillMessagesTests, SelectSkillAcceptsAnyPayload) {
	GameMessages::SelectSkill select;
	select.bFromSkillSet = true;
	select.skillID = 1727;
	const auto copy = RoundTrip(select);
	EXPECT_TRUE(copy.bFromSkillSet);
	EXPECT_EQ(copy.skillID, 1727);
	RakNet::BitStream empty;
	GameMessages::SelectSkill fromEmpty;
	EXPECT_TRUE(fromEmpty.Deserialize(empty));
}

// Independent of the legacy code: hand computed payloads (RakNet writes MSB first).
TEST_F(SkillMessagesTests, GoldenBytes) {
	GameMessages::AddSkill add;
	add.skillID = 1;
	add.slotID = BehaviorSlot::Primary;
	// 5 zero bits (weight, fromSkillSet, castType, time, timesCanCast), skillID 1, bit + slot 0, temporary.
	EXPECT_PACKET_EQ(FromHex("00 08 00 00 04 00 00 00 02", 71), Payload(add));

	GameMessages::RemoveSkill remove;
	remove.skillID = 1727;
	EXPECT_PACKET_EQ(FromHex("5f 83 00 00 00", 33), Payload(remove));

	GameMessages::SyncSkill sync;
	sync.bDone = true;
	sync.sBitStream = std::string("\xab", 1);
	sync.uiBehaviorHandle = 2;
	sync.uiSkillHandle = 3;
	EXPECT_PACKET_EQ(FromHex("80 80 00 00 55 81 00 00 00 01 80 00 00 00", 105), Payload(sync));

	GameMessages::RequestServerProjectileImpact request;
	request.i64TargetID = 0x11;
	EXPECT_PACKET_EQ(FromHex("44 40 00 00 00 00 00 00 00 00 00 00 00", 98), Payload(request));
}
