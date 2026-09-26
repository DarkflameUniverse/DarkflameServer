#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

/**
 * OAuth2 for SMTP (XOAUTH2), so the dashboard can send through Microsoft 365 or Google accounts
 * without a mailbox password.
 *
 * Two ways to get tokens:
 *  - authorization_code: an operator connects the mailbox once from the dashboard; the refresh token is kept
 *    in a file next to the binary and used to get short-lived access tokens.
 *  - client_credentials: app-only access (Microsoft 365 "SMTP.SendAsApp"); no sign-in needed.
 */
namespace OAuth2 {
	enum class eGrant : uint8_t {
		AUTHORIZATION_CODE,
		CLIENT_CREDENTIALS
	};

	struct Config {
		std::string provider;      // microsoft, google or custom
		eGrant grant{ eGrant::AUTHORIZATION_CODE };
		std::string clientId;
		std::string clientSecret;
		std::string authorizeUrl;
		std::string tokenUrl;
		std::string scope;
		std::string refreshToken;  // optional, from config; a connected account takes precedence
		std::string tokenFile;     // where a connected account's refresh token is stored
		uint32_t timeoutSeconds{ 20 };
	};

	// Fill in URLs and scopes for known providers. Returns an error for unknown providers or missing settings.
	std::optional<std::string> ApplyProviderDefaults(Config& config, const std::string& tenant);

	// ---- Pure helpers, exposed for testing ----

	std::string UrlEncode(std::string_view value);
	std::string FormEncode(const std::map<std::string, std::string>& fields);

	// Base64url without padding (RFC 4648 section 5)
	std::string Base64Url(std::string_view data);

	// PKCE S256 challenge for a verifier
	std::string PkceChallenge(const std::string& verifier);

	std::string BuildAuthorizeUrl(const Config& config, const std::string& redirectUri, const std::string& state, const std::string& codeChallenge);

	struct TokenResponse {
		std::string accessToken;
		std::string refreshToken; // empty if the provider didn't issue a new one
		int64_t expiresIn{ 3600 };
	};

	// Parse a token endpoint response body; returns the provider's error description on failure
	std::optional<TokenResponse> ParseTokenResponse(const std::string& body, std::string& error);

	/**
	 * Access token cache. Token operations (Get/Exchange/Disconnect) block on HTTPS and must only run on the
	 * email worker thread; HasCredentials and LastError are safe from any thread.
	 */
	class TokenManager {
	public:
		void Configure(Config config);

		// A valid access token, refreshing if needed. Blocking (HTTPS request).
		std::optional<std::string> GetAccessToken(bool forceRefresh, std::string& error);

		// Exchange an authorization code from the connect flow and store the refresh token. Blocking.
		std::optional<std::string> ExchangeCode(const std::string& code, const std::string& codeVerifier, const std::string& redirectUri);

		// Forget the connected account
		void Disconnect();

		bool HasCredentials();
		std::string LastError();
		const Config& GetConfig() const { return m_Config; }

	private:
		std::optional<std::string> RequestToken(const std::map<std::string, std::string>& fields, TokenResponse& response);
		void StoreRefreshToken(const std::string& refreshToken);
		std::string LoadStoredRefreshToken();
		void SetError(const std::string& error);

		// Worker thread only
		Config m_Config;
		std::string m_RefreshToken;
		std::string m_AccessToken;
		int64_t m_AccessTokenExpires{ 0 };

		// Shared with the main thread
		std::mutex m_StatusMutex;
		bool m_HasRefreshToken{ false };
		std::string m_LastError;
	};
}
