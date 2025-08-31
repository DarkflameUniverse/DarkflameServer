#include "TestCommon.h"
#include "ChatPacketHandler.h"
#include "PlayerContainer.h"
#include "eAddFriendResponseType.h"
#include "eGameMasterLevel.h"
#include "MessageType/Client.h"
#include "MessageType/Chat.h"
#include "LUWString.h"
#include "dCommonVars.h"
#include "GeneralUtils.h"

class ChatPacketHandlerTests : public PacketTestsBase {
protected:
    void SetUp() override {
        PacketTestsBase::SetUp();
    }
    
    PlayerData CreateMockPlayerData(LWOOBJID playerID, const std::string& playerName, const LWOZONEID& zoneID = LWOZONEID()) {
        PlayerData player;
        player.playerID = playerID;
        player.playerName = playerName;
        player.zoneID = zoneID;
        player.worldServerSysAddr = SystemAddress{127, 0, 0, 1, 1001};
        player.gmLevel = static_cast<eGameMasterLevel>(0); // CIVILIAN
        return player;
    }
};

TEST_F(ChatPacketHandlerTests, SendPrivateChatMessageSerialization) {
    // Setup test data
    PlayerData sender = CreateMockPlayerData(12345, "SenderPlayer");
    PlayerData receiver = CreateMockPlayerData(67890, "ReceiverPlayer");
    PlayerData routeTo = receiver; // Route to receiver
    LUWString message(u"Hello World!");
    eChatChannel channel = eChatChannel::PRIVATE_CHAT;
    eChatMessageResponseCode responseCode = eChatMessageResponseCode::SENT;
    
    // Call the function to test
    ChatPacketHandler::SendPrivateChatMessage(sender, receiver, routeTo, message, channel, responseCode);
    
    // Verify the bitstream
    auto* bitStream = GetMostRecentBitStream();
    ASSERT_NE(bitStream, nullptr);
    
    // Verify the WORLD_ROUTE_PACKET header
    VerifyHeader(bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::WORLD_ROUTE_PACKET));
    
    // Read route-to player ID
    LWOOBJID readRouteToPlayerID;
    bitStream->Read(readRouteToPlayerID);
    ASSERT_EQ(readRouteToPlayerID, routeTo.playerID);
    
    // Verify the PRIVATE_CHAT_MESSAGE header
    VerifyHeader(bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::PRIVATE_CHAT_MESSAGE));
    
    // Read the private chat message data according to SendPrivateChatMessage implementation
    LWOOBJID readSenderPlayerID;
    eChatChannel readChannel;
    uint32_t readNotUsed;
    LUWString readSenderName;
    LWOOBJID readSenderPlayerID2;
    uint16_t readSourceID;
    eGameMasterLevel readSenderGMLevel;
    LUWString readReceiverName;
    eGameMasterLevel readReceiverGMLevel;
    eChatMessageResponseCode readResponseCode;
    LUWString readMessage;
    
    bitStream->Read(readSenderPlayerID);
    bitStream->Read(readChannel);
    bitStream->Read(readNotUsed);
    bitStream->Read(readSenderName);
    bitStream->Read(readSenderPlayerID2);
    bitStream->Read(readSourceID);
    bitStream->Read(readSenderGMLevel);
    bitStream->Read(readReceiverName);
    bitStream->Read(readReceiverGMLevel);
    bitStream->Read(readResponseCode);
    bitStream->Read(readMessage);
    
    // Verify all data
    ASSERT_EQ(readSenderPlayerID, sender.playerID);
    ASSERT_EQ(readChannel, channel);
    ASSERT_EQ(readNotUsed, 0u);
    ASSERT_EQ(readSenderName.string, GeneralUtils::ASCIIToUTF16(sender.playerName));
    ASSERT_EQ(readSenderPlayerID2, sender.playerID);
    ASSERT_EQ(readSourceID, 0u);
    ASSERT_EQ(readSenderGMLevel, sender.gmLevel);
    ASSERT_EQ(readReceiverName.string, GeneralUtils::ASCIIToUTF16(receiver.playerName));
    ASSERT_EQ(readReceiverGMLevel, receiver.gmLevel);
    ASSERT_EQ(readResponseCode, responseCode);
    ASSERT_EQ(readMessage.string, message.string);
    ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(ChatPacketHandlerTests, SendFriendUpdateSerialization) {
    // Setup test data
    PlayerData friendData = CreateMockPlayerData(11111, "FriendPlayer");
    PlayerData playerData = CreateMockPlayerData(22222, "UpdatedPlayer", LWOZONEID(1000, 0, 0));
    uint8_t notifyType = 1; // friend logged in
    uint8_t isBestFriend = 0;
    
    // Call the function to test
    ChatPacketHandler::SendFriendUpdate(friendData, playerData, notifyType, isBestFriend);
    
    // Verify the bitstream
    auto* bitStream = GetMostRecentBitStream();
    ASSERT_NE(bitStream, nullptr);
    
    // Verify the WORLD_ROUTE_PACKET header
    VerifyHeader(bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::WORLD_ROUTE_PACKET));
    
    // Read route-to player ID
    LWOOBJID readFriendPlayerID;
    bitStream->Read(readFriendPlayerID);
    ASSERT_EQ(readFriendPlayerID, friendData.playerID);
    
    // Verify the UPDATE_FRIEND_NOTIFY header
    VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::UPDATE_FRIEND_NOTIFY));
    
    // Read the friend update data
    uint8_t readNotifyType;
    LUWString readPlayerName;
    LWOMAPID readMapID;
    LWOINSTANCEID readInstanceID;
    LWOCLONEID readCloneID;
    uint8_t readIsBestFriend;
    uint8_t readIsFTP;
    
    bitStream->Read(readNotifyType);
    bitStream->Read(readPlayerName);
    bitStream->Read(readMapID);
    bitStream->Read(readInstanceID);
    bitStream->Read(readCloneID);
    bitStream->Read(readIsBestFriend);
    bitStream->Read(readIsFTP);
    
    // Verify all data
    ASSERT_EQ(readNotifyType, notifyType);
    ASSERT_EQ(readPlayerName.string, GeneralUtils::ASCIIToUTF16(playerData.playerName));
    ASSERT_EQ(readMapID, playerData.zoneID.GetMapID());
    ASSERT_EQ(readInstanceID, playerData.zoneID.GetInstanceID());
    ASSERT_EQ(readCloneID, 0u); // Clone ID logic in SendFriendUpdate
    ASSERT_EQ(readIsBestFriend, isBestFriend);
    ASSERT_EQ(readIsFTP, 0u);
    ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(ChatPacketHandlerTests, SendFriendRequestSerialization) {
    // Setup test data
    PlayerData receiver = CreateMockPlayerData(33333, "ReceiverPlayer");
    PlayerData sender = CreateMockPlayerData(44444, "SenderPlayer");
    
    // Call the function to test
    ChatPacketHandler::SendFriendRequest(receiver, sender);
    
    // Verify the bitstream
    auto* bitStream = GetMostRecentBitStream();
    ASSERT_NE(bitStream, nullptr);
    
    // Verify the WORLD_ROUTE_PACKET header
    VerifyHeader(bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::WORLD_ROUTE_PACKET));
    
    // Read route-to player ID
    LWOOBJID readReceiverPlayerID;
    bitStream->Read(readReceiverPlayerID);
    ASSERT_EQ(readReceiverPlayerID, receiver.playerID);
    
    // Verify the ADD_FRIEND_REQUEST header
    VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::ADD_FRIEND_REQUEST));
    
    // Read the friend request data
    LUWString readSenderName;
    uint8_t readBFFFlag;
    
    bitStream->Read(readSenderName);
    bitStream->Read(readBFFFlag);
    
    // Verify all data
    ASSERT_EQ(readSenderName.string, GeneralUtils::ASCIIToUTF16(sender.playerName));
    ASSERT_EQ(readBFFFlag, 0u); // BFF flag is unused and always 0
    ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(ChatPacketHandlerTests, SendFriendResponseAcceptedSerialization) {
    // Setup test data
    PlayerData receiver = CreateMockPlayerData(55555, "ReceiverPlayer");
    PlayerData sender = CreateMockPlayerData(66666, "SenderPlayer", LWOZONEID(2000, 1, 0));
    eAddFriendResponseType responseCode = eAddFriendResponseType::ACCEPTED;
    uint8_t isBestFriendsAlready = 0;
    uint8_t isBestFriendRequest = 1;
    
    // Call the function to test
    ChatPacketHandler::SendFriendResponse(receiver, sender, responseCode, isBestFriendsAlready, isBestFriendRequest);
    
    // Verify the bitstream
    auto* bitStream = GetMostRecentBitStream();
    ASSERT_NE(bitStream, nullptr);
    
    // Verify the WORLD_ROUTE_PACKET header
    VerifyHeader(bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::WORLD_ROUTE_PACKET));
    
    // Read route-to player ID
    LWOOBJID readReceiverPlayerID;
    bitStream->Read(readReceiverPlayerID);
    ASSERT_EQ(readReceiverPlayerID, receiver.playerID);
    
    // Verify the ADD_FRIEND_RESPONSE header
    VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::ADD_FRIEND_RESPONSE));
    
    // Read the friend response data
    eAddFriendResponseType readResponseCode;
    uint8_t readOnlineFlag;
    LUWString readSenderName;
    LWOOBJID readSenderPlayerID;
    LWOZONEID readSenderZoneID;
    uint8_t readIsBFF;
    uint8_t readIsFTP;
    
    bitStream->Read(readResponseCode);
    bitStream->Read(readOnlineFlag);
    bitStream->Read(readSenderName);
    bitStream->Read(readSenderPlayerID);
    bitStream->Read(readSenderZoneID);
    bitStream->Read(readIsBFF);
    bitStream->Read(readIsFTP);
    
    // Verify all data
    ASSERT_EQ(readResponseCode, responseCode);
    ASSERT_EQ(readOnlineFlag, 1u); // sender.worldServerSysAddr != UNASSIGNED_SYSTEM_ADDRESS
    ASSERT_EQ(readSenderName.string, GeneralUtils::ASCIIToUTF16(sender.playerName));
    ASSERT_EQ(readSenderPlayerID, sender.playerID);
    ASSERT_EQ(readSenderZoneID.GetMapID(), sender.zoneID.GetMapID());
    ASSERT_EQ(readSenderZoneID.GetInstanceID(), sender.zoneID.GetInstanceID());
    ASSERT_EQ(readSenderZoneID.GetCloneID(), sender.zoneID.GetCloneID());
    ASSERT_EQ(readIsBFF, isBestFriendRequest);
    ASSERT_EQ(readIsFTP, 0u);
    ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(ChatPacketHandlerTests, SendFriendResponseDeclinedSerialization) {
    // Setup test data
    PlayerData receiver = CreateMockPlayerData(77777, "ReceiverPlayer");
    PlayerData sender = CreateMockPlayerData(88888, "SenderPlayer");
    eAddFriendResponseType responseCode = eAddFriendResponseType::DECLINED;
    uint8_t isBestFriendsAlready = 1;
    uint8_t isBestFriendRequest = 0;
    
    // Call the function to test
    ChatPacketHandler::SendFriendResponse(receiver, sender, responseCode, isBestFriendsAlready, isBestFriendRequest);
    
    // Verify the bitstream
    auto* bitStream = GetMostRecentBitStream();
    ASSERT_NE(bitStream, nullptr);
    
    // Verify the WORLD_ROUTE_PACKET header
    VerifyHeader(bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::WORLD_ROUTE_PACKET));
    
    // Read route-to player ID
    LWOOBJID readReceiverPlayerID;
    bitStream->Read(readReceiverPlayerID);
    ASSERT_EQ(readReceiverPlayerID, receiver.playerID);
    
    // Verify the ADD_FRIEND_RESPONSE header
    VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::ADD_FRIEND_RESPONSE));
    
    // Read the friend response data (no extra data for non-accepted responses)
    eAddFriendResponseType readResponseCode;
    uint8_t readBestFriendFlag;
    LUWString readSenderName;
    
    bitStream->Read(readResponseCode);
    bitStream->Read(readBestFriendFlag);
    bitStream->Read(readSenderName);
    
    // Verify all data
    ASSERT_EQ(readResponseCode, responseCode);
    ASSERT_EQ(readBestFriendFlag, isBestFriendsAlready); // For non-accepted responses
    ASSERT_EQ(readSenderName.string, GeneralUtils::ASCIIToUTF16(sender.playerName));
    ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(ChatPacketHandlerTests, SendRemoveFriendSerialization) {
    // Setup test data
    PlayerData receiver = CreateMockPlayerData(99999, "ReceiverPlayer");
    std::string personToRemove = "RemovedFriend";
    bool isSuccessful = true;
    
    // Call the function to test
    ChatPacketHandler::SendRemoveFriend(receiver, personToRemove, isSuccessful);
    
    // Verify the bitstream
    auto* bitStream = GetMostRecentBitStream();
    ASSERT_NE(bitStream, nullptr);
    
    // Verify the WORLD_ROUTE_PACKET header
    VerifyHeader(bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::WORLD_ROUTE_PACKET));
    
    // Read route-to player ID
    LWOOBJID readReceiverPlayerID;
    bitStream->Read(readReceiverPlayerID);
    ASSERT_EQ(readReceiverPlayerID, receiver.playerID);
    
    // Verify the REMOVE_FRIEND_RESPONSE header
    VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::REMOVE_FRIEND_RESPONSE));
    
    // Read the remove friend data
    uint8_t readIsSuccessful;
    LUWString readPersonToRemove;
    
    bitStream->Read(readIsSuccessful);
    bitStream->Read(readPersonToRemove);
    
    // Verify all data
    ASSERT_EQ(readIsSuccessful, static_cast<uint8_t>(isSuccessful));
    ASSERT_EQ(readPersonToRemove.string, GeneralUtils::ASCIIToUTF16(personToRemove));
    ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(ChatPacketHandlerTests, FriendDataSerialization) {
    // Test the FriendData serialization used in friends list responses
    FriendData friendData;
    friendData.isOnline = true;
    friendData.isBestFriend = false;
    friendData.isFTP = false;
    friendData.zoneID = LWOZONEID(1200, 0, 5);
    friendData.friendID = 123456789;
    friendData.friendName = "TestFriend";
    
    RakNet::BitStream bitStream;
    friendData.Serialize(bitStream);
    
    // Read back and verify
    uint8_t readIsOnline;
    uint8_t readIsBestFriend;
    uint8_t readIsFTP;
    uint32_t unknown1;
    uint8_t unknown2;
    LWOMAPID readMapID;
    LWOINSTANCEID readInstanceID;
    LWOCLONEID readCloneID;
    LWOOBJID readFriendID;
    
    bitStream.Read(readIsOnline);
    bitStream.Read(readIsBestFriend);
    bitStream.Read(readIsFTP);
    bitStream.Read(unknown1);
    bitStream.Read(unknown2);
    bitStream.Read(readMapID);
    bitStream.Read(readInstanceID);
    bitStream.Read(readCloneID);
    bitStream.Read(readFriendID);
    
    ASSERT_EQ(readIsOnline, 1u);
    ASSERT_EQ(readIsBestFriend, 0u);
    ASSERT_EQ(readIsFTP, 0u);
    ASSERT_EQ(unknown1, 0u);
    ASSERT_EQ(unknown2, 0u);
    ASSERT_EQ(readMapID, 1200u);
    ASSERT_EQ(readInstanceID, 0u);
    ASSERT_EQ(readCloneID, 5u);
    ASSERT_EQ(readFriendID, 123456789);
    
    // Read the name (33 characters, padded with nulls)
    std::string readName;
    for (uint32_t i = 0; i < 33; ++i) {
        uint16_t nameChar;
        bitStream.Read(nameChar);
        if (nameChar != 0 && i < friendData.friendName.length()) {
            readName += static_cast<char>(nameChar);
        }
    }
    
    // Skip the final padding
    uint32_t finalPadding1;
    uint16_t finalPadding2;
    bitStream.Read(finalPadding1);
    bitStream.Read(finalPadding2);
    
    ASSERT_EQ(readName, "TestFriend");
    ASSERT_EQ(finalPadding1, 0u);
    ASSERT_EQ(finalPadding2, 0u);
    ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

TEST_F(ChatPacketHandlerTests, PrivateChatMessageWithUnicode) {
    // Test private chat with Unicode characters
    PlayerData sender = CreateMockPlayerData(10001, "UnicodePlayer");
    PlayerData receiver = CreateMockPlayerData(10002, "ReceiverPlayer");
    PlayerData routeTo = receiver;
    LUWString message(u"Hello 世界! 🌟"); // Mixed ASCII, Chinese, and emoji
    eChatChannel channel = eChatChannel::PRIVATE_CHAT;
    eChatMessageResponseCode responseCode = eChatMessageResponseCode::RECEIVEDNEWWHISPER;
    
    // Call the function to test
    ChatPacketHandler::SendPrivateChatMessage(sender, receiver, routeTo, message, channel, responseCode);
    
    // Verify the bitstream exists and contains expected Unicode message
    auto* bitStream = GetMostRecentBitStream();
    ASSERT_NE(bitStream, nullptr);
    
    // Skip headers and data to get to the message
    LWOOBJID dummy1;
    eChatChannel dummyChannel;
    uint32_t dummy2;
    LUWString dummyString1;
    LWOOBJID dummy3;
    uint16_t dummy4;
    eGameMasterLevel dummy5;
    LUWString dummyString2;
    eGameMasterLevel dummy6;
    eChatMessageResponseCode dummyResponse;
    LUWString readMessage;
    
    // Skip the first two headers (16 bytes each)
    bitStream->IgnoreBytes(16); // First header + route-to ID
    bitStream->IgnoreBytes(8);  // Second header
    
    bitStream->Read(dummy1);
    bitStream->Read(dummyChannel);
    bitStream->Read(dummy2);
    bitStream->Read(dummyString1);
    bitStream->Read(dummy3);
    bitStream->Read(dummy4);
    bitStream->Read(dummy5);
    bitStream->Read(dummyString2);
    bitStream->Read(dummy6);
    bitStream->Read(dummyResponse);
    bitStream->Read(readMessage);
    
    // Verify Unicode message is preserved
    ASSERT_EQ(readMessage.string, message.string);
    ASSERT_EQ(dummyResponse, responseCode);
}