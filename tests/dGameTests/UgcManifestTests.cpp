#include "UgcManifest.h"

#include "ClientPackets.h"
#include "dConfig.h"
#include "GameDependencies.h"
#include "MD5.h"
#include "PacketTestUtils.h"
#include "Sd0.h"

#include <gtest/gtest.h>

using namespace PacketTestUtils;
using UgcManifest::eAction;

namespace {
	std::string Md5Hex(const std::string& data) {
		MD5 md5;
		md5.update(reinterpret_cast<const unsigned char*>(data.data()), static_cast<MD5::size_type>(data.size()));
		md5.finalize();
		return md5.hexdigest();
	}
}

// What each request gets. A placed model's client asks for its NIF, HKX and LXFML and waits for every answer without a
// timeout (LWOResMgr2Interface::RequestBlueprintManifestThenLoad, 0x0105a910), so model files are always answered:
// from the UGC server when it made the mesh and models are served, else with the LXFML for the client to build. The HKX
// always gets the LXFML: the UGC server makes no physics, the client builds its own (then gets the served mesh).
TEST(UgcManifestTests, DecideCoversEveryType) {
	using T = eUgcResourceType;
	for (const auto type : { T::LXFML, T::NIF, T::HKX, T::DDS }) {
		EXPECT_EQ(UgcManifest::Decide(type, false, true, true), eAction::NONE);
	}
	EXPECT_EQ(UgcManifest::Decide(T::DDS, true, false, false), eAction::WAIT);
	EXPECT_EQ(UgcManifest::Decide(T::DDS, true, true, true), eAction::WAIT);

	EXPECT_EQ(UgcManifest::Decide(T::NIF, true, true, true), eAction::ANSWER);
	EXPECT_EQ(UgcManifest::Decide(T::LXFML, true, true, true), eAction::ANSWER);
	EXPECT_EQ(UgcManifest::Decide(T::HKX, true, true, true), eAction::SEND_LXFML);

	for (const auto type : { T::LXFML, T::NIF, T::HKX }) {
		EXPECT_EQ(UgcManifest::Decide(type, true, true, false), eAction::SEND_LXFML);  // not made yet
		EXPECT_EQ(UgcManifest::Decide(type, true, false, true), eAction::SEND_LXFML);  // models not served
		EXPECT_EQ(UgcManifest::Decide(type, true, false, false), eAction::SEND_LXFML);
	}
	EXPECT_EQ(UgcManifest::Decide(static_cast<T>(7), true, true, true), eAction::NONE);
}

class UgcManifestRequestTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override {
		UgcManifest::OnDisconnect(Client());
		TearDownDependencies();
	}

	static SystemAddress Client() {
		SystemAddress address;
		address.binaryAddress = 0x0100007f;
		address.port = 15001;
		return address;
	}

	void Settings(const std::string& manifest, const std::string& models) {
		Game::config->SetDatabaseValues({ { "ugc_manifest", manifest }, { "ugc_manifest_models", models } }, {});
	}
};

// Off (the default): nothing is answered, as before
TEST_F(UgcManifestRequestTests, NothingWhenOff) {
	Settings("0", "1");
	const auto sent = Capture([] { UgcManifest::OnRequest(Client(), 1234, eUgcResourceType::NIF); });
	EXPECT_TRUE(sent.empty());
	EXPECT_FALSE(UgcManifest::ServesModels());
	EXPECT_FALSE(UgcManifest::ServesMesh(1234));
}

// A model file of a blueprint that isn't a player model: answered as not known (37 bytes, valid 0), never left waiting
TEST_F(UgcManifestRequestTests, UnknownModelFilesAreAnsweredNotKnown) {
	Settings("1", "1");
	EXPECT_TRUE(UgcManifest::ServesModels());
	EXPECT_FALSE(UgcManifest::ServesMesh(1234)); // its mesh isn't made
	for (const auto type : { eUgcResourceType::NIF, eUgcResourceType::HKX, eUgcResourceType::LXFML }) {
		const auto sent = Capture([type] { UgcManifest::OnRequest(Client(), 0x0102030405060708, type); });
		ASSERT_EQ(sent.size(), 1);
		EXPECT_EQ(sent[0].sysAddr, Client());
		EXPECT_FALSE(sent[0].broadcast);
		EXPECT_PACKET_EQ(FromHex("53 05 00 3c 00 00 00 00 08 07 06 05 04 03 02 01 0" + std::to_string(static_cast<int>(type)) +
			" 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"), FromCapture(sent[0]));
	}
}

// An icon that isn't made yet waits (answered by Update once the UGC server stored its checksum)
TEST_F(UgcManifestRequestTests, IconsWait) {
	Settings("1", "0");
	const auto sent = Capture([] { UgcManifest::OnRequest(Client(), 1234, eUgcResourceType::DDS); });
	EXPECT_TRUE(sent.empty());
}

// Models made while nobody is on a property with them: nothing to tell
TEST_F(UgcManifestRequestTests, ModelsMadeWithNobodyToTell) {
	Settings("1", "1");
	const auto sent = Capture([] { UgcManifest::OnModelsMade({ 1234, 5678 }); });
	EXPECT_TRUE(sent.empty());
}

// The client checks the MD5 and size of the LXFML as it has it after inflating the UGC server's .lxfml.sd0, which is
// the stored LXFML (sd0, or plain XML in old rows) inflated
TEST_F(UgcManifestRequestTests, LxfmlChecksumIsOfTheInflatedLxfml) {
	const std::string lxfml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n<LXFML versionMajor=\"5\"></LXFML>\n";
	const auto plain = UgcManifest::LxfmlChecksum(lxfml);
	ASSERT_TRUE(plain.has_value());
	EXPECT_EQ(plain->md5, Md5Hex(lxfml));
	EXPECT_EQ(plain->size, lxfml.size());

	const auto compressed = UgcManifest::LxfmlChecksum(Sd0::Compress(lxfml));
	ASSERT_TRUE(compressed.has_value());
	EXPECT_EQ(compressed->md5, plain->md5);
	EXPECT_EQ(compressed->size, plain->size);

	EXPECT_FALSE(UgcManifest::LxfmlChecksum("").has_value());
	EXPECT_FALSE(UgcManifest::LxfmlChecksum("not an sd0 stream").has_value());
}

namespace {
	SystemAddress Address(uint16_t port) {
		SystemAddress address;
		address.binaryAddress = 0x0100007f;
		address.port = port;
		return address;
	}
}

// A client never sent the model's LXFML (it built the model long ago, or never): switched at once
TEST(UgcServedMeshSwitchesTests, SwitchedAtOnceWithoutARecentLxfml) {
	UgcManifest::ServedMeshSwitches switches;
	const auto t0 = UgcManifest::Clock::time_point{} + std::chrono::hours(1);
	switches.Schedule(Address(1), 1234, t0);
	const auto due = switches.TakeDue(t0);
	ASSERT_EQ(due.size(), 1);
	EXPECT_EQ(due[0].sysAddr, Address(1));
	EXPECT_EQ(due[0].blueprintId, 1234);
	EXPECT_EQ(switches.Pending(), 0);
	EXPECT_TRUE(switches.TakeDue(t0 + std::chrono::minutes(5)).empty()); // once
}

// A client sent the model's LXFML lately is building the model, and its build writes its own NIF and caches its checksum
// over the served one (LWOBBBInterface::GenerateModelFromLxfml, 0x00b6c220; MainThread_ProcessModelResponse,
// 0x00b5a1e0): it's switched BUILD_SETTLE after that send, not before
TEST(UgcServedMeshSwitchesTests, WaitsForTheClientsOwnBuild) {
	UgcManifest::ServedMeshSwitches switches;
	const auto t0 = UgcManifest::Clock::time_point{} + std::chrono::hours(1);
	switches.LxfmlSent(Address(1), 1234, t0);
	switches.Schedule(Address(1), 1234, t0 + std::chrono::seconds(5));
	switches.Schedule(Address(2), 1234, t0 + std::chrono::seconds(5)); // another client: never sent it

	auto due = switches.TakeDue(t0 + std::chrono::seconds(5));
	ASSERT_EQ(due.size(), 1);
	EXPECT_EQ(due[0].sysAddr, Address(2));
	EXPECT_TRUE(switches.TakeDue(t0 + UgcManifest::BUILD_SETTLE - std::chrono::seconds(1)).empty());
	due = switches.TakeDue(t0 + UgcManifest::BUILD_SETTLE);
	ASSERT_EQ(due.size(), 1);
	EXPECT_EQ(due[0].sysAddr, Address(1));
	EXPECT_EQ(due[0].blueprintId, 1234);
}

// The LXFML sent again while a switch waits (the client asked again): it starts another build, so the switch waits again
TEST(UgcServedMeshSwitchesTests, AnotherLxfmlPushesTheSwitchBack) {
	UgcManifest::ServedMeshSwitches switches;
	const auto t0 = UgcManifest::Clock::time_point{} + std::chrono::hours(1);
	switches.LxfmlSent(Address(1), 1234, t0);
	switches.Schedule(Address(1), 1234, t0);
	switches.LxfmlSent(Address(1), 1234, t0 + std::chrono::seconds(20));
	EXPECT_TRUE(switches.TakeDue(t0 + UgcManifest::BUILD_SETTLE).empty());
	EXPECT_EQ(switches.TakeDue(t0 + std::chrono::seconds(20) + UgcManifest::BUILD_SETTLE).size(), 1);

	// Another model's LXFML doesn't hold this one
	switches.LxfmlSent(Address(1), 5678, t0 + std::chrono::minutes(2));
	switches.Schedule(Address(1), 1234, t0 + std::chrono::minutes(2));
	EXPECT_EQ(switches.TakeDue(t0 + std::chrono::minutes(2)).size(), 1);
}

// SentWithin is the 10 second guard against sending the LXFML for each of a model's three requests
TEST(UgcServedMeshSwitchesTests, SentWithin) {
	UgcManifest::ServedMeshSwitches switches;
	const auto t0 = UgcManifest::Clock::time_point{} + std::chrono::hours(1);
	EXPECT_FALSE(switches.SentWithin(Address(1), 1234, std::chrono::seconds(10), t0));
	switches.LxfmlSent(Address(1), 1234, t0);
	EXPECT_TRUE(switches.SentWithin(Address(1), 1234, std::chrono::seconds(10), t0 + std::chrono::seconds(9)));
	EXPECT_FALSE(switches.SentWithin(Address(1), 1234, std::chrono::seconds(10), t0 + std::chrono::seconds(10)));
	EXPECT_FALSE(switches.SentWithin(Address(2), 1234, std::chrono::seconds(10), t0));
}

// A client that leaves loses its pending switches and sends
TEST(UgcServedMeshSwitchesTests, ForgetDropsTheClient) {
	UgcManifest::ServedMeshSwitches switches;
	const auto t0 = UgcManifest::Clock::time_point{} + std::chrono::hours(1);
	switches.LxfmlSent(Address(1), 1234, t0);
	switches.Schedule(Address(1), 1234, t0);
	switches.Schedule(Address(2), 1234, t0 + std::chrono::seconds(1));
	switches.Forget(Address(1));
	EXPECT_FALSE(switches.SentWithin(Address(1), 1234, UgcManifest::BUILD_SETTLE, t0));
	const auto due = switches.TakeDue(t0 + std::chrono::hours(1));
	ASSERT_EQ(due.size(), 1);
	EXPECT_EQ(due[0].sysAddr, Address(2));
}

// The switch of one client: the served NIF's checksum first (the client's cached one is its own build's), then
// NotifyClientUGCModelReady to each placed model, all to that client only
TEST_F(UgcManifestRequestTests, SwitchSendsTheChecksumThenModelReady) {
	Settings("1", "1");
	const IUgc::FileChecksum checksum{ "00112233445566778899aabbccddeeff", 0x01020304 };
	const auto sent = Capture([&] { UgcManifest::SwitchClient(Client(), 0x0102030405060708, checksum, { 0x1122334455667788, 0x1122334455667799 }); });
	ASSERT_EQ(sent.size(), 3);
	for (const auto& packet : sent) {
		EXPECT_EQ(packet.sysAddr, Client());
		EXPECT_FALSE(packet.broadcast);
	}
	// UGC_MANIFEST_RESPONSE: blueprint, type 1 (NIF), valid, size, MD5
	EXPECT_PACKET_EQ(FromHex("53 05 00 3c 00 00 00 00 08 07 06 05 04 03 02 01 01 01 04 03 02 01 "
		"00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff"), FromCapture(sent[0]));
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 88 77 66 55 44 33 22 11 8d 03 08 07 06 05 04 03 02 01"), FromCapture(sent[1]));
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 99 77 66 55 44 33 22 11 8d 03 08 07 06 05 04 03 02 01"), FromCapture(sent[2]));
}

namespace {
	// A property with four placed models: 1234 (made) twice, 5678 (not made) and a prefab without a blueprint
	class PropertyDatabase : public TestSQLDatabase {
	public:
		std::vector<IPropertyContents::Model> GetPropertyModels(const LWOOBJID&) override {
			std::vector<IPropertyContents::Model> models(4);
			models[0].ugcId = 1234;
			models[1].ugcId = 1234;
			models[2].ugcId = 5678;
			return models;
		}
		std::optional<IUgc::FileChecksum> GetUgcFileChecksum(const LWOOBJID blueprintId, const std::string_view file) override {
			if (blueprintId == 1234 && file == "model.nif") return IUgc::FileChecksum{ "00112233445566778899aabbccddeeff", 0x01020304 };
			return std::nullopt;
		}
	};
}

// A property load sends the served NIF checksum of each made model once, before the models are constructed: the
// client's cached one can be its own build's, which it would use as it is
TEST_F(UgcManifestRequestTests, PropertyLoadSendsTheServedChecksums) {
	Database::_setDatabase(new PropertyDatabase());
	Settings("1", "1");
	const auto sent = Capture([] { EXPECT_EQ(UgcManifest::OnPropertyLoading(Client(), 42), 1); });
	ASSERT_EQ(sent.size(), 1);
	EXPECT_EQ(sent[0].sysAddr, Client());
	EXPECT_PACKET_EQ(FromHex("53 05 00 3c 00 00 00 00 d2 04 00 00 00 00 00 00 01 01 04 03 02 01 "
		"00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff"), FromCapture(sent[0]));

	// Models not served: nothing
	Settings("1", "0");
	EXPECT_TRUE(Capture([] { EXPECT_EQ(UgcManifest::OnPropertyLoading(Client(), 42), 0); }).empty());
}
