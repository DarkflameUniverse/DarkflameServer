#include "GameDependencies.h"
#include "GameMessageTestUtils.h"

#include "Mail.h"
#include "MovementMessages.h"
#include "ZoneMessages.h"

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

// What the world sends while a player loads into a zone, compared with the live captures.
class LoadSequenceTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }

	static constexpr LWOOBJID PLAYER = 0x1000000000000001LL;
	static constexpr LWOOBJID ZONE_CONTROL = 0x3FFF'FFFFFFFELL;
};

// Live: PlayerReady to the player, then to the zone control object, both to the loading client only.
// Bytes: the live pair with the player ID replaced.
TEST_F(LoadSequenceTests, PlayerReadyGoesToThePlayerThenTheZoneControl) {
	const auto sent = Capture([&] { GameMessages::SendPlayerReady(PLAYER, ZONE_CONTROL, ClientAddress()); });
	ASSERT_EQ(sent.size(), 2u);
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 fd 01"), FromCapture(sent[0]));
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 fe ff ff ff ff 3f 00 00 fd 01"), FromCapture(sent[1]));
	for (const auto& packet : sent) {
		EXPECT_EQ(packet.sysAddr, ClientAddress());
		EXPECT_FALSE(packet.broadcast);
	}
}

TEST_F(LoadSequenceTests, PlayerReadyWithoutZoneControlGoesToThePlayerOnly) {
	const auto sent = Capture([&] { GameMessages::SendPlayerReady(PLAYER, LWOOBJID_EMPTY, ClientAddress()); });
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 fd 01"), FromCapture(sent[0]));
}

// Live: ServerDoneLoadingAllObjects, then right after it the respawn checkpoint with a rotation. Bytes: a live pair
// from Avant Gardens Survival (a first load, so the spawn point) with the player ID replaced.
TEST_F(LoadSequenceTests, RespawnCheckpointFollowsDoneLoading) {
	const NiPoint3 spawn(35.218f, 365.7804f, -201.3283f);
	const NiQuaternion facing(0.7015f, 0.0f, -0.7126f, 0.0f);
	const auto sent = Capture([&] { GameMessages::SendDoneLoading(PLAYER, NiPoint3Constant::ZERO, spawn, facing, ClientAddress()); });
	ASSERT_EQ(sent.size(), 2u);
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 6a 06"), FromCapture(sent[0]));
	EXPECT_PACKET_EQ(FromHex(
		"53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 10 05 3b df 0c 42 e4 e3 b6 43 0b 54 49 c3 c0 ca 99 9f 80 00 00 00 "
		"7a 36 1b 5f 80 00 00 00 00", 369), FromCapture(sent[1]));
	for (const auto& packet : sent) {
		EXPECT_EQ(packet.sysAddr, ClientAddress());
		EXPECT_FALSE(packet.broadcast);
	}
}

// A saved checkpoint wins over where the player loaded in; DLU keeps no rotation for it.
TEST_F(LoadSequenceTests, SavedCheckpointIsSentUnrotated) {
	const NiPoint3 saved(100.0f, 200.0f, 300.0f);
	const auto sent = Capture([&] { GameMessages::SendDoneLoading(PLAYER, saved, NiPoint3(1.0f, 2.0f, 3.0f), NiQuaternion(0.7015f, 0.0f, -0.7126f, 0.0f), ClientAddress()); });
	const auto checkpoints = SentGameMessages<GameMessages::PlayerReachedRespawnCheckpoint>(sent);
	ASSERT_EQ(checkpoints.size(), 1u);
	EXPECT_EQ(checkpoints[0].pos, saved);
	EXPECT_EQ(checkpoints[0].rot, QuatUtils::IDENTITY);
}

// Unread mail is announced during the load without the client asking, one NewMail notice per unread mail (oldest
// first, a count of 1 each, as live sent two notices for two mails); nothing is sent without unread mail.
TEST_F(LoadSequenceTests, UnreadMailIsAnnouncedOnLoad) {
	EXPECT_TRUE(Mail::UnreadMailNotices({}, PLAYER).empty());

	MailInfo read;
	read.id = 1;
	read.wasRead = true;
	MailInfo newer;
	newer.id = 9;
	newer.itemLOT = 0;
	newer.itemCount = 1;
	MailInfo older;
	older.id = 4;
	older.itemID = 0x1000000000000123;
	older.itemLOT = 3038;
	older.itemCount = 100;
	const auto notices = Mail::UnreadMailNotices({ read, newer, older }, PLAYER);
	ASSERT_EQ(notices.size(), 2u);
	EXPECT_EQ(notices[0].mailID, 4u);
	EXPECT_EQ(notices[0].attachmentLOT, 3038);
	EXPECT_EQ(notices[1].mailID, 9u);
	EXPECT_EQ(notices[1].attachmentLOT, LOT_NULL);
	for (const auto& notice : notices) {
		EXPECT_EQ(notice.status, Mail::eNotificationResponse::NewMail);
		EXPECT_EQ(notice.receiverID, PLAYER);
		EXPECT_EQ(notice.mailCount, 1u);
	}
}

// Live, right before TRANSFER_TO_WORLD on a rocket launch to Nimbus Station (spawn point MedPropLand): flag 32 on,
// TransferToZone, TransferToZoneCheckedIM, flag 32 off, all to the launching client. Bytes: live with the player ID
// replaced (the live UpdatePlayerStatistic between the first two is left out).
TEST_F(LoadSequenceTests, ZoneTransferNoticeMatchesLive) {
	const auto sent = Capture([&] { GameMessages::SendZoneTransferNotice(PLAYER, 1200, 0, u"MedPropLand", ClientAddress()); });
	ASSERT_EQ(sent.size(), 4u);
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 d8 01 90 00 00 00 00", 177), FromCapture(sent[0]));
	EXPECT_PACKET_EQ(FromHex(
		"53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 04 02 80 05 80 00 00 26 80 32 80 32 00 28 00 39 00 37 80 38 00 "
		"26 00 30 80 37 00 32 00 00 6c 01 00", 386), FromCapture(sent[1]));
	EXPECT_PACKET_EQ(FromHex(
		"53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 05 02 00 05 80 00 00 26 80 32 80 32 00 28 00 39 00 37 80 38 00 "
		"26 00 30 80 37 00 32 00 00 6c 01 00", 386), FromCapture(sent[2]));
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 d8 01 10 00 00 00 00", 177), FromCapture(sent[3]));
	for (const auto& packet : sent) {
		EXPECT_EQ(packet.sysAddr, ClientAddress());
		EXPECT_FALSE(packet.broadcast);
	}
}

// A property launch carries the clone; the messages read back to what was sent.
TEST_F(LoadSequenceTests, ZoneTransferCarriesThePropertyClone) {
	const auto sent = Capture([&] { GameMessages::SendZoneTransferNotice(PLAYER, 1250, 545173, u"", ClientAddress()); });
	const auto transfers = SentGameMessages<GameMessages::TransferToZone>(sent);
	const auto checked = SentGameMessages<GameMessages::TransferToZoneCheckedIM>(sent);
	ASSERT_EQ(transfers.size(), 1u);
	ASSERT_EQ(checked.size(), 1u);
	EXPECT_TRUE(transfers[0].bCheckTransferAllowed);
	EXPECT_EQ(transfers[0].cloneID, 545173u);
	EXPECT_EQ(transfers[0].zoneID, 1250);
	EXPECT_FALSE(checked[0].bIsThereaQueue);
	EXPECT_EQ(checked[0].cloneID, 545173u);
	RoundTrip(transfers[0]);
	RoundTrip(checked[0]);
	ExpectTruncatedFails(transfers[0]);
}
