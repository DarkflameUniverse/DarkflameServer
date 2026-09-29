#ifndef GAMEMESSAGETESTUTILS_H
#define GAMEMESSAGETESTUTILS_H

// Shared helpers for the byte-equality tests of converted game messages (see docs/PacketArchitecture.md).

#include "GameMessages.h"
#include "PacketTestUtils.h"

#include <array>
#include <functional>

#include <gtest/gtest.h>

namespace GameMessageTestUtils {
	using namespace PacketTestUtils;

	inline SystemAddress ClientAddress() {
		SystemAddress address;
		address.binaryAddress = 0x0100007f;
		address.port = 2003;
		return address;
	}

	// Synthetic object IDs: empty, a player-style ID and one with every byte different.
	inline const std::array<LWOOBJID, 3> g_Targets = { LWOOBJID_EMPTY, 0x1000000000000001LL, 0x0102030405060708LL };
	inline const std::array<SystemAddress, 2> g_Addresses = { ClientAddress(), UNASSIGNED_SYSTEM_ADDRESS };

	// The complete packet msg.Send(sysAddr) puts on the wire.
	inline PacketBytes StructPacket(const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream bitStream;
		msg.WritePacket(bitStream);
		return FromBitStream(bitStream);
	}

	// How the struct is sent in a comparison: Send (UNASSIGNED broadcasts; legacy functions that did
	// "if (UNASSIGNED) SEND_PACKET_BROADCAST; SEND_PACKET;"), SendToClient (legacy functions with only SEND_PACKET)
	// or Broadcast (legacy functions with only SEND_PACKET_BROADCAST, whatever address they were given: the struct is
	// sent with Send(UNASSIGNED_SYSTEM_ADDRESS)).
	enum class SendMode { Send, SendToClient, Broadcast };

	// Sends the same message through the frozen legacy function and through the struct, to one client and to
	// UNASSIGNED_SYSTEM_ADDRESS, and requires identical bytes and the same effective destination.
	// legacySend receives the address to send to.
	inline void ExpectSameAsLegacy(const std::function<void(const SystemAddress&)>& legacySend, const GameMessages::NetGameMsg& msg, SendMode mode = SendMode::Send) {
		for (const auto& address : g_Addresses) {
			SCOPED_TRACE(address == UNASSIGNED_SYSTEM_ADDRESS ? "unassigned address" : "single client");
			const auto legacyPackets = Capture([&] { legacySend(address); });
			const auto newPackets = Capture([&] {
				if (mode == SendMode::Send) msg.Send(address);
				else if (mode == SendMode::SendToClient) msg.SendToClient(address);
				else msg.Send(UNASSIGNED_SYSTEM_ADDRESS);
				});

			ASSERT_FALSE(legacyPackets.empty());
			ASSERT_EQ(newPackets.size(), 1);
			// The legacy broadcast path also did a second Send(UNASSIGNED_SYSTEM_ADDRESS, broadcast = false),
			// which RakPeer::Send rejects without sending anything. Every copy must still match byte for byte.
			for (const auto& legacyPacket : legacyPackets) {
				EXPECT_PACKET_EQ(FromCapture(legacyPacket), FromCapture(newPackets[0]));
			}
			EXPECT_EQ(legacyPackets[0].broadcast, newPackets[0].broadcast);
			EXPECT_EQ(legacyPackets[0].sysAddr, newPackets[0].sysAddr);
			EXPECT_PACKET_EQ(FromCapture(newPackets[0]), StructPacket(msg));
		}
	}

	// Serializes msg, reads it back into a fresh T and checks the copy consumed every bit and serializes to the
	// same bytes. Returns the copy so callers can compare fields.
	template<typename T>
	T RoundTrip(const T& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		T copy;
		EXPECT_TRUE(copy.Deserialize(bitStream));
		EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
		RakNet::BitStream again;
		copy.Serialize(again);
		EXPECT_PACKET_EQ(FromBitStream(bitStream), FromBitStream(again));
		return copy;
	}

	// Reads a whole game message packet copied from a live capture (hex), requires its header to carry T's message ID
	// and the payload to be consumed up to the byte padding, and requires the struct to write the same bytes back.
	template<typename T>
	T FromLiveCapture(const std::string& hex) {
		auto live = FromHex(hex);
		RakNet::BitStream bitStream(live.bytes.data(), live.bytes.size(), false);
		T msg;
		LWOOBJID target{};
		MessageType::Game msgId{};
		EXPECT_TRUE(GameMessages::NetGameMsg::ReadPacketHeader(bitStream, target, msgId));
		EXPECT_EQ(msgId, msg.msgId);
		EXPECT_TRUE(msg.Deserialize(bitStream));
		EXPECT_LT(bitStream.GetNumberOfUnreadBits(), 8u);
		msg.target = target;
		EXPECT_EQ(StructPacket(msg).bytes, live.bytes);
		return msg;
	}

	// Checks that every strict prefix of msg's serialized payload fails to deserialize.
	template<typename T>
	void ExpectTruncatedFails(const T& msg) {
		RakNet::BitStream full;
		msg.Serialize(full);
		for (uint32_t bits = 0; bits < full.GetNumberOfBitsUsed(); bits++) {
			RakNet::BitStream prefix;
			prefix.WriteBits(full.GetData(), bits, false);
			T copy;
			EXPECT_FALSE(copy.Deserialize(prefix)) << "deserialized from only " << bits << " bits";
		}
	}
}

#endif // GAMEMESSAGETESTUTILS_H
