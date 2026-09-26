#include <gtest/gtest.h>

#include "DashboardMessages.h"
#include "EconomyScan.h"
#include "IDashboardAdmin.h"
#include "Totp.h"
#include "WebhookFormat.h"

namespace {
	std::vector<uint8_t> Bytes(const std::string& text) { return std::vector<uint8_t>(text.begin(), text.end()); }
}

// ---- TOTP ----

TEST(TotpTests, MatchesRfc6238Vectors) {
	// RFC 6238 appendix B, SHA1, secret "12345678901234567890"; the last 6 of its 8-digit codes
	const auto secret = Bytes("12345678901234567890");
	EXPECT_EQ(Totp::CodeAt(secret, 59 / 30), "287082");
	EXPECT_EQ(Totp::CodeAt(secret, 1111111109 / 30), "081804");
	EXPECT_EQ(Totp::CodeAt(secret, 1111111111 / 30), "050471");
	EXPECT_EQ(Totp::CodeAt(secret, 1234567890 / 30), "005924");
	EXPECT_EQ(Totp::CodeAt(secret, 2000000000 / 30), "279037");
}

TEST(TotpTests, Base32MatchesRfc4648) {
	EXPECT_EQ(Totp::Base32Encode(Bytes("foobar")), "MZXW6YTBOI");
	EXPECT_EQ(Totp::Base32Encode(Bytes("f")), "MY");
	const auto decoded = Totp::Base32Decode("mzxw 6ytb oi");
	ASSERT_TRUE(decoded.has_value());
	EXPECT_EQ(std::string(decoded->begin(), decoded->end()), "foobar");
	EXPECT_FALSE(Totp::Base32Decode("MZ1W").has_value()); // 1 is not base32
}

TEST(TotpTests, VerifyAllowsOneStepOfDrift) {
	const auto secretText = Totp::Base32Encode(Bytes("12345678901234567890"));
	const int64_t time = 1111111111;
	const auto secret = Bytes("12345678901234567890");
	EXPECT_EQ(Totp::Verify(secretText, Totp::CodeAt(secret, time / 30), time), time / 30);
	EXPECT_EQ(Totp::Verify(secretText, Totp::CodeAt(secret, time / 30 - 1), time), time / 30 - 1);
	EXPECT_EQ(Totp::Verify(secretText, Totp::CodeAt(secret, time / 30 + 1), time), time / 30 + 1);
	EXPECT_FALSE(Totp::Verify(secretText, Totp::CodeAt(secret, time / 30 + 2), time).has_value());
	EXPECT_EQ(Totp::Verify(secretText, "050 471", time), time / 30); // spaces are fine
	EXPECT_FALSE(Totp::Verify(secretText, "12345", time).has_value());
	EXPECT_FALSE(Totp::Verify("", "050471", time).has_value());
}

TEST(TotpTests, GeneratedSecretsAreUsable) {
	const auto secret = Totp::GenerateSecret();
	EXPECT_EQ(secret.size(), 32u); // 160 bits
	EXPECT_NE(secret, Totp::GenerateSecret());
	EXPECT_TRUE(Totp::Base32Decode(secret).has_value());
}

TEST(TotpTests, ProvisioningUriIsEncoded) {
	EXPECT_EQ(Totp::ProvisioningUri("My Server", "bob smith", "ABC"),
		"otpauth://totp/My%20Server:bob%20smith?secret=ABC&issuer=My%20Server&algorithm=SHA1&digits=6&period=30");
}

TEST(TotpTests, RecoveryCodes) {
	const auto codes = Totp::GenerateRecoveryCodes(10);
	ASSERT_EQ(codes.size(), 10u);
	for (const auto& code : codes) {
		EXPECT_EQ(code.size(), 14u); // xxxx-xxxx-xxxx
		EXPECT_EQ(Totp::NormalizeRecoveryCode(code).size(), 12u);
	}
	EXPECT_EQ(Totp::NormalizeRecoveryCode(" AbCd-EFGH-jkmn "), "abcdefghjkmn");
}

TEST(TotpTests, SecretsAreEncryptedAndAuthenticated) {
	Totp::SetKeyForTesting(std::vector<uint8_t>(32, 7));
	const auto encrypted = Totp::EncryptSecret("JBSWY3DPEHPK3PXP");
	ASSERT_TRUE(encrypted.has_value());
	EXPECT_EQ(encrypted->find("JBSWY3DPEHPK3PXP"), std::string::npos);
	EXPECT_NE(*encrypted, *Totp::EncryptSecret("JBSWY3DPEHPK3PXP")); // fresh nonce each time
	EXPECT_EQ(Totp::DecryptSecret(*encrypted), "JBSWY3DPEHPK3PXP");

	auto tampered = *encrypted;
	tampered[30] = tampered[30] == 'a' ? 'b' : 'a';
	EXPECT_FALSE(Totp::DecryptSecret(tampered).has_value());

	Totp::SetKeyForTesting(std::vector<uint8_t>(32, 8));
	EXPECT_FALSE(Totp::DecryptSecret(*encrypted).has_value());
	Totp::SetKeyForTesting({});
	EXPECT_FALSE(Totp::EncryptSecret("X").has_value());
}

// ---- Webhooks ----

TEST(WebhookFormatTests, EventMatching) {
	EXPECT_TRUE(WebhookFormat::Matches("*", "bug_report"));
	EXPECT_TRUE(WebhookFormat::Matches("moderation, bug_report", "bug_report"));
	EXPECT_FALSE(WebhookFormat::Matches("moderation", "bug_report"));
	EXPECT_TRUE(WebhookFormat::Matches("moderation", "test")); // tests always go through
	EXPECT_EQ(WebhookFormat::NormalizeEvents("bug_report, nonsense ,server,test"), "bug_report,server");
	EXPECT_EQ(WebhookFormat::NormalizeEvents("server,*"), "*");
	EXPECT_EQ(WebhookFormat::NormalizeEvents("nonsense"), "");
}

TEST(WebhookFormatTests, BodiesPerFormat) {
	const WebhookFormat::Alert alert{ "bug_report", "Bug report #5", "It broke @everyone", { { "Reporter", "Bob" } }, "https://dash.example/bug_reports/5", 0 };

	const auto discord = nlohmann::json::parse(WebhookFormat::BuildBody("discord", alert));
	EXPECT_EQ(discord["embeds"][0]["title"], "Bug report #5");
	EXPECT_EQ(discord["embeds"][0]["url"], "https://dash.example/bug_reports/5");
	EXPECT_EQ(discord["embeds"][0]["fields"][0]["value"], "Bob");
	EXPECT_EQ(discord["embeds"][0]["timestamp"], "1970-01-01T00:00:00Z");
	EXPECT_TRUE(discord["allowed_mentions"]["parse"].empty()); // player text can't ping anyone

	const auto slack = nlohmann::json::parse(WebhookFormat::BuildBody("slack", alert));
	EXPECT_NE(slack["text"].get<std::string>().find("*Reporter:* Bob"), std::string::npos);

	const auto json = nlohmann::json::parse(WebhookFormat::BuildBody("json", alert));
	EXPECT_EQ(json["event"], "bug_report");
	EXPECT_EQ(json["fields"]["Reporter"], "Bob");
}

TEST(WebhookFormatTests, LongTextIsCutForDiscord) {
	const WebhookFormat::Alert alert{ "bug_report", std::string(300, 'x'), std::string(5000, 'y'), {}, "", 0 };
	const auto discord = nlohmann::json::parse(WebhookFormat::BuildBody("discord", alert));
	EXPECT_LE(discord["embeds"][0]["title"].get<std::string>().size(), 256u);
	EXPECT_LE(discord["embeds"][0]["description"].get<std::string>().size(), 4000u);
}

TEST(WebhookFormatTests, SignatureIsHmacSha256) {
	EXPECT_EQ(WebhookFormat::Sign("key", "The quick brown fox jumps over the lazy dog"),
		"f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
}

TEST(WebhookFormatTests, UrlChecks) {
	EXPECT_FALSE(WebhookFormat::ValidateUrl("https://discord.com/api/webhooks/1/abc").has_value());
	EXPECT_FALSE(WebhookFormat::ValidateUrl("http://localhost:8080/hook").has_value());
	EXPECT_TRUE(WebhookFormat::ValidateUrl("ftp://example.com/x").has_value());
	EXPECT_TRUE(WebhookFormat::ValidateUrl("file:///etc/passwd").has_value());
	EXPECT_TRUE(WebhookFormat::ValidateUrl("https://user:pw@example.com/").has_value());
	EXPECT_TRUE(WebhookFormat::ValidateUrl("https:///nohost").has_value());
	EXPECT_TRUE(WebhookFormat::ValidateUrl("https://example.com/a b").has_value());
	EXPECT_EQ(WebhookFormat::MaskUrl("https://discord.com/api/webhooks/123/secret-token"), "https://discord.com/…");
	EXPECT_EQ(WebhookFormat::MaskUrl("https://hooks.example/SECRETTOKEN"), "https://hooks.example/…");
	EXPECT_EQ(WebhookFormat::MaskUrl("https://example.com/?token=abc"), "https://example.com/…");
	EXPECT_EQ(WebhookFormat::MaskUrl("https://example.com"), "https://example.com");
}

// ---- Economy checks ----

TEST(EconomyChecksTests, UnusualIncome) {
	int64_t median = 0;
	std::vector<std::pair<LWOOBJID, int64_t>> incomes{ { 1, 1000 }, { 2, 1200 }, { 3, 900 }, { 4, 500000 }, { 5, 1100 } };
	const auto flagged = EconomyScan::UnusualIncome(incomes, 20, 100000, median);
	EXPECT_EQ(median, 1100);
	ASSERT_EQ(flagged.size(), 1u);
	EXPECT_EQ(flagged[0].first, 4);

	// Everyone earns little: the minimum keeps a quiet day from flagging anyone
	EXPECT_TRUE(EconomyScan::UnusualIncome({ { 1, 10 }, { 2, 5000 } }, 20, 100000, median).empty());
	EXPECT_TRUE(EconomyScan::UnusualIncome({}, 20, 100000, median).empty());
}

TEST(EconomyChecksTests, ItemSpikes) {
	const std::map<LOT, int64_t> day{ { 1, 50 }, { 2, 5000 }, { 3, 300 } };
	const std::map<LOT, int64_t> history{ { 1, 1500 }, { 2, 3000 } }; // 30 days: 50/day and 100/day
	const auto spikes = EconomyScan::ItemSpikes(day, history, 30, 10, 200);
	ASSERT_EQ(spikes.size(), 2u);
	EXPECT_EQ(spikes[0].lot, 2); // 5000 > 10 x 100
	EXPECT_EQ(spikes[0].dailyAverage, 100);
	EXPECT_EQ(spikes[1].lot, 3); // new item above the minimum
}

TEST(EconomyChecksTests, MonthStartDay) {
	EXPECT_EQ(IDashboardAdmin::MonthStartDay(20721), 20697u); // 2026-09-25
	EXPECT_EQ(IDashboardAdmin::MonthStartDay(20697), 20697u); // 2026-09-01
	EXPECT_EQ(IDashboardAdmin::MonthStartDay(19782), 19754u); // 2024-02-29
	EXPECT_EQ(IDashboardAdmin::MonthStartDay(11017), 11017u); // 2000-03-01
	EXPECT_EQ(IDashboardAdmin::MonthStartDay(0), 0u);
}

// ---- Messages ----

TEST(DashboardMessagesTests, PlayerPositionsRoundTrip) {
	PlayerPositions positions;
	positions.zoneId = 1100;
	positions.instanceId = 3;
	positions.cloneId = 0;
	positions.players.push_back({ 1152921504606846999LL, 1.5f, 2.5f, -3.5f });
	RakNet::BitStream stream;
	positions.Serialize(stream);
	PlayerPositions read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.zoneId, 1100u);
	ASSERT_EQ(read.players.size(), 1u);
	EXPECT_EQ(read.players[0].characterId, 1152921504606846999LL);
	EXPECT_FLOAT_EQ(read.players[0].z, -3.5f);
}

TEST(DashboardMessagesTests, AnnouncementRoundTripAndLimits) {
	Announcement announcement{ "Title", "Hello everyone" };
	RakNet::BitStream stream;
	announcement.Serialize(stream);
	Announcement read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.title, "Title");
	EXPECT_EQ(read.message, "Hello everyone");

	RakNet::BitStream tooLong;
	tooLong.Write<uint16_t>(Announcement::MAX_TITLE + 1);
	EXPECT_FALSE(read.Deserialize(tooLong));
}
