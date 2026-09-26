#include <gtest/gtest.h>
#include "OAuth2.h"

using namespace OAuth2;

TEST(OAuth2Test, PkceMatchesRfc7636Example) {
	// RFC 7636 appendix B
	EXPECT_EQ(PkceChallenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"), "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");
}

TEST(OAuth2Test, Base64UrlHasNoPaddingOrUnsafeCharacters) {
	EXPECT_EQ(Base64Url("f"), "Zg");
	EXPECT_EQ(Base64Url("fo"), "Zm8");
	EXPECT_EQ(Base64Url("foo"), "Zm9v");
	EXPECT_EQ(Base64Url("\xfb\xff"), "-_8");
}

TEST(OAuth2Test, FormEncodingEscapesReservedCharacters) {
	EXPECT_EQ(UrlEncode("a b&c=d/é"), "a%20b%26c%3Dd%2F%C3%A9");
	EXPECT_EQ(FormEncode({ {"grant_type", "refresh_token"}, {"scope", "a b"} }), "grant_type=refresh_token&scope=a%20b");
}

TEST(OAuth2Test, GooglePresetRequestsOfflineAccess) {
	Config config;
	config.provider = "google";
	config.clientId = "client";
	ASSERT_FALSE(ApplyProviderDefaults(config, "").has_value());
	EXPECT_EQ(config.tokenUrl, "https://oauth2.googleapis.com/token");
	EXPECT_EQ(config.scope, "https://mail.google.com/");

	const auto url = BuildAuthorizeUrl(config, "https://dash.example.com/oauth2/callback", "state123", "challenge");
	EXPECT_EQ(url.rfind("https://accounts.google.com/o/oauth2/v2/auth?", 0), 0u);
	EXPECT_NE(url.find("access_type=offline"), std::string::npos);
	EXPECT_NE(url.find("prompt=consent"), std::string::npos);
	EXPECT_NE(url.find("redirect_uri=https%3A%2F%2Fdash.example.com%2Foauth2%2Fcallback"), std::string::npos);
	EXPECT_NE(url.find("code_challenge_method=S256"), std::string::npos);
	EXPECT_NE(url.find("state=state123"), std::string::npos);
}

TEST(OAuth2Test, MicrosoftPresets) {
	Config delegated;
	delegated.provider = "microsoft";
	delegated.clientId = "client";
	ASSERT_FALSE(ApplyProviderDefaults(delegated, "").has_value());
	EXPECT_EQ(delegated.tokenUrl, "https://login.microsoftonline.com/common/oauth2/v2.0/token");
	EXPECT_NE(delegated.scope.find("SMTP.Send"), std::string::npos);
	EXPECT_NE(delegated.scope.find("offline_access"), std::string::npos);

	Config appOnly = delegated;
	appOnly.grant = eGrant::CLIENT_CREDENTIALS;
	appOnly.scope.clear();
	appOnly.tokenUrl.clear();
	appOnly.authorizeUrl.clear();
	EXPECT_TRUE(ApplyProviderDefaults(appOnly, "common").has_value()) << "app-only needs a specific tenant";
	ASSERT_FALSE(ApplyProviderDefaults(appOnly, "contoso.onmicrosoft.com").has_value());
	EXPECT_EQ(appOnly.scope, "https://outlook.office365.com/.default");
}

TEST(OAuth2Test, RejectsBadConfig) {
	Config config;
	config.provider = "yahoo";
	config.clientId = "client";
	EXPECT_TRUE(ApplyProviderDefaults(config, "").has_value());

	Config google;
	google.provider = "google";
	google.grant = eGrant::CLIENT_CREDENTIALS;
	google.clientId = "client";
	EXPECT_TRUE(ApplyProviderDefaults(google, "").has_value());

	Config missingClient;
	missingClient.provider = "google";
	EXPECT_TRUE(ApplyProviderDefaults(missingClient, "").has_value());
}

TEST(OAuth2Test, ParsesTokenResponses) {
	std::string error;
	auto ok = ParseTokenResponse(R"({"access_token":"abc","refresh_token":"r2","expires_in":3599,"token_type":"Bearer"})", error);
	ASSERT_TRUE(ok.has_value());
	EXPECT_EQ(ok->accessToken, "abc");
	EXPECT_EQ(ok->refreshToken, "r2");
	EXPECT_EQ(ok->expiresIn, 3599);

	auto stringExpiry = ParseTokenResponse(R"({"access_token":"abc","expires_in":"120"})", error);
	ASSERT_TRUE(stringExpiry.has_value());
	EXPECT_EQ(stringExpiry->expiresIn, 120);
	EXPECT_TRUE(stringExpiry->refreshToken.empty());

	EXPECT_FALSE(ParseTokenResponse(R"({"error":"invalid_grant","error_description":"Token has been expired or revoked."})", error).has_value());
	EXPECT_EQ(error, "Token has been expired or revoked.");
	EXPECT_FALSE(ParseTokenResponse("<html>", error).has_value());
	EXPECT_FALSE(ParseTokenResponse(R"({"token_type":"Bearer"})", error).has_value());
}
