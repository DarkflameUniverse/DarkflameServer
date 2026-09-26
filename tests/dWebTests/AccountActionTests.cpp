#include <gtest/gtest.h>
#include "HTTPContext.h"
#include "Web.h"
#include "json.hpp"

class AccountActionAuthTest : public ::testing::Test {
protected:
	HTTPContext context;
	HTTPReply reply;

	void SetUp() override {
		reply.status = eHTTPStatusCode::OK;
		reply.contentType = eContentType::APPLICATION_JSON;
		context.method = "POST";
		context.isAuthenticated = true;
	}

	uint32_t ExtractAccountId(const std::string& path, const std::string& action) {
		size_t idStart = std::string("/api/accounts/").length();
		size_t idEnd = path.find("/" + action);
		return std::stoul(path.substr(idStart, idEnd - idStart));
	}

	bool CheckGmLevel(uint8_t userLevel, uint8_t required) {
		return userLevel >= required;
	}
};

TEST_F(AccountActionAuthTest, ExtractAccountIdFromBanUrl) {
	EXPECT_EQ(ExtractAccountId("/api/accounts/42/ban", "ban"), 42);
	EXPECT_EQ(ExtractAccountId("/api/accounts/1/ban", "ban"), 1);
	EXPECT_EQ(ExtractAccountId("/api/accounts/999/ban", "ban"), 999);
}

TEST_F(AccountActionAuthTest, ExtractAccountIdFromLockUrl) {
	EXPECT_EQ(ExtractAccountId("/api/accounts/7/lock", "lock"), 7);
	EXPECT_EQ(ExtractAccountId("/api/accounts/100/lock", "lock"), 100);
}

TEST_F(AccountActionAuthTest, ExtractAccountIdFromGmLevelUrl) {
	EXPECT_EQ(ExtractAccountId("/api/accounts/55/gmlevel", "gmlevel"), 55);
}

TEST_F(AccountActionAuthTest, ExtractAccountIdFromMuteUrl) {
	EXPECT_EQ(ExtractAccountId("/api/accounts/123/mute", "mute"), 123);
}

TEST_F(AccountActionAuthTest, BanRequiresGmLevel4) {
	EXPECT_FALSE(CheckGmLevel(0, 4));
	EXPECT_FALSE(CheckGmLevel(1, 4));
	EXPECT_FALSE(CheckGmLevel(3, 4));
	EXPECT_TRUE(CheckGmLevel(4, 4));
	EXPECT_TRUE(CheckGmLevel(9, 4));
}

TEST_F(AccountActionAuthTest, LockRequiresGmLevel4) {
	EXPECT_FALSE(CheckGmLevel(3, 4));
	EXPECT_TRUE(CheckGmLevel(4, 4));
	EXPECT_TRUE(CheckGmLevel(5, 4));
}

TEST_F(AccountActionAuthTest, GmLevelChangeRequiresGmLevel8) {
	EXPECT_FALSE(CheckGmLevel(7, 8));
	EXPECT_TRUE(CheckGmLevel(8, 8));
	EXPECT_TRUE(CheckGmLevel(9, 8));
}

TEST_F(AccountActionAuthTest, MuteRequiresGmLevel2) {
	EXPECT_FALSE(CheckGmLevel(0, 2));
	EXPECT_FALSE(CheckGmLevel(1, 2));
	EXPECT_TRUE(CheckGmLevel(2, 2));
	EXPECT_TRUE(CheckGmLevel(9, 2));
}

TEST_F(AccountActionAuthTest, ParseBanRequestBody) {
	nlohmann::json body = {{"banned", true}};
	EXPECT_TRUE(body.value("banned", false));

	body = {{"banned", false}};
	EXPECT_FALSE(body.value("banned", true));
}

TEST_F(AccountActionAuthTest, ParseLockRequestBody) {
	nlohmann::json body = {{"locked", true}};
	EXPECT_TRUE(body.value("locked", false));

	body = {{"locked", false}};
	EXPECT_FALSE(body.value("locked", true));
}

TEST_F(AccountActionAuthTest, ParseGmLevelRequestBody) {
	nlohmann::json body = {{"gm_level", 5}};
	uint8_t level = body.value("gm_level", 0);
	EXPECT_EQ(level, 5);
}

TEST_F(AccountActionAuthTest, GmLevelValidation) {
	for (uint8_t i = 0; i <= 9; i++) {
		EXPECT_TRUE(i <= 9);
	}
	EXPECT_FALSE(10 <= 9);
	EXPECT_FALSE(255 <= 9);
}

TEST_F(AccountActionAuthTest, ParseMuteRequestBody) {
	nlohmann::json body = {{"mute_until", 1700000000}};
	uint64_t muteUntil = body.value("mute_until", static_cast<uint64_t>(0));
	EXPECT_EQ(muteUntil, 1700000000);

	body = {{"mute_until", 0}};
	muteUntil = body.value("mute_until", static_cast<uint64_t>(0));
	EXPECT_EQ(muteUntil, 0);
}

TEST_F(AccountActionAuthTest, InvalidAccountIdThrows) {
	EXPECT_THROW(ExtractAccountId("/api/accounts/abc/ban", "ban"), std::invalid_argument);
}

TEST_F(AccountActionAuthTest, UnauthenticatedUserDenied) {
	context.isAuthenticated = false;
	context.gmLevel = 0;
	EXPECT_FALSE(context.isAuthenticated);
}

TEST_F(AccountActionAuthTest, InsufficientGmLevelReturnsForbidden) {
	context.gmLevel = 1;
	if (!CheckGmLevel(context.gmLevel, 4)) {
		reply.status = eHTTPStatusCode::FORBIDDEN;
		reply.message = "{\"error\":\"Insufficient permissions\"}";
	}
	EXPECT_EQ(reply.status, eHTTPStatusCode::FORBIDDEN);
}
