#include "GameMessages.h"
#include "GameDependencies.h"
#include "AMFDeserialize.h"

#include "AddActionMessage.h"
#include "AddStripMessage.h"
#include "AddMessage.h"
#include "MigrateActionsMessage.h"
#include "MoveToInventoryMessage.h"
#include "MergeStripsMessage.h"
#include "RearrangeStripMessage.h"
#include "RemoveActionsMessage.h"
#include "RemoveStripMessage.h"
#include "RenameMessage.h"
#include "SplitStripMessage.h"
#include "UpdateActionMessage.h"
#include "UpdateStripUiMessage.h"

#include <gtest/gtest.h>
#include <fstream>

class GameMessageTests: public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
	}
	void TearDown() override {
		TearDownDependencies();
	}

	std::string ReadFromFile(std::string filename) {
		std::ifstream file(filename, std::ios::binary);
		std::string readFile;
		while (file.good()) {
			char readCharacter = file.get();
			readFile.push_back(readCharacter);
		}

		return readFile;
	}

	AMFArrayValue* ReadArrayFromBitStream(RakNet::BitStream* inStream) {
		AMFDeserialize des;
		AMFValue* readArray = des.Read(inStream);
		EXPECT_EQ(readArray->GetValueType(), AMFValueType::AMFArray);
		return static_cast<AMFArrayValue*>(readArray);
	}
};

/**
 * @brief Tests that the serialization struct BlueprintLoadItemResponse is serialized correctly
 *
 */
TEST_F(GameMessageTests, SendBlueprintLoadItemResponse) {
	GameMessages::SendBlueprintLoadItemResponse(UNASSIGNED_SYSTEM_ADDRESS, true, 515, 990);
	auto* bitStream = static_cast<dServerMock*>(Game::server)->GetMostRecentBitStream();
	ASSERT_NE(bitStream, nullptr);
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 200);
	// First read in the packets' header
	uint8_t rakNetPacketId{};
	uint16_t remoteConnectionType{};
	uint32_t packetId{};
	uint8_t always0{};

	bitStream->Read(rakNetPacketId);
	bitStream->Read(remoteConnectionType);
	bitStream->Read(packetId);
	bitStream->Read(always0);
	ASSERT_EQ(rakNetPacketId, 0x53);
	ASSERT_EQ(remoteConnectionType, 0x05);
	ASSERT_EQ(packetId, 0x17);
	ASSERT_EQ(always0, 0x00);

	// Next read in packet data

	uint8_t bSuccess{}; // unsigned bool
	LWOOBJID previousId{};
	LWOOBJID newId{};
	bitStream->Read(bSuccess);
	bitStream->Read(previousId);
	bitStream->Read(newId);
	ASSERT_EQ(bSuccess, static_cast<uint8_t>(true));
	ASSERT_EQ(previousId, 515);
	ASSERT_EQ(newId, 990);

	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(GameMessageTests, ControlBehaviorAddStrip) {
	auto data = ReadFromFile("addStrip");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	AddStripMessage addStrip(ReadArrayFromBitStream(&inStream));
	ASSERT_FLOAT_EQ(addStrip.GetXPosition(), 50.65);
	ASSERT_FLOAT_EQ(addStrip.GetYPosition(), 178.05);
	ASSERT_EQ(addStrip.GetStripId(), 0);
	ASSERT_EQ(static_cast<uint32_t>(addStrip.GetStateId()), 0);
	ASSERT_EQ(addStrip.GetBehaviorId(), -1);
	ASSERT_EQ(addStrip.GetType(), "DropImagination");
	ASSERT_EQ(addStrip.GetValueParameterName(), "Amount");
	ASSERT_EQ(addStrip.GetValueParameterString(), "");
	ASSERT_FLOAT_EQ(addStrip.GetValueParameterDouble(), 1.0);
}

TEST_F(GameMessageTests, ControlBehaviorRemoveStrip) {
	auto data = ReadFromFile("removeStrip");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	RemoveStripMessage removeStrip(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(static_cast<int32_t>(removeStrip.GetStripId()), 1);
	ASSERT_EQ(static_cast<int32_t>(removeStrip.GetBehaviorState()), 0);
	ASSERT_EQ(removeStrip.GetBehaviorId(), -1);
}

TEST_F(GameMessageTests, ControlBehaviorMergeStrips) {
	auto data = ReadFromFile("mergeStrips");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	MergeStripsMessage mergeStrips(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(mergeStrips.GetSrcStripID(), 2);
	ASSERT_EQ(mergeStrips.GetDstStripID(), 0);
	ASSERT_EQ(static_cast<uint32_t>(mergeStrips.GetSrcStateID()), 0);
	ASSERT_EQ(static_cast<uint32_t>(mergeStrips.GetDstStateID()), 0);
	ASSERT_EQ(mergeStrips.GetDstActionIndex(), 0);
	ASSERT_EQ(mergeStrips.GetBehaviorID(), -1);
}

TEST_F(GameMessageTests, ControlBehaviorSplitStrip) {
	auto data = ReadFromFile("splitStrip");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	SplitStripMessage splitStrip(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(splitStrip.GetBehaviorId(), -1);

	ASSERT_FLOAT_EQ(splitStrip.GetXPosition(), 275.65);
	ASSERT_FLOAT_EQ(splitStrip.GetYPosition(), 28.7);
	ASSERT_EQ(splitStrip.GetSrcStripId(), 0);
	ASSERT_EQ(splitStrip.GetDstStripId(), 2);
	ASSERT_EQ(static_cast<uint32_t>(splitStrip.GetSrcStateId()), 0);
	ASSERT_EQ(static_cast<uint32_t>(splitStrip.GetDstStateId()), 0);
	ASSERT_EQ(splitStrip.GetSrcActionIndex(), 1);
}

TEST_F(GameMessageTests, ControlBehaviorUpdateStripUI) {
	auto data = ReadFromFile("updateStripUI");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	UpdateStripUiMessage updateStripUi(ReadArrayFromBitStream(&inStream));
	ASSERT_FLOAT_EQ(updateStripUi.GetXPosition(), 116.65);
	ASSERT_FLOAT_EQ(updateStripUi.GetYPosition(), 35.35);
	ASSERT_EQ(updateStripUi.GetStripID(), 0);
	ASSERT_EQ(static_cast<uint32_t>(updateStripUi.GetStateID()), 0);
	ASSERT_EQ(updateStripUi.GetBehaviorID(), -1);
}

TEST_F(GameMessageTests, ControlBehaviorAddAction) {
	auto data = ReadFromFile("addAction");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	AddActionMessage addAction(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(addAction.GetActionIndex(), 3);
	ASSERT_EQ(addAction.GetStripId(), 0);
	ASSERT_EQ(static_cast<uint32_t>(addAction.GetStateId()), 0);
	ASSERT_EQ(addAction.GetType(), "DoDamage");
	ASSERT_EQ(addAction.GetValueParameterName(), "");
	ASSERT_EQ(addAction.GetValueParameterString(), "");
	ASSERT_EQ(addAction.GetValueParameterDouble(), 0.0);
	ASSERT_EQ(addAction.GetBehaviorId(), -1);
}

TEST_F(GameMessageTests, ControlBehaviorMigrateActions) {
	auto data = ReadFromFile("migrateActions");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	MigrateActionsMessage migrateActions(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(migrateActions.GetSrcActionIndex(), 1);
	ASSERT_EQ(migrateActions.GetDstActionIndex(), 2);
	ASSERT_EQ(migrateActions.GetSrcStripID(), 1);
	ASSERT_EQ(migrateActions.GetDstStripID(), 0);
	ASSERT_EQ(static_cast<uint32_t>(migrateActions.GetSrcStateID()), 0);
	ASSERT_EQ(static_cast<uint32_t>(migrateActions.GetDstStateID()), 0);
	ASSERT_EQ(migrateActions.GetBehaviorID(), -1);
}

TEST_F(GameMessageTests, ControlBehaviorRearrangeStrip) {
	auto data = ReadFromFile("rearrangeStrip");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	RearrangeStripMessage rearrangeStrip(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(rearrangeStrip.GetSrcActionIndex(), 2);
	ASSERT_EQ(rearrangeStrip.GetDstActionIndex(), 1);
	ASSERT_EQ(rearrangeStrip.GetStripID(), 0);
	ASSERT_EQ(rearrangeStrip.GetBehaviorID(), -1);
	ASSERT_EQ(static_cast<uint32_t>(rearrangeStrip.GetStateID()), 0);
}

TEST_F(GameMessageTests, ControlBehaviorAdd) {
	auto data = ReadFromFile("add");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	AddMessage add(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(add.GetBehaviorId(), 10446);
	ASSERT_EQ(add.GetBehaviorIndex(), 0);
}

TEST_F(GameMessageTests, ControlBehaviorRemoveActions) {
	auto data = ReadFromFile("removeActions");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	RemoveActionsMessage removeActions(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(removeActions.GetBehaviorID(), -1);
	ASSERT_EQ(removeActions.GetActionIndex(), 1);
	ASSERT_EQ(removeActions.GetStripID(), 0);
	ASSERT_EQ(static_cast<uint32_t>(removeActions.GetStateID()), 0);
}

TEST_F(GameMessageTests, ControlBehaviorRename) {
	auto data = ReadFromFile("rename");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	RenameMessage rename(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(rename.GetName(), "test");
	ASSERT_EQ(rename.GetBehaviorID(), -1);
}

TEST_F(GameMessageTests, ControlBehaviorUpdateAction) {
	auto data = ReadFromFile("updateAction");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	UpdateActionMessage updateAction(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(updateAction.GetType(), "FlyDown");
	ASSERT_EQ(updateAction.GetValueParameterName(), "Distance");
	ASSERT_EQ(updateAction.GetValueParameterString(), "");
	ASSERT_EQ(updateAction.GetValueParameterDouble(), 50.0);
	ASSERT_EQ(updateAction.GetBehaviorID(), -1);
	ASSERT_EQ(updateAction.GetActionIndex(), 1);
	ASSERT_EQ(updateAction.GetStripID(), 0);
	ASSERT_EQ(static_cast<uint32_t>(updateAction.GetStateID()), 0);
}
