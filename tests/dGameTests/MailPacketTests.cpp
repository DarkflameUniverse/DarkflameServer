// Pins the bytes of the mail packets and the mailbox messages against the layout the client reads for the mail
// list and notifications, and against live captures. Padding in the captures holds uninitialized memory; the
// expected bytes below use zero there, which is what DLU writes and what the client ignores.

#include "Mail.h"
#include "MailInfo.h"
#include "EffectsMessages.h"
#include "ObjectMessages.h"
#include "Amf3.h"
#include "PacketTestUtils.h"

#include <cstring>
#include <string>

#include <gtest/gtest.h>

using PacketTestUtils::FromBitStream;
using PacketTestUtils::FromHex;
using PacketTestUtils::PacketsEqual;

namespace {
	// Little endian writer for building the expected bytes field by field.
	struct Bytes {
		std::vector<uint8_t> data;
		template<typename T> void Put(const T value) {
			uint8_t raw[sizeof(T)];
			std::memcpy(raw, &value, sizeof(T));
			data.insert(data.end(), raw, raw + sizeof(T));
		}
		// Fixed size UTF-16 field, zero filled
		void PutWide(const std::string& text, const size_t chars) {
			for (size_t i = 0; i < chars; i++) Put<uint16_t>(i < text.size() ? static_cast<uint16_t>(text[i]) : 0);
		}
		PacketTestUtils::PacketBytes Packet() const { return { data, static_cast<uint32_t>(data.size() * 8) }; }
	};

	void PutClientMailHeader(Bytes& b, const Mail::eMessageID id) {
		b.Put<uint8_t>(0x53);     // ID_USER_PACKET_ENUM
		b.Put<uint16_t>(5);       // ServiceType::CLIENT
		b.Put<uint32_t>(0x31);    // MessageType::Client::MAIL
		b.Put<uint8_t>(0);
		b.Put<uint32_t>(static_cast<uint32_t>(id));
	}

	PacketTestUtils::PacketBytes Write(const LUBitStream& packet) {
		RakNet::BitStream bitStream;
		packet.WritePacket(bitStream);
		return FromBitStream(bitStream);
	}

	PacketTestUtils::PacketBytes Write(const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream bitStream;
		msg.WritePacket(bitStream);
		return FromBitStream(bitStream);
	}
}

// An empty mailbox: throttled 0, count 0. The client clears its list and sends LoadMailList with no entries.
TEST(MailPacketTests, EmptyDataResponse) {
	Mail::DataResponse response;
	Bytes expected;
	PutClientMailHeader(expected, Mail::eMessageID::DataResponse);
	expected.Put<uint32_t>(0); // throttled
	expected.Put<uint16_t>(0); // count
	expected.Put<uint16_t>(0); // padding
	ASSERT_EQ(expected.data.size(), 20u);
	EXPECT_TRUE(PacketsEqual(expected.Packet(), Write(response)));
}

// One player mail with an attachment, values from a live capture (100 of LOT 3038 from another player).
// Every entry is 1040 bytes.
TEST(MailPacketTests, DataResponseEntryMatchesLive) {
	MailInfo mail;
	mail.id = 0x100000016c4b30e2;
	mail.subject = "blue1";
	mail.body = "";
	mail.senderUsername = "ShastaFantastic";
	mail.itemID = 0x100000016c4b30e3;
	mail.itemLOT = 3038;
	mail.itemSubkey = 0;
	mail.itemCount = 100;
	mail.timeSent = 1327102410;
	mail.wasRead = false;

	Mail::DataResponse response;
	response.playerMail.push_back(mail);

	Bytes expected;
	PutClientMailHeader(expected, Mail::eMessageID::DataResponse);
	expected.Put<uint32_t>(0); // throttled
	expected.Put<uint16_t>(1); // count
	expected.Put<uint16_t>(0);
	const auto entryStart = expected.data.size();
	expected.Put<uint64_t>(0x100000016c4b30e2); // mail id
	expected.PutWide("blue1", 50);               // subject
	expected.PutWide("", 400);                   // body
	expected.PutWide("ShastaFantastic", 32);     // sender
	expected.Put<uint32_t>(0);
	expected.Put<uint64_t>(0);                   // attached currency
	expected.Put<int64_t>(0x100000016c4b30e3);   // attachment object id
	expected.Put<int32_t>(3038);                 // attachment LOT
	expected.Put<uint32_t>(0);
	expected.Put<int64_t>(0);                    // attachment subkey
	expected.Put<int16_t>(100);                  // attachment count
	expected.Put<uint8_t>(0);                    // subject type
	expected.Put<uint8_t>(0);
	expected.Put<uint32_t>(0);
	expected.Put<uint64_t>(1327102410);          // expiration
	expected.Put<uint64_t>(1327102410);          // sent
	expected.Put<uint8_t>(0);                    // was read
	expected.Put<uint8_t>(0);                    // is localized (player mail)
	expected.Put<uint16_t>(1033);                // language
	expected.Put<uint32_t>(0);
	ASSERT_EQ(expected.data.size() - entryStart, 1040u);

	EXPECT_TRUE(PacketsEqual(expected.Packet(), Write(response)));
}

// No attachment: the client checks for LOT -1 (LOT_NULL), not 0.
TEST(MailPacketTests, NoAttachmentIsLotNull) {
	MailInfo mail;
	mail.itemLOT = 0;
	RakNet::BitStream bitStream;
	mail.Serialize(bitStream);
	const auto bytes = FromBitStream(bitStream).bytes;
	ASSERT_EQ(bytes.size(), 1040u);
	int32_t lot{};
	std::memcpy(&lot, bytes.data() + 8 + 100 + 800 + 64 + 4 + 8 + 8, sizeof(lot));
	EXPECT_EQ(lot, -1);
}

// Answer to NotificationRequest: 56 bytes, the unread count at offset 48 (live sent 2 there for two unread mails).
TEST(MailPacketTests, NotificationResponseLayout) {
	Mail::NotificationResponse response;
	response.status = Mail::eNotificationResponse::NewMail;
	response.mailCount = 2;
	const auto actual = Write(response);
	ASSERT_EQ(actual.bytes.size(), 56u);
	Bytes expected;
	PutClientMailHeader(expected, Mail::eMessageID::NotificationResponse);
	expected.Put<uint32_t>(0); // NewMail
	expected.Put<uint64_t>(0);
	expected.Put<uint64_t>(0);
	expected.Put<uint64_t>(0);
	expected.Put<uint64_t>(0);
	expected.Put<uint32_t>(2); // mail count
	expected.Put<uint32_t>(0);
	EXPECT_TRUE(PacketsEqual(expected.Packet(), actual));
}

// The mailbox opens the Mail UI with pushGameState {state: "Mail"}; bytes from a live capture.
TEST(MailPacketTests, MailboxPushGameStateMatchesLive) {
	GameMessages::UIMessageServerToSingleClient msg;
	msg.target = 0x100000015b7b141f;
	msg.strMessageName = "pushGameState";
	msg.args.Insert("state", "Mail");
	EXPECT_TRUE(PacketsEqual(
		FromHex("5305000c000000001f147b5b01000010a00409010b737461746506094d61696c010d0000007075736847616d655374617465"),
		Write(msg)));
}

// Closing the mailbox: ToggleMail {visible: false}; bytes from a live capture.
TEST(MailPacketTests, MailboxToggleMailMatchesLive) {
	GameMessages::UIMessageServerToSingleClient msg;
	msg.target = 0x100000015b7b141f;
	msg.strMessageName = "ToggleMail";
	msg.args.Insert("visible", false);
	EXPECT_TRUE(PacketsEqual(
		FromHex("5305000c000000001f147b5b01000010a00409010f76697369626c6502010a000000546f67676c654d61696c"),
		Write(msg)));
}

// After pushGameState the mailbox itself is told OpenMail, and CloseMail after ToggleMail; its client script keeps
// the mailbox marked in use (no interact icon, no second use) in between. Bytes from a live capture.
TEST(MailPacketTests, MailboxOpenCloseNotifyMatchesLive) {
	constexpr LWOOBJID mailbox = 0x000044800000b5c0;
	EXPECT_TRUE(PacketsEqual(
		FromHex("5305000c00000000c0b50000804400001204080000004f00700065006e004d00610069006c000000000000000000000000000000000000000000"),
		Write(GameMessages::NotifyClientObject(mailbox, u"OpenMail"))));
	EXPECT_TRUE(PacketsEqual(
		FromHex("5305000c00000000c0b500008044000012040900000043006c006f00730065004d00610069006c000000000000000000000000000000000000000000"),
		Write(GameMessages::NotifyClientObject(mailbox, u"CloseMail"))));
}
