#include <gtest/gtest.h>

#include "BitStream.h"
#include "GameDependencies.h"
#include "TacArcBehavior.h"

// TacArc target lists as the client writes and reads them (TacArcBehavior::DoHit / DoUnserializeBS)
class TacArcTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

namespace {
	void WriteIds(RakNet::BitStream& stream, const std::vector<LWOOBJID>& ids) {
		stream.Write<uint32_t>(ids.size());
		for (const auto id : ids) stream.Write(id);
	}
}

TEST_F(TacArcTests, TargetsAreReadAscendingAndOnce) {
	// The client handles each listed target once, in ascending id order, and skips empty ids
	RakNet::BitStream stream;
	WriteIds(stream, { 30, 10, LWOOBJID_EMPTY, 20, 10 });
	stream.Write<uint8_t>(0xAB); // the actions' data follows

	std::set<LWOOBJID> targets;
	ASSERT_TRUE(TacArcBehavior::ReadTargets(stream, 100, targets));
	EXPECT_EQ(std::vector<LWOOBJID>(targets.begin(), targets.end()), (std::vector<LWOOBJID>{ 10, 20, 30 }));

	uint8_t next = 0;
	ASSERT_TRUE(stream.Read(next));
	EXPECT_EQ(next, 0xAB);
}

TEST_F(TacArcTests, TooManyTargetsAreRefused) {
	RakNet::BitStream stream;
	WriteIds(stream, { 1, 2, 3 });
	std::set<LWOOBJID> targets;
	EXPECT_FALSE(TacArcBehavior::ReadTargets(stream, 2, targets));
}

TEST_F(TacArcTests, CutShortDataIsRefused) {
	RakNet::BitStream stream;
	stream.Write<uint32_t>(2);
	stream.Write<LWOOBJID>(5);
	std::set<LWOOBJID> targets;
	EXPECT_FALSE(TacArcBehavior::ReadTargets(stream, 100, targets));
}

TEST_F(TacArcTests, TheClosestTargetsAreWrittenAscending) {
	// Closest first; the two closest are kept and written in ascending id order, as the client's DoHit does
	RakNet::BitStream stream;
	const auto order = TacArcBehavior::WriteTargets(stream, { 90, 40, 70 }, 2);
	EXPECT_EQ(std::vector<LWOOBJID>(order.begin(), order.end()), (std::vector<LWOOBJID>{ 40, 90 }));

	uint32_t count = 0;
	LWOOBJID first = 0;
	LWOOBJID second = 0;
	ASSERT_TRUE(stream.Read(count));
	ASSERT_TRUE(stream.Read(first));
	ASSERT_TRUE(stream.Read(second));
	EXPECT_EQ(count, 2u);
	EXPECT_EQ(first, 40);
	EXPECT_EQ(second, 90);
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0);
}

TEST_F(TacArcTests, WrittenTargetsReadBack) {
	RakNet::BitStream stream;
	const auto written = TacArcBehavior::WriteTargets(stream, { 7, 3, 5 }, 100);
	std::set<LWOOBJID> read;
	ASSERT_TRUE(TacArcBehavior::ReadTargets(stream, 100, read));
	EXPECT_EQ(read, written);
}
