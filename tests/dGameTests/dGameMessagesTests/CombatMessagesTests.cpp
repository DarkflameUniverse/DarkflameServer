#include "CombatMessages.h"
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"
#include "Legacy/CombatMessagesLegacy.h"

#include "eKillType.h"
#include "eStateChangeType.h"

#include <array>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	const std::vector<std::u16string> g_Strings = { u"", u"electro-shock-death", u"é中", std::u16string(300, u'x') };
	const std::vector<LWOOBJID> g_Ids = { LWOOBJID_EMPTY, 0x1000000000000001LL, 0x0102030405060708LL };
	const std::vector<float> g_Floats = { 0.0f, 90.0f, -1.5f };

	PacketBytes Payload(const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		return FromBitStream(bitStream);
	}

	template<typename Result>
	Result ReadWithLegacy(const GameMessages::NetGameMsg& msg, const std::function<Result(RakNet::BitStream&)>& read) {
		RakNet::BitStream wire;
		msg.Serialize(wire);
		RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
		auto result = read(legacyStream);
		EXPECT_EQ(legacyStream.GetReadOffset(), wire.GetNumberOfBitsUsed());
		return result;
	}

	// For the old functions that always broadcast (SEND_PACKET_BROADCAST) whatever address they were given:
	// the struct is sent with Send(UNASSIGNED_SYSTEM_ADDRESS) and must produce the same bytes and destination.
	void ExpectSameAsLegacyBroadcast(const std::function<void(const SystemAddress&)>& legacySend, const GameMessages::NetGameMsg& msg) {
		const auto legacyPackets = Capture([&] { legacySend(UNASSIGNED_SYSTEM_ADDRESS); });
		const auto newPackets = Capture([&] { msg.Send(UNASSIGNED_SYSTEM_ADDRESS); });
		ASSERT_EQ(legacyPackets.size(), 1);
		ASSERT_EQ(newPackets.size(), 1);
		EXPECT_PACKET_EQ(FromCapture(legacyPackets[0]), FromCapture(newPackets[0]));
		EXPECT_TRUE(legacyPackets[0].broadcast);
		EXPECT_TRUE(newPackets[0].broadcast);
		EXPECT_EQ(legacyPackets[0].sysAddr, newPackets[0].sysAddr);
	}

	// Bit i of mask, for walking every combination of a message's bools.
	bool Bit(uint32_t mask, int i) { return (mask >> i) & 1; }
}

class CombatMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(CombatMessagesTests, DieMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto& deathType : g_Strings) {
			for (const auto killType : { eKillType::VIOLENT, eKillType::SILENT }) {
				for (const auto killer : g_Ids) {
					for (const auto lootOwner : g_Ids) {
						for (uint32_t mask = 0; mask < 4; mask++) {
							for (const float value : g_Floats) {
								GameMessages::Die msg;
								msg.target = target;
								msg.bClientDeath = Bit(mask, 0);
								msg.bSpawnLoot = Bit(mask, 1);
								msg.deathType = deathType;
								msg.directionRelative_AngleXZ = value;
								msg.directionRelative_AngleY = value * 2;
								msg.directionRelative_Force = value * 3;
								msg.killType = killType;
								msg.killerID = killer;
								msg.lootOwnerID = lootOwner;
								ExpectSameAsLegacyBroadcast([&](const SystemAddress&) {
									LegacyGameMessages::SendDie(&entity, killer, lootOwner, true, killType, deathType, value * 2, value, value * 3, Bit(mask, 0), Bit(mask, 1), 1.0f);
									}, msg);
								ExpectSameAsLegacyBroadcast([&](const SystemAddress&) {
									LegacyGameMessages::SendDieNoImplCode(&entity, killer, lootOwner, killType, deathType, value * 2, value, value * 3, Bit(mask, 0), Bit(mask, 1));
									}, msg);
								const auto copy = RoundTrip(msg);
								EXPECT_EQ(copy.deathType, deathType);
								EXPECT_EQ(copy.killType, killType);
								EXPECT_EQ(copy.lootOwnerID, lootOwner);

								// RequestDie is read with the same layout.
								GameMessages::RequestDie request;
								request.bClientDeath = msg.bClientDeath;
								request.bSpawnLoot = msg.bSpawnLoot;
								request.deathType = deathType;
								request.directionRelative_AngleXZ = value;
								request.directionRelative_AngleY = value * 2;
								request.directionRelative_Force = value * 3;
								request.killType = killType;
								request.killerID = killer;
								request.lootOwnerID = lootOwner;
								const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyRequestDie>(request, LegacyGameMessages::ReadRequestDie);
								const auto requestCopy = RoundTrip(request);
								EXPECT_EQ(requestCopy.bClientDeath, legacy.bClientDeath);
								EXPECT_EQ(requestCopy.bSpawnLoot, legacy.bSpawnLoot);
								EXPECT_EQ(requestCopy.deathType, legacy.deathType);
								EXPECT_EQ(requestCopy.directionRelative_AngleXZ, legacy.directionRelativeAngleXZ);
								EXPECT_EQ(requestCopy.directionRelative_AngleY, legacy.directionRelativeAngleY);
								EXPECT_EQ(requestCopy.directionRelative_Force, legacy.directionRelativeForce);
								EXPECT_EQ(requestCopy.killType, legacy.killType);
								EXPECT_EQ(requestCopy.killerID, legacy.killerID);
								EXPECT_EQ(requestCopy.lootOwnerID, legacy.lootOwnerID);
								if (deathType.size() < 30 && value == 0.0f) ExpectTruncatedFails(request);
							}
						}
					}
				}
			}
		}
	}
}

TEST_F(CombatMessagesTests, ResurrectAndRespawnMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		GameMessages::Resurrect msg;
		msg.target = target;
		ExpectSameAsLegacyBroadcast([&](const SystemAddress&) { LegacyGameMessages::SendResurrect(&entity); }, msg);
		for (const bool immediately : { false, true }) {
			msg.bRezImmediately = immediately;
			EXPECT_EQ(ReadWithLegacy<bool>(msg, LegacyGameMessages::ReadResurrect), immediately);
			EXPECT_EQ(RoundTrip(msg).bRezImmediately, immediately);
			ExpectTruncatedFails(msg);

			GameMessages::SetPlayerAllowedRespawn respawn;
			respawn.target = target;
			respawn.dontPromptForRespawn = immediately;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendPlayerAllowedRespawn(target, immediately, a); }, respawn, SendMode::SendToClient);
			EXPECT_EQ(RoundTrip(respawn).dontPromptForRespawn, immediately);
		}

		for (const int32_t armor : { -1, 0, 5 }) {
			for (const int32_t health : { -1, 0, 7 }) {
				for (const int32_t imagination : { -1, 0, std::numeric_limits<int32_t>::max() }) {
					GameMessages::SetResurrectRestoreValues restore;
					restore.target = target;
					restore.iArmorRestore = armor;
					restore.iHealthRestore = health;
					restore.iImaginationRestore = imagination;
					ExpectSameAsLegacyBroadcast([&](const SystemAddress&) { LegacyGameMessages::SendSetResurrectRestoreValues(&entity, armor, health, imagination); }, restore);
					const auto copy = RoundTrip(restore);
					EXPECT_EQ(copy.iArmorRestore, armor);
					EXPECT_EQ(copy.iImaginationRestore, imagination);
				}
			}
		}
	}
}

TEST_F(CombatMessagesTests, KnockbackAndSmashMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto caster : g_Ids) {
			for (const auto originator : g_Ids) {
				for (const int32_t time : { 0, 1000, -1 }) {
					GameMessages::Knockback msg;
					msg.target = target;
					msg.Caster = caster;
					msg.Originator = originator;
					msg.iKnockBackTimeMS = time;
					msg.vector = NiPoint3(-20.0f, 10.0f, 0.5f);
					ExpectSameAsLegacyBroadcast([&](const SystemAddress&) { LegacyGameMessages::SendKnockback(target, caster, originator, time, msg.vector); }, msg);
					const auto copy = RoundTrip(msg);
					EXPECT_EQ(copy.Caster, caster);
					EXPECT_EQ(copy.iKnockBackTimeMS, time);
					EXPECT_EQ(copy.vector, msg.vector);
				}
			}
			for (const float force : g_Floats) {
				for (const bool ignore : { false, true }) {
					GameMessages::Smash smash;
					smash.target = target;
					smash.bIgnoreObjectVisibility = ignore;
					smash.force = force;
					smash.ghostOpacity = force / 2;
					smash.killerID = caster;
					ExpectSameAsLegacyBroadcast([&](const SystemAddress&) { LegacyGameMessages::SendSmash(&entity, force, force / 2, caster, ignore); }, smash);
					EXPECT_EQ(RoundTrip(smash).killerID, caster);
				}
				GameMessages::UnSmash unsmash;
				unsmash.target = target;
				unsmash.builderID = caster;
				unsmash.duration = force == 0.0f ? 3.0f : force;
				ExpectSameAsLegacyBroadcast([&](const SystemAddress&) { LegacyGameMessages::SendUnSmash(&entity, caster, unsmash.duration); }, unsmash);
			}
		}
	}
}

TEST_F(CombatMessagesTests, StunsAndImmunitiesMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const auto originator : g_Ids) {
			for (const auto state : { eStateChangeType::PUSH, eStateChangeType::POP }) {
				// 17 bools: walk each one alone, all of them, none, and alternating patterns.
				std::vector<uint32_t> masks = { 0, 0x1FFFF, 0x0AAAA, 0x15555 };
				for (int i = 0; i < 17; i++) masks.push_back(1u << i);
				for (const auto mask : masks) {
					GameMessages::SetStunned msg;
					msg.target = target;
					msg.Originator = originator;
					msg.StateChangeType = state;
					msg.bCantAttack = Bit(mask, 0);
					msg.bCantEquip = Bit(mask, 1);
					msg.bCantInteract = Bit(mask, 2);
					msg.bCantJump = Bit(mask, 3);
					msg.bCantMove = Bit(mask, 4);
					msg.bCantTurn = Bit(mask, 5);
					msg.bCantUseItem = Bit(mask, 6);
					msg.bDontTerminateInteract = Bit(mask, 7);
					msg.bIgnoreImmunity = Bit(mask, 8);
					msg.bCantAttackOutChangeWasApplied = Bit(mask, 9);
					msg.bCantEquipOutChangeWasApplied = Bit(mask, 10);
					msg.bCantInteractOutChangeWasApplied = Bit(mask, 11);
					msg.bCantJumpOutChangeWasApplied = Bit(mask, 12);
					msg.bCantMoveOutChangeWasApplied = Bit(mask, 13);
					msg.bCantTurnOutChangeWasApplied = Bit(mask, 14);
					msg.bCantUseItemOutChangeWasApplied = Bit(mask, 15);
					ExpectSameAsLegacy([&](const SystemAddress& a) {
						LegacyGameMessages::SendSetStunned(target, state, a, originator, Bit(mask, 0), Bit(mask, 1), Bit(mask, 2), Bit(mask, 3), Bit(mask, 4), Bit(mask, 5), Bit(mask, 6), Bit(mask, 7), Bit(mask, 8),
							Bit(mask, 9), Bit(mask, 10), Bit(mask, 11), Bit(mask, 12), Bit(mask, 13), Bit(mask, 14), Bit(mask, 15));
						}, msg);
					const auto copy = RoundTrip(msg);
					EXPECT_EQ(copy.bCantUseItemOutChangeWasApplied, msg.bCantUseItemOutChangeWasApplied);
					EXPECT_EQ(copy.Originator, originator);

					GameMessages::SetStunImmunity immunity;
					immunity.target = target;
					immunity.Caster = originator;
					immunity.StateChangeType = state;
					immunity.bImmuneToStunAttack = Bit(mask, 0);
					immunity.bImmuneToStunEquip = Bit(mask, 1);
					immunity.bImmuneToStunInteract = Bit(mask, 2);
					immunity.bImmuneToStunJump = Bit(mask, 3);
					immunity.bImmuneToStunMove = Bit(mask, 4);
					immunity.bImmuneToStunTurn = Bit(mask, 5);
					immunity.bImmuneToStunUseItem = Bit(mask, 6);
					ExpectSameAsLegacy([&](const SystemAddress& a) {
						LegacyGameMessages::SendSetStunImmunity(target, state, a, originator, Bit(mask, 0), Bit(mask, 1), Bit(mask, 2), Bit(mask, 3), Bit(mask, 4), Bit(mask, 5), Bit(mask, 6));
						}, immunity);
					EXPECT_EQ(RoundTrip(immunity).bImmuneToStunUseItem, Bit(mask, 6));

					GameMessages::SetStatusImmunity status;
					status.target = target;
					status.StateChangeType = state;
					status.bImmuneToBasicAttack = Bit(mask, 0);
					status.bImmuneToDOT = Bit(mask, 1);
					status.bImmuneToKnockback = Bit(mask, 2);
					status.bImmuneToInterrupt = Bit(mask, 3);
					status.bImmuneToSpeed = Bit(mask, 4);
					status.bImmuneToImaginationGain = Bit(mask, 5);
					status.bImmuneToImaginationLoss = Bit(mask, 6);
					status.bImmuneToQuickbuildInterrupt = Bit(mask, 7);
					status.bImmuneToPullToPoint = Bit(mask, 8);
					// WIRE FIX: the legacy bytes use DLU's flag order; see SetStatusImmunityUsesClientOrder.
					EXPECT_EQ(RoundTrip(status).bImmuneToPullToPoint, Bit(mask, 8));
				}
			}
		}
	}
}

TEST_F(CombatMessagesTests, BuffsMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto caster : g_Ids) {
			for (const uint32_t buffID : { 0u, 42u, 0xFFFFFFFFu }) {
				for (const uint32_t duration : { 0u, 5000u }) {
					for (uint32_t mask = 0; mask < (1u << 11); mask += 37) {
						GameMessages::AddBuff msg;
						msg.target = target;
						msg.bAddedByTeammate = Bit(mask, 0);
						msg.bApplyOnTeammates = Bit(mask, 1);
						msg.bCancelOnDamaged = Bit(mask, 2);
						msg.bCancelOnDeath = Bit(mask, 3);
						msg.bCancelOnLogOut = Bit(mask, 4);
						msg.bCancelOnRemoveBuff = Bit(mask, 5);
						msg.bCancelOnUI = Bit(mask, 6);
						msg.bCancelOnUnEquip = Bit(mask, 7);
						msg.bCancelOnZone = Bit(mask, 8);
						msg.bIsImmunity = Bit(mask, 9);
						msg.i64AddedBy = caster;
						msg.uiBuffID = buffID;
						msg.uiDurationMS = duration;
						auto objectID = target;
						ExpectSameAsLegacy([&](const SystemAddress& a) {
							LegacyGameMessages::SendAddBuff(objectID, caster, buffID, duration, Bit(mask, 9), Bit(mask, 2), Bit(mask, 3), Bit(mask, 4), Bit(mask, 5), Bit(mask, 6), Bit(mask, 7), Bit(mask, 8), Bit(mask, 0), Bit(mask, 1), a);
							}, msg);
						const auto copy = RoundTrip(msg);
						EXPECT_EQ(copy.uiDurationMS, duration);
						EXPECT_EQ(copy.i64AddedBy, caster);
					}
				}
				for (uint32_t mask = 0; mask < 4; mask++) {
					GameMessages::RemoveBuff remove;
					remove.target = target;
					remove.bFromUnEquip = Bit(mask, 0);
					remove.bRemoveImmunity = Bit(mask, 1);
					remove.uiBuffID = buffID;
					ExpectSameAsLegacyBroadcast([&](const SystemAddress&) { LegacyGameMessages::SendRemoveBuff(&entity, Bit(mask, 0), Bit(mask, 1), buffID); }, remove);
					EXPECT_EQ(RoundTrip(remove).uiBuffID, buffID);
				}
			}
			for (const uint32_t modifier : { 500u, 0u, 750u }) {
				GameMessages::AddRunSpeedModifier add;
				add.target = target;
				add.i64Caster = caster;
				add.uiModifier = modifier;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendAddRunSpeedModifier(target, caster, modifier, a); }, add);
				EXPECT_EQ(RoundTrip(add).uiModifier, modifier);

				GameMessages::RemoveRunSpeedModifier remove;
				remove.target = target;
				remove.uiModifier = modifier;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendRemoveRunSpeedModifier(target, modifier, a); }, remove);
			}
		}

		GameMessages::DeactivateBubbleBuffFromServer deactivate;
		deactivate.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendDeactivateBubbleBuffFromServer(target, a); }, deactivate);
	}

	for (const auto& type : { std::u16string(u""), std::u16string(u"skunk"), std::u16string(u"energy") }) {
		for (const bool special : { false, true }) {
			GameMessages::ActivateBubbleBuff activate;
			activate.bSpecialAnims = special;
			activate.wszType = type;
			const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyActivateBubbleBuff>(activate, LegacyGameMessages::ReadActivateBubbleBuff);
			EXPECT_TRUE(legacy.read);
			EXPECT_EQ(legacy.specialAnimations, special);
			EXPECT_EQ(legacy.type, type);
			EXPECT_EQ(RoundTrip(activate).wszType, type);
			ExpectTruncatedFails(activate);
		}
	}

	RakNet::BitStream empty;
	EXPECT_TRUE(GameMessages::DeactivateBubbleBuff().Deserialize(empty));
	EXPECT_TRUE(GameMessages::RequestSmashPlayer().Deserialize(empty));
	EXPECT_TRUE(GameMessages::RequestResurrect().Deserialize(empty));
}

// Independent of the legacy code: hand computed payloads (RakNet writes MSB first).
TEST_F(CombatMessagesTests, GoldenBytes) {
	GameMessages::Die die;
	die.killerID = 0x11;
	die.deathType = u"A";
	// bClientDeath 0, bSpawnLoot 1, u32 1, 'A' u16, three zero floats, killType flag 0, killer, loot owner flag 0.
	EXPECT_PACKET_EQ(FromHex("40 40 00 00 10 40 00 00 00 00 00 00 00 00 00 00 00 00 02 20 00 00 00 00 00 00 00", 212), Payload(die));

	GameMessages::Knockback knockback;
	knockback.iKnockBackTimeMS = 1000;
	knockback.vector = NiPoint3(1.0f, 0.0f, 0.0f);
	EXPECT_PACKET_EQ(FromHex("3d 00 60 00 00 00 10 07 e0 00 00 00 00 00 00 00 00", 131), Payload(knockback));

	GameMessages::SetStunned stun;
	stun.StateChangeType = eStateChangeType::POP;
	stun.bCantMove = true;
	// originator flag 0, u32 1, 14 flags (bCantMove is the 9th), bDontTerminateInteract 0, bIgnoreImmunity 1.
	EXPECT_PACKET_EQ(FromHex("00 80 00 00 00 40 80", 49), Payload(stun));

	GameMessages::RemoveBuff remove;
	remove.bRemoveImmunity = true;
	remove.uiBuffID = 3;
	EXPECT_PACKET_EQ(FromHex("20 60 00 00 00", 35), Payload(remove));
}

// A packet from a 2011/2012 live capture: RemoveBuffsAppliedByObject (1726) sent to a player when another player
// left the world. The client reads a flag and an optional object ID (0x00d808e0 in 1.10.64).
TEST_F(CombatMessagesTests, RemoveBuffsAppliedByObjectMatchesLiveCapture) {
	const auto msg = FromLiveCapture<GameMessages::RemoveBuffsAppliedByObject>("5305000c00000000b592d85a01000010be06db7bbdb58080000800");
	EXPECT_EQ(msg.target, 0x100000015ad892b5LL);
	EXPECT_EQ(msg.objectID, 0x100000016b7bf7b6LL);

	GameMessages::RemoveBuffsAppliedByObject empty;
	EXPECT_PACKET_EQ(FromHex("00", 1), Payload(empty));
	EXPECT_EQ(RoundTrip(msg).objectID, msg.objectID);
	ExpectTruncatedFails(msg);
}

// WIRE FIX: the client writes and reads the immunity flags in alphabetical order after the u32 state
// (GameMessage::SetStatusImmunity::Serialize @ 0x00d8f140). Setting one flag at a time must set exactly that bit.
TEST_F(CombatMessagesTests, SetStatusImmunityUsesClientOrder) {
	using Flag = bool GameMessages::SetStatusImmunity::*;
	const std::array<Flag, 9> clientOrder = {
		&GameMessages::SetStatusImmunity::bImmuneToBasicAttack,
		&GameMessages::SetStatusImmunity::bImmuneToDOT,
		&GameMessages::SetStatusImmunity::bImmuneToImaginationGain,
		&GameMessages::SetStatusImmunity::bImmuneToImaginationLoss,
		&GameMessages::SetStatusImmunity::bImmuneToInterrupt,
		&GameMessages::SetStatusImmunity::bImmuneToKnockback,
		&GameMessages::SetStatusImmunity::bImmuneToPullToPoint,
		&GameMessages::SetStatusImmunity::bImmuneToQuickbuildInterrupt,
		&GameMessages::SetStatusImmunity::bImmuneToSpeed,
	};
	for (size_t i = 0; i < clientOrder.size(); i++) {
		GameMessages::SetStatusImmunity msg;
		msg.StateChangeType = eStateChangeType::POP;
		msg.*clientOrder[i] = true;
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), 32 + 9);
		uint32_t state{};
		ASSERT_TRUE(bitStream.Read(state));
		EXPECT_EQ(state, 1u);
		for (size_t bit = 0; bit < clientOrder.size(); bit++) {
			bool value{};
			ASSERT_TRUE(bitStream.Read(value));
			EXPECT_EQ(value, bit == i) << "flag " << i << ", bit " << bit;
		}
	}
}
