#include "BuildingMessages.h"
#include "ClientPackets.h"
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"
#include "Legacy/BuildingMessagesLegacy.h"

#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	const std::vector<int32_t> g_Ints = { 0, 1, -1, 8, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };
	const std::vector<NiPoint3> g_Points = { NiPoint3Constant::ZERO, NiPoint3(1.5f, -2.0f, 1e6f) };

	PacketBytes Payload(const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		return FromBitStream(bitStream);
	}

	// Serializes msg, then reads it with the legacy read sequence; returns the result and how many bits it read.
	template<typename Result>
	std::pair<Result, uint32_t> ReadWithLegacy(const GameMessages::NetGameMsg& msg, const std::function<Result(RakNet::BitStream&)>& read) {
		RakNet::BitStream wire;
		msg.Serialize(wire);
		RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
		auto result = read(legacyStream);
		return { result, static_cast<uint32_t>(legacyStream.GetReadOffset()) };
	}

	uint32_t Bits(const GameMessages::NetGameMsg& msg) { return Payload(msg).bits; }

	// A packet struct sent to one client and to UNASSIGNED must match the legacy bytes. The legacy code always did
	// SEND_PACKET (never a broadcast); LUBitStream::Send broadcasts for UNASSIGNED, which none of the callers pass.
	void ExpectPacketSameAsLegacy(const std::function<void(const SystemAddress&)>& legacySend, const LUBitStream& packet) {
		const auto address = ClientAddress();
		const auto legacy = Capture([&] { legacySend(address); });
		const auto ours = Capture([&] { packet.Send(address); });
		ASSERT_EQ(legacy.size(), 1);
		ASSERT_EQ(ours.size(), 1);
		EXPECT_PACKET_EQ(FromCapture(legacy[0]), FromCapture(ours[0]));
		EXPECT_EQ(legacy[0].broadcast, ours[0].broadcast);
		EXPECT_EQ(legacy[0].sysAddr, ours[0].sysAddr);
	}
}

class BuildingMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(BuildingMessagesTests, StartArrangingWithItemMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const bool firstTime : { false, true }) {
			for (const auto area : g_Targets) {
				for (const auto value : g_Ints) {
					for (const auto& point : g_Points) {
						GameMessages::StartArrangingWithItem msg;
						msg.target = target;
						msg.firstTime = firstTime;
						msg.buildAreaID = area;
						msg.buildStartPos = point;
						msg.sourceBag = value;
						msg.sourceID = ~area;
						msg.sourceLot = value / 2;
						msg.sourceType = -value;
						msg.targetID = area / 3;
						msg.targetLot = value / 5;
						msg.targetPos = NiPoint3(point.z, point.y, point.x);
						msg.targetType = value / 7;
						ExpectSameAsLegacy([&](const SystemAddress& a) {
							LegacyGameMessages::SendStartArrangingWithItem(&entity, a, firstTime, area, point, value, msg.sourceID, msg.sourceLot, msg.sourceType, msg.targetID, msg.targetLot, msg.targetPos, msg.targetType);
							}, msg, SendMode::SendToClient);
						const auto copy = RoundTrip(msg);
						EXPECT_EQ(copy.buildAreaID, area);
						EXPECT_EQ(copy.targetType, msg.targetType);
						ExpectTruncatedFails(msg);
					}
				}
			}
		}

		// The struct's defaults are the old declaration's default arguments
		GameMessages::StartArrangingWithItem defaults;
		defaults.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendStartArrangingWithItem(&entity, a, true, LWOOBJID_EMPTY, NiPoint3Constant::ZERO, 0, LWOOBJID_EMPTY, 0, 8, 0, 0, NiPoint3Constant::ZERO, 0); }, defaults, SendMode::SendToClient);
	}
}

TEST_F(BuildingMessagesTests, FinishArrangingAndModularBuildEndMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		// The legacy functions sent to the entity's own address.
		const auto address = entity.GetSystemAddress();
		for (const auto area : g_Targets) {
			const auto legacy = Capture([&] { LegacyGameMessages::SendFinishArrangingWithItem(&entity, area); });
			const auto ours = Capture([&] {
				GameMessages::FinishArrangingWithItem finish;
				finish.target = target;
				finish.buildAreaID = area;
				finish.SendToClient(address);
				});
			ASSERT_EQ(legacy.size(), 1);
			ASSERT_EQ(ours.size(), 1);
			EXPECT_PACKET_EQ(FromCapture(legacy[0]), FromCapture(ours[0]));
			EXPECT_EQ(legacy[0].broadcast, ours[0].broadcast);
			EXPECT_EQ(legacy[0].sysAddr, ours[0].sysAddr);

			GameMessages::FinishArrangingWithItem finish;
			finish.buildAreaID = area;
			finish.newTargetPos = NiPoint3(1, 2, 3);
			finish.oldItemLot = 7;
			const auto copy = RoundTrip(finish);
			EXPECT_EQ(copy.buildAreaID, area);
			EXPECT_EQ(copy.newTargetPos, finish.newTargetPos);
			ExpectTruncatedFails(finish);
		}

		const auto legacyEnd = Capture([&] { LegacyGameMessages::SendModularBuildEnd(&entity); });
		const auto ourEnd = Capture([&] {
			GameMessages::ModularBuildEnd end;
			end.target = target;
			end.SendToClient(address);
			});
		ASSERT_EQ(legacyEnd.size(), 1);
		ASSERT_EQ(ourEnd.size(), 1);
		EXPECT_PACKET_EQ(FromCapture(legacyEnd[0]), FromCapture(ourEnd[0]));
		EXPECT_EQ(legacyEnd[0].sysAddr, ourEnd[0].sysAddr);
	}
}

TEST_F(BuildingMessagesTests, SetBuildModeConfirmedMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const bool start : { false, true }) {
			for (const bool warn : { false, true }) {
				for (const auto value : g_Ints) {
					for (const auto& point : g_Points) {
						GameMessages::SetBuildModeConfirmed msg;
						msg.target = target;
						msg.start = start;
						msg.warnVisitors = warn;
						msg.modePaused = !start;
						msg.modeValue = value;
						msg.playerId = ~target;
						msg.startPos = point;
						ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendSetBuildModeConfirmed(target, a, start, warn, !start, value, ~target, point); }, msg);
						const auto copy = RoundTrip(msg);
						EXPECT_EQ(copy.modeValue, value);
						EXPECT_EQ(copy.startPos, point);
						EXPECT_EQ(copy.warnVisitors, warn);
						ExpectTruncatedFails(msg);
					}
				}
			}
		}
	}
}

TEST_F(BuildingMessagesTests, BlueprintPacketsMatchLegacy) {
	for (const auto id : g_Targets) {
		for (const bool success : { false, true }) {
			ClientPackets::BlueprintLoadItemResponse load;
			load.success = success;
			load.itemId = id;
			load.destItemId = ~id;
			ExpectPacketSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendBlueprintLoadItemResponse(a, success, id, ~id); }, load);
		}

		ClientPackets::BlueprintSaveResponse failed;
		failed.reasonCode = eBlueprintSaveResponseType::PlacementFailed;
		ExpectPacketSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::WriteUnUseModelSaveResponse(a); }, failed);

		const std::vector<std::vector<std::string>> chunkSets = { {}, { "sd0" }, { std::string("\x73\x64\x30\x01\xff", 5), std::string(1000, '\x7f'), "" } };
		std::vector<std::pair<LWOOBJID, std::vector<std::string>>> saved;
		ClientPackets::BlueprintSaveResponse save;
		save.localId = id;
		save.reasonCode = eBlueprintSaveResponseType::EverythingWorked;
		std::vector<std::pair<LWOOBJID, std::string>> levelModels;
		ClientPackets::BlueprintSaveResponse level;
		level.reasonCode = eBlueprintSaveResponseType::EverythingWorked;
		for (size_t i = 0; i <= chunkSets.size(); i++) {
			ExpectPacketSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::WriteBBBSaveResponse(a, id, saved); }, save);
			ExpectPacketSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::WriteLevelLoadSaveResponse(a, levelModels); }, level);
			if (i == chunkSets.size()) break;
			saved.emplace_back(id + static_cast<LWOOBJID>(i), chunkSets[i]);
			auto& model = save.models.emplace_back();
			model.blueprintId = id + static_cast<LWOOBJID>(i);
			for (const auto& chunk : chunkSets[i]) model.data += chunk;
			levelModels.emplace_back(model.blueprintId, model.data);
			level.models.push_back(model);
		}

		// Round trip
		RakNet::BitStream bitStream;
		save.WritePacket(bitStream);
		ClientPackets::BlueprintSaveResponse copy;
		ASSERT_TRUE(copy.ReadHeader(bitStream));
		ASSERT_TRUE(copy.Deserialize(bitStream));
		EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
		ASSERT_EQ(copy.models.size(), save.models.size());
		for (size_t i = 0; i < copy.models.size(); i++) EXPECT_EQ(copy.models[i].data, save.models[i].data);
	}
}

TEST_F(BuildingMessagesTests, InboundReadsLikeLegacy) {
	for (const auto id : g_Targets) {
		for (const auto value : g_Ints) {
			for (const bool flag : { false, true }) {
				GameMessages::StartBuildingWithItem start;
				start.firstTime = flag;
				start.success = !flag;
				start.sourceBag = value;
				start.sourceId = id;
				start.sourceLot = value / 2;
				start.sourceType = value / 3;
				start.targetId = ~id;
				start.targetLot = -value;
				start.targetPos = NiPoint3(1, 2, 3);
				start.targetType = value / 5;
				const auto [legacy, bits] = ReadWithLegacy<LegacyGameMessages::LegacyStartBuildingWithItem>(start, LegacyGameMessages::ReadStartBuildingWithItem);
				EXPECT_EQ(bits, Bits(start));
				const auto copy = RoundTrip(start);
				EXPECT_EQ(copy.firstTime, legacy.firstTime);
				EXPECT_EQ(copy.success, legacy.success);
				EXPECT_EQ(copy.sourceBag, legacy.sourceBag);
				EXPECT_EQ(copy.sourceId, legacy.sourceId);
				EXPECT_EQ(copy.sourceLot, legacy.sourceLot);
				EXPECT_EQ(copy.sourceType, legacy.sourceType);
				EXPECT_EQ(copy.targetId, legacy.targetId);
				EXPECT_EQ(copy.targetLot, legacy.targetLot);
				EXPECT_EQ(copy.targetPos, legacy.targetPosition);
				EXPECT_EQ(copy.targetType, legacy.targetType);
				ExpectTruncatedFails(start);

				for (const auto& point : g_Points) {
					GameMessages::SetBuildMode mode;
					mode.start = flag;
					mode.distanceType = value;
					mode.modePaused = !flag;
					mode.modeValue = -value;
					mode.playerId = id;
					mode.startPos = point;
					const auto [legacyMode, modeBits] = ReadWithLegacy<LegacyGameMessages::LegacySetBuildMode>(mode, LegacyGameMessages::ReadSetBuildMode);
					EXPECT_EQ(modeBits, Bits(mode));
					const auto modeCopy = RoundTrip(mode);
					EXPECT_EQ(modeCopy.start, legacyMode.start);
					EXPECT_EQ(modeCopy.distanceType, legacyMode.distanceType);
					EXPECT_EQ(modeCopy.modePaused, legacyMode.modePaused);
					EXPECT_EQ(modeCopy.modeValue, legacyMode.modeValue);
					EXPECT_EQ(modeCopy.playerId, legacyMode.playerId);
					EXPECT_EQ(modeCopy.startPos, legacyMode.startPosition);
					ExpectTruncatedFails(mode);

					// BuildModeSet has SetBuildMode's layout; DLU only ever read its first field.
					GameMessages::BuildModeSet set;
					set.start = flag;
					set.distanceType = value;
					set.modeValue = -value;
					set.playerId = id;
					set.startPos = point;
					EXPECT_EQ(ReadWithLegacy<bool>(set, LegacyGameMessages::ReadBuildModeSet).first, flag);
					EXPECT_EQ(RoundTrip(set).startPos, point);
					ExpectTruncatedFails(set);
				}
			}

			GameMessages::DoneArrangingWithItem done;
			done.newSourceBag = value;
			done.newSourceID = id;
			done.newSourceLot = value / 2;
			done.newSourceType = value / 3;
			done.newTargetID = ~id;
			done.newTargetLot = -value;
			done.newTargetType = value / 5;
			done.newTargetPosition = NiPoint3(4, 5, 6);
			done.oldItemBag = value / 7;
			done.oldItemID = id / 3;
			done.oldItemLot = value / 11;
			done.oldItemType = value / 13;
			const auto [legacyDone, doneBits] = ReadWithLegacy<LegacyGameMessages::LegacyDoneArranging>(done, LegacyGameMessages::ReadDoneArrangingWithItem);
			EXPECT_EQ(doneBits, Bits(done));
			const auto doneCopy = RoundTrip(done);
			EXPECT_EQ(doneCopy.newSourceBag, legacyDone.newSourceBAG);
			EXPECT_EQ(doneCopy.newSourceID, legacyDone.newSourceID);
			EXPECT_EQ(doneCopy.newSourceLot, legacyDone.newSourceLOT);
			EXPECT_EQ(doneCopy.newSourceType, legacyDone.newSourceTYPE);
			EXPECT_EQ(doneCopy.newTargetID, legacyDone.newTargetID);
			EXPECT_EQ(doneCopy.newTargetLot, legacyDone.newTargetLOT);
			EXPECT_EQ(doneCopy.newTargetType, legacyDone.newTargetTYPE);
			EXPECT_EQ(doneCopy.newTargetPosition, legacyDone.newTargetPOS);
			EXPECT_EQ(doneCopy.oldItemBag, legacyDone.oldItemBAG);
			EXPECT_EQ(doneCopy.oldItemID, legacyDone.oldItemID);
			EXPECT_EQ(doneCopy.oldItemLot, legacyDone.oldItemLOT);
			EXPECT_EQ(doneCopy.oldItemType, legacyDone.oldItemTYPE);
			ExpectTruncatedFails(done);

			GameMessages::ModularBuildMoveAndEquip move;
			move.templateID = value;
			EXPECT_EQ(ReadWithLegacy<LOT>(move, LegacyGameMessages::ReadModularBuildMoveAndEquip).first, value);
			EXPECT_EQ(RoundTrip(move).templateID, value);
			ExpectTruncatedFails(move);
		}

		GameMessages::ModularBuildConvertModel convert;
		convert.modelID = id;
		EXPECT_EQ(ReadWithLegacy<LWOOBJID>(convert, LegacyGameMessages::ReadObjectId).first, id);
		EXPECT_EQ(RoundTrip(convert).modelID, id);
		ExpectTruncatedFails(convert);

		GameMessages::BBBLoadItemRequest load;
		load.itemID = id;
		EXPECT_EQ(ReadWithLegacy<LWOOBJID>(load, LegacyGameMessages::ReadObjectId).first, id);
		EXPECT_EQ(RoundTrip(load).itemID, id);
		ExpectTruncatedFails(load);

		for (const bool hasTransform : { false, true }) {
			GameMessages::UnUseBBBModel unUse;
			unUse.bHasWorldTransform = hasTransform;
			unUse.modelID = id;
			const auto legacyUnUse = ReadWithLegacy<std::pair<bool, LWOOBJID>>(unUse, LegacyGameMessages::ReadUnUseModel).first;
			const auto unUseCopy = RoundTrip(unUse);
			EXPECT_EQ(unUseCopy.bHasWorldTransform, legacyUnUse.first);
			EXPECT_EQ(unUseCopy.modelID, legacyUnUse.second);
			ExpectTruncatedFails(unUse);
			unUse.worldPos = NiPoint3(1, 2, 3);
			unUse.worldRot = NiQuaternion(0.5f, 0.5f, 0.5f, 0.5f);
			const auto withTransform = RoundTrip(unUse);
			EXPECT_EQ(withTransform.worldPos, unUse.worldPos);
			EXPECT_EQ(withTransform.worldRot, unUse.worldRot);
		}

		for (const auto& data : { std::string(), std::string("\x73\x64\x30\x01\xff\x00\x10", 7), std::string(5000, 'z') }) {
			GameMessages::BBBSaveRequest save;
			save.localID = id;
			save.lxfmlDataCompressed = data;
			save.timeTakenInMs = 1234;
			const auto [legacySave, saveBits] = ReadWithLegacy<LegacyGameMessages::LegacyBBBSaveRequest>(save, LegacyGameMessages::ReadBBBSaveRequest);
			EXPECT_EQ(saveBits, Bits(save));
			const auto saveCopy = RoundTrip(save);
			EXPECT_EQ(saveCopy.localID, legacySave.localId);
			EXPECT_EQ(saveCopy.lxfmlDataCompressed, legacySave.sd0);
			EXPECT_EQ(saveCopy.timeTakenInMs, legacySave.timeTaken);
		}
		GameMessages::BBBSaveRequest small;
		small.lxfmlDataCompressed = "abc";
		ExpectTruncatedFails(small);
	}

	// ModularBuildFinish: the old handler read the parts only for 3 to 7 of them.
	for (const std::vector<LOT> modules : { std::vector<LOT>{ 1, 2, 3 }, std::vector<LOT>{ 8129, 8129, -1, 4, 5, 6, 7 } }) {
		GameMessages::ModularBuildFinish finish;
		finish.modules = modules;
		const auto [legacy, bits] = ReadWithLegacy<std::vector<uint32_t>>(finish, LegacyGameMessages::ReadModularBuildFinish);
		EXPECT_EQ(bits, Bits(finish));
		ASSERT_EQ(legacy.size(), modules.size());
		for (size_t i = 0; i < modules.size(); i++) EXPECT_EQ(static_cast<LOT>(legacy[i]), modules[i]);
		EXPECT_EQ(RoundTrip(finish).modules, modules);
		ExpectTruncatedFails(finish);
	}
}

// Independent of the legacy code: hand computed payloads (RakNet writes MSB first).
TEST_F(BuildingMessagesTests, GoldenBytes) {
	GameMessages::SetBuildModeConfirmed confirmed;
	confirmed.start = true;
	// 1, 1, 0, flag 1, 01 00 00 00, 8 zero bytes (player), flag 1, 12 zero bytes (position)
	EXPECT_PACKET_EQ(FromHex("d0 10 00 00 00 00 00 00 00 00 00 00 08 00 00 00 00 00 00 00 00 00 00 00 00", 197), Payload(confirmed));

	GameMessages::ModularBuildFinish finish;
	finish.modules = { 6416, 1, 2 };
	EXPECT_PACKET_EQ(FromHex("03 10 19 00 00 01 00 00 00 02 00 00 00"), Payload(finish));

	ClientPackets::BlueprintLoadItemResponse load;
	load.success = true;
	load.itemId = 515;
	load.destItemId = 990;
	RakNet::BitStream bitStream;
	load.WritePacket(bitStream);
	EXPECT_PACKET_EQ(FromHex("53 05 00 17 00 00 00 00 01 03 02 00 00 00 00 00 00 de 03 00 00 00 00 00 00"), FromBitStream(bitStream));
}

// Messages added for the brick by brick workflow (docs/BuildWorkflow.md), laid out as the 1.10.64 client reads them.
TEST_F(BuildingMessagesTests, BrickModeMessagesRoundTrip) {
	GameMessages::ActivateBrickMode enter;
	enter.buildObjectID = 0x1122334455667788;
	enter.enterBuildFromWorld = false;
	enter.enterFlag = true;
	// flag 1 + the build area, flag 0 (build type 2, on a property), 0, 1 (0x00d8ecb0)
	EXPECT_PACKET_EQ(FromHex("c4 3b b3 2a a2 19 91 08 90", 68), Payload(enter));
	const auto enterCopy = RoundTrip(enter);
	EXPECT_EQ(enterCopy.buildObjectID, enter.buildObjectID);
	EXPECT_EQ(enterCopy.buildType, 2);
	EXPECT_FALSE(enterCopy.enterBuildFromWorld);
	EXPECT_TRUE(enterCopy.enterFlag);
	ExpectTruncatedFails(enter);

	GameMessages::ActivateBrickMode leave;
	leave.buildType = 1;
	leave.enterBuildFromWorld = true;
	leave.enterFlag = false;
	const auto leaveCopy = RoundTrip(leave);
	EXPECT_EQ(leaveCopy.buildObjectID, LWOOBJID_EMPTY);
	EXPECT_EQ(leaveCopy.buildType, 1);
	EXPECT_FALSE(leaveCopy.enterFlag);

	// The client clears its autosave with the bare sd0 header: u32 size, then the bytes (0x00f2af60)
	GameMessages::SetBBBAutosave clear;
	clear.lxfmlDataCompressed = std::string("sd0\x01\xff", 5);
	EXPECT_PACKET_EQ(FromHex("05 00 00 00 73 64 30 01 ff"), Payload(clear));
	EXPECT_EQ(RoundTrip(clear).lxfmlDataCompressed, clear.lxfmlDataCompressed);
	ExpectTruncatedFails(clear);

	GameMessages::SetBBBAutosave empty;
	EXPECT_PACKET_EQ(FromHex("00 00 00 00"), Payload(empty));
	EXPECT_TRUE(RoundTrip(empty).lxfmlDataCompressed.empty());

	// A size larger than what is left is dropped rather than read past the end
	RakNet::BitStream tooLong;
	tooLong.Write<uint32_t>(100);
	tooLong.Write<uint8_t>(1);
	GameMessages::SetBBBAutosave read;
	EXPECT_FALSE(read.Deserialize(tooLong));

	GameMessages::RebuildBBBAutosaveMsg rebuilt;
	rebuilt.count = 2;
	EXPECT_PACKET_EQ(FromHex("02 00 00 00"), Payload(rebuilt));
	EXPECT_EQ(RoundTrip(rebuilt).count, 2);
	ExpectTruncatedFails(rebuilt);
}

// FetchModelMetadataRequest / Response as the client writes and reads them (GameMessage::FetchModelMetadataResponse::
// Serialize, 0x00e3ccb0; UGObjectMetadata::Serialize, 0x00f5f590; BlueprintMetadata::Serialize, 0x00f5f760)
TEST_F(BuildingMessagesTests, FetchModelMetadataWire) {
	GameMessages::FetchModelMetadataRequest request;
	request.context = 1;
	request.objectID = 2;
	request.requestorID = 3;
	request.ugID = 4;
	EXPECT_PACKET_EQ(FromHex("01 00 00 00 02 00 00 00 00 00 00 00 03 00 00 00 00 00 00 00 04 00 00 00 00 00 00 00"), Payload(request));
	const auto requestCopy = RoundTrip(request);
	EXPECT_EQ(requestCopy.context, 1);
	EXPECT_EQ(requestCopy.objectID, 2);
	EXPECT_EQ(requestCopy.requestorID, 3);
	EXPECT_EQ(requestCopy.ugID, 4);
	ExpectTruncatedFails(request);

	// Nothing known: ugID, objectID, requestorID, context, then the two flags
	GameMessages::FetchModelMetadataResponse none;
	none.ugID = 4;
	EXPECT_EQ(Bits(none), 8 * 28 + 2);

	GameMessages::FetchModelMetadataResponse full;
	full.ugID = 4;
	full.bHasUGData = true;
	full.bHasBPData = true;
	full.ugData.userModelID = 4;
	full.ugData.blueprintID = 5;
	full.ugData.userModelName = u"Car";
	full.ugData.owningPlayerID = 6;
	full.ugData.accountID = 7;
	full.ugData.owningPlayerName = u"Builder";
	full.ugData.userModelBehaviors = { 8, 0, 0, 0, 0 };
	full.bpData.blueprintID = 5;
	full.bpData.modelBoxMins = NiPoint3(-1.6f, 0.0f, -0.8f);
	full.bpData.modelBoxMaxs = NiPoint3(1.6f, 0.5f, 0.8f);
	full.bpData.brickListColonDelim = u"66:";
	full.bpData.numberOfBricks = 1;
	// UG: 2 ids, name (4 + 6), desc (4), 2 ids, owner name (4 + 14), u8 count, 5 ids
	const uint32_t ugBits = 8 * (16 + 10 + 4 + 16 + 18 + 1 + 40);
	// BP: id, timestamp, mod, 2 points, 2 flags, bricks (4 + 6), flag, count
	const uint32_t bpBits = 8 * (8 + 8 + 4 + 24 + 10 + 4) + 3;
	EXPECT_EQ(Bits(full), 8 * 28 + 2 + ugBits + bpBits);
	const auto copy = RoundTrip(full);
	EXPECT_EQ(copy.ugData.userModelName, u"Car");
	EXPECT_EQ(copy.ugData.owningPlayerName, u"Builder");
	EXPECT_EQ(copy.ugData.userModelBehaviors, full.ugData.userModelBehaviors);
	EXPECT_EQ(copy.bpData.brickListColonDelim, u"66:");
	EXPECT_EQ(copy.bpData.modelBoxMaxs, full.bpData.modelBoxMaxs);
	EXPECT_EQ(copy.bpData.numberOfBricks, 1);
	EXPECT_TRUE(copy.bpData.userModelOpt);
	EXPECT_TRUE(copy.bpData.ugcIconReady);
	EXPECT_TRUE(copy.bpData.neverFalseIfPresentInCaps);
	ExpectTruncatedFails(full);
}
