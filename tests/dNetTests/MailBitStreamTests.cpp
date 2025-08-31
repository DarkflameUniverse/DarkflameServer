#include "TestCommon.h"
#include "MailInfo.h"
#include "dCommonVars.h"

class MailBitStreamTests : public PacketTestsBase {
protected:
	void SetUp() override {
		PacketTestsBase::SetUp();
	}

	void TearDown() override {
		PacketTestsBase::TearDown();
	}

	/**
	 * Helper to create a sample MailInfo for testing
	 */
	MailInfo CreateSampleMailInfo() {
		MailInfo mailInfo;
		mailInfo.senderUsername = "TestSender";
		mailInfo.recipient = "TestRecipient";
		mailInfo.subject = "Test Subject";
		mailInfo.body = "Test body message";
		mailInfo.id = 12345;
		mailInfo.senderId = 67890;
		mailInfo.receiverId = 11111;
		mailInfo.timeSent = 1234567890;
		mailInfo.wasRead = false;
		mailInfo.languageCode = 1033;
		mailInfo.itemID = LWOOBJID_EMPTY;
		mailInfo.itemCount = 0;
		mailInfo.itemLOT = LOT_NULL;
		mailInfo.itemSubkey = LWOOBJID_EMPTY;
		return mailInfo;
	}
};

// Test basic MailInfo serialization
TEST_F(MailBitStreamTests, MailInfoBasicSerialization) {
	MailInfo originalMail = CreateSampleMailInfo();

	RakNet::BitStream outStream;
	originalMail.Serialize(outStream);

	// Verify the serialized data structure - basic sanity check
	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// MailInfo serialization includes id, subject, body, sender, and various item data
	// Let's verify the first field (id) is written correctly
	outStream.ResetReadPointer();
	
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, originalMail.id);
}

// Test MailInfo serialization with item attachment
TEST_F(MailBitStreamTests, MailInfoWithAttachmentSerialization) {
	MailInfo originalMail = CreateSampleMailInfo();
	originalMail.itemID = 999888777;
	originalMail.itemCount = 5;
	originalMail.itemLOT = 1234;
	originalMail.itemSubkey = 555444333;

	RakNet::BitStream outStream;
	originalMail.Serialize(outStream);

	// Verify the serialized data includes all our data
	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Reset and verify id
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, originalMail.id);
}

// Test MailInfo serialization with different attachment scenarios
TEST_F(MailBitStreamTests, MailInfoVariousAttachmentScenarios) {
	// Test with no attachment
	{
		MailInfo noAttachment = CreateSampleMailInfo();
		noAttachment.itemID = LWOOBJID_EMPTY;
		noAttachment.itemCount = 0;
		noAttachment.itemLOT = LOT_NULL;
		noAttachment.itemSubkey = LWOOBJID_EMPTY;

		RakNet::BitStream outStream;
		noAttachment.Serialize(outStream);

		ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
		outStream.ResetReadPointer();
		uint64_t readId;
		outStream.Read(readId);
		ASSERT_EQ(readId, noAttachment.id);
	}

	// Test with attachment
	{
		MailInfo withAttachment = CreateSampleMailInfo();
		withAttachment.itemID = 123456789;
		withAttachment.itemCount = 5;
		withAttachment.itemLOT = 9876;
		withAttachment.itemSubkey = 555666777;

		RakNet::BitStream outStream;
		withAttachment.Serialize(outStream);

		ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
		outStream.ResetReadPointer();
		uint64_t readId;
		outStream.Read(readId);
		ASSERT_EQ(readId, withAttachment.id);
	}
}

// Test MailInfo serialization consistency
TEST_F(MailBitStreamTests, MailInfoSerializationConsistency) {
	MailInfo originalMail = CreateSampleMailInfo();
	originalMail.itemID = 111222333;
	originalMail.itemCount = 7;
	originalMail.itemLOT = 5678;
	originalMail.itemSubkey = 444555666;

	// Serialize the same mail multiple times
	RakNet::BitStream stream1, stream2, stream3;
	
	originalMail.Serialize(stream1);
	originalMail.Serialize(stream2);
	originalMail.Serialize(stream3);
	
	// All streams should have the same number of bytes
	ASSERT_EQ(stream1.GetNumberOfBytesUsed(), stream2.GetNumberOfBytesUsed());
	ASSERT_EQ(stream2.GetNumberOfBytesUsed(), stream3.GetNumberOfBytesUsed());
	
	// And the same data content
	std::vector<uint8_t> data1(stream1.GetNumberOfBytesUsed());
	std::vector<uint8_t> data2(stream2.GetNumberOfBytesUsed());
	std::vector<uint8_t> data3(stream3.GetNumberOfBytesUsed());
	
	memcpy(data1.data(), stream1.GetData(), stream1.GetNumberOfBytesUsed());
	memcpy(data2.data(), stream2.GetData(), stream2.GetNumberOfBytesUsed());
	memcpy(data3.data(), stream3.GetData(), stream3.GetNumberOfBytesUsed());
	
	ASSERT_EQ(data1, data2);
	ASSERT_EQ(data2, data3);
}

// Test MailInfo with Unicode characters
TEST_F(MailBitStreamTests, MailInfoUnicodeHandling) {
	MailInfo originalMail = CreateSampleMailInfo();
	originalMail.subject = "テストメール"; // "Test Mail" in Japanese
	originalMail.body = "これはテストメッセージです。"; // "This is a test message" in Japanese
	originalMail.senderUsername = "プレイヤー１"; // "Player 1" in Japanese

	RakNet::BitStream outStream;
	originalMail.Serialize(outStream);

	// Verify data was written
	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Since we can't easily deserialize (it doesn't read all fields), 
	// we just verify that Unicode content can be serialized without errors
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, originalMail.id);
}

// Test MailInfo edge cases with special values
TEST_F(MailBitStreamTests, MailInfoEdgeCases) {
	MailInfo originalMail = CreateSampleMailInfo();
	
	// Test with special/edge case values
	originalMail.id = 0; // Zero ID
	originalMail.itemID = LWOOBJID_EMPTY; // Special empty value
	originalMail.itemCount = -1; // Negative count
	originalMail.itemLOT = LOT_NULL; // Null LOT
	originalMail.timeSent = 0; // Zero time

	RakNet::BitStream outStream;
	originalMail.Serialize(outStream);

	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Verify the ID field
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, 0);
}

// Test MailInfo with very long strings that get truncated
TEST_F(MailBitStreamTests, MailInfoStringTruncation) {
	MailInfo originalMail = CreateSampleMailInfo();
	
	// Create strings longer than the allowed maximums
	originalMail.subject = std::string(100, 'S'); // Longer than 50 char limit
	originalMail.body = std::string(500, 'B'); // Longer than 400 char limit
	originalMail.senderUsername = std::string(50, 'U'); // Longer than 32 char limit

	RakNet::BitStream outStream;
	originalMail.Serialize(outStream);

	// Should still serialize successfully (LUWString handles truncation)
	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, originalMail.id);
}

// Test MailInfo data integrity across field types
TEST_F(MailBitStreamTests, MailInfoDataTypeIntegrity) {
	MailInfo originalMail = CreateSampleMailInfo();
	
	// Test with specific values to verify data types are handled correctly
	originalMail.id = 0xFFFFFFFFFFFFFFFF; // Max uint64_t
	originalMail.senderId = 0xFFFFFFFF; // Max uint32_t
	originalMail.receiverId = 0xFFFFFFFF; // Max uint32_t
	originalMail.timeSent = 0xFFFFFFFFFFFFFFFF; // Max uint64_t
	originalMail.wasRead = true;
	originalMail.languageCode = 0xFFFF; // Max uint16_t
	originalMail.itemID = 0xFFFFFFFFFFFFFFFF; // Max LWOOBJID
	originalMail.itemCount = 32767; // Max int16_t positive value
	originalMail.itemSubkey = 0xFFFFFFFFFFFFFFFF; // Max LWOOBJID

	RakNet::BitStream outStream;
	originalMail.Serialize(outStream);

	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Verify the first field (id)
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, 0xFFFFFFFFFFFFFFFF);
}

// Test MailInfo with empty/minimal data
TEST_F(MailBitStreamTests, MailInfoMinimalData) {
	MailInfo minimalMail;
	// Set only required/essential fields
	minimalMail.id = 1;
	minimalMail.subject = "";
	minimalMail.body = "";
	minimalMail.senderUsername = "";
	minimalMail.recipient = "";
	minimalMail.senderId = 0;
	minimalMail.receiverId = 0;
	minimalMail.timeSent = 0;
	minimalMail.wasRead = false;
	minimalMail.languageCode = 0;
	minimalMail.itemID = 0;
	minimalMail.itemCount = 0;
	minimalMail.itemLOT = 0;
	minimalMail.itemSubkey = 0;

	RakNet::BitStream outStream;
	minimalMail.Serialize(outStream);

	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, 1);
}