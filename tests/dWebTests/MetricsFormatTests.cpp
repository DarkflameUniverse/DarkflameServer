#include <gtest/gtest.h>
#include <limits>
#include "MetricsFormat.h"

using namespace MetricsFormat;

TEST(MetricsFormatTests, LabelValuesAreEscaped) {
	EXPECT_EQ(EscapeLabelValue("Nimbus Station"), "Nimbus Station");
	EXPECT_EQ(EscapeLabelValue(R"(a "quoted" \ name)"), R"(a \"quoted\" \\ name)");
	EXPECT_EQ(EscapeLabelValue("two\nlines"), "two\\nlines");
	// Help text keeps its quotes
	EXPECT_EQ(EscapeHelp("say \"hi\"\\\n"), "say \"hi\"\\\\\\n");
}

TEST(MetricsFormatTests, NamesAreMadeValid) {
	EXPECT_EQ(Name("darkflame_players_online"), "darkflame_players_online");
	EXPECT_EQ(Name("darkflame:rate"), "darkflame:rate");
	EXPECT_EQ(Name("darkflame:rate", true), "darkflame_rate");
	EXPECT_EQ(Name("9lives-left"), "_lives_left");
	EXPECT_EQ(Name(""), "_");
}

TEST(MetricsFormatTests, ValuesArePrometheusNumbers) {
	EXPECT_EQ(Value(0), "0");
	EXPECT_EQ(Value(42), "42");
	EXPECT_EQ(Value(-3), "-3");
	EXPECT_EQ(Value(1234567890123.0), "1234567890123");
	EXPECT_EQ(Value(0.25), "0.25");
	EXPECT_EQ(Value(std::numeric_limits<double>::quiet_NaN()), "NaN");
	EXPECT_EQ(Value(std::numeric_limits<double>::infinity()), "+Inf");
	EXPECT_EQ(Value(-std::numeric_limits<double>::infinity()), "-Inf");
}

TEST(MetricsFormatTests, FamiliesAreWrittenTogetherOnce) {
	Writer w;
	w.Add("darkflame_zone_players", "Players in a zone", "gauge", { {"zone_id", "1000"}, {"zone_name", "Venture \"Explorer\""} }, 3);
	w.Add("darkflame_up", "Up", "gauge", 1);
	w.Add("darkflame_zone_players", "ignored: the first help counts", "gauge", { {"zone_id", "1100"}, {"zone_name", "Avant Gardens"} }, 5);
	EXPECT_EQ(w.Text(),
		"# HELP darkflame_zone_players Players in a zone\n"
		"# TYPE darkflame_zone_players gauge\n"
		"darkflame_zone_players{zone_id=\"1000\",zone_name=\"Venture \\\"Explorer\\\"\"} 3\n"
		"darkflame_zone_players{zone_id=\"1100\",zone_name=\"Avant Gardens\"} 5\n"
		"# HELP darkflame_up Up\n"
		"# TYPE darkflame_up gauge\n"
		"darkflame_up 1\n");
}

TEST(MetricsFormatTests, DeclaredFamiliesWithoutSamplesKeepTheirHeader) {
	Writer w;
	w.Declare("darkflame_chat_messages_total", "Chat messages", "counter");
	w.Declare("darkflame_chat_messages_total", "Declared twice", "counter");
	w.Add("darkflame_chat_messages_total", "", "counter", { {"channel", "zone"} }, 7);
	EXPECT_EQ(w.Text(),
		"# HELP darkflame_chat_messages_total Chat messages\n"
		"# TYPE darkflame_chat_messages_total counter\n"
		"darkflame_chat_messages_total{channel=\"zone\"} 7\n");

	Writer empty;
	empty.Declare("darkflame_instance_players", "Players in one instance", "gauge");
	EXPECT_EQ(empty.Text(), "# HELP darkflame_instance_players Players in one instance\n# TYPE darkflame_instance_players gauge\n");
}

TEST(MetricsFormatTests, TokensCompareExactly) {
	EXPECT_TRUE(ConstantTimeEquals("s3cret-scraper-token", "s3cret-scraper-token"));
	EXPECT_FALSE(ConstantTimeEquals("s3cret-scraper-token", "s3cret-scraper-tokeN"));
	EXPECT_FALSE(ConstantTimeEquals("s3cret", "s3cret-scraper-token"));
	EXPECT_FALSE(ConstantTimeEquals("s3cret-scraper-token", "s3cret"));
	EXPECT_FALSE(ConstantTimeEquals("", "x"));
	EXPECT_TRUE(ConstantTimeEquals("", ""));
}

TEST(MetricsFormatTests, AllowListMatchesAddressesAndRanges) {
	EXPECT_TRUE(AddressAllowed("", "203.0.113.9"));
	EXPECT_TRUE(AddressAllowed(" , ", "203.0.113.9"));
	EXPECT_TRUE(AddressAllowed("127.0.0.1", "127.0.0.1"));
	EXPECT_FALSE(AddressAllowed("127.0.0.1", "127.0.0.2"));
	EXPECT_TRUE(AddressAllowed("127.0.0.1, 10.0.0.0/8", "10.20.30.40"));
	EXPECT_FALSE(AddressAllowed("10.0.0.0/8", "11.0.0.1"));
	EXPECT_TRUE(AddressAllowed("192.168.1.0/24", "192.168.1.255"));
	EXPECT_FALSE(AddressAllowed("192.168.1.0/24", "192.168.2.1"));
	EXPECT_TRUE(AddressAllowed("0.0.0.0/0", "8.8.8.8"));
	EXPECT_TRUE(AddressAllowed("::1", "::1"));
	// Malformed entries match nothing, and still make the list non-empty
	EXPECT_FALSE(AddressAllowed("10.0.0.0/33", "10.0.0.1"));
	EXPECT_FALSE(AddressAllowed("10.0.0.0/8x", "10.0.0.1"));
	EXPECT_FALSE(AddressAllowed("10.0.0/8", "10.0.0.1"));
	EXPECT_FALSE(AddressAllowed("10.0.0.0/8", "10.0.0.1.5"));
	EXPECT_FALSE(AddressAllowed("10.0.0.0/8", "unknown"));
}
