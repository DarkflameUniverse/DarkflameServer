#ifndef PACKETTESTUTILS_H
#define PACKETTESTUTILS_H

// Helpers for byte-for-byte packet comparisons used while migrating packets / game messages
// to the struct based architecture (see docs/PacketArchitecture.md).

#include "BitStream.h"
#include "GameDependencies.h"

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace PacketTestUtils {
	struct PacketBytes {
		std::vector<uint8_t> bytes;
		uint32_t bits = 0;
	};

	inline PacketBytes FromBitStream(const RakNet::BitStream& bitStream) {
		auto& nonConst = const_cast<RakNet::BitStream&>(bitStream); // GetData is not const in RakNet
		return { { nonConst.GetData(), nonConst.GetData() + nonConst.GetNumberOfBytesUsed() }, static_cast<uint32_t>(nonConst.GetNumberOfBitsUsed()) };
	}

	inline PacketBytes FromCapture(const CapturedPacket& packet) {
		return { packet.bytes, packet.bits };
	}

	inline std::string ToHex(const PacketBytes& packet) {
		std::string out;
		char buf[4];
		for (size_t i = 0; i < packet.bytes.size(); i++) {
			std::snprintf(buf, sizeof(buf), "%02x", packet.bytes[i]);
			if (i != 0) out += ' ';
			out += buf;
		}
		return out + " (" + std::to_string(packet.bits) + " bits)";
	}

	inline PacketBytes FromHex(const std::string& hex, uint32_t bits = 0) {
		PacketBytes packet;
		for (size_t i = 0; i + 1 < hex.size();) {
			if (hex[i] == ' ') { i++; continue; }
			packet.bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
			i += 2;
		}
		packet.bits = bits == 0 ? static_cast<uint32_t>(packet.bytes.size() * 8) : bits;
		return packet;
	}

	// Compares the exact number of bits and every byte, printing both packets as hex on failure.
	inline ::testing::AssertionResult PacketsEqual(const PacketBytes& expected, const PacketBytes& actual) {
		if (expected.bits == actual.bits && expected.bytes == actual.bytes) return ::testing::AssertionSuccess();
		return ::testing::AssertionFailure()
			<< "\n  expected: " << ToHex(expected)
			<< "\n  actual:   " << ToHex(actual);
	}

	// Runs sendFunction against the mock server and returns every packet it sent.
	inline std::vector<CapturedPacket> Capture(const std::function<void()>& sendFunction) {
		auto* server = static_cast<dServerMock*>(Game::server);
		server->ClearSentPackets();
		sendFunction();
		auto packets = server->GetSentPackets();
		server->ClearSentPackets();
		return packets;
	}
}

#define EXPECT_PACKET_EQ(expected, actual) EXPECT_TRUE(PacketTestUtils::PacketsEqual(expected, actual))
#define ASSERT_PACKET_EQ(expected, actual) ASSERT_TRUE(PacketTestUtils::PacketsEqual(expected, actual))

#endif // PACKETTESTUTILS_H
