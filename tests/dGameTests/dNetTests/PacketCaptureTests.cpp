#include "PacketCapture.h"
#include "PacketDecoder.h"
#include "CaptureBundle.h"
#include "CaptureTools.h"
#include "AuthPackets.h"
#include "ChatPackets.h"
#include "ClientPackets.h"
#include "MasterPackets.h"
#include "WorldPackets.h"
#include "master/MessageCapture.h"
#include "MessageIdentifiers.h"
#include "RakNetTypes.h"
#include "ServiceType.h"
#include "sqlite3.h"
#include "GameMessageDecoder.h"
#include "ObjectMessages.h"
#include "MessageType/World.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

namespace {
	std::string Bytes(const LUBitStream& packet) {
		RakNet::BitStream stream;
		packet.WritePacket(stream);
		return std::string(reinterpret_cast<const char*>(stream.GetData()), stream.GetNumberOfBytesUsed());
	}

	SystemAddress Address(uint32_t ip, uint16_t port) {
		SystemAddress address;
		address.binaryAddress = ip;
		address.port = port;
		return address;
	}

	// UTF-16 LE, as LUWString writes text
	std::string Wide(const std::string& text) {
		std::string out;
		for (const char c : text) {
			out += c;
			out += '\0';
		}
		return out;
	}

	struct Captured {
		std::vector<CaptureBundle::Record> records;
		std::vector<MessageCaptureData> batches;
		std::string all; // every record's bytes, to look for secrets
	};

	class PacketCaptureTest : public ::testing::Test {
	protected:
		Captured captured;
		bool connected = true;

		void Start(ServiceType server) {
			PacketCapture::Reset();
			PacketCapture::Attach(server, nullptr, nullptr, 1100, 3);
			PacketCapture::SetSettings({ .flushIntervalMs = 1000, .flushBytes = 1, .maxBufferBytes = 1024 * 1024 });
			PacketCapture::SetSink([this](MessageCaptureData& data) {
				if (!connected) return false;
				captured.batches.push_back(data);
				PacketRecord::ForEach(data.packets, [this](const PacketRecordHeader& header, std::string_view bytes) {
					captured.records.push_back({ header, std::string(bytes) });
					captured.all += bytes;
				});
				return true;
			});
		}

		void TearDown() override { PacketCapture::Reset(); }

		void Arm(uint8_t slot, uint32_t captureId, eCaptureTarget target, uint32_t accountId = 0, const std::string& name = "", std::vector<LWOOBJID> characters = {}) {
			MessageCaptureControl control;
			control.action = eMessageCaptureControl::ARM;
			control.captureId = captureId;
			control.slot = slot;
			control.seconds = 60;
			control.target = target;
			control.accountId = accountId;
			control.accountName = name;
			control.characterIds = std::move(characters);
			PacketCapture::Control(control);
			// Arming reads the settings; keep one record per batch for the tests
			PacketCapture::SetSettings({ .flushIntervalMs = 1000, .flushBytes = 1, .maxBufferBytes = 1024 * 1024 });
		}

		void Receive(const SystemAddress& from, const std::string& bytes) {
			Packet packet{};
			packet.systemAddress = from;
			packet.length = static_cast<unsigned int>(bytes.size());
			packet.bitSize = static_cast<BitSize_t>(bytes.size() * 8);
			packet.data = reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.data()));
			PacketCapture::OnReceive(&packet);
			PacketCapture::OnReceive(nullptr);
		}

		void Send(const SystemAddress& to, const std::string& bytes, bool broadcast = false) {
			PacketCapture::RecordForTest(to, true, broadcast, reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<uint32_t>(bytes.size() * 8));
		}
	};
}

TEST(MessageCaptureTests, PacketCaptureControlRoundTrips) {
	MessageCaptureControl control;
	control.action = eMessageCaptureControl::ARM;
	control.captureId = 12;
	control.slot = 5;
	control.seconds = 120;
	control.target = eCaptureTarget::ACCOUNT;
	control.accountId = 44;
	control.accountName = "Alice";
	control.characterIds = { 1152921510436607007LL, 1152921510436607008LL };
	RakNet::BitStream stream;
	control.Serialize(stream);
	MessageCaptureControl read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.action, eMessageCaptureControl::ARM);
	EXPECT_EQ(read.slot, 5);
	EXPECT_EQ(read.target, eCaptureTarget::ACCOUNT);
	EXPECT_EQ(read.accountId, 44u);
	EXPECT_EQ(read.accountName, "Alice");
	EXPECT_EQ(read.characterIds, control.characterIds);

	// A slot past the last is refused
	RakNet::BitStream bad;
	control.slot = MessageCapture::MAX_SLOTS;
	control.Serialize(bad);
	EXPECT_FALSE(read.Deserialize(bad));
}

TEST(MessageCaptureTests, PacketBatchRoundTrips) {
	MessageCaptureData data;
	data.status = eMessageCaptureStatus::PACKETS;
	data.source = static_cast<uint8_t>(eCaptureSource::CHAT);
	data.slots[2] = 99;
	data.packetCount = 1;
	data.packetsDropped = 7;
	PacketRecordHeader header;
	header.length = 3;
	PacketRecord::Append(data.packets, header, "abc");
	RakNet::BitStream stream;
	data.Serialize(stream);
	MessageCaptureData read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.status, eMessageCaptureStatus::PACKETS);
	EXPECT_EQ(read.source, data.source);
	EXPECT_EQ(read.slots[2], 99u);
	EXPECT_EQ(read.packetsDropped, 7u);
	EXPECT_EQ(read.packets, data.packets);
}

TEST_F(PacketCaptureTest, NothingRecordedWhenNotArmed) {
	Start(ServiceType::WORLD);
	EXPECT_FALSE(PacketCapture::g_Armed);
	WorldPackets::CharacterListRequest request;
	Receive(Address(1, 1000), Bytes(request));
	PacketCapture::Update();
	EXPECT_TRUE(captured.records.empty());
}

TEST_F(PacketCaptureTest, AccountCaptureKeepsWhatCameBeforeTheLoginAndRedactsSecrets) {
	Start(ServiceType::WORLD);
	Arm(0, 10, eCaptureTarget::ACCOUNT, 7, "Alice");
	const auto alice = Address(0x0100007f, 50000), bob = Address(0x0200007f, 50001);

	WorldPackets::Validation validation;
	validation.username = LUWString(std::string("alice"));
	validation.sessionKey = LUWString(std::string("SECRETSESSIONKEY"));
	Receive(alice, Bytes(validation));
	Receive(bob, Bytes(validation));
	PacketCapture::Update();
	EXPECT_TRUE(captured.records.empty()) << "nothing is known to be the account's yet";

	PacketCapture::Bind(alice, 7, "alice");
	ClientPackets::LoadStaticZone zone;
	zone.mapID = 1100;
	Send(alice, Bytes(zone));
	Send(bob, Bytes(zone));
	WorldPackets::CharacterListRequest list;
	Receive(bob, Bytes(list));
	PacketCapture::Update();

	ASSERT_EQ(captured.records.size(), 2u);
	EXPECT_EQ(PacketDecoder::Decode(captured.records[0].bytes, true).name, "VALIDATION");
	EXPECT_EQ(PacketDecoder::Decode(captured.records[1].bytes, false).name, "LOAD_STATIC_ZONE");
	for (const auto& record : captured.records) {
		EXPECT_EQ(record.header.mask, 1);
		EXPECT_EQ(record.header.accountId, 7u);
		EXPECT_EQ(record.header.source, static_cast<uint8_t>(eCaptureSource::WORLD));
		EXPECT_EQ(record.header.zoneId, 1100);
	}
	EXPECT_EQ(captured.batches.front().slots[0], 10u);
	EXPECT_EQ(captured.all.find(Wide("SECRETSESSIONKEY")), std::string::npos);
	EXPECT_EQ(captured.all.find("SECRETSESSIONKEY"), std::string::npos);
}

TEST_F(PacketCaptureTest, AuthNeverStoresPasswordsOrUserKeys) {
	Start(ServiceType::AUTH);
	Arm(0, 11, eCaptureTarget::ACCOUNT, 7, "alice");
	const auto client = Address(0x0100007f, 50000);

	AuthPackets::LoginRequest login;
	login.username = LUWString(std::string("alice"));
	login.password = LUWString(std::string("hunter2password"), 41);
	Receive(client, Bytes(login));
	PacketCapture::Bind(client, 7, "alice");

	ClientPackets::LoginResponse response;
	response.userKey = LUWString(std::string("USERKEY0123456789"));
	Send(client, Bytes(response));
	// Anything else on auth isn't kept
	MasterPackets::SetSessionKey other;
	Send(client, Bytes(other));
	PacketCapture::Update();

	ASSERT_EQ(captured.records.size(), 2u);
	for (const auto& secret : { std::string("hunter2password"), std::string("USERKEY0123456789"), std::string("alice") }) {
		EXPECT_EQ(captured.all.find(Wide(secret)), std::string::npos) << secret;
		EXPECT_EQ(captured.all.find(secret), std::string::npos) << secret;
	}
	// Still readable, with the secrets blank
	const auto decoded = PacketDecoder::Decode(captured.records[0].bytes, true);
	ASSERT_TRUE(decoded.fields);
	EXPECT_EQ((*decoded.fields)["username"], "");
}

TEST_F(PacketCaptureTest, CharacterCaptureStartsWhenTheCharacterIsPicked) {
	Start(ServiceType::WORLD);
	constexpr LWOOBJID character = 1152921510436607007LL;
	Arm(3, 12, eCaptureTarget::CHARACTER, 7, "alice", { character });
	const auto alice = Address(0x0100007f, 50000);
	PacketCapture::Bind(alice, 7, "alice");
	WorldPackets::CharacterListRequest list;
	Receive(alice, Bytes(list));
	PacketCapture::BindCharacter(alice, character);
	WorldPackets::LevelLoadComplete loaded;
	Receive(alice, Bytes(loaded));
	PacketCapture::Update();
	ASSERT_EQ(captured.records.size(), 1u);
	EXPECT_EQ(captured.records[0].header.mask, 1 << 3);
	EXPECT_EQ(captured.records[0].header.characterId, character);
}

TEST_F(PacketCaptureTest, BroadcastsReachCapturedPlayers) {
	Start(ServiceType::WORLD);
	Arm(0, 13, eCaptureTarget::ACCOUNT, 7, "alice");
	const auto alice = Address(1, 1), bob = Address(2, 2);
	PacketCapture::Bind(alice, 7, "alice");
	PacketCapture::Bind(bob, 8, "bob");
	ClientPackets::LoadStaticZone zone;
	Send(bob, Bytes(zone), true);   // everyone but Bob: Alice gets it
	Send(alice, Bytes(zone), true); // everyone but Alice
	PacketCapture::Update();
	ASSERT_EQ(captured.records.size(), 1u);
	EXPECT_TRUE(captured.records[0].header.flags & PacketRecordFlags::BROADCAST);
}

TEST_F(PacketCaptureTest, EverythingRecordsAllButTheCapturesOwnTraffic) {
	Start(ServiceType::MASTER);
	Arm(1, 14, eCaptureTarget::EVERYTHING);
	MasterPackets::PlayerAdded added;
	Receive(Address(1, 1), Bytes(added));
	MessageCaptureData data;
	Receive(Address(1, 1), Bytes(data));
	PacketCapture::IgnorePeer(Address(9, 9));
	Receive(Address(9, 9), Bytes(added));
	PacketCapture::Update();
	ASSERT_EQ(captured.records.size(), 1u);
	EXPECT_EQ(captured.records[0].header.mask, 1 << 1);
	EXPECT_EQ(captured.records[0].header.source, static_cast<uint8_t>(eCaptureSource::MASTER));
}

TEST_F(PacketCaptureTest, ChatFindsThePlayerInThePacket) {
	Start(ServiceType::CHAT);
	constexpr LWOOBJID character = 1152921510436607007LL;
	Arm(0, 15, eCaptureTarget::ACCOUNT, 7, "alice", { character });
	ChatPackets::GeneralChatMessage mine, theirs;
	mine.playerID = character;
	theirs.playerID = character + 1;
	Receive(Address(1, 1), Bytes(mine));
	Receive(Address(1, 1), Bytes(theirs));
	PacketCapture::Update();
	ASSERT_EQ(captured.records.size(), 1u);
	EXPECT_EQ(captured.records[0].header.characterId, character);
}

TEST_F(PacketCaptureTest, BatchesWaitForMasterAndDropTheOldestPastTheCap) {
	Start(ServiceType::WORLD);
	Arm(0, 16, eCaptureTarget::EVERYTHING);
	PacketCapture::SetSettings({ .flushIntervalMs = 1000, .flushBytes = 1, .maxBufferBytes = 2000 });
	connected = false;
	ClientPackets::LoadStaticZone zone;
	const auto bytes = Bytes(zone);
	for (int i = 0; i < 100; i++) Send(Address(1, 1), bytes);
	PacketCapture::Update();
	EXPECT_TRUE(captured.batches.empty());
	EXPECT_GT(PacketCapture::GetStats().dropped, 0u);

	connected = true;
	PacketCapture::Update();
	ASSERT_FALSE(captured.batches.empty());
	EXPECT_EQ(captured.batches.front().packetsDropped, PacketCapture::GetStats().dropped);
	EXPECT_EQ(captured.records.size() + PacketCapture::GetStats().dropped, 100u);
	// The sequence shows where the gap is
	EXPECT_GT(captured.records.front().header.seq, 1u);
}

TEST_F(PacketCaptureTest, DisarmSendsWhatIsLeftThenStops) {
	Start(ServiceType::WORLD);
	Arm(0, 17, eCaptureTarget::EVERYTHING);
	PacketCapture::SetSettings({ .flushIntervalMs = 60000, .flushBytes = 1024 * 1024, .maxBufferBytes = 1024 * 1024 });
	ClientPackets::LoadStaticZone zone;
	Send(Address(1, 1), Bytes(zone));
	MessageCaptureControl control;
	control.action = eMessageCaptureControl::DISARM;
	control.captureId = 17;
	PacketCapture::Control(control);
	PacketCapture::Update();
	ASSERT_EQ(captured.records.size(), 1u);
	EXPECT_EQ(captured.batches.front().slots[0], 17u) << "sent under the capture it was recorded for";
	EXPECT_FALSE(PacketCapture::g_Armed);
}

TEST(PacketDecoderTests, NamesEveryService) {
	WorldPackets::PositionUpdate position;
	position.update.position = NiPoint3(1, 2, 3);
	RakNet::BitStream stream;
	position.WritePacket(stream);
	const std::string bytes(reinterpret_cast<const char*>(stream.GetData()), stream.GetNumberOfBytesUsed());
	const auto decoded = PacketDecoder::Decode(bytes, true);
	EXPECT_EQ(decoded.service, "WORLD");
	EXPECT_EQ(decoded.name, "POSITION_UPDATE");
	ASSERT_TRUE(decoded.fields);
	EXPECT_FLOAT_EQ((*decoded.fields)["position"][1].get<float>(), 2.0f);
	const auto where = PacketDecoder::Position(bytes);
	ASSERT_TRUE(where);
	EXPECT_FLOAT_EQ(where->z, 3.0f);

	const std::string raknet(1, static_cast<char>(ID_REPLICA_MANAGER_CONSTRUCTION));
	EXPECT_EQ(PacketDecoder::Decode(raknet, false).name, "ID_REPLICA_MANAGER_CONSTRUCTION");
	EXPECT_EQ(PacketDecoder::Name(ServiceType::MASTER, static_cast<uint32_t>(MessageType::Master::SESSION_KEY_RESPONSE)), "SESSION_KEY_RESPONSE");
	EXPECT_TRUE(PacketDecoder::HasSecrets(ServiceType::MASTER, static_cast<uint32_t>(MessageType::Master::SET_SESSION_KEY)));
	EXPECT_FALSE(PacketDecoder::HasSecrets(ServiceType::WORLD, static_cast<uint32_t>(MessageType::World::POSITION_UPDATE)));
}

TEST(PacketDecoderTests, SessionKeysBetweenServersAreBlanked) {
	MasterPackets::SetSessionKey key;
	key.sessionKey = 0xDEADBEEF;
	key.username = LUString("alice");
	RakNet::BitStream stream;
	key.WritePacket(stream);
	std::string bytes(reinterpret_cast<const char*>(stream.GetData()), stream.GetNumberOfBytesUsed());
	ASSERT_TRUE(PacketDecoder::Redact(bytes));
	const uint32_t secret = 0xDEADBEEF;
	EXPECT_EQ(bytes.find(std::string(reinterpret_cast<const char*>(&secret), 4)), std::string::npos);
}

namespace {
	CaptureBundle::Record MakeRecord(const LUBitStream& packet, int64_t timeUs, eCaptureSource source, ePacketDirection direction, LWOOBJID character = 0) {
		CaptureBundle::Record record;
		record.bytes = Bytes(packet);
		record.header.timeUs = timeUs;
		record.header.source = static_cast<uint8_t>(source);
		record.header.direction = static_cast<uint8_t>(direction);
		record.header.characterId = character;
		record.header.zoneId = 1100;
		record.header.length = static_cast<uint32_t>(record.bytes.size());
		record.header.bits = record.header.length * 8;
		return record;
	}
}

TEST(CaptureToolsTests, TimelineTracksAndBundles) {
	constexpr LWOOBJID character = 1152921510436607007LL;
	WorldPackets::PositionUpdate a, b;
	a.update.position = NiPoint3(0, 0, 0);
	b.update.position = NiPoint3(10, 0, 0);
	WorldPackets::CharacterLoginRequest login;
	login.playerID = character;
	CaptureBundle::Bundle bundle;
	bundle.records.push_back(MakeRecord(b, 3000000, eCaptureSource::WORLD, ePacketDirection::RECEIVED, character));
	bundle.records.push_back(MakeRecord(a, 1000000, eCaptureSource::WORLD, ePacketDirection::RECEIVED, character));
	bundle.records.push_back(MakeRecord(login, 500000, eCaptureSource::WORLD, ePacketDirection::RECEIVED, character));
	CaptureTools::SortTimeline(bundle.records);
	EXPECT_EQ(bundle.records.front().header.timeUs, 500000);

	const auto tracks = CaptureTools::Tracks(bundle.records, 0);
	ASSERT_EQ(tracks.size(), 1u);
	ASSERT_EQ(tracks[0].samples.size(), 8u);
	EXPECT_FLOAT_EQ(tracks[0].samples[0], 1.0f);
	EXPECT_FLOAT_EQ(tracks[0].samples[5], 10.0f);

	// Portable: the character's ID is gone from the bytes, a placeholder is in its place
	const auto characters = CaptureTools::MakePortable(bundle);
	ASSERT_EQ(characters.size(), 1u);
	EXPECT_EQ(characters.at("char#1"), character);
	const auto decoded = PacketDecoder::Decode(bundle.records.front().bytes, true);
	EXPECT_EQ((*decoded.fields)["playerID"], std::to_string(CaptureTools::PLACEHOLDER_BASE + 1));
	EXPECT_EQ(bundle.meta["ids"]["char#1"]["kind"], "character");

	// Saved and read back
	const auto path = std::filesystem::temp_directory_path() / "dlu_capture_tools_test.bundle";
	ASSERT_TRUE(CaptureBundle::Save(path, bundle));
	CaptureBundle::Bundle read;
	std::string error;
	ASSERT_TRUE(CaptureBundle::Load(path, read, error)) << error;
	EXPECT_EQ(read.records.size(), 3u);
	EXPECT_EQ(read.records[1].bytes, bundle.records[1].bytes);
	EXPECT_TRUE(read.meta["portable"].get<bool>());
	std::filesystem::remove(path);
}

TEST(CaptureToolsTests, AnonymiseKeepsSizes) {
	WorldPackets::GeneralChatMessage chat;
	chat.message = u"my secret plans";
	CaptureBundle::Bundle bundle;
	bundle.records.push_back(MakeRecord(chat, 1, eCaptureSource::WORLD, ePacketDirection::RECEIVED));
	const auto size = bundle.records[0].bytes.size();
	EXPECT_EQ(CaptureTools::Anonymise(bundle), 1u);
	EXPECT_EQ(bundle.records[0].bytes.size(), size);
	EXPECT_EQ(bundle.records[0].bytes.find(Wide("secret")), std::string::npos);
}

TEST(CaptureToolsTests, DiffPairsAnswersAndIgnoresVolatileFields) {
	ClientPackets::LoadStaticZone zone, otherInstance, otherChecksum;
	zone.mapID = otherInstance.mapID = otherChecksum.mapID = 1100;
	zone.instanceID = 1;
	otherInstance.instanceID = 7; // differs every run: not a difference
	otherChecksum.mapChecksum = 1234;
	ClientPackets::TransferToWorld transfer;
	std::vector<CaptureBundle::Record> expected{ MakeRecord(zone, 1, eCaptureSource::WORLD, ePacketDirection::SENT),
		MakeRecord(zone, 2, eCaptureSource::WORLD, ePacketDirection::SENT), MakeRecord(transfer, 3, eCaptureSource::WORLD, ePacketDirection::SENT) };
	std::vector<CaptureBundle::Record> actual{ MakeRecord(otherInstance, 1, eCaptureSource::WORLD, ePacketDirection::SENT),
		MakeRecord(otherChecksum, 2, eCaptureSource::WORLD, ePacketDirection::SENT), MakeRecord(zone, 3, eCaptureSource::WORLD, ePacketDirection::SENT) };
	const auto report = CaptureTools::Diff(expected, actual);
	EXPECT_EQ(report.expected, 3u);
	EXPECT_EQ(report.matched, 1u);
	EXPECT_EQ(report.differing, 1u);
	EXPECT_EQ(report.missing, 1u);
	EXPECT_EQ(report.extra, 1u);
	EXPECT_EQ(report.missingByName.at("TRANSFER_TO_WORLD"), 1u);
}

/**
 * The cost of capturing everything (docs/CaptureReplay.md, "Overhead"): a synthetic load of world packets through the
 * tap with an EVERYTHING capture armed, then what the dashboard does with the batches: append them to a file, or
 * (for comparison) insert every packet as a row in one SQLite transaction per batch.
 */
TEST_F(PacketCaptureTest, OverheadOfCapturingEverything) {
	using Clock = std::chrono::steady_clock;
	constexpr int PACKETS = 300000;
	Start(ServiceType::WORLD);
	Arm(0, 18, eCaptureTarget::EVERYTHING);
	PacketCapture::SetSettings({ .flushIntervalMs = 1000, .flushBytes = 256 * 1024, .maxBufferBytes = 64 * 1024 * 1024 });
	std::vector<std::string> batches;
	PacketCapture::SetSink([&](MessageCaptureData& data) { batches.push_back(std::move(data.packets)); return true; });

	WorldPackets::PositionUpdate position;
	position.hasVelocity = true;
	const auto bytes = Bytes(position);
	const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
	const auto bits = static_cast<uint32_t>(bytes.size() * 8);

	const auto start = Clock::now();
	for (int i = 0; i < PACKETS; i++) {
		PacketCapture::RecordForTest(Address(static_cast<uint32_t>(i % 100), 1000), (i & 1) != 0, false, data, bits);
		if (i % 1000 == 0) PacketCapture::Update(); // about one main loop frame
	}
	PacketCapture::Update();
	const auto tapNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();

	size_t total = 0;
	for (const auto& batch : batches) total += batch.size();
	ASSERT_EQ(PacketCapture::GetStats().recorded, static_cast<uint64_t>(PACKETS));

	// The dashboard's side: one append per batch
	const auto path = std::filesystem::temp_directory_path() / "dlu_capture_overhead.bundle";
	const auto fileStart = Clock::now();
	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		for (const auto& batch : batches) {
			file.write(batch.data(), static_cast<std::streamsize>(batch.size()));
			file.flush();
		}
	}
	const auto fileNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - fileStart).count();
	std::filesystem::remove(path);

	// For comparison: a row per packet, one transaction per batch
	const auto dbPath = std::filesystem::temp_directory_path() / "dlu_capture_overhead.sqlite";
	std::filesystem::remove(dbPath);
	sqlite3* db = nullptr;
	ASSERT_EQ(sqlite3_open(dbPath.string().c_str(), &db), SQLITE_OK);
	sqlite3_exec(db, "PRAGMA journal_mode=WAL; CREATE TABLE e (session INTEGER, seq INTEGER, t INTEGER, payload BLOB, PRIMARY KEY (session, seq));", nullptr, nullptr, nullptr);
	sqlite3_stmt* insert = nullptr;
	sqlite3_prepare_v2(db, "INSERT INTO e VALUES (1, ?, ?, ?);", -1, &insert, nullptr);
	const auto dbStart = Clock::now();
	int seq = 0;
	for (const auto& batch : batches) {
		sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
		PacketRecord::ForEach(batch, [&](const PacketRecordHeader& header, std::string_view payload) {
			sqlite3_bind_int(insert, 1, ++seq);
			sqlite3_bind_int64(insert, 2, header.timeUs);
			sqlite3_bind_blob(insert, 3, payload.data(), static_cast<int>(payload.size()), SQLITE_STATIC);
			sqlite3_step(insert);
			sqlite3_reset(insert);
		});
		sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
	}
	const auto dbNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - dbStart).count();
	sqlite3_finalize(insert);
	sqlite3_close(db);
	std::filesystem::remove(dbPath);
	std::filesystem::remove(dbPath.string() + "-wal");
	std::filesystem::remove(dbPath.string() + "-shm");

	const double seconds = static_cast<double>(tapNs) / 1e9;
	std::printf("[overhead] %d packets of %zu bytes through the tap: %.0f ns each, %.0f packets/s on one core, %.1f MB/s of records, %zu batches\n",
		PACKETS, bytes.size(), static_cast<double>(tapNs) / PACKETS, PACKETS / seconds, static_cast<double>(total) / 1e6 / seconds, batches.size());
	std::printf("[overhead] dashboard: file append %.1f ms (%.0f ns/packet); SQLite row per packet %.1f ms (%.0f ns/packet)\n",
		static_cast<double>(fileNs) / 1e6, static_cast<double>(fileNs) / PACKETS, static_cast<double>(dbNs) / 1e6, static_cast<double>(dbNs) / PACKETS);
	// Generous bounds: this is a smoke test, the numbers above are the measurement
	EXPECT_LT(static_cast<double>(tapNs) / PACKETS, 20000.0);
}

namespace {
	struct FixtureResult {
		size_t packets{};  // records read
		size_t checked{};  // packets with a struct, read and written again
		size_t gameMessages{}; // of them, client game messages read with the server's own struct
		std::vector<std::string> failures;
	};

	// Game message fields come from the server's own structs (the tests link the game)
	void UseGameMessageDecoder() {
		PacketDecoder::SetGameMessageDecoder([](MessageType::Game id, bool toServer, RakNet::BitStream& payload) {
			return GameMessageDecoder::Decode(id, toServer, payload);
		});
	}

	/**
	 * What every fixture must pass: each packet the server has a struct for reads and writes back to the same bytes,
	 * client game messages included (with the struct the server reads them with), and none fails to decode.
	 */
	FixtureResult CheckFixture(const CaptureBundle::Bundle& bundle) {
		FixtureResult result;
		for (const auto& record : bundle.records) {
			if (record.header.flags & (PacketRecordFlags::GAP | PacketRecordFlags::CUT)) continue;
			result.packets++;
			const auto decoded = PacketDecoder::Decode(record.bytes, CaptureTools::FromClient(record.header));
			const auto where = "record " + std::to_string(record.header.seq) + " " + decoded.name;
			if (decoded.failed) result.failures.push_back(where + " doesn't read with its struct");
			if (const auto same = PacketDecoder::RoundTrip(record.bytes)) {
				result.checked++;
				if (!*same) result.failures.push_back(where + " doesn't write back to the same bytes");
				continue;
			}
			// A client game message: the LU header, the object and the message ID, then its fields
			if (decoded.gameMessageId < 0 || decoded.serviceId != static_cast<uint16_t>(ServiceType::WORLD)) continue;
			constexpr size_t FIELDS = 8 + sizeof(LWOOBJID) + sizeof(uint16_t);
			if (record.bytes.size() < FIELDS) continue;
			RakNet::BitStream payload(reinterpret_cast<unsigned char*>(const_cast<char*>(record.bytes.data())) + FIELDS,
				static_cast<unsigned int>(record.bytes.size() - FIELDS), false);
			const auto same = GameMessageDecoder::RoundTripReceived(static_cast<MessageType::Game>(decoded.gameMessageId), payload);
			if (!same) continue;
			result.checked++;
			result.gameMessages++;
			if (!*same) result.failures.push_back(where + " doesn't write back to the same bits");
		}
		return result;
	}

	// A client game message as it arrives: WORLD GAME_MSG, the object, the message ID, then Serialize
	std::string ClientGameMessage(LWOOBJID object, const GameMessages::NetGameMsg& message) {
		RakNet::BitStream stream;
		LUBitStream(ServiceType::WORLD, MessageType::World::GAME_MSG).WriteHeader(stream);
		stream.Write(object);
		stream.Write(message.msgId);
		message.Serialize(stream);
		return std::string(reinterpret_cast<const char*>(stream.GetData()), stream.GetNumberOfBytesUsed());
	}

	CaptureBundle::Record Recorded(uint32_t seq, eCaptureSource source, bool sent, std::string bytes) {
		CaptureBundle::Record record;
		record.header.seq = seq;
		record.header.timeUs = 1000000 + seq * 1000;
		record.header.source = static_cast<uint8_t>(source);
		record.header.direction = static_cast<uint8_t>(sent ? ePacketDirection::SENT : ePacketDirection::RECEIVED);
		record.header.bits = static_cast<uint32_t>(bytes.size() * 8);
		record.bytes = std::move(bytes);
		return record;
	}
}

/**
 * A synthetic fixture, made here from the server's structs (committed fixtures are never recorded ones): it goes
 * through the same export steps as a real capture (portable, anonymised, saved and read again) and passes the same
 * checks as local fixtures.
 */
TEST(CaptureFixtureTests, SyntheticFixturePassesTheFixtureChecks) {
	UseGameMessageDecoder();
	constexpr LWOOBJID CHARACTER = 1152921510436607007;
	CaptureBundle::Bundle bundle;
	bundle.meta = { {"format", CaptureBundle::FORMAT_VERSION}, {"origin", "dlu-capture"}, {"target", "character"} };

	AuthPackets::LoginRequest login;
	login.username = LUWString("", 33);
	login.password = LUWString("", 41);
	bundle.records.push_back(Recorded(1, eCaptureSource::AUTH, false, Bytes(login)));

	WorldPackets::PositionUpdate position;
	position.update.position = NiPoint3(10.0f, 20.0f, 30.0f);
	bundle.records.push_back(Recorded(2, eCaptureSource::WORLD, false, Bytes(position)));

	GameMessages::RequestUse use;
	use.object = 70000;
	use.secondary = true;
	auto used = Recorded(3, eCaptureSource::WORLD, false, ClientGameMessage(CHARACTER, use));
	used.header.characterId = CHARACTER;
	bundle.records.push_back(used);

	CaptureTools::MakePortable(bundle);
	CaptureTools::Anonymise(bundle);
	const auto path = std::filesystem::temp_directory_path() / "dlu_synthetic_fixture.bundle";
	ASSERT_TRUE(CaptureBundle::Save(path, bundle));
	CaptureBundle::Bundle loaded;
	std::string error;
	ASSERT_TRUE(CaptureBundle::Load(path, loaded, error)) << error;
	std::filesystem::remove(path);
	ASSERT_EQ(loaded.records.size(), 3u);

	const auto result = CheckFixture(loaded);
	EXPECT_TRUE(result.failures.empty()) << result.failures.front();
	EXPECT_EQ(result.checked, 3u);
	EXPECT_EQ(result.gameMessages, 1u);
	// The character's ID is a placeholder once the bundle is portable
	EXPECT_EQ(loaded.records[2].bytes.find(std::string(reinterpret_cast<const char*>(&CHARACTER), sizeof(CHARACTER))), std::string::npos);
}

// A message with more than its struct reads (a field the struct leaves out) fails the check
TEST(CaptureFixtureTests, FixtureCheckCatchesFieldsTheStructLeavesOut) {
	GameMessages::RequestUse use;
	use.object = 70000;
	CaptureBundle::Bundle bundle;
	bundle.records.push_back(Recorded(1, eCaptureSource::WORLD, false, ClientGameMessage(1152921510436607007, use)));
	EXPECT_TRUE(CheckFixture(bundle).failures.empty());
	bundle.records[0].bytes += std::string("\x12\x34", 2);
	EXPECT_EQ(CheckFixture(bundle).failures.size(), 1u);
}

/**
 * Local fixtures (docs/CaptureReplay.md): bundles exported with anonymise=1 and put in tests/fixtures-local (never
 * committed). Every packet the server has a struct for must read and write back to the same bytes, client game
 * messages included.
 */
TEST(CaptureFixtureTests, RecordedPacketsRoundTrip) {
	UseGameMessageDecoder();
	const std::filesystem::path folder = std::filesystem::path(DLU_SOURCE_DIR) / "tests" / "fixtures-local";
	std::error_code ec;
	if (!std::filesystem::is_directory(folder, ec)) GTEST_SKIP() << "No local fixtures in " << folder.string();
	size_t checked = 0, gameMessages = 0, bundles = 0;
	for (const auto& entry : std::filesystem::directory_iterator(folder)) {
		if (entry.path().extension() != ".bundle") continue;
		CaptureBundle::Bundle bundle;
		std::string error;
		ASSERT_TRUE(CaptureBundle::Load(entry.path(), bundle, error)) << entry.path() << ": " << error;
		bundles++;
		const auto result = CheckFixture(bundle);
		for (const auto& failure : result.failures) ADD_FAILURE() << entry.path().filename() << " " << failure;
		checked += result.checked;
		gameMessages += result.gameMessages;
	}
	if (bundles == 0) GTEST_SKIP() << "No .bundle files in " << folder.string();
	std::printf("[fixtures] %zu bundle(s), %zu packets checked (%zu client game messages)\n", bundles, checked, gameMessages);
}
