#include "PetMessages.h"
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"
#include "Legacy/PetMessagesLegacy.h"

#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	const std::vector<int32_t> g_Ints = { 0, 1, -1, 1727, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };
	const std::vector<std::u16string> g_Strings = { u"", u"Rex", u"Pét ☃ \U0001F436", std::u16string(300, u'x') };
	const std::vector<NiPoint3> g_Points = { NiPoint3Constant::ZERO, NiPoint3(1.5f, -2.0f, 1e6f) };
	const std::vector<NiQuaternion> g_Rotations = { QuatUtils::IDENTITY, NiQuaternion(0.0f, 0.0f, 0.0f, 0.0f), NiQuaternion(0.5f, 0.5f, -0.5f, 0.5f) };
	const std::vector<std::vector<Brick>> g_BrickSets = { {}, { { 3001, 21 } }, { { 1, 2 }, { 0xFFFFFFFF, 0 }, { 3040, 194 } } };

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

	bool SameBricks(const std::vector<Brick>& a, const std::vector<Brick>& b) {
		if (a.size() != b.size()) return false;
		for (size_t i = 0; i < a.size(); i++) {
			if (a[i].designerID != b[i].designerID || a[i].materialID != b[i].materialID) return false;
		}
		return true;
	}
}

class PetMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(PetMessagesTests, NotifyPetTamingMinigameMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto id : g_Targets) {
			for (const bool force : { false, true }) {
				for (const auto type : { ePetTamingNotifyType::SUCCESS, ePetTamingNotifyType::BEGIN, ePetTamingNotifyType::NAMINGPET, static_cast<ePetTamingNotifyType>(0xFFFFFFFF) }) {
					for (const auto& point : g_Points) {
						for (const auto& rotation : g_Rotations) {
							GameMessages::NotifyPetTamingMinigame msg;
							msg.target = target;
							msg.PetID = id;
							msg.PlayerTamingID = target;
							msg.bForceTeleport = force;
							msg.notifyType = type;
							msg.petsDestPos = point;
							msg.telePos = NiPoint3(point.z, point.x, point.y);
							msg.teleRot = rotation;
							ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendNotifyPetTamingMinigame(target, id, target, force, type, point, msg.telePos, rotation, a); }, msg);
							const auto copy = RoundTrip(msg);
							EXPECT_EQ(copy.PetID, id);
							EXPECT_EQ(copy.notifyType, type);
							EXPECT_EQ(copy.teleRot, rotation);
							ExpectTruncatedFails(msg);
						}
					}
				}
			}
		}
	}
}

TEST_F(PetMessagesTests, NoPayloadMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		GameMessages::NotifyTamingModelLoadedOnServer msg;
		msg.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendNotifyTamingModelLoadedOnServer(target, a); }, msg);
		EXPECT_EQ(Payload(msg).bits, 0);
	}
}

TEST_F(PetMessagesTests, BrickMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const auto& bricks : g_BrickSets) {
			GameMessages::NotifyPetTamingPuzzleSelected msg;
			msg.target = target;
			msg.bricks = bricks;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendNotifyPetTamingPuzzleSelected(target, bricks, a); }, msg);
			EXPECT_TRUE(SameBricks(RoundTrip(msg).bricks, bricks));
			ExpectTruncatedFails(msg);

			for (const bool clientFailed : { false, true }) {
				GameMessages::PetTamingTryBuild tryBuild;
				tryBuild.bricks = bricks;
				tryBuild.clientFailed = clientFailed;
				const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyPetTamingTryBuild>(tryBuild, LegacyGameMessages::ReadPetTamingTryBuild);
				const auto copy = RoundTrip(tryBuild);
				EXPECT_FALSE(legacy.rejected);
				EXPECT_TRUE(SameBricks(copy.bricks, legacy.bricks));
				EXPECT_EQ(copy.clientFailed, legacy.clientFailed);
				ExpectTruncatedFails(tryBuild);
			}
		}
	}

	// A brick count the stream cannot hold is rejected before anything is allocated.
	RakNet::BitStream huge;
	huge.Write<uint32_t>(MAX_MESSAGE_LENGTH + 1);
	GameMessages::PetTamingTryBuild tryBuild;
	EXPECT_FALSE(tryBuild.Deserialize(huge));
	RakNet::BitStream lying;
	lying.Write<uint32_t>(1000);
	lying.Write<uint64_t>(0);
	EXPECT_FALSE(tryBuild.Deserialize(lying));
}

TEST_F(PetMessagesTests, PetTamingTryBuildResultMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const bool success : { false, true }) {
			for (const auto numCorrect : g_Ints) {
				GameMessages::PetTamingTryBuildResult msg;
				msg.target = target;
				msg.bSuccess = success;
				msg.iNumCorrect = numCorrect;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendPetTamingTryBuildResult(target, success, numCorrect, a); }, msg);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.bSuccess, success);
				EXPECT_EQ(copy.iNumCorrect, numCorrect);
			}
		}
	}
}

TEST_F(PetMessagesTests, PetResponseMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto value : g_Ints) {
			GameMessages::PetResponse msg;
			msg.target = target;
			msg.ObjIDPet = target ^ 0x55;
			msg.iPetCommandType = value;
			msg.iResponse = -value;
			msg.iTypeID = value / 2;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendPetResponse(target, msg.ObjIDPet, msg.iPetCommandType, msg.iResponse, msg.iTypeID, a); }, msg);
			const auto copy = RoundTrip(msg);
			EXPECT_EQ(copy.ObjIDPet, msg.ObjIDPet);
			EXPECT_EQ(copy.iResponse, msg.iResponse);
			ExpectTruncatedFails(msg);
		}
	}
}

TEST_F(PetMessagesTests, AddPetToPlayerMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto& name : g_Strings) {
			for (const LOT lot : { LOT_NULL, 3050, 0 }) {
				GameMessages::AddPetToPlayer msg;
				msg.target = target;
				msg.iElementalType = lot / 7;
				msg.name = name;
				msg.petDBID = target;
				msg.petLOT = lot;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendAddPetToPlayer(target, msg.iElementalType, name, target, lot, a); }, msg);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.name, name);
				EXPECT_EQ(copy.petLOT, lot);
				ExpectTruncatedFails(msg);
			}
		}
	}
}

TEST_F(PetMessagesTests, RegisterPetMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const auto id : g_Targets) {
			GameMessages::RegisterPetID petId;
			petId.target = target;
			petId.objID = id;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendRegisterPetID(target, id, a); }, petId);
			EXPECT_EQ(RoundTrip(petId).objID, id);

			GameMessages::RegisterPetDBID dbId;
			dbId.target = target;
			dbId.petDBID = id;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendRegisterPetDBID(target, id, a); }, dbId);
			EXPECT_EQ(RoundTrip(dbId).petDBID, id);
		}
	}
}

TEST_F(PetMessagesTests, BoolMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const bool value : { false, true }) {
			GameMessages::ClientExitTamingMinigame exit;
			exit.target = target;
			exit.bVoluntaryExit = value;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendClientExitTamingMinigame(target, value, a); }, exit);
			EXPECT_EQ(ReadWithLegacy<bool>(exit, LegacyGameMessages::ReadClientExitTamingMinigame), value);
			EXPECT_EQ(RoundTrip(exit).bVoluntaryExit, value);
			ExpectTruncatedFails(exit);

			GameMessages::BouncerActiveStatus bouncer;
			bouncer.target = target;
			bouncer.bActive = value;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendBouncerActiveStatus(target, value, a); }, bouncer);
			EXPECT_EQ(RoundTrip(bouncer).bActive, value);

			GameMessages::DespawnPet despawn;
			despawn.bDeletePet = value;
			EXPECT_EQ(ReadWithLegacy<bool>(despawn, LegacyGameMessages::ReadDespawnPet), value);
			EXPECT_EQ(RoundTrip(despawn).bDeletePet, value);
			ExpectTruncatedFails(despawn);

			for (const auto ability : { ePetAbilityType::Invalid, ePetAbilityType::DigAtPosition, static_cast<ePetAbilityType>(0xFFFFFFFF) }) {
				GameMessages::ShowPetActionButton button;
				button.target = target;
				button.ButtonLabel = ability;
				button.bShow = value;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendShowPetActionButton(target, ability, value, a); }, button);
				const auto copy = RoundTrip(button);
				EXPECT_EQ(copy.ButtonLabel, ability);
				EXPECT_EQ(copy.bShow, value);
			}
		}
	}
}

TEST_F(PetMessagesTests, PetNameMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const auto& name : g_Strings) {
			for (const auto dbId : g_Targets) {
				GameMessages::SetPetName setName;
				setName.target = target;
				setName.name = name;
				setName.petDBID = dbId;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendSetPetName(target, name, dbId, a); }, setName);
				const auto copy = RoundTrip(setName);
				EXPECT_EQ(copy.name, name);
				EXPECT_EQ(copy.petDBID, dbId);
				ExpectTruncatedFails(setName);
			}

			for (const auto status : g_Ints) {
				GameMessages::PetNameChanged changed;
				changed.target = target;
				changed.moderationStatus = status;
				changed.name = name;
				changed.ownerName = name + u"'s owner";
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendPetNameChanged(target, status, name, changed.ownerName, a); }, changed);
				const auto copy = RoundTrip(changed);
				EXPECT_EQ(copy.name, name);
				EXPECT_EQ(copy.ownerName, changed.ownerName);
				ExpectTruncatedFails(changed);
			}

			GameMessages::RequestSetPetName request;
			request.name = name;
			const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyRequestSetPetName>(request, LegacyGameMessages::ReadRequestSetPetName);
			EXPECT_FALSE(legacy.rejected);
			EXPECT_EQ(RoundTrip(request).name, legacy.name);
			ExpectTruncatedFails(request);
		}

		for (const auto dbId : g_Targets) {
			for (const auto status : g_Ints) {
				GameMessages::SetPetNameModerated moderated;
				moderated.target = target;
				moderated.PetDBID = dbId;
				moderated.nModerationStatus = status;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendSetPetNameModerated(target, dbId, status, a); }, moderated);
				const auto copy = RoundTrip(moderated);
				EXPECT_EQ(copy.PetDBID, dbId);
				EXPECT_EQ(copy.nModerationStatus, status);
			}
		}
	}
}

TEST_F(PetMessagesTests, InboundReadsLikeLegacy) {
	for (const auto& point : g_Points) {
		GameMessages::NotifyTamingBuildSuccess success;
		success.buildPosition = point;
		EXPECT_EQ(ReadWithLegacy<NiPoint3>(success, LegacyGameMessages::ReadNotifyTamingBuildSuccess), point);
		EXPECT_EQ(RoundTrip(success).buildPosition, point);
		ExpectTruncatedFails(success);

		for (const auto id : g_Targets) {
			for (const auto value : g_Ints) {
				for (const bool obey : { false, true }) {
					GameMessages::CommandPet command;
					command.GenericPosInfo = point;
					command.ObjIDSource = id;
					command.iPetCommandType = value;
					command.iTypeID = -value;
					command.overrideObey = obey;
					const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyCommandPet>(command, LegacyGameMessages::ReadCommandPet);
					const auto copy = RoundTrip(command);
					EXPECT_EQ(copy.GenericPosInfo, legacy.genericPosInfo);
					EXPECT_EQ(copy.ObjIDSource, legacy.objIdSource);
					EXPECT_EQ(copy.iPetCommandType, legacy.iPetCommandType);
					EXPECT_EQ(copy.iTypeID, legacy.iTypeID);
					EXPECT_EQ(copy.overrideObey, legacy.overrideObey);
					ExpectTruncatedFails(command);
				}
			}
		}
	}
}

// Independent of the legacy code: hand computed payloads (RakNet writes MSB first).
TEST_F(PetMessagesTests, GoldenBytes) {
	GameMessages::PetTamingTryBuildResult result;
	EXPECT_PACKET_EQ(FromHex("80", 2), Payload(result));
	result.bSuccess = false;
	result.iNumCorrect = 3;
	// 0, 1, then 03 00 00 00 shifted right by two bits
	EXPECT_PACKET_EQ(FromHex("40 c0 00 00 00", 34), Payload(result));

	GameMessages::NotifyPetTamingPuzzleSelected puzzle;
	puzzle.bricks = { { 3001, 21 } };
	EXPECT_PACKET_EQ(FromHex("01 00 00 00 b9 0b 00 00 15 00 00 00"), Payload(puzzle));

	GameMessages::SetPetName setName;
	setName.name = u"Hi";
	EXPECT_PACKET_EQ(FromHex("02 00 00 00 48 00 69 00 00", 65), Payload(setName));
	setName.petDBID = 0x11;
	EXPECT_PACKET_EQ(FromHex("02 00 00 00 48 00 69 00 88 80 00 00 00 00 00 00 00", 129), Payload(setName));

	GameMessages::SetPetNameModerated moderated;
	moderated.nModerationStatus = 2;
	EXPECT_PACKET_EQ(FromHex("01 00 00 00 00", 33), Payload(moderated));

	GameMessages::ShowPetActionButton button;
	button.ButtonLabel = ePetAbilityType::JumpOnObject;
	button.bShow = true;
	EXPECT_PACKET_EQ(FromHex("02 00 00 00 80", 33), Payload(button));

	GameMessages::DespawnPet despawn;
	despawn.bDeletePet = true;
	EXPECT_PACKET_EQ(FromHex("80", 1), Payload(despawn));
}
