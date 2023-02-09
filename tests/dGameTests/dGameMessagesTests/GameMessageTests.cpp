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
	AddStripMessage addStrip;
	addStrip.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_FLOAT_EQ(addStrip.xPosition, 50.65);
	ASSERT_FLOAT_EQ(addStrip.yPosition, 178.05);
	ASSERT_EQ(addStrip.stripId, 0);
	ASSERT_EQ(static_cast<uint32_t>(addStrip.stateId), 0);
	ASSERT_EQ(addStrip.behaviorId, -1);
	ASSERT_EQ(addStrip.type, "DropImagination");
	ASSERT_EQ(addStrip.valueParameterName, "Amount");
	ASSERT_EQ(addStrip.valueParameterString, "");
	ASSERT_FLOAT_EQ(addStrip.valueParameterDouble, 1.0);
}

TEST_F(GameMessageTests, ControlBehaviorRemoveStrip) {
	auto data = ReadFromFile("removeStrip");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	RemoveStripMessage removeStrip;
	removeStrip.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(static_cast<int32_t>(removeStrip.stripId), 1);
	ASSERT_EQ(static_cast<int32_t>(removeStrip.behaviorState), 0);
	ASSERT_EQ(removeStrip.behaviorId, -1);
}

TEST_F(GameMessageTests, ControlBehaviorMergeStrips) {
	auto data = ReadFromFile("mergeStrips");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	MergeStripsMessage mergeStrips;
	mergeStrips.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(mergeStrips.srcStripID, 2);
	ASSERT_EQ(mergeStrips.dstStripID, 0);
	ASSERT_EQ(static_cast<uint32_t>(mergeStrips.srcStateID), 0);
	ASSERT_EQ(static_cast<uint32_t>(mergeStrips.dstStateID), 0);
	ASSERT_EQ(mergeStrips.dstActionIndex, 0);
	ASSERT_EQ(mergeStrips.behaviorID, -1);
}

TEST_F(GameMessageTests, ControlBehaviorSplitStrip) {
	auto data = ReadFromFile("splitStrip");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	SplitStripMessage splitStrip;
	splitStrip.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(splitStrip.behaviorId, -1);

	ASSERT_FLOAT_EQ(splitStrip.xPosition, 275.65);
	ASSERT_FLOAT_EQ(splitStrip.yPosition, 28.7);
	ASSERT_EQ(splitStrip.srcStripId, 0);
	ASSERT_EQ(splitStrip.dstStripId, 2);
	ASSERT_EQ(static_cast<uint32_t>(splitStrip.srcStateId), 0);
	ASSERT_EQ(static_cast<uint32_t>(splitStrip.dstStateId), 0);
	ASSERT_EQ(splitStrip.srcActionIndex, 1);
}

TEST_F(GameMessageTests, ControlBehaviorUpdateStripUI) {
	auto data = ReadFromFile("updateStripUi");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	UpdateStripUiMessage updateStripUi;
	updateStripUi.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_FLOAT_EQ(updateStripUi.xPosition, 116.65);
	ASSERT_FLOAT_EQ(updateStripUi.yPosition, 35.35);
	ASSERT_EQ(updateStripUi.stripID, 0);
	ASSERT_EQ(static_cast<uint32_t>(updateStripUi.stateID), 0);
	ASSERT_EQ(updateStripUi.behaviorID, -1);
}

TEST_F(GameMessageTests, ControlBehaviorAddAction) {
	auto data = ReadFromFile("addAction");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	AddActionMessage addAction;
	addAction.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(addAction.actionIndex, 3);
	ASSERT_EQ(addAction.stripId, 0);
	ASSERT_EQ(static_cast<uint32_t>(addAction.stateId), 0);
	ASSERT_EQ(addAction.type, "DoDamage");
	ASSERT_EQ(addAction.valueParameterName, "");
	ASSERT_EQ(addAction.valueParameterString, "");
	ASSERT_EQ(addAction.valueParameterDouble, 0.0);
	ASSERT_EQ(addAction.behaviorId, -1);
}

TEST_F(GameMessageTests, ControlBehaviorMigrateActions) {
	auto data = ReadFromFile("migrateActions");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	MigrateActionsMessage migrateActions;
	migrateActions.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(migrateActions.srcActionIndex, 1);
	ASSERT_EQ(migrateActions.dstActionIndex, 2);
	ASSERT_EQ(migrateActions.srcStripID, 1);
	ASSERT_EQ(migrateActions.dstStripID, 0);
	ASSERT_EQ(static_cast<uint32_t>(migrateActions.srcStateID), 0);
	ASSERT_EQ(static_cast<uint32_t>(migrateActions.dstStateID), 0);
	ASSERT_EQ(migrateActions.behaviorID, -1);
}

TEST_F(GameMessageTests, ControlBehaviorRearrangeStrip) {
	auto data = ReadFromFile("rearrangeStrip");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	RearrangeStripMessage rearrangeStrip;
	rearrangeStrip.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(rearrangeStrip.srcActionIndex, 2);
	ASSERT_EQ(rearrangeStrip.dstActionIndex, 1);
	ASSERT_EQ(rearrangeStrip.stripID, 0);
	ASSERT_EQ(rearrangeStrip.behaviorID, -1);
	ASSERT_EQ(static_cast<uint32_t>(rearrangeStrip.stateID), 0);
}

TEST_F(GameMessageTests, ControlBehaviorAdd) {
	auto data = ReadFromFile("add");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	AddMessage add;
	add.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(add.behaviorId, 10446);
	ASSERT_EQ(add.behaviorIndex, 0);
}

TEST_F(GameMessageTests, ControlBehaviorRemoveActions) {
	auto data = ReadFromFile("removeActions");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	RemoveActionsMessage removeActions;
	removeActions.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(removeActions.behaviorID, -1);
	ASSERT_EQ(removeActions.actionIndex, 1);
	ASSERT_EQ(removeActions.stripID, 0);
	ASSERT_EQ(static_cast<uint32_t>(removeActions.stateID), 0);
}

TEST_F(GameMessageTests, ControlBehaviorRename) {
	auto data = ReadFromFile("rename");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	RenameMessage rename;
	rename.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(rename.name, "test");
	ASSERT_EQ(rename.behaviorID, -1);
}

TEST_F(GameMessageTests, ControlBehaviorUpdateAction) {
	auto data = ReadFromFile("updateAction");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	UpdateActionMessage updateAction;
	updateAction.Parse(ReadArrayFromBitStream(&inStream));
	ASSERT_EQ(updateAction.type, "FlyDown");
	ASSERT_EQ(updateAction.valueParameterName, "Distance");
	ASSERT_EQ(updateAction.valueParameterString, "");
	ASSERT_EQ(updateAction.valueParameterDouble, 50.0);
	ASSERT_EQ(updateAction.behaviorID, -1);
	ASSERT_EQ(updateAction.actionIndex, 1);
	ASSERT_EQ(updateAction.stripID, 0);
	ASSERT_EQ(static_cast<uint32_t>(updateAction.stateID), 0);
}
