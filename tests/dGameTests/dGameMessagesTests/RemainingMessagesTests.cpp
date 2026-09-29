#include "ActivityMessages.h"
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"
#include "Legacy/RemainingMessagesLegacy.h"
#include "MovementMessages.h"
#include "ObjectMessages.h"
#include "PlayerMessages.h"
#include "QuickBuildMessages.h"
#include "InventoryMessages.h"
#include "SkillMessages.h"
#include "StatisticID.h"
#include "ZoneMessages.h"

#include <array>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

// Byte equality of the movement, zone, player, object, quickbuild and remaining activity game messages with the
// frozen hand written functions they replaced (Legacy/RemainingMessagesLegacy.h).
using namespace GameMessageTestUtils;

namespace {
	const std::vector<std::u16string> g_WStrings = { u"", u"ZonePlayer", u"é中", std::u16string(300, u'x') };
	const std::vector<std::string> g_Strings = { "", "base", "\xff\x80z", std::string(300, 'y') };
	const std::vector<LWOOBJID> g_Ids = { LWOOBJID_EMPTY, 0x1000000000000001LL, 0x0102030405060708LL };
	const std::vector<int32_t> g_Ints = { 0, -1, 1, 10, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };
	const std::vector<float> g_Floats = { 0.0f, 1.0f, 10.0f, 15.0f, -2.5f };
	const std::vector<NiPoint3> g_Points = { NiPoint3Constant::ZERO, NiPoint3(1.0f, 0.0f, -3.5f), NiPoint3(-100.0f, 25.0f, 1e6f) };
	const std::vector<NiQuaternion> g_Rotations = { QuatUtils::IDENTITY, NiQuaternion(0.5f, 0.5f, -0.5f, 0.5f), NiQuaternion(1.0f, 0.0f, 1.0f, 0.0f) };

	bool Bit(uint32_t mask, int i) { return (mask >> i) & 1; }

	// Compares every packet (bytes, destination, broadcast flag) the legacy call and the struct send.
	void ExpectSameSends(const std::function<void()>& legacySend, const std::function<void()>& structSend) {
		const auto legacyPackets = Capture(legacySend);
		const auto newPackets = Capture(structSend);
		ASSERT_FALSE(legacyPackets.empty());
		ASSERT_EQ(newPackets.size(), 1);
		for (const auto& legacyPacket : legacyPackets) EXPECT_PACKET_EQ(FromCapture(legacyPacket), FromCapture(newPackets[0]));
		EXPECT_EQ(legacyPackets[0].broadcast, newPackets[0].broadcast);
		EXPECT_EQ(legacyPackets[0].sysAddr, newPackets[0].sysAddr);
	}

	// The broadcast-only legacy functions compared with Send(UNASSIGNED).
	void ExpectSameAsLegacyBroadcast(const std::function<void()>& legacySend, const GameMessages::NetGameMsg& msg) {
		ExpectSameSends(legacySend, [&] { msg.Send(UNASSIGNED_SYSTEM_ADDRESS); });
	}

	// Reads msg's payload with a legacy read sequence and checks it consumed every bit.
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

class RemainingMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

// ---------------------------------------------------------------- Movement

TEST_F(RemainingMessagesTests, TeleportMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto& pos : g_Points) {
			for (const auto& rot : g_Rotations) {
				for (const bool setRotation : { false, true }) {
					GameMessages::Teleport msg(target, pos, rot, setRotation);
					for (const auto& address : g_Addresses) {
						ExpectSameSends([&] { LegacyGameMessages::SendTeleport(target, pos, rot, address, setRotation); }, [&] { msg.SendToClient(address); });
					}
					const auto copy = RoundTrip(msg);
					EXPECT_EQ(copy.pos, pos);
					EXPECT_EQ(copy.bIgnoreY, pos.y == 0.0f);
				}
			}
		}
	}
}

TEST_F(RemainingMessagesTests, StartPathingAndPlatformResyncMatchLegacy) {
	for (const LOT lot : { 999, 12341, 9483, 11306, 6267 }) {
		info.lot = lot;
		Entity entity(0x1000000000000001LL, info);
		ExpectSameAsLegacyBroadcast([&] { LegacyGameMessages::SendStartPathing(&entity); }, [&] { GameMessages::StartPathing msg; msg.target = entity.GetObjectID(); return msg; }());
		for (uint32_t mask = 0; mask < 4; mask++) {
			for (const auto state : { eMovementPlatformState::Moving, eMovementPlatformState::Stationary, eMovementPlatformState::Stopped }) {
				for (const int32_t index : { 0, 1, 5 }) {
					GameMessages::PlatformResync msg(entity, Bit(mask, 0), index, index + 1, index + 2, state, Bit(mask, 1));
					ExpectSameAsLegacy([&](const SystemAddress& address) {
						LegacyGameMessages::SendPlatformResync(&entity, address, Bit(mask, 0), index, index + 1, index + 2, state, Bit(mask, 1));
						}, msg);
					RoundTrip(msg);
				}
			}
		}
	}
	GameMessages::PlatformResync rotated;
	rotated.qUnexpectedRotation = NiQuaternion(0.5f, 0.5f, -0.5f, 0.5f);
	const auto copy = RoundTrip(rotated);
	EXPECT_EQ(copy.qUnexpectedRotation, rotated.qUnexpectedRotation);
}

TEST_F(RemainingMessagesTests, SmallMovementMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const float value : g_Floats) {
			for (const bool relative : { false, true }) {
				GameMessages::OrientToAngle orient;
				orient.target = target;
				orient.bRelativeToCurrent = relative;
				orient.fAngle = value;
				ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendOrientToAngle(target, relative, value, address); }, orient);
				RoundTrip(orient);
			}
			GameMessages::SetGravityScale gravity;
			gravity.target = target;
			gravity.scale = value;
			ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendSetGravityScale(target, value, address); }, gravity, SendMode::SendToClient);
			RoundTrip(gravity);
		}
		for (const auto& name : g_Strings) {
			GameMessages::LockNodeRotation lock;
			lock.target = target;
			lock.nodeName = name;
			ExpectSameAsLegacyBroadcast([&] { LegacyGameMessages::SendLockNodeRotation(&entity, name); }, lock);
			EXPECT_EQ(RoundTrip(lock).nodeName, name);
		}
		for (const auto id : g_Ids) {
			GameMessages::SetMountInventoryID mount;
			mount.target = target;
			mount.inventoryMountID = id;
			ExpectSameAsLegacyBroadcast([&] { LegacyGameMessages::SendSetMountInventoryID(&entity, id, UNASSIGNED_SYSTEM_ADDRESS); }, mount);
			RoundTrip(mount);
		}
		for (const auto scheme : { eControlScheme::SCHEME_A, eControlScheme::SCHEME_D, static_cast<eControlScheme>(9) }) {
			GameMessages::SetPlayerControlScheme msg;
			msg.target = target;
			msg.iScheme = scheme;
			ExpectSameSends([&] { LegacyGameMessages::SendSetPlayerControlScheme(&entity, scheme); }, [&] { msg.SendToClient(entity.GetSystemAddress()); });
			RoundTrip(msg);
		}
		for (const auto& pos : g_Points) {
			for (const auto& rot : g_Rotations) {
				GameMessages::PlayerReachedRespawnCheckpoint msg;
				msg.target = target;
				msg.pos = pos;
				msg.rot = rot;
				ExpectSameSends([&] { LegacyGameMessages::SendPlayerReachedRespawnCheckpoint(&entity, pos, rot); }, [&] { msg.SendToClient(entity.GetSystemAddress()); });
				EXPECT_EQ(RoundTrip(msg).rot, rot);
			}
		}
		for (const auto mode : { eCameraTargetCyclingMode::ALLOW_CYCLE_TEAMMATES, eCameraTargetCyclingMode::DISALLOW_CYCLING }) {
			for (const auto id : g_Ids) {
				for (const bool force : { false, true }) {
					GameMessages::ForceCameraTargetCycle msg;
					msg.target = target;
					msg.bForceCycling = force;
					msg.cyclingMode = mode;
					msg.optionalTargetID = id;
					ExpectSameSends([&] { LegacyGameMessages::SendForceCameraTargetCycle(&entity, force, mode, id); }, [&] { msg.SendToClient(entity.GetSystemAddress()); });
					RoundTrip(msg);
				}
			}
		}
		for (const auto mode : { eCyclingMode::ALLOW_CYCLE_TEAMMATES, eCyclingMode::DISALLOW_CYCLING }) {
			for (const bool deadOnly : { false, true }) {
				GameMessages::PlayerSetCameraCyclingMode msg;
				msg.target = target;
				msg.bAllowCyclingWhileDeadOnly = deadOnly;
				msg.cyclingMode = mode;
				ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendPlayerSetCameraCyclingMode(target, address, deadOnly, mode); }, msg, SendMode::SendToClient);
				RoundTrip(msg);
			}
		}
	}
}

TEST_F(RemainingMessagesTests, SetJetPackModeMatchesLegacy) {
	Entity entity(0x0102030405060708LL, info);
	for (uint32_t mask = 0; mask < 8; mask++) {
		for (const int32_t effect : { -1, 167 }) {
			for (const float speed : g_Floats) {
				for (const int32_t warning : { -1, 5 }) {
					GameMessages::SetJetPackMode msg;
					msg.target = entity.GetObjectID();
					msg.bUse = Bit(mask, 0);
					msg.bBypassChecks = Bit(mask, 1);
					msg.bDoHover = Bit(mask, 2);
					msg.effectID = effect;
					msg.fAirspeed = speed;
					msg.fMaxAirspeed = speed;
					msg.fVertVel = speed;
					msg.iWarningEffectID = warning;
					ExpectSameAsLegacyBroadcast([&] { LegacyGameMessages::SendSetJetPackMode(&entity, Bit(mask, 0), Bit(mask, 1), Bit(mask, 2), effect, speed, speed, speed, warning); }, msg);
					RoundTrip(msg);
				}
			}
		}
	}
}

TEST_F(RemainingMessagesTests, RailMovementMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto& path : g_WStrings) {
			for (const uint32_t start : { 0u, 3u }) {
				for (const int32_t component : { -1, 0, 42 }) {
					for (const auto activator : g_Ids) {
						for (uint32_t mask = 0; mask < 2; mask++) {
							GameMessages::SetRailMovement set;
							set.target = target;
							set.pathGoForward = Bit(mask, 0);
							set.pathName = path;
							set.pathStart = start;
							set.railActivatorComponentID = component;
							set.railActivatorObjectID = activator;
							ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendSetRailMovement(target, Bit(mask, 0), path, start, address, component, activator); }, set);
							EXPECT_EQ(RoundTrip(set).pathName, path);
						}
						for (uint32_t mask = 0; mask < 256; mask += 37) {
							GameMessages::StartRailMovement msg;
							msg.target = target;
							msg.pathName = path;
							msg.startSound = u"start";
							msg.loopSound = path;
							msg.stopSound = u"";
							msg.pathStart = start;
							msg.goForward = Bit(mask, 0);
							msg.bDamageImmune = Bit(mask, 1);
							msg.bNoAggro = Bit(mask, 2);
							msg.bNotifyActor = Bit(mask, 3);
							msg.bShowNameBillboard = Bit(mask, 4);
							msg.bCameraLocked = Bit(mask, 5);
							msg.bCollisionEnabled = Bit(mask, 6);
							msg.bUseDB = Bit(mask, 7);
							msg.railActivatorComponentID = component;
							msg.railActivatorObjectID = activator;
							ExpectSameAsLegacy([&](const SystemAddress& address) {
								LegacyGameMessages::SendStartRailMovement(target, path, u"start", path, u"", address, start, Bit(mask, 0), Bit(mask, 1), Bit(mask, 2),
									Bit(mask, 3), Bit(mask, 4), Bit(mask, 5), Bit(mask, 6), Bit(mask, 7), component, activator);
								}, msg);
							RoundTrip(msg);
						}
					}
				}
			}
		}
	}
}

TEST_F(RemainingMessagesTests, InboundMovementMatchesLegacy) {
	for (const auto id : g_Ids) {
		GameMessages::DismountComplete dismount;
		dismount.mountID = id;
		EXPECT_EQ(ReadWithLegacy<LWOOBJID>(dismount, LegacyGameMessages::ReadDismountComplete), id);
		EXPECT_EQ(RoundTrip(dismount).mountID, id);
		ExpectTruncatedFails(dismount);

		GameMessages::AcknowledgePossession ack;
		ack.possessedObjID = id;
		EXPECT_EQ(ReadWithLegacy<LWOOBJID>(ack, LegacyGameMessages::ReadAcknowledgePossession), id);
		EXPECT_EQ(RoundTrip(ack).possessedObjID, id);
		ExpectTruncatedFails(ack);
	}
	for (const bool value : { false, true }) {
		GameMessages::ToggleGhostReferenceOverride toggle;
		toggle.bOverride = value;
		EXPECT_EQ(ReadWithLegacy<bool>(toggle, LegacyGameMessages::ReadToggleGhostReferenceOverride), value);
		ExpectTruncatedFails(toggle);

		GameMessages::CancelRailMovement cancel;
		cancel.bImmediate = value;
		EXPECT_EQ(ReadWithLegacy<bool>(cancel, LegacyGameMessages::ReadCancelRailMovement), value);
		ExpectTruncatedFails(cancel);
	}
	for (const auto& pos : g_Points) {
		GameMessages::SetGhostReferencePosition ghost;
		ghost.pos = pos;
		EXPECT_EQ(ReadWithLegacy<NiPoint3>(ghost, LegacyGameMessages::ReadSetGhostReferencePosition), pos);
		ExpectTruncatedFails(ghost);
	}
	for (const auto& path : g_WStrings) {
		for (const int32_t waypoint : g_Ints) {
			GameMessages::PlayerRailArrivedNotification arrived;
			arrived.pathName = path;
			arrived.waypointNumber = waypoint;
			const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyRailArrived>(arrived, LegacyGameMessages::ReadPlayerRailArrivedNotification);
			EXPECT_EQ(legacy.pathName, path);
			EXPECT_EQ(legacy.waypointNumber, waypoint);
			RoundTrip(arrived);
		}
	}
}

// ---------------------------------------------------------------- Zone

TEST_F(RemainingMessagesTests, ZoneMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		GameMessages::PlayerReady ready;
		ready.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendPlayerReady(&entity, address); }, ready, SendMode::SendToClient);
		GameMessages::RestoreToPostLoadStats restore;
		restore.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendRestoreToPostLoadStats(&entity, address); }, restore, SendMode::SendToClient);
		GameMessages::ServerDoneLoadingAllObjects done;
		done.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendServerDoneLoadingAllObjects(&entity, address); }, done, SendMode::SendToClient);

		for (const auto& url : g_WStrings) {
			for (uint32_t mask = 0; mask < 4; mask++) {
				GameMessages::InvalidZoneTransferList list;
				list.target = target;
				list.customerFeedbackURL = url;
				list.invalidMapTransferList = u"1000,1001";
				list.bCustomerFeedbackOnExit = Bit(mask, 0);
				list.bCustomerFeedbackOnInvalidMapTransfer = Bit(mask, 1);
				ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendInvalidZoneTransferList(&entity, address, url, u"1000,1001", Bit(mask, 0), Bit(mask, 1)); }, list, SendMode::SendToClient);
				RoundTrip(list);
			}
			GameMessages::LocalizedAnnouncementServerToSingleClient announcement;
			announcement.target = target;
			announcement.body = url;
			announcement.title = u"UI_TITLE";
			ExpectSameSends([&] { LegacyGameMessages::SendLocalizedAnnouncement(&entity, url, u"UI_TITLE"); }, [&] { announcement.SendToClient(entity.GetSystemAddress()); });
			EXPECT_EQ(RoundTrip(announcement).body, url);
		}
		for (const auto sender : g_Ids) {
			for (uint32_t mask = 0; mask < 4; mask++) {
				GameMessages::DisplayZoneSummary summary;
				summary.target = target;
				summary.isPropertyMap = Bit(mask, 0);
				summary.isZoneStart = Bit(mask, 1);
				summary.sender = sender;
				ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendDisplayZoneSummary(target, address, Bit(mask, 0), Bit(mask, 1), sender); }, summary);
				RoundTrip(summary);
			}
			GameMessages::ZoneSummaryDismissed dismissed;
			dismissed.playerID = sender;
			EXPECT_EQ(ReadWithLegacy<LWOOBJID>(dismissed, LegacyGameMessages::ReadZoneSummaryDismissed), sender);
			ExpectTruncatedFails(dismissed);
		}
		for (const auto state : { eObjectWorldState::INWORLD, eObjectWorldState::ATTACHED, eObjectWorldState::INVENTORY }) {
			GameMessages::ChangeObjectWorldState msg;
			msg.target = target;
			msg.newState = state;
			EXPECT_EQ(RoundTrip(msg).newState, state);
		}
	}
	// Name-value text: a null terminator follows non-empty text and is not counted.
	GameMessages::LocalizedAnnouncementServerToSingleClient withParams;
	withParams.bodyParams = u"name=0:Bob";
	withParams.titleParams = u"x=1:2";
	const auto copy = RoundTrip(withParams);
	EXPECT_EQ(copy.bodyParams, withParams.bodyParams);
	EXPECT_EQ(copy.titleParams, withParams.titleParams);
	ExpectTruncatedFails(withParams);
}

TEST_F(RemainingMessagesTests, ZoneGoldenBytes) {
	GameMessages::DisplayZoneSummary summary;
	summary.target = 0x0102030405060708LL;
	summary.isZoneStart = true;
	summary.sender = 0x1122334455667788LL;
	// header 53 05 00 0c 00 00 00 00, target, msgId 1043 (u16), bits 0 1 1 then the sender
	EXPECT_PACKET_EQ(FromHex("5305000c00000000" "0807060504030201" "1304" "710eeccaa886644220", 211), StructPacket(summary));
}

// ---------------------------------------------------------------- Player

TEST_F(RemainingMessagesTests, PlayerMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto level : { eGameMasterLevel::CIVILIAN, eGameMasterLevel::DEVELOPER, eGameMasterLevel::OPERATOR }) {
			GameMessages::UpdateChatMode chat;
			chat.target = target;
			chat.level = level;
			ExpectSameAsLegacyBroadcast([&] { LegacyGameMessages::SendChatModeUpdate(target, level); }, chat);
			RoundTrip(chat);
			GameMessages::SetGMLevel gm;
			gm.target = target;
			gm.level = level;
			ExpectSameAsLegacyBroadcast([&] { LegacyGameMessages::SendGMLevelBroadcast(target, level); }, gm);
			RoundTrip(gm);
		}
		for (const int64_t value : { 0LL, -5LL, 123456789012LL }) {
			for (const auto source : { eLootSourceType::NONE, eLootSourceType::MISSION, eLootSourceType::PICKUP }) {
				GameMessages::ModifyLEGOScore score;
				score.target = target;
				score.score = value;
				score.sourceType = source;
				ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendModifyLEGOScore(&entity, address, value, source); }, score, SendMode::SendToClient);
				RoundTrip(score);
				for (const int32_t smallValue : { 0, 7 }) {
					GameMessages::SetCurrency currency;
					currency.target = target;
					currency.currency = value;
					currency.lootType = smallValue;
					currency.sourceID = smallValue;
					currency.sourceLOT = smallValue;
					currency.sourceTradeID = smallValue;
					currency.sourceType = source;
					ExpectSameSends([&] { LegacyGameMessages::SendSetCurrency(&entity, value, smallValue, smallValue, smallValue, smallValue, true, source); }, [&] { currency.SendToClient(entity.GetSystemAddress()); });
					RoundTrip(currency);
				}
			}
			GameMessages::UpdateReputation reputation;
			reputation.target = target;
			reputation.reputation = value;
			ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendUpdateReputation(target, value, address); }, reputation, SendMode::SendToClient);
			RoundTrip(reputation);
		}
	}
}

TEST_F(RemainingMessagesTests, InboundPlayerMessagesMatchLegacy) {
	for (const uint32_t amount : { 0u, 1u, 0xffffffffu }) {
		GameMessages::PickupCurrency pickup;
		pickup.currency = amount;
		EXPECT_EQ(ReadWithLegacy<unsigned int>(pickup, LegacyGameMessages::ReadPickupCurrency), amount);
		ExpectTruncatedFails(pickup);
	}
	for (const auto& name : g_WStrings) {
		for (const int32_t value : g_Ints) {
			for (const LWOMAPID zone : { LWOMAPID_INVALID, LWOMAPID{ 1100 } }) {
				GameMessages::ModifyPlayerZoneStatistic stat;
				stat.bSet = value > 0;
				stat.statName = name;
				stat.statValue = value;
				stat.zoneID = zone;
				const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyZoneStatistic>(stat, LegacyGameMessages::ReadModifyPlayerZoneStatistic);
				EXPECT_EQ(legacy.set, stat.bSet);
				EXPECT_EQ(legacy.statisticsName, name);
				EXPECT_EQ(legacy.value, value);
				EXPECT_EQ(legacy.zone, zone);
				RoundTrip(stat);
			}
		}
		GameMessages::ParseChatMessage chat;
		chat.iClientState = 3;
		chat.wsString = name;
		const auto legacyChat = ReadWithLegacy<LegacyGameMessages::LegacyParseChatMessage>(chat, LegacyGameMessages::ReadParseChatMessage);
		EXPECT_EQ(legacyChat.iClientState, 3);
		EXPECT_EQ(legacyChat.wsString, name);
		RoundTrip(chat);

		GameMessages::ReportBug report;
		report.body = name;
		report.clientVersion = "1.10.64";
		report.otherPlayerID = "0";
		report.selection = "selection";
		const auto legacyReport = ReadWithLegacy<LegacyGameMessages::LegacyBugReport>(report, LegacyGameMessages::ReadReportBug);
		std::string narrowed;
		for (const auto character : name) narrowed.push_back(static_cast<char>(character));
		EXPECT_EQ(legacyReport.body, narrowed);
		EXPECT_EQ(legacyReport.clientVersion, "1.10.64");
		EXPECT_EQ(legacyReport.otherPlayer, "0");
		EXPECT_EQ(legacyReport.selection, "selection");
		RoundTrip(report);
	}
	for (const int32_t id : g_Ints) {
		for (const int64_t value : { int64_t{ 1 }, int64_t{ 0 }, int64_t{ -7 }, int64_t{ 1 } << 40 }) {
			GameMessages::UpdatePlayerStatistic stat;
			stat.updateID = id;
			stat.updateValue = value;
			const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyPlayerStatistic>(stat, LegacyGameMessages::ReadUpdatePlayerStatistic);
			EXPECT_EQ(legacy.updateID, id);
			EXPECT_EQ(legacy.updateValue, value);
			ExpectTruncatedFails(stat);
		}
	}
	for (const auto& bytes : g_Strings) {
		for (const uint32_t handle : { 0u, 9u }) {
			GameMessages::VerifyAck ack;
			ack.bDifferent = handle != 0;
			ack.sBitStream = bytes;
			ack.uiHandle = handle;
			const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyVerifyAck>(ack, LegacyGameMessages::ReadVerifyAck);
			EXPECT_EQ(legacy.bDifferent, ack.bDifferent);
			EXPECT_EQ(legacy.sBitStream, bytes);
			EXPECT_EQ(legacy.uiHandle, handle);
			RoundTrip(ack);
		}
	}
}

// ---------------------------------------------------------------- Objects and scripts

TEST_F(RemainingMessagesTests, NotifyMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const auto& name : g_WStrings) {
			for (const int32_t param : g_Ints) {
				for (const auto& paramStr : g_Strings) {
					GameMessages::NotifyClientObject notify(target, name, param, -param, target, paramStr);
					ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendNotifyClientObject(target, name, param, -param, target, paramStr, address); }, notify);
					EXPECT_EQ(RoundTrip(notify).paramStr, paramStr);
					GameMessages::NotifyClientZoneObject zoneNotify(target, name, param, -param, target, paramStr);
					ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendNotifyClientZoneObject(target, name, param, -param, target, paramStr, address); }, zoneNotify);
					RoundTrip(zoneNotify);
				}
				GameMessages::NotifyObject notifyObject(target, 0x1000000000000001LL, name, param, param / 2);
				ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendNotifyObject(target, 0x1000000000000001LL, name, address, param, param / 2); }, notifyObject);
				RoundTrip(notifyObject);
				GameMessages::NotifyClientFailedPrecondition precondition;
				precondition.target = target;
				precondition.failedReason = name;
				precondition.preconditionID = param;
				ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendNotifyClientFailedPrecondition(target, address, name, param); }, precondition);
				RoundTrip(precondition);
			}
			for (const auto object : g_Ids) {
				// The old function never wrote param1 or param2 (both flags always 0), whatever it was given.
				GameMessages::FireEventClientSide fire(target, name, object, 0x0102030405060708LL);
				ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendFireEventClientSide(target, address, name, object, 5, 6, 0x0102030405060708LL); }, fire, SendMode::SendToClient);
				RoundTrip(fire);
			}
			GameMessages::SetName setName;
			setName.target = target;
			setName.name = name;
			ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendSetName(target, name, address); }, setName);
			RoundTrip(setName);
		}
		Entity entity(target, info);
		for (const auto& data : g_Strings) {
			GameMessages::ScriptNetworkVarUpdate scriptVar;
			scriptVar.target = target;
			scriptVar.tableOfVars = GeneralUtils::ASCIIToUTF16(data);
			ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendSetNetworkScriptVar(&entity, address, data); }, scriptVar);
			RoundTrip(scriptVar);
		}
		for (const auto type : { eTerminateType::RANGE, eTerminateType::USER, eTerminateType::FROM_INTERACTION }) {
			for (const auto terminator : g_Ids) {
				GameMessages::TerminateInteraction terminate(target, type, terminator);
				ExpectSameAsLegacyBroadcast([&] { LegacyGameMessages::SendTerminateInteraction(target, type, terminator); }, terminate);
				RoundTrip(terminate);
			}
		}
	}
}

TEST_F(RemainingMessagesTests, FireEventServerSideMatchesLegacy) {
	for (const auto& args : g_WStrings) {
		for (const int32_t param : g_Ints) {
			for (const auto sender : g_Ids) {
				GameMessages::FireEventServerSide fire;
				fire.args = args;
				fire.param1 = param;
				fire.param2 = -1;
				fire.param3 = param / 3;
				fire.senderID = sender;
				const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyFireEventServerSide>(fire, LegacyGameMessages::ReadFireEventServerSide);
				EXPECT_EQ(legacy.args, args);
				EXPECT_EQ(legacy.param1, param);
				EXPECT_EQ(legacy.param2, -1);
				EXPECT_EQ(legacy.param3, param / 3);
				EXPECT_EQ(legacy.senderID, sender);
				RoundTrip(fire);
			}
		}
	}
	GameMessages::FireEventServerSide fire;
	fire.args = u"ZonePlayer";
	fire.param3 = 1100;
	ExpectTruncatedFails(fire);
}

// ---------------------------------------------------------------- Quickbuild

TEST_F(RemainingMessagesTests, QuickBuildMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto player : g_Ids) {
			for (const auto state : { eQuickBuildState::OPEN, eQuickBuildState::COMPLETED, eQuickBuildState::BUILDING }) {
				GameMessages::RebuildNotifyState notify;
				notify.target = target;
				notify.prevState = eQuickBuildState::RESETTING;
				notify.state = state;
				notify.player = player;
				ExpectSameAsLegacyBroadcast([&] { LegacyGameMessages::SendQuickBuildNotifyState(&entity, eQuickBuildState::RESETTING, state, player); }, notify);
				RoundTrip(notify);
			}
			for (uint32_t mask = 0; mask < 8; mask++) {
				for (const auto reason : { eQuickBuildFailReason::NOT_GIVEN, eQuickBuildFailReason::CANCELED_EARLY, eQuickBuildFailReason::OUT_OF_IMAGINATION }) {
					for (const float duration : g_Floats) {
						GameMessages::EnableRebuild enable;
						enable.target = target;
						enable.bEnable = Bit(mask, 0);
						enable.bFail = Bit(mask, 1);
						enable.bSuccess = Bit(mask, 2);
						enable.eFailReason = reason;
						enable.fDuration = duration;
						enable.user = player;
						ExpectSameAsLegacyBroadcast([&] { LegacyGameMessages::SendEnableQuickBuild(&entity, Bit(mask, 0), Bit(mask, 1), Bit(mask, 2), reason, duration, player); }, enable);
						RoundTrip(enable);
					}
				}
			}
			for (const bool early : { false, true }) {
				GameMessages::RebuildCancel cancel;
				cancel.bEarlyRelease = early;
				cancel.userID = player;
				const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyQuickBuildCancel>(cancel, LegacyGameMessages::ReadQuickBuildCancel);
				EXPECT_EQ(legacy.bEarlyRelease, early);
				EXPECT_EQ(legacy.userID, player);
				ExpectTruncatedFails(cancel);
			}
		}
	}
}

// ---------------------------------------------------------------- Activities

TEST_F(RemainingMessagesTests, ActivityMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const int32_t value : g_Ints) {
			GameMessages::MatchResponse response;
			response.target = target;
			response.response = value;
			ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendMatchResponse(&entity, address, value); }, response, SendMode::SendToClient);
			RoundTrip(response);
		}
		for (const auto& data : g_Strings) {
			for (const auto type : { eMatchUpdate::PLAYER_ADDED, eMatchUpdate::PHASE_WAIT_READY }) {
				GameMessages::MatchUpdate update;
				update.target = target;
				update.data = data;
				update.type = type;
				ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendMatchUpdate(&entity, address, data, type); }, update, SendMode::SendToClient);
				EXPECT_EQ(RoundTrip(update).data, data);

				GameMessages::MatchRequest request;
				request.activator = target;
				request.playerChoices = data;
				request.type = 1;
				request.value = static_cast<int32_t>(type);
				const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyMatchRequest>(request, LegacyGameMessages::ReadMatchRequest);
				EXPECT_EQ(legacy.activator, target);
				EXPECT_EQ(legacy.playerChoices, data);
				EXPECT_EQ(legacy.type, 1);
				EXPECT_EQ(legacy.value, request.value);
				RoundTrip(request);
			}
		}
		for (const auto& pos : g_Points) {
			for (const int32_t score : { 0, 1500, -1 }) {
				GameMessages::NotifyClientShootingGalleryScore galleryScore;
				galleryScore.target = target;
				galleryScore.addTime = 2.5f;
				galleryScore.score = score;
				galleryScore.targetID = 0x1000000000000001LL;
				galleryScore.targetPos = pos;
				ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendNotifyClientShootingGalleryScore(target, address, 2.5f, score, 0x1000000000000001LL, pos); }, galleryScore);
				RoundTrip(galleryScore);
			}
			GameMessages::UpdateShootingGalleryRotation rotation;
			rotation.angle = 1.25f;
			rotation.facing = pos;
			rotation.muzzlePos = NiPoint3(1.0f, 2.0f, 3.0f);
			const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyShootingGalleryRotation>(rotation, LegacyGameMessages::ReadUpdateShootingGalleryRotation);
			EXPECT_EQ(legacy.angle, 1.25f);
			EXPECT_EQ(legacy.facing, pos);
			EXPECT_EQ(legacy.muzzlePos, rotation.muzzlePos);
			ExpectTruncatedFails(rotation);
		}
		for (const auto& text : g_WStrings) {
			GameMessages::ActivityStateChangeRequest change;
			change.objectID = target;
			change.value1 = 4;
			change.value2 = -4;
			change.stringValue = text;
			const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyActivityStateChangeRequest>(change, LegacyGameMessages::ReadActivityStateChangeRequest);
			EXPECT_EQ(legacy.objectID, target);
			EXPECT_EQ(legacy.value1, 4);
			EXPECT_EQ(legacy.value2, -4);
			EXPECT_EQ(legacy.stringValue, text);
			RoundTrip(change);
		}
	}
}

TEST_F(RemainingMessagesTests, LeaderboardMessagesMatchLegacy) {
	for (const int32_t gameID : { 0, 1864 }) {
		for (const int32_t queryType : { 0, 1, 2 }) {
			for (const int32_t end : { 10, 0 }) {
				for (const int32_t start : { 0, 5 }) {
					for (const bool weekly : { false, true }) {
						GameMessages::RequestActivitySummaryLeaderboardData msg;
						msg.target = 0x0102030405060708LL;
						msg.gameID = gameID;
						msg.queryType = queryType;
						msg.resultsEnd = end;
						msg.resultsStart = start;
						msg.weekly = weekly;
						msg.targetID = 0x0102030405060708LL;
						ExpectSameAsLegacy([&](const SystemAddress& address) {
							LegacyGameMessages::SendRequestActivitySummaryLeaderboardData(0x0102030405060708LL, 0x0102030405060708LL, address, gameID, queryType, end, start, weekly);
							}, msg, SendMode::SendToClient);
						const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyLeaderboardRequest>(msg, LegacyGameMessages::ReadRequestActivitySummaryLeaderboardData);
						EXPECT_EQ(legacy.gameID, gameID);
						EXPECT_EQ(static_cast<int32_t>(legacy.queryType), queryType);
						EXPECT_EQ(legacy.resultsEnd, end);
						EXPECT_EQ(legacy.resultsStart, start);
						EXPECT_EQ(legacy.weekly, weekly);
						RoundTrip(msg);
					}
				}
			}
		}
	}
	Leaderboard leaderboard(1864, Leaderboard::InfoType::Top, false, 0x1000000000000001LL, 10, Leaderboard::Type::ShootingGallery);
	GameMessages::SendActivitySummaryLeaderboardData data;
	data.target = 0x1000000000000001LL;
	data.leaderboard = &leaderboard;
	ExpectSameAsLegacy([&](const SystemAddress& address) { LegacyGameMessages::SendActivitySummaryLeaderboardData(0x1000000000000001LL, &leaderboard, address); }, data, SendMode::SendToClient);
}

// Packets from 2011/2012 live captures: live sent UpdatePlayerStatistic (1481) server -> client. The client reads
// a u32 statistic and an optional i64 amount that defaults to 1 (0x00d8c870 in 1.10.64).
TEST_F(RemainingMessagesTests, UpdatePlayerStatisticMatchesLiveCapture) {
	// CurrencyCollected after a SetCurrency, amount left at the default.
	const auto currency = FromLiveCapture<GameMessages::UpdatePlayerStatistic>("5305000c000000005e7dea8e00000010c9050100000000");
	EXPECT_EQ(currency.target, 0x100000008eea7d5eLL);
	EXPECT_EQ(currency.updateID, static_cast<int32_t>(StatisticID::CurrencyCollected));
	EXPECT_EQ(currency.updateValue, 1);

	// CurrencyCollected with an amount.
	const auto coins = FromLiveCapture<GameMessages::UpdatePlayerStatistic>("5305000c000000005e7dea8e00000010c90501000000fa0080000000000000");
	EXPECT_EQ(coins.updateID, static_cast<int32_t>(StatisticID::CurrencyCollected));
	EXPECT_EQ(coins.updateValue, 500);

	// MetersTraveled, sent periodically while the player moves.
	const auto meters = FromLiveCapture<GameMessages::UpdatePlayerStatistic>("5305000c000000005e7dea8e00000010c9050c0000008e8000000000000000");
	EXPECT_EQ(meters.updateID, static_cast<int32_t>(StatisticID::MetersTraveled));
	EXPECT_EQ(meters.updateValue, 29);
}

// Packets from 2011/2012 live captures, sent when items were equipped and unequipped. DLU wrote ChangeObjectWorldState's
// state without the flag the client reads first, so the client read ATTACHED as INWORLD.
TEST_F(RemainingMessagesTests, EquipMessagesMatchLiveCapture) {
	const auto attached = FromLiveCapture<GameMessages::ChangeObjectWorldState>("5305000c000000002d21026701000010c7048080000000");
	EXPECT_EQ(attached.target, 0x100000016702212dLL);
	EXPECT_EQ(attached.newState, eObjectWorldState::ATTACHED);
	const auto inventory = FromLiveCapture<GameMessages::ChangeObjectWorldState>("5305000c00000000b5915b6701000010c7048100000000");
	EXPECT_EQ(inventory.newState, eObjectWorldState::INVENTORY);

	const auto uncast = FromLiveCapture<GameMessages::UncastSkill>("5305000c000000001f147b5b01000010b6046a010000");
	EXPECT_EQ(uncast.skillID, 362);

	const auto unequip = FromLiveCapture<GameMessages::UnEquipInventory>("5305000c000000001f147b5b01000010e9004cb607ae4020000200");
	EXPECT_FALSE(unequip.bEvenIfDead);
	EXPECT_TRUE(unequip.bIgnoreCooldown);
	EXPECT_FALSE(unequip.bOutSuccess);
	EXPECT_EQ(unequip.replacementObjectID, LWOOBJID_EMPTY);
}
