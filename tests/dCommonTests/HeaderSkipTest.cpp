#include <gtest/gtest.h>

#include "dCommonDependencies.h"
#include "dCommonVars.h"
#include "BitStream.h"

#define PacketUniquePtr std::unique_ptr<Packet>

TEST(dCommonTests, HeaderSkipExcessTest) {
	PacketUniquePtr packet = std::make_unique<Packet>();
	unsigned char headerAndData[] = {0x53, 0x02, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00}; // positive
	packet->data = headerAndData;
	packet->length = sizeof(headerAndData);
	CINSTREAM_SKIP_HEADER;
	ASSERT_EQ(inStream.GetNumberOfUnreadBits(), 64);
	ASSERT_EQ(inStream.GetNumberOfBitsAllocated(), 128);
}

TEST(dCommonTests, HeaderSkipExactDataTest) {
	PacketUniquePtr packet = std::make_unique<Packet>();
	unsigned char header[] = {0x53, 0x02, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00}; // positive
	packet->data = header;
	packet->length = sizeof(header);
	CINSTREAM_SKIP_HEADER;
	ASSERT_EQ(inStream.GetNumberOfUnreadBits(), 0);
	ASSERT_EQ(inStream.GetNumberOfBitsAllocated(), 64);
}

TEST(dCommonTests, HeaderSkipNotEnoughDataTest) {
	PacketUniquePtr packet = std::make_unique<Packet>();
	unsigned char notEnoughData[] = {0x53, 0x02, 0x00, 0x07, 0x00, 0x00}; // negative
	packet->data = notEnoughData;
	packet->length = sizeof(notEnoughData);
	CINSTREAM_SKIP_HEADER;
	ASSERT_EQ(inStream.GetNumberOfUnreadBits(), 0);
	ASSERT_EQ(inStream.GetNumberOfBitsAllocated(), 48);
}
