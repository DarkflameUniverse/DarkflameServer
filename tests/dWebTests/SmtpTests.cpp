#include <gtest/gtest.h>
#include "SmtpClient.h"

namespace {
	std::string Decode(const std::string& in) {
		static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		int val = 0, bits = -8;
		for (const char c : in) {
			const auto pos = chars.find(c);
			if (pos == std::string::npos) continue;
			val = (val << 6) + static_cast<int>(pos);
			bits += 6;
			if (bits >= 0) {
				out.push_back(static_cast<char>((val >> bits) & 0xFF));
				bits -= 8;
			}
		}
		return out;
	}

	Smtp::Config TestConfig() {
		Smtp::Config config;
		config.fromAddress = "noreply@example.com";
		config.fromName = "Test Server";
		return config;
	}
}

TEST(SmtpTest, ValidatesAddresses) {
	EXPECT_TRUE(Smtp::IsValidAddress("player@example.com"));
	EXPECT_TRUE(Smtp::IsValidAddress("first.last+tag@sub.example.co.uk"));
	EXPECT_FALSE(Smtp::IsValidAddress("no-at-sign.example.com"));
	EXPECT_FALSE(Smtp::IsValidAddress("two@@example.com"));
	EXPECT_FALSE(Smtp::IsValidAddress("@example.com"));
	EXPECT_FALSE(Smtp::IsValidAddress("user@localhost"));
	EXPECT_FALSE(Smtp::IsValidAddress("user@example.com>\r\nRCPT TO:<victim@example.com"));
	EXPECT_FALSE(Smtp::IsValidAddress("has space@example.com"));
	EXPECT_FALSE(Smtp::IsValidAddress("a<b@example.com"));
}

TEST(SmtpTest, Base64MatchesKnownValues) {
	EXPECT_EQ(Smtp::Base64("", false), "");
	EXPECT_EQ(Smtp::Base64("f", false), "Zg==");
	EXPECT_EQ(Smtp::Base64("fo", false), "Zm8=");
	EXPECT_EQ(Smtp::Base64("foo", false), "Zm9v");
	EXPECT_EQ(Smtp::Base64("foobar", false), "Zm9vYmFy");
}

TEST(SmtpTest, Base64WrapsLongLines) {
	const auto encoded = Smtp::Base64(std::string(200, 'x'), true);
	size_t start = 0;
	while (start < encoded.size()) {
		const auto end = encoded.find("\r\n", start);
		const auto length = (end == std::string::npos ? encoded.size() : end) - start;
		EXPECT_LE(length, 76u);
		if (end == std::string::npos) break;
		start = end + 2;
	}
}

TEST(SmtpTest, EncodesNonAsciiHeaders) {
	EXPECT_EQ(Smtp::EncodeHeader("Plain subject"), "Plain subject");
	const auto encoded = Smtp::EncodeHeader("Café");
	EXPECT_EQ(encoded.rfind("=?UTF-8?B?", 0), 0u);
	EXPECT_EQ(Decode(encoded.substr(10, encoded.size() - 12)), "Café");
}

TEST(SmtpTest, BuildMessageBlocksHeaderInjection) {
	Smtp::Message message{ "player@example.com", "Hello\r\nBcc: victim@example.com", "Body" };
	const auto data = Smtp::BuildMessage(TestConfig(), message, "Mon, 01 Jan 2024 00:00:00 +0000", "id@example.com");
	const auto headers = data.substr(0, data.find("\r\n\r\n"));
	EXPECT_EQ(headers.find("\r\nBcc:"), std::string::npos);
	EXPECT_NE(headers.find("Subject: HelloBcc: victim@example.com"), std::string::npos);
}

TEST(SmtpTest, BuildMessageEncodesBodyWithCrlf) {
	Smtp::Message message{ "player@example.com", "Reset", "Line one\nLine two\n.starts with a dot" };
	const auto data = Smtp::BuildMessage(TestConfig(), message, "Mon, 01 Jan 2024 00:00:00 +0000", "id@example.com");
	EXPECT_NE(data.find("From: \"Test Server\" <noreply@example.com>\r\n"), std::string::npos);
	EXPECT_NE(data.find("To: <player@example.com>\r\n"), std::string::npos);
	EXPECT_NE(data.find("Content-Transfer-Encoding: base64\r\n"), std::string::npos);

	const auto body = data.substr(data.find("\r\n\r\n") + 4);
	EXPECT_EQ(Decode(body), "Line one\r\nLine two\r\n.starts with a dot");
}
