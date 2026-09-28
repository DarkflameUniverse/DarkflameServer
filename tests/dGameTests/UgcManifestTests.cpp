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
