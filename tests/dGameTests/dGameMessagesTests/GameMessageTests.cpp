#include "Action.h"
#include "Amf3.h"
#include "AMFDeserialize.h"
#include "GameMessages.h"
#include "GameDependencies.h"

#include <gtest/gtest.h>

// Message includes
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
	std::unique_ptr<AMFArrayValue> ReadArrayFromBitStream(RakNet::BitStream* inStream) {
		AMFDeserialize des;
		AMFBaseValue* readArray = des.Read(inStream).release();
		EXPECT_EQ(readArray->GetValueType(), eAmf::Array);
		return std::unique_ptr<AMFArrayValue>{ static_cast<AMFArrayValue*>(readArray) };
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
	auto msg = ReadArrayFromBitStream(&inStream);
	AddStripMessage addStrip(*msg.get());
	ASSERT_FLOAT_EQ(addStrip.GetPosition().GetX(), 50.65);
	ASSERT_FLOAT_EQ(addStrip.GetPosition().GetY(), 178.05);
	ASSERT_EQ(addStrip.GetActionContext().GetStripId(), 0);
	ASSERT_EQ(static_cast<uint32_t>(addStrip.GetActionContext().GetStateId()), 0);
	ASSERT_EQ(addStrip.GetBehaviorId(), BehaviorMessageBase::DefaultBehaviorId);
	ASSERT_EQ(addStrip.GetActionsToAdd().front().GetType(), "DropImagination");
	ASSERT_EQ(addStrip.GetActionsToAdd().front().GetValueParameterName(), "Amount");
	ASSERT_EQ(addStrip.GetActionsToAdd().front().GetValueParameterString(), "");
	ASSERT_FLOAT_EQ(addStrip.GetActionsToAdd().front().GetValueParameterDouble(), 1.0);
}

TEST_F(GameMessageTests, ControlBehaviorRemoveStrip) {
	auto data = ReadFromFile("removeStrip");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	auto msg = ReadArrayFromBitStream(&inStream);
	RemoveStripMessage removeStrip(*msg.get());
	ASSERT_EQ(static_cast<int32_t>(removeStrip.GetActionContext().GetStripId()), 1);
	ASSERT_EQ(static_cast<int32_t>(removeStrip.GetActionContext().GetStateId()), 0);
	ASSERT_EQ(removeStrip.GetBehaviorId(), BehaviorMessageBase::DefaultBehaviorId);
}

TEST_F(GameMessageTests, ControlBehaviorMergeStrips) {
	auto data = ReadFromFile("mergeStrips");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	auto msg = ReadArrayFromBitStream(&inStream);
	MergeStripsMessage mergeStrips(*msg.get());
	ASSERT_EQ(mergeStrips.GetSourceActionContext().GetStripId(), 2);
	ASSERT_EQ(mergeStrips.GetDestinationActionContext().GetStripId(), 0);
	ASSERT_EQ(static_cast<uint32_t>(mergeStrips.GetSourceActionContext().GetStateId()), 0);
	ASSERT_EQ(static_cast<uint32_t>(mergeStrips.GetDestinationActionContext().GetStateId()), 0);
	ASSERT_EQ(mergeStrips.GetDstActionIndex(), 0);
	ASSERT_EQ(mergeStrips.GetBehaviorId(), BehaviorMessageBase::DefaultBehaviorId);
}

TEST_F(GameMessageTests, ControlBehaviorSplitStrip) {
	auto data = ReadFromFile("splitStrip");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	auto msg = ReadArrayFromBitStream(&inStream);
	SplitStripMessage splitStrip(*msg.get());
	ASSERT_EQ(splitStrip.GetBehaviorId(), BehaviorMessageBase::DefaultBehaviorId);

	ASSERT_FLOAT_EQ(splitStrip.GetPosition().GetX(), 275.65);
	ASSERT_FLOAT_EQ(splitStrip.GetPosition().GetY(), 28.7);
	ASSERT_EQ(splitStrip.GetSourceActionContext().GetStripId(), 0);
	ASSERT_EQ(splitStrip.GetDestinationActionContext().GetStripId(), 2);
	ASSERT_EQ(static_cast<uint32_t>(splitStrip.GetSourceActionContext().GetStateId()), 0);
	ASSERT_EQ(static_cast<uint32_t>(splitStrip.GetDestinationActionContext().GetStateId()), 0);
	ASSERT_EQ(splitStrip.GetSrcActionIndex(), 1);
}

TEST_F(GameMessageTests, ControlBehaviorUpdateStripUI) {
	auto data = ReadFromFile("updateStripUI");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	auto msg = ReadArrayFromBitStream(&inStream);
	UpdateStripUiMessage updateStripUi(*msg.get());
	ASSERT_FLOAT_EQ(updateStripUi.GetPosition().GetX(), 116.65);
	ASSERT_FLOAT_EQ(updateStripUi.GetPosition().GetY(), 35.35);
	ASSERT_EQ(updateStripUi.GetActionContext().GetStripId(), 0);
	ASSERT_EQ(static_cast<uint32_t>(updateStripUi.GetActionContext().GetStateId()), 0);
	ASSERT_EQ(updateStripUi.GetBehaviorId(), BehaviorMessageBase::DefaultBehaviorId);
}

TEST_F(GameMessageTests, ControlBehaviorAddAction) {
	auto data = ReadFromFile("addAction");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	auto msg = ReadArrayFromBitStream(&inStream);
	AddActionMessage addAction{ *msg.get() };
	ASSERT_EQ(addAction.GetActionIndex(), 3);
	ASSERT_EQ(addAction.GetActionContext().GetStripId(), 0);
	ASSERT_EQ(static_cast<uint32_t>(addAction.GetActionContext().GetStateId()), 0);
	ASSERT_EQ(addAction.GetAction().GetType(), "DoDamage");
	ASSERT_EQ(addAction.GetAction().GetValueParameterName(), "");
	ASSERT_EQ(addAction.GetAction().GetValueParameterString(), "");
	ASSERT_EQ(addAction.GetAction().GetValueParameterDouble(), 0.0);
	ASSERT_EQ(addAction.GetBehaviorId(), BehaviorMessageBase::DefaultBehaviorId);
}

TEST_F(GameMessageTests, ControlBehaviorMigrateActions) {
	auto data = ReadFromFile("migrateActions");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	auto msg = ReadArrayFromBitStream(&inStream);
	MigrateActionsMessage migrateActions(*msg.get());
	ASSERT_EQ(migrateActions.GetSrcActionIndex(), 1);
	ASSERT_EQ(migrateActions.GetDstActionIndex(), 2);
	ASSERT_EQ(migrateActions.GetSourceActionContext().GetStripId(), 1);
	ASSERT_EQ(migrateActions.GetDestinationActionContext().GetStripId(), 0);
	ASSERT_EQ(static_cast<uint32_t>(migrateActions.GetSourceActionContext().GetStateId()), 0);
	ASSERT_EQ(static_cast<uint32_t>(migrateActions.GetDestinationActionContext().GetStateId()), 0);
	ASSERT_EQ(migrateActions.GetBehaviorId(), BehaviorMessageBase::DefaultBehaviorId);
}

TEST_F(GameMessageTests, ControlBehaviorRearrangeStrip) {
	auto data = ReadFromFile("rearrangeStrip");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	auto msg = ReadArrayFromBitStream(&inStream);
	RearrangeStripMessage rearrangeStrip(*msg.get());
	ASSERT_EQ(rearrangeStrip.GetSrcActionIndex(), 2);
	ASSERT_EQ(rearrangeStrip.GetDstActionIndex(), 1);
	ASSERT_EQ(rearrangeStrip.GetActionContext().GetStripId(), 0);
	ASSERT_EQ(rearrangeStrip.GetBehaviorId(), BehaviorMessageBase::DefaultBehaviorId);
	ASSERT_EQ(static_cast<uint32_t>(rearrangeStrip.GetActionContext().GetStateId()), 0);
}

TEST_F(GameMessageTests, ControlBehaviorAdd) {
	auto data = ReadFromFile("add");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	auto msg = ReadArrayFromBitStream(&inStream);
	AddMessage add(*msg.get());
	ASSERT_EQ(add.GetBehaviorId(), 10446);
	ASSERT_EQ(add.GetBehaviorIndex(), 0);
}

TEST_F(GameMessageTests, ControlBehaviorRemoveActions) {
	auto data = ReadFromFile("removeActions");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	auto msg = ReadArrayFromBitStream(&inStream);
	RemoveActionsMessage removeActions(*msg.get());
	ASSERT_EQ(removeActions.GetBehaviorId(), -1);
	ASSERT_EQ(removeActions.GetActionIndex(), 1);
	ASSERT_EQ(removeActions.GetActionContext().GetStripId(), 0);
	ASSERT_EQ(static_cast<uint32_t>(removeActions.GetActionContext().GetStateId()), 0);
}

TEST_F(GameMessageTests, ControlBehaviorRename) {
	auto data = ReadFromFile("rename");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	auto msg = ReadArrayFromBitStream(&inStream);
	RenameMessage rename(*msg.get());
	ASSERT_EQ(rename.GetName(), "test");
	ASSERT_EQ(rename.GetBehaviorId(), BehaviorMessageBase::DefaultBehaviorId);
}

TEST_F(GameMessageTests, ControlBehaviorUpdateAction) {
	auto data = ReadFromFile("updateAction");
	RakNet::BitStream inStream((unsigned char*)data.c_str(), data.length(), true);
	auto msg = ReadArrayFromBitStream(&inStream);
	UpdateActionMessage updateAction(*msg.get());
	ASSERT_EQ(updateAction.GetAction().GetType(), "FlyDown");
	ASSERT_EQ(updateAction.GetAction().GetValueParameterName(), "Distance");
	ASSERT_EQ(updateAction.GetAction().GetValueParameterString(), "");
	ASSERT_EQ(updateAction.GetAction().GetValueParameterDouble(), 50.0);
	ASSERT_EQ(updateAction.GetBehaviorId(), BehaviorMessageBase::DefaultBehaviorId);
	ASSERT_EQ(updateAction.GetActionIndex(), 1);
	ASSERT_EQ(updateAction.GetActionContext().GetStripId(), 0);
	ASSERT_EQ(static_cast<uint32_t>(updateAction.GetActionContext().GetStateId()), 0);
}
