#include "ClientPackets.h"
#include "GameDependencies.h"
#include "PacketTestUtils.h"
#include "WorldPackets.h"
#include "ZoneEditLock.h"
#include "ZoneEditor.h"

#include <gtest/gtest.h>

using namespace PacketTestUtils;

namespace {
	// Written, read back and written again: the same bytes, with nothing left unread
	template<typename T>
	T RoundTrip(const T& packet) {
		RakNet::BitStream first;
		packet.WritePacket(first);
		T copy;
		EXPECT_TRUE(copy.ReadHeader(first));
		EXPECT_TRUE(copy.Deserialize(first));
		EXPECT_EQ(first.GetNumberOfUnreadBits(), 0);
		RakNet::BitStream second;
		copy.WritePacket(second);
		EXPECT_PACKET_EQ(FromBitStream(first), FromBitStream(second));
		return copy;
	}

	// Cut anywhere after its header, the packet does not read
	template<typename T>
	void ExpectTruncatedFails(const T& packet) {
		RakNet::BitStream full;
		packet.WritePacket(full);
		const auto bytes = full.GetNumberOfBytesUsed();
		for (uint32_t cut = 8; cut < bytes; cut++) {
			RakNet::BitStream truncated(full.GetData(), cut, true);
			T copy;
			ASSERT_TRUE(copy.ReadHeader(truncated));
			EXPECT_FALSE(copy.Deserialize(truncated)) << "cut at " << cut << " of " << bytes;
		}
	}
}

// The level editor's packets, byte for byte: what a client sends...
TEST(ZoneEditorPacketsTests, ClientToServerLayouts) {
	WorldPackets::HappyFlowerModeNotify mode;
	mode.on = 1;
	RakNet::BitStream modeBytes;
	mode.WritePacket(modeBytes);
	EXPECT_PACKET_EQ(FromHex("53 04 00 08 00 00 00 00 01"), FromBitStream(modeBytes));
	EXPECT_EQ(RoundTrip(mode).on, 1);
	ExpectTruncatedFails(mode);

	WorldPackets::SlashReloadMap reload;
	RakNet::BitStream reloadBytes;
	reload.WritePacket(reloadBytes);
	EXPECT_PACKET_EQ(FromHex("53 04 00 09 00 00 00 00"), FromBitStream(reloadBytes));

	WorldPackets::SlashPushMapRequest request;
	request.fileType = 1;
	request.levelFileId = 0x01020304;
	RakNet::BitStream requestBytes;
	request.WritePacket(requestBytes);
	EXPECT_PACKET_EQ(FromHex("53 04 00 0a 00 00 00 00 01 04 03 02 01"), FromBitStream(requestBytes));
	EXPECT_EQ(RoundTrip(request).levelFileId, 0x01020304);
	ExpectTruncatedFails(request);

	// The file type, the level file ID, then the packed files to the end of the packet
	WorldPackets::SlashPushMap push;
	push.fileType = 3;
	push.levelFileId = -1;
	push.packedFiles = std::string("\x0c\x00\x00\x00\x05\x00\x00\x00" "a\0.\0l\0u\0z\0\0\0" "sd0\x01\xff", 25);
	RakNet::BitStream pushBytes;
	push.WritePacket(pushBytes);
	EXPECT_PACKET_EQ(FromHex("53 04 00 0b 00 00 00 00 03 ff ff ff ff 0c 00 00 00 05 00 00 00 61 00 2e 00 6c 00 75 00 7a 00 00 00 73 64 30 01 ff"), FromBitStream(pushBytes));
	const auto pushRead = RoundTrip(push);
	EXPECT_EQ(pushRead.fileType, 3);
	EXPECT_EQ(pushRead.levelFileId, -1);
	EXPECT_EQ(pushRead.packedFiles, push.packedFiles);
	// Nothing after the ID reads as no files, which the unpacking then refuses
	push.packedFiles.clear();
	EXPECT_TRUE(RoundTrip(push).packedFiles.empty());

	WorldPackets::SlashPullMap pull;
	pull.fileType = 4;
	pull.levelFileId = 7;
	RakNet::BitStream pullBytes;
	pull.WritePacket(pullBytes);
	EXPECT_PACKET_EQ(FromHex("53 04 00 0c 00 00 00 00 04 07 00 00 00"), FromBitStream(pullBytes));
	EXPECT_EQ(RoundTrip(pull).levelFileId, 7);
	ExpectTruncatedFails(pull);

	WorldPackets::LockMapRequest lock;
	lock.lock = 1;
	RakNet::BitStream lockBytes;
	lock.WritePacket(lockBytes);
	EXPECT_PACKET_EQ(FromHex("53 04 00 0d 00 00 00 00 01"), FromBitStream(lockBytes));
	EXPECT_EQ(RoundTrip(lock).lock, 1);
	ExpectTruncatedFails(lock);
}

// ...and what the server answers
TEST(ZoneEditorPacketsTests, ServerToClientLayouts) {
	ClientPackets::ImpendingReloadNotify notify;
	RakNet::BitStream notifyBytes;
	notify.WritePacket(notifyBytes);
	EXPECT_PACKET_EQ(FromHex("53 05 00 0f 00 00 00 00"), FromBitStream(notifyBytes));

	// The result, the file type and level file ID as asked, the holder's name as UTF-16 with its null
	ClientPackets::SlashPushMapResponse push;
	push.result = 3;
	push.fileType = 1;
	push.levelFileId = 5;
	push.lockedBy = u"bob";
	RakNet::BitStream pushBytes;
	push.WritePacket(pushBytes);
	EXPECT_PACKET_EQ(FromHex("53 05 00 12 00 00 00 00 03 01 05 00 00 00 62 00 6f 00 62 00 00 00"), FromBitStream(pushBytes));
	EXPECT_EQ(RoundTrip(push).lockedBy, u"bob");
	// Allowed: no name, only its null
	ClientPackets::SlashPushMapResponse allowed;
	RakNet::BitStream allowedBytes;
	allowed.WritePacket(allowedBytes);
	EXPECT_PACKET_EQ(FromHex("53 05 00 12 00 00 00 00 00 00 00 00 00 00 00 00"), FromBitStream(allowedBytes));

	ClientPackets::SlashPullMapResponse pull;
	pull.fileType = 2;
	pull.levelFileId = 0;
	pull.packedFiles = std::string("\x04\x00\x00\x00\x05\x00\x00\x00" "r\0\0\0" "sd0\x01\xff", 17);
	RakNet::BitStream pullBytes;
	pull.WritePacket(pullBytes);
	EXPECT_PACKET_EQ(FromHex("53 05 00 13 00 00 00 00 02 00 00 00 00 04 00 00 00 05 00 00 00 72 00 00 00 73 64 30 01 ff"), FromBitStream(pullBytes));
	EXPECT_EQ(RoundTrip(pull).packedFiles, pull.packedFiles);

	// The result, 33 UTF-16 characters of holder name (66 bytes), whether the client holds the lock now
	ClientPackets::SlashLockMapResponse lock;
	lock.result = 3;
	lock.lockedBy.string = u"al";
	lock.locked = 0;
	RakNet::BitStream lockBytes;
	lock.WritePacket(lockBytes);
	std::string expected = "53 05 00 14 00 00 00 00 03 61 00 6c 00";
	for (int i = 0; i < 31; ++i) expected += " 00 00";
	expected += " 00";
	EXPECT_PACKET_EQ(FromHex(expected), FromBitStream(lockBytes));
	EXPECT_EQ(lockBytes.GetNumberOfBytesUsed(), 8u + 1u + 66u + 1u);
	const auto lockRead = RoundTrip(lock);
	EXPECT_EQ(lockRead.result, 3);
	EXPECT_EQ(lockRead.lockedBy.string.substr(0, 2), u"al");
	ExpectTruncatedFails(lock);
}

// While a client's level editor is on it is never told to load a zone
TEST(ZoneEditorTests, NoZoneLoadWhileTheEditorIsOn) {
	SystemAddress client{};
	client.binaryAddress = 0x0100007f;
	client.port = 2001;
	SystemAddress other = client;
	other.port = 2002;

	EXPECT_TRUE(ZoneEditor::MaySendZoneLoad(client));
	ZoneEditor::SetEditing(client, true);
	EXPECT_TRUE(ZoneEditor::IsEditing(client));
	EXPECT_FALSE(ZoneEditor::MaySendZoneLoad(client));
	EXPECT_TRUE(ZoneEditor::MaySendZoneLoad(other)); // only that client
	// Told twice, still on; ended once, off
	ZoneEditor::SetEditing(client, true);
	ZoneEditor::SetEditing(client, false);
	EXPECT_TRUE(ZoneEditor::MaySendZoneLoad(client));
	// A connection that goes away takes its editor state with it
	ZoneEditor::SetEditing(client, true);
	ZoneEditor::Forget(client);
	EXPECT_FALSE(ZoneEditor::IsEditing(client));
	EXPECT_TRUE(ZoneEditor::MaySendZoneLoad(client));
}

// What the editor's requests are answered with: 0 ok, 1 editor level too low, 2 not assigned, 3 locked by, 4 no change
TEST(ZoneEditorTests, GatesAndResults) {
	using namespace ZoneEditLock;
	const Levels levels{ 9, 9 };
	const Actor admin{ 1, 9, false }, low{ 2, 8, true }, off{ 3, 0, true };
	const Levels open{ 6, 9 };
	const Actor editor{ 4, 6, true }, stranger{ 5, 6, false };

	// Locking (answered with the lock response)
	EXPECT_EQ(static_cast<uint8_t>(Take(admin, std::nullopt, levels)), 0);
	EXPECT_EQ(static_cast<uint8_t>(Take(low, std::nullopt, levels)), 1);
	EXPECT_EQ(static_cast<uint8_t>(Take(off, std::nullopt, levels)), 1); // the editor is off for the account
	EXPECT_EQ(static_cast<uint8_t>(Take(stranger, std::nullopt, open)), 2);
	EXPECT_EQ(static_cast<uint8_t>(Take(editor, 1u, open)), 3);
	EXPECT_EQ(static_cast<uint8_t>(Take(editor, 4u, open)), 4);
	EXPECT_EQ(static_cast<uint8_t>(Release(editor, std::nullopt, open)), 4);
	EXPECT_EQ(static_cast<uint8_t>(Release(editor, 4u, open)), 0);
	EXPECT_EQ(static_cast<uint8_t>(Release(stranger, 4u, open)), 3);

	// Sending files (answered with the push response, which has no 4): only the lock's holder
	EXPECT_EQ(static_cast<uint8_t>(ChangeOrWhyNot(editor, 4u, open)), 0);
	EXPECT_EQ(static_cast<uint8_t>(ChangeOrWhyNot(low, 2u, levels)), 1);
	EXPECT_EQ(static_cast<uint8_t>(ChangeOrWhyNot(stranger, std::nullopt, open)), 2);
	EXPECT_EQ(static_cast<uint8_t>(ChangeOrWhyNot(editor, 1u, open)), 3);
	EXPECT_EQ(static_cast<uint8_t>(ChangeOrWhyNot(admin, 4u, open)), 3);        // an admin has to hold it too
	EXPECT_EQ(static_cast<uint8_t>(ChangeOrWhyNot(editor, std::nullopt, open)), 3); // could lock it and hasn't
	EXPECT_EQ(static_cast<uint8_t>(ChangeOrWhyNot(stranger, 4u, open)), 3);

	// Asking for the files and for a reload: an editor of the zone or an admin, lock or not
	EXPECT_EQ(static_cast<uint8_t>(Read(editor, open)), 0);
	EXPECT_EQ(static_cast<uint8_t>(Read(admin, open)), 0);
	EXPECT_EQ(static_cast<uint8_t>(Read(stranger, open)), 2);
	EXPECT_EQ(static_cast<uint8_t>(Read(off, open)), 1);
}
