#include <gtest/gtest.h>

#include "master/MessageCapture.h"
#include "InspectorFormat.h"

using namespace std::chrono_literals;

namespace {
	MessageCaptureEntry Entry(uint32_t sequence, size_t payloadBytes = 4) {
		MessageCaptureEntry entry;
		entry.sequence = sequence;
		entry.messageId = static_cast<uint16_t>(MessageType::Game::REQUEST_USE);
		entry.payload.assign(payloadBytes, 'x');
		entry.bits = static_cast<uint32_t>(payloadBytes * 8);
		return entry;
	}
}

TEST(MessageCaptureTest, ControlRoundTrip) {
	MessageCaptureControl control;
	control.captureId = 7;
	control.action = eMessageCaptureControl::START;
	control.characterId = 1152921504606846999LL;
	control.seconds = 120;
	control.toServer = false;
	control.only = { 1, 2, 3 };
	control.skip = { 4 };

	RakNet::BitStream stream;
	control.Serialize(stream);
	MessageCaptureControl read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.captureId, 7u);
	EXPECT_EQ(read.action, eMessageCaptureControl::START);
	EXPECT_EQ(read.characterId, 1152921504606846999LL);
	EXPECT_EQ(read.seconds, 120u);
	EXPECT_FALSE(read.toServer);
	EXPECT_TRUE(read.toClient);
	EXPECT_EQ(read.only, (std::vector<uint16_t>{ 1, 2, 3 }));
	EXPECT_EQ(read.skip, (std::vector<uint16_t>{ 4 }));
}

TEST(MessageCaptureTest, ControlCapsTheTimeLimit) {
	MessageCaptureControl control;
	control.seconds = 100000;
	RakNet::BitStream stream;
	control.Serialize(stream);
	MessageCaptureControl read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.seconds, MessageCapture::MAX_SECONDS);
}

TEST(MessageCaptureTest, ControlRejectsOversizedFilters) {
	RakNet::BitStream stream;
	stream.Write<uint32_t>(1);
	stream.Write(eMessageCaptureControl::START);
	stream.Write<LWOOBJID>(1);
	stream.Write<uint32_t>(10);
	stream.Write<uint8_t>(1);
	stream.Write<uint8_t>(1);
	stream.Write<uint16_t>(MessageCapture::MAX_FILTER + 1);
	MessageCaptureControl read;
	EXPECT_FALSE(read.Deserialize(stream));
}

TEST(MessageCaptureTest, Filters) {
	MessageCaptureControl control;
	EXPECT_TRUE(control.Wants(eMessageDirection::TO_SERVER, 5));
	control.toClient = false;
	EXPECT_FALSE(control.Wants(eMessageDirection::TO_CLIENT, 5));
	control.skip = { 5 };
	EXPECT_FALSE(control.Wants(eMessageDirection::TO_SERVER, 5));
	control.only = { 6 };
	EXPECT_TRUE(control.Wants(eMessageDirection::TO_SERVER, 6));
	EXPECT_FALSE(control.Wants(eMessageDirection::TO_SERVER, 7));
	// Skip wins over only
	control.skip = { 6 };
	EXPECT_FALSE(control.Wants(eMessageDirection::TO_SERVER, 6));
}

TEST(MessageCaptureTest, DataRoundTrip) {
	MessageCaptureData data;
	data.captureId = 3;
	data.status = eMessageCaptureStatus::ENDED;
	data.characterId = 42;
	data.zoneId = 1200;
	data.instanceId = 2;
	data.reason = eMessageCaptureEnd::PLAYER_LEFT;
	data.dropped = 9;
	data.cloneId = 1152;
	auto entry = Entry(11);
	entry.timeMs = 1700000000123;
	entry.direction = eMessageDirection::TO_CLIENT;
	entry.objectId = 99;
	entry.decoded = R"({"a":1})";
	data.entries.push_back(entry);

	RakNet::BitStream stream;
	data.Serialize(stream);
	MessageCaptureData read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.captureId, 3u);
	EXPECT_EQ(read.status, eMessageCaptureStatus::ENDED);
	EXPECT_EQ(read.zoneId, 1200u);
	EXPECT_EQ(read.instanceId, 2u);
	EXPECT_EQ(read.reason, eMessageCaptureEnd::PLAYER_LEFT);
	EXPECT_EQ(read.dropped, 9u);
	EXPECT_EQ(read.cloneId, 1152u);
	ASSERT_EQ(read.entries.size(), 1u);
	EXPECT_EQ(read.entries[0].sequence, 11u);
	EXPECT_EQ(read.entries[0].timeMs, 1700000000123);
	EXPECT_EQ(read.entries[0].direction, eMessageDirection::TO_CLIENT);
	EXPECT_EQ(read.entries[0].objectId, 99);
	EXPECT_EQ(read.entries[0].payload, "xxxx");
	EXPECT_EQ(read.entries[0].decoded, R"({"a":1})");
}

TEST(MessageCaptureTest, DataRejectsTruncatedPackets) {
	MessageCaptureData data;
	data.entries.push_back(Entry(1, 100));
	RakNet::BitStream stream;
	data.Serialize(stream);
	RakNet::BitStream cut(stream.GetData(), stream.GetNumberOfBytesUsed() - 10, true);
	MessageCaptureData read;
	EXPECT_FALSE(read.Deserialize(cut));
}

TEST(MessageCaptureTest, Hex) {
	EXPECT_EQ(MessageCapture::ToHex(std::string("\x00\x0f\xa5\xff", 4)), "000fa5ff");
	EXPECT_EQ(MessageCapture::ToHex(""), "");
}

TEST(MessageCaptureQueueTest, LimitsRatePerSecond) {
	MessageCaptureQueue queue(100, 3);
	const auto now = MessageCaptureQueue::Clock::now();
	EXPECT_TRUE(queue.Push(Entry(1), now));
	EXPECT_TRUE(queue.Push(Entry(2), now));
	EXPECT_TRUE(queue.Push(Entry(3), now));
	EXPECT_FALSE(queue.Push(Entry(4), now + 500ms));
	EXPECT_EQ(queue.TakeDropped(), 1u);
	EXPECT_EQ(queue.TakeDropped(), 0u);
	// A new second allows more
	EXPECT_TRUE(queue.Push(Entry(5), now + 1s));
	EXPECT_EQ(queue.Pending(), 4u);
}

TEST(MessageCaptureQueueTest, LimitsPending) {
	MessageCaptureQueue queue(2, 1000);
	const auto now = MessageCaptureQueue::Clock::now();
	EXPECT_TRUE(queue.Push(Entry(1), now));
	EXPECT_TRUE(queue.Push(Entry(2), now));
	EXPECT_FALSE(queue.Push(Entry(3), now));
	EXPECT_EQ(queue.TakeDropped(), 1u);
	queue.Take(1, 1 << 20);
	EXPECT_TRUE(queue.Push(Entry(4), now));
}

TEST(MessageCaptureQueueTest, TakesBatchesInOrderWithinLimits) {
	MessageCaptureQueue queue(100, 1000);
	const auto now = MessageCaptureQueue::Clock::now();
	for (uint32_t i = 1; i <= 5; i++) queue.Push(Entry(i, 100), now);

	auto batch = queue.Take(2, 1 << 20);
	ASSERT_EQ(batch.size(), 2u);
	EXPECT_EQ(batch[0].sequence, 1u);
	EXPECT_EQ(batch[1].sequence, 2u);

	// By size: each entry is about 132 bytes
	batch = queue.Take(10, 200);
	ASSERT_EQ(batch.size(), 1u);
	EXPECT_EQ(batch[0].sequence, 3u);

	// An entry larger than the byte limit still goes, alone
	batch = queue.Take(10, 1);
	ASSERT_EQ(batch.size(), 1u);
	EXPECT_EQ(batch[0].sequence, 4u);

	batch = queue.Take(10, 1 << 20);
	ASSERT_EQ(batch.size(), 1u);
	EXPECT_TRUE(queue.Take(10, 1 << 20).empty());
}

TEST(InspectorFormatTest, MessageNames) {
	EXPECT_EQ(InspectorFormat::MessageName(static_cast<uint16_t>(MessageType::Game::REQUEST_USE)), "REQUEST_USE");
	EXPECT_EQ(InspectorFormat::MessageName(65000), "#65000");
	EXPECT_FALSE(InspectorFormat::AllMessages().empty());
}

TEST(InspectorFormatTest, ParsesFilterEntries) {
	const auto requestUse = static_cast<uint16_t>(MessageType::Game::REQUEST_USE);
	EXPECT_EQ(InspectorFormat::ParseMessage("REQUEST_USE"), requestUse);
	EXPECT_EQ(InspectorFormat::ParseMessage(" request_use "), requestUse);
	EXPECT_EQ(InspectorFormat::ParseMessage(std::to_string(requestUse)), requestUse);
	EXPECT_EQ(InspectorFormat::ParseMessage(nlohmann::json(requestUse)), requestUse);
	EXPECT_FALSE(InspectorFormat::ParseMessage("NOT_A_MESSAGE"));
	EXPECT_FALSE(InspectorFormat::ParseMessage("70000"));
	EXPECT_FALSE(InspectorFormat::ParseMessage(nlohmann::json(-1)));

	std::string error;
	const auto list = InspectorFormat::ParseMessageList(nlohmann::json::array({ "REQUEST_USE", requestUse, "PICKUP_ITEM" }), error);
	ASSERT_TRUE(list);
	EXPECT_EQ(list->size(), 2u); // duplicates merged
	EXPECT_FALSE(InspectorFormat::ParseMessageList(nlohmann::json::array({ "NOPE" }), error));
	EXPECT_NE(error.find("NOPE"), std::string::npos);
	EXPECT_FALSE(InspectorFormat::ParseMessageList(nlohmann::json("REQUEST_USE"), error));
	EXPECT_TRUE(InspectorFormat::ParseMessageList(nullptr, error)->empty());
}

TEST(InspectorFormatTest, EntryJson) {
	auto entry = Entry(5, 3);
	entry.payload = std::string("\x01\x02\xff", 3);
	entry.bits = 30; // 4 bytes, only 3 kept
	entry.objectId = 1152921504606846999LL;
	entry.direction = eMessageDirection::TO_SERVER;
	entry.decoded = R"({"object":"12"})";
	const auto json = InspectorFormat::EntryJson(entry);
	EXPECT_EQ(json["seq"], 5);
	EXPECT_EQ(json["dir"], "to_server");
	EXPECT_EQ(json["name"], "REQUEST_USE");
	EXPECT_EQ(json["object"], "1152921504606846999");
	EXPECT_EQ(json["bytes"], 4);
	EXPECT_EQ(json["hex"], "0102ff");
	EXPECT_TRUE(json["truncated"].get<bool>());
	EXPECT_EQ(json["fields"]["object"], "12");

	entry.decoded = "not json";
	EXPECT_TRUE(InspectorFormat::EntryJson(entry)["fields"].is_null());
}

TEST(InspectorFormatTest, IdsText) {
	EXPECT_EQ(InspectorFormat::IdsText({}), "");
	EXPECT_EQ(InspectorFormat::IdsText({ 154, 1234 }), "154,1234");
	EXPECT_EQ(InspectorFormat::ParseIds("154,1234"), (std::vector<uint16_t>{ 154, 1234 }));
	EXPECT_TRUE(InspectorFormat::ParseIds("").empty());
	EXPECT_EQ(InspectorFormat::ParseIds("1,x,,70000,2"), (std::vector<uint16_t>{ 1, 2 }));
}

TEST(InspectorFormatTest, GapCounter) {
	InspectorFormat::GapCounter gaps;
	EXPECT_EQ(gaps.Next(1), 0u);
	EXPECT_EQ(gaps.Next(2), 0u);
	EXPECT_EQ(gaps.Next(6), 3u); // 3, 4 and 5 were left out
	EXPECT_EQ(gaps.Next(7), 0u);
	// The player changed worlds: the new world numbers from 1 again
	EXPECT_EQ(gaps.Next(1), 0u);
	EXPECT_EQ(gaps.Next(3), 1u);
	// Started again in the same world, which keeps its numbers: the first one after the restart never counts
	gaps.Restart();
	EXPECT_EQ(gaps.Next(40), 0u);
	EXPECT_EQ(gaps.Next(41), 0u);
}

TEST(InspectorFormatTest, RecordJson) {
	auto entry = Entry(9, 2);
	entry.payload = std::string("\xab\x00", 2);
	entry.bits = 16;
	entry.timeMs = 1700000000999;
	entry.direction = eMessageDirection::TO_CLIENT;
	const auto record = InspectorFormat::ToRecord(entry, 12, 3, 4, 1200, 2, 7);
	EXPECT_EQ(record.sessionId, 12u);
	EXPECT_EQ(record.seq, 3u); // the dashboard's number, not the world's
	EXPECT_EQ(InspectorFormat::StoredBytes(record), 56u + 2u);
	const auto json = InspectorFormat::RecordJson(record);
	EXPECT_EQ(json["seq"], 3);
	EXPECT_EQ(json["dir"], "to_client");
	EXPECT_EQ(json["hex"], "ab00");
	EXPECT_FALSE(json["truncated"].get<bool>());
	EXPECT_EQ(json["gap"], 4);
	EXPECT_EQ(json["zone"], 1200);
	EXPECT_EQ(json["instance"], 2);
	EXPECT_EQ(json["clone"], 7);
	EXPECT_EQ(json["time"], 1700000000999);
	EXPECT_TRUE(json["fields"].is_null());
}

TEST(InspectorFormatTest, SessionOrders) {
	EXPECT_EQ(InspectorFormat::ParseOrder("started"), IMessageCaptures::eSessionOrder::STARTED);
	EXPECT_EQ(InspectorFormat::ParseOrder("Started_By"), IMessageCaptures::eSessionOrder::STARTED_BY);
	EXPECT_FALSE(InspectorFormat::ParseOrder("name; DROP TABLE"));
	EXPECT_FALSE(InspectorFormat::ParseOrder(""));
	const auto names = InspectorFormat::OrderNames();
	ASSERT_EQ(names.size(), magic_enum::enum_count<IMessageCaptures::eSessionOrder>());
	for (const auto& name : names) EXPECT_TRUE(InspectorFormat::ParseOrder(name)) << name;
	EXPECT_EQ(names[0], "started");
}

TEST(InspectorFormatTest, RetentionByAge) {
	constexpr int64_t DAY = 24 * 60 * 60;
	const int64_t now = 100 * DAY;
	const std::vector<InspectorFormat::StoredSession> sessions{
		{ 1, now - 40 * DAY, 10, false },
		{ 2, now - 31 * DAY, 10, true },  // running: never deleted
		{ 3, now - 29 * DAY, 10, false },
		{ 4, now - DAY, 10, false },
	};
	EXPECT_EQ(InspectorFormat::SelectExpired(sessions, now, 30, 0), (std::vector<uint64_t>{ 1 }));
	EXPECT_TRUE(InspectorFormat::SelectExpired(sessions, now, 0, 0).empty()); // no limits
	EXPECT_EQ(InspectorFormat::SelectExpired(sessions, now, 2, 0), (std::vector<uint64_t>{ 1, 3 }));
}

TEST(InspectorFormatTest, RetentionBySize) {
	const std::vector<InspectorFormat::StoredSession> sessions{
		{ 5, 500, 100, false },
		{ 1, 100, 100, true },   // the oldest, but running
		{ 2, 200, 100, false },
		{ 3, 300, 100, false },
		{ 4, 300, 100, false },  // same start as 3: the lower id goes first
	};
	// 500 bytes in all: down to 250 deletes the oldest finished ones, 2, 3 and 4
	EXPECT_EQ(InspectorFormat::SelectExpired(sessions, 1000, 0, 250), (std::vector<uint64_t>{ 2, 3, 4 }));
	EXPECT_TRUE(InspectorFormat::SelectExpired(sessions, 1000, 0, 500).empty());
	// Running captures count but stay: the limit may not be reachable
	EXPECT_EQ(InspectorFormat::SelectExpired(sessions, 1000, 0, 50), (std::vector<uint64_t>{ 2, 3, 4, 5 }));
	// Age first, then size over what is left
	EXPECT_EQ(InspectorFormat::SelectExpired({ { 1, 0, 100, false }, { 2, 100000, 300, false }, { 3, 100050, 100, false } }, 2 * 24 * 60 * 60, 1, 350),
		(std::vector<uint64_t>{ 1, 2 }));
}
