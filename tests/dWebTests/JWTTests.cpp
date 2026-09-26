#include <gtest/gtest.h>
#include "JWTUtils.h"
#include "json.hpp"

namespace {
	const std::string SECRET = "0123456789abcdef0123456789abcdef-test-secret";

	std::string Base64(const std::string& in) {
		static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		int val = 0, bits = -6;
		for (unsigned char c : in) {
			val = (val << 8) + c;
			bits += 8;
			while (bits >= 0) { out.push_back(chars[(val >> bits) & 0x3F]); bits -= 6; }
		}
		if (bits > -6) out.push_back(chars[((val << 8) >> (bits + 8)) & 0x3F]);
		while (out.size() % 4) out.push_back('=');
		return out;
	}
}

class JWTTest : public ::testing::Test {
protected:
	void SetUp() override { JWTUtils::SetSecretKey(SECRET); }
};

TEST_F(JWTTest, RefusesShortSecrets) {
	JWTUtils::SetSecretKey("short");
	// The previous (valid) secret stays in place
	EXPECT_TRUE(JWTUtils::HasSecretKey());
	EXPECT_FALSE(JWTUtils::GenerateToken("alice", 0).empty());
}

TEST_F(JWTTest, RoundTrip) {
	const auto token = JWTUtils::GenerateToken("alice", 3, 3600, false);
	JWTUtils::JWTPayload payload;
	ASSERT_TRUE(JWTUtils::ValidateToken(token, payload));
	EXPECT_EQ(payload.username, "alice");
	EXPECT_EQ(payload.gmLevel, 3);
}

TEST_F(JWTTest, RejectsTamperedPayload) {
	const auto token = JWTUtils::GenerateToken("alice", 0, 3600, false);
	const auto firstDot = token.find('.');
	const auto secondDot = token.find('.', firstDot + 1);
	const std::string forged = Base64(R"({"username":"admin","gmLevel":9,"exp":9999999999})");
	const auto tampered = token.substr(0, firstDot + 1) + forged + token.substr(secondDot);

	JWTUtils::JWTPayload payload;
	EXPECT_FALSE(JWTUtils::ValidateToken(tampered, payload));
}

TEST_F(JWTTest, RejectsTokenSignedWithAnotherSecret) {
	const auto token = JWTUtils::GenerateToken("alice", 0, 3600, false);
	JWTUtils::SetSecretKey("another-secret-that-is-at-least-32-bytes-long");
	JWTUtils::JWTPayload payload;
	EXPECT_FALSE(JWTUtils::ValidateToken(token, payload));
}

TEST_F(JWTTest, RejectsExpiredToken) {
	const auto token = JWTUtils::GenerateToken("alice", 0, -10, false);
	JWTUtils::JWTPayload payload;
	EXPECT_FALSE(JWTUtils::ValidateToken(token, payload));
}

TEST_F(JWTTest, UsernameCannotInjectClaims) {
	const auto token = JWTUtils::GenerateToken(R"(x","username":"admin","gmLevel":9,"a":")", 0, 3600, false);
	JWTUtils::JWTPayload payload;
	ASSERT_TRUE(JWTUtils::ValidateToken(token, payload));
	EXPECT_EQ(payload.username, R"(x","username":"admin","gmLevel":9,"a":")");
	EXPECT_EQ(payload.gmLevel, 0);
}

TEST_F(JWTTest, RejectsMalformedTokens) {
	JWTUtils::JWTPayload payload;
	EXPECT_FALSE(JWTUtils::ValidateToken("", payload));
	EXPECT_FALSE(JWTUtils::ValidateToken("abc", payload));
	EXPECT_FALSE(JWTUtils::ValidateToken("a.b", payload));
	EXPECT_FALSE(JWTUtils::ValidateToken("a.b.c", payload));
}

TEST_F(JWTTest, GeneratedSecretsAreRandomHex) {
	const auto a = JWTUtils::GenerateSecret();
	const auto b = JWTUtils::GenerateSecret();
	EXPECT_EQ(a.size(), 96u);
	EXPECT_NE(a, b);
}

TEST_F(JWTTest, SessionTokensCarryTheAccountId) {
	JWTUtils::JWTPayload payload;
	ASSERT_TRUE(JWTUtils::ValidateToken(JWTUtils::GenerateSessionToken(42, "alice", 3, false), payload));
	EXPECT_EQ(payload.accountId, 42u);
	EXPECT_EQ(payload.username, "alice");
	EXPECT_FALSE(payload.rememberMe);
	EXPECT_EQ(payload.expiresAt - payload.issuedAt, 24 * 60 * 60);

	ASSERT_TRUE(JWTUtils::ValidateToken(JWTUtils::GenerateSessionToken(42, "alice", 3, true), payload));
	EXPECT_EQ(payload.expiresAt - payload.issuedAt, 30 * 24 * 60 * 60);

	ASSERT_TRUE(JWTUtils::ValidateToken(JWTUtils::GenerateSessionToken(42, "alice", 3, false, 7 * 24 * 60 * 60), payload));
	EXPECT_EQ(payload.expiresAt - payload.issuedAt, 7 * 24 * 60 * 60);

	// Tokens without the claim read as account 0, which matches no account
	ASSERT_TRUE(JWTUtils::ValidateToken(JWTUtils::GenerateToken("alice", 3, 3600, false), payload));
	EXPECT_EQ(payload.accountId, 0u);
}
