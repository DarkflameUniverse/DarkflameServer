#include "TestCommon.h"
#include "ChatPackets.h"
#include "ClientPackets.h"
#include "MessageType/Chat.h"
#include "MessageType/Client.h"

class ChatPacketTests : public PacketTestsBase {};

// ===== ChatPackets Tests =====

TEST_F(ChatPacketTests, ChatShowAllRequestSerialization) {
	ShowAllRequest request;
	request.requestor = 123456789;
	request.displayZoneData = true;
	request.displayIndividualPlayers = false;
	
	RakNet::BitStream bitStream;
	request.Serialize(bitStream);
	
	VerifyHeader(&bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::SHOW_ALL));
	
	LWOOBJID readRequestor;
	bool readDisplayZoneData;
	bool readDisplayIndividualPlayers;
	
	bitStream.Read(readRequestor);
	bitStream.Read(readDisplayZoneData);
	bitStream.Read(readDisplayIndividualPlayers);
	
	ASSERT_EQ(readRequestor, 123456789);
	ASSERT_EQ(readDisplayZoneData, true);
	ASSERT_EQ(readDisplayIndividualPlayers, false);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

TEST_F(ChatPacketTests, ChatShowAllRequestRoundTrip) {
	ShowAllRequest original;
	original.requestor = 987654321;
	original.displayZoneData = false;
	original.displayIndividualPlayers = true;
	
	RakNet::BitStream bitStream;
	original.Serialize(bitStream);
	
	// Skip header for deserialization
	uint8_t rakNetPacketId{};
	uint16_t serviceType{};
	uint32_t packetId{};
	uint8_t always0{};
	bitStream.Read(rakNetPacketId);
	bitStream.Read(serviceType);
	bitStream.Read(packetId);
	bitStream.Read(always0);
	
	ShowAllRequest deserialized;
	deserialized.Deserialize(bitStream);
	
	ASSERT_EQ(deserialized.requestor, original.requestor);
	ASSERT_EQ(deserialized.displayZoneData, original.displayZoneData);
	ASSERT_EQ(deserialized.displayIndividualPlayers, original.displayIndividualPlayers);
}

TEST_F(ChatPacketTests, ChatFindPlayerRequestSerialization) {
	FindPlayerRequest request;
	request.requestor = 555555555;
	request.playerName = LUWString(u"TestPlayer123");
	
	RakNet::BitStream bitStream;
	request.Serialize(bitStream);
	
	VerifyHeader(&bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::WHO));
	
	LWOOBJID readRequestor;
	LUWString readPlayerName;
	
	bitStream.Read(readRequestor);
	bitStream.Read(readPlayerName);
	
	ASSERT_EQ(readRequestor, 555555555);
	ASSERT_EQ(readPlayerName.string, u"TestPlayer123");
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

TEST_F(ChatPacketTests, ChatFindPlayerRequestRoundTrip) {
	FindPlayerRequest original;
	original.requestor = 777777777;
	original.playerName = LUWString(u"PlayerToFind");
	
	RakNet::BitStream bitStream;
	original.Serialize(bitStream);
	
	// Skip header for deserialization
	uint8_t rakNetPacketId{};
	uint16_t serviceType{};
	uint32_t packetId{};
	uint8_t always0{};
	bitStream.Read(rakNetPacketId);
	bitStream.Read(serviceType);
	bitStream.Read(packetId);
	bitStream.Read(always0);
	
	FindPlayerRequest deserialized;
	deserialized.Deserialize(bitStream);
	
	ASSERT_EQ(deserialized.requestor, original.requestor);
	ASSERT_EQ(deserialized.playerName.string, original.playerName.string);
}

TEST_F(ChatPacketTests, ChatAnnouncementSerialization) {
	ChatPackets::Announcement announcement;
	announcement.title = "Test Title";
	announcement.message = "Test announcement message";
	
	// Call Send which will add header and serialize
	announcement.Send(UNASSIGNED_SYSTEM_ADDRESS);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::GM_ANNOUNCE));
	
	// Read announcement data (size prefixed strings)
	uint32_t titleSize;
	bitStream->Read(titleSize);
	ASSERT_EQ(titleSize, announcement.title.size());
	
	std::string readTitle(titleSize, '\0');
	bitStream->ReadBits(reinterpret_cast<unsigned char*>(readTitle.data()), BYTES_TO_BITS(titleSize), true);
	
	uint32_t messageSize;
	bitStream->Read(messageSize);
	ASSERT_EQ(messageSize, announcement.message.size());
	
	std::string readMessage(messageSize, '\0');
	bitStream->ReadBits(reinterpret_cast<unsigned char*>(readMessage.data()), BYTES_TO_BITS(messageSize), true);
	
	ASSERT_EQ(readTitle, "Test Title");
	ASSERT_EQ(readMessage, "Test announcement message");
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(ChatPacketTests, ChatAchievementNotifySerialization) {
	ChatPackets::AchievementNotify notify;
	notify.targetPlayerName = LUWString(u"TargetPlayer");
	notify.missionEmailID = 12345;
	notify.earningPlayerID = 987654321;
	notify.earnerName = LUWString(u"EarnerPlayer");
	
	// Call Send which will add header and serialize
	notify.Send(UNASSIGNED_SYSTEM_ADDRESS);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::ACHIEVEMENT_NOTIFY));
	
	// Read achievement data based on actual Serialize implementation
	// Order: packing(13), earnerName, packing(15), missionEmailID, earningPlayerID, targetPlayerName
	uint64_t packing1;
	uint32_t packing2;
	uint8_t packing3;
	LUWString readEarnerName;
	uint64_t packing4;
	uint32_t packing5;
	uint16_t packing6;
	uint8_t packing7;
	uint32_t readMissionEmailID;
	LWOOBJID readEarningPlayerID;
	LUWString readTargetPlayerName;
	
	bitStream->Read(packing1);
	bitStream->Read(packing2);
	bitStream->Read(packing3);
	bitStream->Read(readEarnerName);
	bitStream->Read(packing4);
	bitStream->Read(packing5);
	bitStream->Read(packing6);
	bitStream->Read(packing7);
	bitStream->Read(readMissionEmailID);
	bitStream->Read(readEarningPlayerID);
	bitStream->Read(readTargetPlayerName);
	
	ASSERT_EQ(readEarnerName.string, u"EarnerPlayer");
	ASSERT_EQ(readMissionEmailID, 12345u);
	ASSERT_EQ(readEarningPlayerID, 987654321);
	ASSERT_EQ(readTargetPlayerName.string, u"TargetPlayer");
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(ChatPacketTests, ChatTeamInviteInitialResponseSerialization) {
	ChatPackets::TeamInviteInitialResponse response;
	response.inviteFailedToSend = true;
	response.playerName = LUWString(u"InviteePlayer");
	
	// Call Send which will add header and serialize
	response.Send(UNASSIGNED_SYSTEM_ADDRESS);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::TEAM_INVITE_INITIAL_RESPONSE));
	
	// Read team invite response data  
	uint8_t readInviteFailedToSend;  // Written as uint8_t in actual implementation
	LUWString readPlayerName;
	
	bitStream->Read(readInviteFailedToSend);
	bitStream->Read(readPlayerName);
	
	ASSERT_EQ(readInviteFailedToSend, static_cast<uint8_t>(true));
	ASSERT_EQ(readPlayerName.string, u"InviteePlayer");
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}