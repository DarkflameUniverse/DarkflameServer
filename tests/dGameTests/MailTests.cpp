#include "GameDependencies.h"
#include "Mail.h"
#include "MailInfo.h"
#include "BitStream.h"
#include "Entity.h"
#include "User.h"
#include "Character.h"
#include "dCommonVars.h"
#include "BitStreamUtils.h"

class MailTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

// Test SendRequest packet with MailInfo serialization
TEST_F(MailTests, SendRequestMailInfoSerialization) {
	Mail::SendRequest sendRequest;
	
	// Set up test mail info
	sendRequest.mailInfo.senderUsername = "TestSender";
	sendRequest.mailInfo.recipient = "TestRecipient";
	sendRequest.mailInfo.subject = "Test Subject";
	sendRequest.mailInfo.body = "Test mail body content";
	sendRequest.mailInfo.id = 12345;
	sendRequest.mailInfo.senderId = 678;
	sendRequest.mailInfo.receiverId = 901;
	sendRequest.mailInfo.timeSent = 1640995200;
	sendRequest.mailInfo.wasRead = false;
	sendRequest.mailInfo.languageCode = 1033;
	sendRequest.mailInfo.itemID = 555;
	sendRequest.mailInfo.itemCount = 10;
	sendRequest.mailInfo.itemLOT = 1234;
	sendRequest.mailInfo.itemSubkey = 999;

	// Test that the mail info serializes to a non-empty bitstream
	RakNet::BitStream mailInfoStream;
	sendRequest.mailInfo.Serialize(mailInfoStream);
	
	// Verify the stream contains data
	ASSERT_GT(mailInfoStream.GetNumberOfBytesUsed(), 0);
	
	// Verify all fields are serialized correctly by reading them back
	mailInfoStream.ResetReadPointer();
	
	// Read id (uint64_t)
	uint64_t readId;
	ASSERT_TRUE(mailInfoStream.Read(readId));
	ASSERT_EQ(readId, 12345);
	
	// Read subject (LUWString, 50 chars)
	LUWString readSubject(50);
	ASSERT_TRUE(mailInfoStream.Read(readSubject));
	ASSERT_EQ(readSubject.GetAsString(), "Test Subject");
	
	// Read body (LUWString, 400 chars)
	LUWString readBody(400);
	ASSERT_TRUE(mailInfoStream.Read(readBody));
	ASSERT_EQ(readBody.GetAsString(), "Test mail body content");
	
	// Read sender (LUWString, 32 chars)
	LUWString readSender(32);
	ASSERT_TRUE(mailInfoStream.Read(readSender));
	ASSERT_EQ(readSender.GetAsString(), "TestSender");
	
	// Read packing (uint32_t = 0)
	uint32_t packing1;
	ASSERT_TRUE(mailInfoStream.Read(packing1));
	ASSERT_EQ(packing1, 0);
	
	// Read attachedCurrency (uint64_t = 0)
	uint64_t attachedCurrency;
	ASSERT_TRUE(mailInfoStream.Read(attachedCurrency));
	ASSERT_EQ(attachedCurrency, 0);
	
	// Read itemID (LWOOBJID)
	LWOOBJID readItemID;
	ASSERT_TRUE(mailInfoStream.Read(readItemID));
	ASSERT_EQ(readItemID, 555);
	
	// Read itemLOT (LOT)
	LOT readItemLOT;
	ASSERT_TRUE(mailInfoStream.Read(readItemLOT));
	ASSERT_EQ(readItemLOT, 1234);
	
	// Read packing (uint32_t = 0)
	uint32_t packing2;
	ASSERT_TRUE(mailInfoStream.Read(packing2));
	ASSERT_EQ(packing2, 0);
	
	// Read itemSubkey (LWOOBJID)
	LWOOBJID readItemSubkey;
	ASSERT_TRUE(mailInfoStream.Read(readItemSubkey));
	ASSERT_EQ(readItemSubkey, 999);
	
	// Read itemCount (int16_t)
	int16_t readItemCount;
	ASSERT_TRUE(mailInfoStream.Read(readItemCount));
	ASSERT_EQ(readItemCount, 10);
	
	// Read subject type (uint8_t = 0)
	uint8_t subjectType;
	ASSERT_TRUE(mailInfoStream.Read(subjectType));
	ASSERT_EQ(subjectType, 0);
	
	// Read packing (uint8_t = 0)
	uint8_t packing3;
	ASSERT_TRUE(mailInfoStream.Read(packing3));
	ASSERT_EQ(packing3, 0);
	
	// Read packing (uint32_t = 0)
	uint32_t packing4;
	ASSERT_TRUE(mailInfoStream.Read(packing4));
	ASSERT_EQ(packing4, 0);
	
	// Read expiration date (uint64_t = timeSent)
	uint64_t expirationDate;
	ASSERT_TRUE(mailInfoStream.Read(expirationDate));
	ASSERT_EQ(expirationDate, 1640995200);
	
	// Read send date (uint64_t = timeSent)
	uint64_t sendDate;
	ASSERT_TRUE(mailInfoStream.Read(sendDate));
	ASSERT_EQ(sendDate, 1640995200);
	
	// Read wasRead (uint8_t)
	uint8_t readWasRead;
	ASSERT_TRUE(mailInfoStream.Read(readWasRead));
	ASSERT_EQ(readWasRead, false);
	
	// Read isLocalized (uint8_t = 0)
	uint8_t isLocalized;
	ASSERT_TRUE(mailInfoStream.Read(isLocalized));
	ASSERT_EQ(isLocalized, 0);
	
	// Read language code (uint16_t = 1033)
	uint16_t readLanguageCode;
	ASSERT_TRUE(mailInfoStream.Read(readLanguageCode));
	ASSERT_EQ(readLanguageCode, 1033);
	
	// Read final packing (uint32_t = 0)
	uint32_t finalPacking;
	ASSERT_TRUE(mailInfoStream.Read(finalPacking));
	ASSERT_EQ(finalPacking, 0);
	
	// Verify we've read all the data
	ASSERT_EQ(mailInfoStream.GetNumberOfUnreadBits(), 0);
}

// Test SendResponse packet serialization
TEST_F(MailTests, SendResponseSerialization) {
	Mail::SendResponse sendResponse;
	sendResponse.status = Mail::eSendResponse::Success;

	RakNet::BitStream outStream;
	sendResponse.Serialize(outStream);
	
	// Verify the stream contains data
	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Verify message ID and status are written
	outStream.ResetReadPointer();
	uint32_t messageID;
	uint32_t status;
	ASSERT_TRUE(outStream.Read(messageID));
	ASSERT_TRUE(outStream.Read(status));
	ASSERT_EQ(messageID, static_cast<uint32_t>(Mail::eMessageID::SendResponse));
	ASSERT_EQ(status, static_cast<uint32_t>(Mail::eSendResponse::Success));
}

// Test NotificationResponse packet serialization
TEST_F(MailTests, NotificationResponseSerialization) {
	Mail::NotificationResponse notificationResponse;
	notificationResponse.status = Mail::eNotificationResponse::NewMail;
	notificationResponse.auctionID = 12345;
	notificationResponse.mailCount = 3;

	RakNet::BitStream outStream;
	notificationResponse.Serialize(outStream);
	
	// Verify the stream contains data
	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Verify message ID and fields are written in the correct order
	// Order: messageID, status, unused, unused, auctionID, unused, mailCount, packing
	outStream.ResetReadPointer();
	uint32_t messageID;
	uint32_t status;
	uint64_t unused1, unused2, auctionID, unused3;
	uint32_t mailCount, packing;
	ASSERT_TRUE(outStream.Read(messageID));
	ASSERT_TRUE(outStream.Read(status));
	ASSERT_TRUE(outStream.Read(unused1));
	ASSERT_TRUE(outStream.Read(unused2));
	ASSERT_TRUE(outStream.Read(auctionID));
	ASSERT_TRUE(outStream.Read(unused3));
	ASSERT_TRUE(outStream.Read(mailCount));
	ASSERT_TRUE(outStream.Read(packing));
	ASSERT_EQ(messageID, static_cast<uint32_t>(Mail::eMessageID::NotificationResponse));
	ASSERT_EQ(status, static_cast<uint32_t>(Mail::eNotificationResponse::NewMail));
	ASSERT_EQ(auctionID, 12345);
	ASSERT_EQ(mailCount, 3);
}

// Test DataResponse packet serialization
TEST_F(MailTests, DataResponseSerialization) {
	Mail::DataResponse dataResponse;
	dataResponse.throttled = 0;
	
	// Add some mail info
	MailInfo mail1, mail2;
	mail1.senderUsername = "Sender1";
	mail1.recipient = "Recipient1";
	mail1.subject = "Subject1";
	mail1.body = "Body1";
	mail1.id = 1;
	mail1.senderId = 100;
	mail1.receiverId = 200;
	mail1.timeSent = 1640995200;
	mail1.wasRead = false;
	mail1.languageCode = 1033;
	mail1.itemID = LWOOBJID_EMPTY;
	mail1.itemCount = 0;
	mail1.itemLOT = LOT_NULL;
	mail1.itemSubkey = LWOOBJID_EMPTY;
	
	mail2.senderUsername = "Sender2";
	mail2.recipient = "Recipient2";
	mail2.subject = "Subject2";
	mail2.body = "Body2";
	mail2.id = 2;
	mail2.senderId = 300;
	mail2.receiverId = 400;
	mail2.timeSent = 1640995300;
	mail2.wasRead = true;
	mail2.languageCode = 1033;
	mail2.itemID = 555;
	mail2.itemCount = 10;
	mail2.itemLOT = 1234;
	mail2.itemSubkey = 999;
	
	dataResponse.playerMail.push_back(mail1);
	dataResponse.playerMail.push_back(mail2);

	RakNet::BitStream outStream;
	dataResponse.Serialize(outStream);
	
	// Verify the stream contains data
	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Verify message ID, throttled, and mail count are written
	outStream.ResetReadPointer();
	uint32_t messageID;
	uint32_t throttled;
	uint16_t mailCount;
	ASSERT_TRUE(outStream.Read(messageID));
	ASSERT_TRUE(outStream.Read(throttled));
	ASSERT_TRUE(outStream.Read(mailCount));
	ASSERT_EQ(messageID, static_cast<uint32_t>(Mail::eMessageID::DataResponse));
	ASSERT_EQ(throttled, 0);
	ASSERT_EQ(mailCount, 2);
}

// Test AttachmentCollectRequest packet deserialization
TEST_F(MailTests, AttachmentCollectRequestDeserialize) {
	// This test verifies the deserialization reads data in the correct order
	RakNet::BitStream inStream;
	uint32_t unknown = 123; // Unknown field that comes first
	uint64_t mailID = 12345;
	uint64_t playerID = 67890;
	
	// The Deserialize method reads: unknown, mailID, playerID
	inStream.Write(unknown);
	inStream.Write(mailID);
	inStream.Write(playerID);
	
	inStream.ResetReadPointer();
	Mail::AttachmentCollectRequest request;
	ASSERT_TRUE(request.Deserialize(inStream));
	
	ASSERT_EQ(request.mailID, mailID);
	ASSERT_EQ(request.playerID, playerID);
}

// Test AttachmentCollectResponse packet serialization
TEST_F(MailTests, AttachmentCollectResponseSerialization) {
	Mail::AttachmentCollectResponse response;
	response.status = Mail::eAttachmentCollectResponse::Success;
	response.mailID = 54321;

	RakNet::BitStream outStream;
	response.Serialize(outStream);
	
	// Verify the stream contains data
	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Verify message ID, status, and mail ID are written
	outStream.ResetReadPointer();
	uint32_t messageID;
	uint32_t status;
	uint64_t mailID;
	ASSERT_TRUE(outStream.Read(messageID));
	ASSERT_TRUE(outStream.Read(status));
	ASSERT_TRUE(outStream.Read(mailID));
	ASSERT_EQ(messageID, static_cast<uint32_t>(Mail::eMessageID::AttachmentCollectResponse));
	ASSERT_EQ(status, static_cast<uint32_t>(Mail::eAttachmentCollectResponse::Success));
	ASSERT_EQ(mailID, 54321);
}

// Test DeleteRequest packet deserialization
TEST_F(MailTests, DeleteRequestDeserialize) {
	// This test verifies the deserialization reads data in the correct order
	RakNet::BitStream inStream;
	int32_t unknown = 456; // Unknown field that comes first
	uint64_t mailID = 98765;
	uint64_t playerID = 11111;
	
	// The Deserialize method reads: unknown, mailID, playerID
	inStream.Write(unknown);
	inStream.Write(mailID);
	inStream.Write(playerID);
	
	inStream.ResetReadPointer();
	Mail::DeleteRequest request;
	ASSERT_TRUE(request.Deserialize(inStream));
	
	ASSERT_EQ(request.mailID, mailID);
	ASSERT_EQ(request.playerID, playerID);
}

// Test DeleteResponse packet serialization
TEST_F(MailTests, DeleteResponseSerialization) {
	Mail::DeleteResponse response;
	response.status = Mail::eDeleteResponse::Success;
	response.mailID = 13579;

	RakNet::BitStream outStream;
	response.Serialize(outStream);
	
	// Verify the stream contains data
	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Verify message ID, status, and mail ID are written
	outStream.ResetReadPointer();
	uint32_t messageID;
	uint32_t status;
	uint64_t mailID;
	ASSERT_TRUE(outStream.Read(messageID));
	ASSERT_TRUE(outStream.Read(status));
	ASSERT_TRUE(outStream.Read(mailID));
	ASSERT_EQ(messageID, static_cast<uint32_t>(Mail::eMessageID::DeleteResponse));
	ASSERT_EQ(status, static_cast<uint32_t>(Mail::eDeleteResponse::Success));
	ASSERT_EQ(mailID, 13579);
}

// Test ReadRequest packet deserialization
TEST_F(MailTests, ReadRequestDeserialize) {
	// This test verifies the deserialization reads data in the correct order
	RakNet::BitStream inStream;
	int32_t unknown = 789; // Unknown field that comes first
	uint64_t mailID = 24680;
	
	// The Deserialize method reads: unknown, mailID
	inStream.Write(unknown);
	inStream.Write(mailID);
	
	inStream.ResetReadPointer();
	Mail::ReadRequest request;
	ASSERT_TRUE(request.Deserialize(inStream));
	
	ASSERT_EQ(request.mailID, mailID);
}

// Test ReadResponse packet serialization
TEST_F(MailTests, ReadResponseSerialization) {
	Mail::ReadResponse response;
	response.mailID = 97531;
	response.status = Mail::eReadResponse::Success;

	RakNet::BitStream outStream;
	response.Serialize(outStream);
	
	// Verify the stream contains data
	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Verify message ID, status, and mail ID are written in correct order
	outStream.ResetReadPointer();
	uint32_t messageID;
	uint32_t status;
	uint64_t mailID;
	ASSERT_TRUE(outStream.Read(messageID));
	ASSERT_TRUE(outStream.Read(status));
	ASSERT_TRUE(outStream.Read(mailID));
	ASSERT_EQ(messageID, static_cast<uint32_t>(Mail::eMessageID::ReadResponse));
	ASSERT_EQ(status, static_cast<uint32_t>(Mail::eReadResponse::Success));
	ASSERT_EQ(mailID, 97531);
}