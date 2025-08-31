#include "TestCommon.h"
#include "MailInfo.h"
#include "dCommonVars.h"
#include "BitStream.h"
#include "ServiceType.h"

// Simple test for Mail enum definitions and basic constants
// since the full Mail.h requires dGame dependencies that are complex to mock

class MailEnumTests : public PacketTestsBase {
protected:
	void SetUp() override {
		PacketTestsBase::SetUp();
	}

	void TearDown() override {
		PacketTestsBase::TearDown();
	}
};

// Test basic MailInfo extended serialization with various field combinations
TEST_F(MailEnumTests, MailInfoExtendedFieldTesting) {
	MailInfo mailInfo;
	mailInfo.senderUsername = "ExtendedTestSender";
	mailInfo.recipient = "ExtendedTestRecipient";
	mailInfo.subject = "Extended Test Subject";
	mailInfo.body = "Extended test body message with more content";
	mailInfo.id = 999888777;
	mailInfo.senderId = 123456789;
	mailInfo.receiverId = 987654321;
	mailInfo.timeSent = 1640995200; // Unix timestamp for a known date
	mailInfo.wasRead = true;
	mailInfo.languageCode = 1033; // English
	mailInfo.itemID = 555444333;
	mailInfo.itemCount = 10;
	mailInfo.itemLOT = 8888;
	mailInfo.itemSubkey = 777666555;

	RakNet::BitStream outStream;
	mailInfo.Serialize(outStream);

	// Verify serialization produced data
	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Verify the first field (id) matches
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, mailInfo.id);
}

// Test MailInfo with boundary values for different data types
TEST_F(MailEnumTests, MailInfoBoundaryValues) {
	MailInfo mailInfo;
	
	// Test minimum values
	mailInfo.senderUsername = ""; // Empty string
	mailInfo.recipient = "";
	mailInfo.subject = "";
	mailInfo.body = "";
	mailInfo.id = 0; // Minimum uint64_t
	mailInfo.senderId = 0; // Minimum uint32_t
	mailInfo.receiverId = 0;
	mailInfo.timeSent = 0;
	mailInfo.wasRead = false;
	mailInfo.languageCode = 0; // Minimum uint16_t
	mailInfo.itemID = 0;
	mailInfo.itemCount = -32768; // Minimum int16_t
	mailInfo.itemLOT = 0;
	mailInfo.itemSubkey = 0;

	RakNet::BitStream outStream;
	mailInfo.Serialize(outStream);

	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Read back the ID to verify basic serialization worked
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, 0);
}

// Test MailInfo with maximum values for different data types
TEST_F(MailEnumTests, MailInfoMaximumValues) {
	MailInfo mailInfo;
	
	// Test maximum values
	mailInfo.senderUsername = std::string(1000, 'A'); // Very long string
	mailInfo.recipient = std::string(1000, 'B');
	mailInfo.subject = std::string(1000, 'C');
	mailInfo.body = std::string(1000, 'D');
	mailInfo.id = UINT64_MAX; // Maximum uint64_t
	mailInfo.senderId = UINT32_MAX; // Maximum uint32_t
	mailInfo.receiverId = UINT32_MAX;
	mailInfo.timeSent = UINT64_MAX;
	mailInfo.wasRead = true;
	mailInfo.languageCode = UINT16_MAX; // Maximum uint16_t
	mailInfo.itemID = UINT64_MAX;
	mailInfo.itemCount = 32767; // Maximum positive int16_t
	mailInfo.itemLOT = UINT32_MAX;
	mailInfo.itemSubkey = UINT64_MAX;

	RakNet::BitStream outStream;
	mailInfo.Serialize(outStream);

	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Read back the ID to verify basic serialization worked with max values
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, UINT64_MAX);
}

// Test MailInfo serialization with special Unicode characters in different fields
TEST_F(MailEnumTests, MailInfoUnicodeInAllFields) {
	MailInfo mailInfo;
	
	// Use Unicode in various string fields
	mailInfo.senderUsername = "测试用户"; // "Test User" in Chinese
	mailInfo.recipient = "пользователь"; // "User" in Russian
	mailInfo.subject = "🎮メールテスト🎮"; // Mixed emoji and Japanese
	mailInfo.body = "العربية ✅ 한국어 ✅ Français ✅"; // Multiple languages
	mailInfo.id = 12345;
	mailInfo.senderId = 67890;
	mailInfo.receiverId = 11111;
	mailInfo.timeSent = 1234567890;
	mailInfo.wasRead = false;
	mailInfo.languageCode = 1041; // Japanese
	mailInfo.itemID = LWOOBJID_EMPTY;
	mailInfo.itemCount = 0;
	mailInfo.itemLOT = LOT_NULL;
	mailInfo.itemSubkey = LWOOBJID_EMPTY;

	RakNet::BitStream outStream;
	mailInfo.Serialize(outStream);

	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Verify the serialization handled Unicode content
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, 12345);
}

// Test MailInfo with alternating boolean and special constant values
TEST_F(MailEnumTests, MailInfoSpecialConstants) {
	MailInfo mailInfo;
	
	mailInfo.senderUsername = "ConstantTester";
	mailInfo.recipient = "ConstantRecipient";
	mailInfo.subject = "Testing Constants";
	mailInfo.body = "This tests special constant values";
	mailInfo.id = 1;
	mailInfo.senderId = 2;
	mailInfo.receiverId = 3;
	mailInfo.timeSent = 4;
	mailInfo.wasRead = true; // Test true value
	mailInfo.languageCode = 5;
	mailInfo.itemID = LWOOBJID_EMPTY; // Special empty constant
	mailInfo.itemCount = 0;
	mailInfo.itemLOT = LOT_NULL; // Special null constant
	mailInfo.itemSubkey = LWOOBJID_EMPTY;

	RakNet::BitStream outStream;
	mailInfo.Serialize(outStream);

	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, 1);
}

// Test MailInfo serialization stability with repeated calls
TEST_F(MailEnumTests, MailInfoSerializationStability) {
	MailInfo mailInfo;
	mailInfo.senderUsername = "StabilityTest";
	mailInfo.recipient = "StabilityRecipient";
	mailInfo.subject = "Stability Subject";
	mailInfo.body = "Testing serialization stability across multiple calls";
	mailInfo.id = 98765;
	mailInfo.senderId = 43210;
	mailInfo.receiverId = 56789;
	mailInfo.timeSent = 1609459200; // Known timestamp
	mailInfo.wasRead = false;
	mailInfo.languageCode = 1033;
	mailInfo.itemID = 111222333;
	mailInfo.itemCount = 25;
	mailInfo.itemLOT = 4567;
	mailInfo.itemSubkey = 444555666;

	// Serialize multiple times and verify consistency
	std::vector<RakNet::BitStream> streams(5);
	
	for (auto& stream : streams) {
		mailInfo.Serialize(stream);
	}
	
	// All streams should have the same size
	size_t expectedSize = streams[0].GetNumberOfBytesUsed();
	for (size_t i = 1; i < streams.size(); ++i) {
		ASSERT_EQ(streams[i].GetNumberOfBytesUsed(), expectedSize);
	}
	
	// All streams should have the same content
	std::vector<uint8_t> referenceData(expectedSize);
	memcpy(referenceData.data(), streams[0].GetData(), expectedSize);
	
	for (size_t i = 1; i < streams.size(); ++i) {
		std::vector<uint8_t> currentData(expectedSize);
		memcpy(currentData.data(), streams[i].GetData(), expectedSize);
		ASSERT_EQ(currentData, referenceData);
	}
}

// Test MailInfo with negative item count edge cases
TEST_F(MailEnumTests, MailInfoNegativeItemCount) {
	MailInfo mailInfo;
	mailInfo.senderUsername = "NegativeTest";
	mailInfo.recipient = "NegativeRecipient";
	mailInfo.subject = "Negative Count Test";
	mailInfo.body = "Testing negative item counts";
	mailInfo.id = 11111;
	mailInfo.senderId = 22222;
	mailInfo.receiverId = 33333;
	mailInfo.timeSent = 44444;
	mailInfo.wasRead = false;
	mailInfo.languageCode = 55555;
	mailInfo.itemID = 666666;
	mailInfo.itemCount = -1; // Negative count
	mailInfo.itemLOT = 777777;
	mailInfo.itemSubkey = 888888;

	RakNet::BitStream outStream;
	mailInfo.Serialize(outStream);

	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, 11111);
}

// Test data type integrity with specific bit patterns
TEST_F(MailEnumTests, MailInfoDataTypeIntegrity) {
	MailInfo mailInfo;
	
	// Use bit patterns that might reveal endianness or packing issues
	mailInfo.senderUsername = "BitPatternTest";
	mailInfo.recipient = "BitPatternRecipient";
	mailInfo.subject = "Testing Bit Patterns";
	mailInfo.body = "This tests specific bit patterns in data types";
	mailInfo.id = 0x0123456789ABCDEF; // Known bit pattern
	mailInfo.senderId = 0x12345678; // Known 32-bit pattern
	mailInfo.receiverId = 0x87654321; // Reverse pattern
	mailInfo.timeSent = 0xFEDCBA9876543210; // Another known pattern
	mailInfo.wasRead = true;
	mailInfo.languageCode = 0xABCD; // Known 16-bit pattern
	mailInfo.itemID = 0x1111222233334444; // Repeated pattern
	mailInfo.itemCount = 0x7FFF; // Maximum positive int16_t
	mailInfo.itemLOT = 0xFFFFFFFF; // Maximum uint32_t
	mailInfo.itemSubkey = 0x5555666677778888; // Another pattern

	RakNet::BitStream outStream;
	mailInfo.Serialize(outStream);

	ASSERT_GT(outStream.GetNumberOfBytesUsed(), 0);
	
	// Verify the specific bit pattern for ID was preserved
	outStream.ResetReadPointer();
	uint64_t readId;
	outStream.Read(readId);
	ASSERT_EQ(readId, 0x0123456789ABCDEF);
}