#include "OAuth2.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>

#include <curl/curl.h>
#include <openssl/sha.h>

#include "json.hpp"

namespace {
	int64_t Now() {
		return static_cast<int64_t>(std::time(nullptr));
	}

	size_t WriteBody(char* data, size_t size, size_t count, void* userData) {
		static_cast<std::string*>(userData)->append(data, size * count);
		return size * count;
	}
}

namespace OAuth2 {
	std::optional<std::string> ApplyProviderDefaults(Config& config, const std::string& tenant) {
		const bool appOnly = config.grant == eGrant::CLIENT_CREDENTIALS;
		if (config.provider == "microsoft") {
			const std::string base = "https://login.microsoftonline.com/" + (tenant.empty() ? std::string("common") : tenant) + "/oauth2/v2.0/";
			if (config.authorizeUrl.empty()) config.authorizeUrl = base + "authorize";
			if (config.tokenUrl.empty()) config.tokenUrl = base + "token";
			if (config.scope.empty()) {
				config.scope = appOnly ? "https://outlook.office365.com/.default" : "https://outlook.office.com/SMTP.Send offline_access";
			}
			if (appOnly && (tenant.empty() || tenant == "common" || tenant == "organizations")) {
				return "Microsoft app-only access needs smtp_oauth2_tenant set to your tenant ID or domain";
			}
		} else if (config.provider == "google") {
			if (appOnly) return "Google does not support client_credentials for SMTP; use authorization_code";
			if (config.authorizeUrl.empty()) config.authorizeUrl = "https://accounts.google.com/o/oauth2/v2/auth";
			if (config.tokenUrl.empty()) config.tokenUrl = "https://oauth2.googleapis.com/token";
			if (config.scope.empty()) config.scope = "https://mail.google.com/";
		} else if (config.provider != "custom") {
			return "Unknown smtp_oauth2_provider '" + config.provider + "' (use microsoft, google or custom)";
		}

		if (config.clientId.empty()) return "smtp_oauth2_client_id is required";
		if (config.tokenUrl.empty()) return "smtp_oauth2_token_url is required";
		if (!appOnly && config.authorizeUrl.empty() && config.refreshToken.empty()) {
			return "smtp_oauth2_authorize_url is required to connect an account";
		}
		return std::nullopt;
	}

	std::string UrlEncode(std::string_view value) {
		static constexpr char hex[] = "0123456789ABCDEF";
		std::string out;
		for (const char c : value) {
			const auto u = static_cast<unsigned char>(c);
			if (std::isalnum(u) || c == '-' || c == '_' || c == '.' || c == '~') {
				out += c;
			} else {
				out += '%';
				out += hex[u >> 4];
				out += hex[u & 0xf];
			}
		}
		return out;
	}

	std::string FormEncode(const std::map<std::string, std::string>& fields) {
		std::string out;
		for (const auto& [key, value] : fields) {
			if (!out.empty()) out += '&';
			out += UrlEncode(key) + "=" + UrlEncode(value);
		}
		return out;
	}

	std::string Base64Url(std::string_view data) {
		static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
		std::string out;
		size_t i = 0;
		for (; i + 2 < data.size(); i += 3) {
			const uint32_t triple = (static_cast<unsigned char>(data[i]) << 16) | (static_cast<unsigned char>(data[i + 1]) << 8) | static_cast<unsigned char>(data[i + 2]);
			out += alphabet[(triple >> 18) & 0x3f];
			out += alphabet[(triple >> 12) & 0x3f];
			out += alphabet[(triple >> 6) & 0x3f];
			out += alphabet[triple & 0x3f];
		}
		if (i < data.size()) {
			const uint32_t b0 = static_cast<unsigned char>(data[i]);
			const uint32_t b1 = i + 1 < data.size() ? static_cast<unsigned char>(data[i + 1]) : 0;
			const uint32_t triple = (b0 << 16) | (b1 << 8);
			out += alphabet[(triple >> 18) & 0x3f];
			out += alphabet[(triple >> 12) & 0x3f];
			if (i + 1 < data.size()) out += alphabet[(triple >> 6) & 0x3f];
		}
		return out;
	}

	std::string PkceChallenge(const std::string& verifier) {
		unsigned char digest[SHA256_DIGEST_LENGTH];
		SHA256(reinterpret_cast<const unsigned char*>(verifier.data()), verifier.size(), digest);
		return Base64Url(std::string_view(reinterpret_cast<const char*>(digest), sizeof(digest)));
	}

	std::string BuildAuthorizeUrl(const Config& config, const std::string& redirectUri, const std::string& state, const std::string& codeChallenge) {
		std::map<std::string, std::string> fields{
			{"response_type", "code"},
			{"client_id", config.clientId},
			{"redirect_uri", redirectUri},
			{"scope", config.scope},
			{"state", state},
			{"code_challenge", codeChallenge},
			{"code_challenge_method", "S256"}
		};
		if (config.provider == "google") {
			// Google only issues a refresh token for offline access, and only on consent
			fields["access_type"] = "offline";
			fields["prompt"] = "consent";
		} else if (config.provider == "microsoft") {
			fields["prompt"] = "select_account";
		}
		const char separator = config.authorizeUrl.find('?') == std::string::npos ? '?' : '&';
		return config.authorizeUrl + separator + FormEncode(fields);
	}

	std::optional<TokenResponse> ParseTokenResponse(const std::string& body, std::string& error) {
		const auto json = nlohmann::json::parse(body, nullptr, false);
		if (json.is_discarded() || !json.is_object()) {
			error = "Token endpoint returned an unreadable response";
			return std::nullopt;
		}
		if (json.contains("error")) {
			error = json.value("error_description", json["error"].is_string() ? json["error"].get<std::string>() : "unknown error");
			return std::nullopt;
		}
		if (!json.contains("access_token") || !json["access_token"].is_string()) {
			error = "Token endpoint response has no access_token";
			return std::nullopt;
		}
		TokenResponse response;
		response.accessToken = json["access_token"].get<std::string>();
		if (json.contains("refresh_token") && json["refresh_token"].is_string()) response.refreshToken = json["refresh_token"].get<std::string>();
		if (json.contains("expires_in")) {
			const auto& expires = json["expires_in"];
			if (expires.is_number_integer()) response.expiresIn = expires.get<int64_t>();
			else if (expires.is_string()) response.expiresIn = std::strtoll(expires.get<std::string>().c_str(), nullptr, 10);
		}
		return response;
	}

	void TokenManager::Configure(Config config) {
		m_Config = std::move(config);
		m_RefreshToken = LoadStoredRefreshToken();
		if (m_RefreshToken.empty()) m_RefreshToken = m_Config.refreshToken;
		m_AccessToken.clear();
		m_AccessTokenExpires = 0;
		std::lock_guard lock(m_StatusMutex);
		m_HasRefreshToken = m_Config.grant == eGrant::CLIENT_CREDENTIALS || !m_RefreshToken.empty();
	}

	bool TokenManager::HasCredentials() {
		std::lock_guard lock(m_StatusMutex);
		return m_HasRefreshToken;
	}

	std::string TokenManager::LastError() {
		std::lock_guard lock(m_StatusMutex);
		return m_LastError;
	}

	void TokenManager::SetError(const std::string& error) {
		std::lock_guard lock(m_StatusMutex);
		m_LastError = error;
	}

	std::optional<std::string> TokenManager::GetAccessToken(bool forceRefresh, std::string& error) {
		// Refresh a minute early so a token never expires mid-send
		if (!forceRefresh && !m_AccessToken.empty() && Now() < m_AccessTokenExpires - 60) return m_AccessToken;

		std::map<std::string, std::string> fields{ {"client_id", m_Config.clientId} };
		if (!m_Config.clientSecret.empty()) fields["client_secret"] = m_Config.clientSecret;
		if (m_Config.grant == eGrant::CLIENT_CREDENTIALS) {
			fields["grant_type"] = "client_credentials";
			fields["scope"] = m_Config.scope;
		} else {
			if (m_RefreshToken.empty()) {
				error = "No mail account is connected; an operator must connect one on their account page";
				SetError(error);
				return std::nullopt;
			}
			fields["grant_type"] = "refresh_token";
			fields["refresh_token"] = m_RefreshToken;
		}

		TokenResponse response;
		if (auto requestError = RequestToken(fields, response)) {
			error = "Could not get an access token: " + *requestError;
			SetError(error);
			return std::nullopt;
		}
		m_AccessToken = response.accessToken;
		m_AccessTokenExpires = Now() + response.expiresIn;
		// Some providers (Microsoft) rotate refresh tokens; keep the newest one
		if (!response.refreshToken.empty() && response.refreshToken != m_RefreshToken) StoreRefreshToken(response.refreshToken);
		SetError("");
		return m_AccessToken;
	}

	std::optional<std::string> TokenManager::ExchangeCode(const std::string& code, const std::string& codeVerifier, const std::string& redirectUri) {
		std::map<std::string, std::string> fields{
			{"grant_type", "authorization_code"},
			{"client_id", m_Config.clientId},
			{"code", code},
			{"code_verifier", codeVerifier},
			{"redirect_uri", redirectUri}
		};
		if (!m_Config.clientSecret.empty()) fields["client_secret"] = m_Config.clientSecret;

		TokenResponse response;
		if (auto error = RequestToken(fields, response)) {
			SetError(*error);
			return error;
		}
		if (response.refreshToken.empty()) {
			const std::string error = "The provider did not return a refresh token (for Google, remove the app's access at myaccount.google.com/permissions and connect again)";
			SetError(error);
			return error;
		}
		StoreRefreshToken(response.refreshToken);
		m_AccessToken = response.accessToken;
		m_AccessTokenExpires = Now() + response.expiresIn;
		SetError("");
		return std::nullopt;
	}

	void TokenManager::Disconnect() {
		m_RefreshToken = m_Config.refreshToken;
		{
			std::lock_guard lock(m_StatusMutex);
			m_HasRefreshToken = m_Config.grant == eGrant::CLIENT_CREDENTIALS || !m_RefreshToken.empty();
		}
		m_AccessToken.clear();
		m_AccessTokenExpires = 0;
		std::error_code ec;
		std::filesystem::remove(m_Config.tokenFile, ec);
	}

	std::optional<std::string> TokenManager::RequestToken(const std::map<std::string, std::string>& fields, TokenResponse& response) {
		CURL* curl = curl_easy_init();
		if (!curl) return "Could not initialise libcurl";

		const std::string body = FormEncode(fields);
		std::string responseBody;
		char errorBuffer[CURL_ERROR_SIZE]{};
		curl_slist* headers = curl_slist_append(nullptr, "Accept: application/json");

		curl_easy_setopt(curl, CURLOPT_URL, m_Config.tokenUrl.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBody);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(m_Config.timeoutSeconds));
		// Client secrets and tokens must only travel over HTTPS, except to a local test server
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https,http");

		const CURLcode result = curl_easy_perform(curl);
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		if (result != CURLE_OK) return std::string(errorBuffer[0] ? errorBuffer : curl_easy_strerror(result));
		std::string error;
		auto parsed = ParseTokenResponse(responseBody, error);
		if (!parsed) return error;
		response = *parsed;
		return std::nullopt;
	}

	void TokenManager::StoreRefreshToken(const std::string& refreshToken) {
		m_RefreshToken = refreshToken;
		{
			std::lock_guard lock(m_StatusMutex);
			m_HasRefreshToken = true;
		}
		if (m_Config.tokenFile.empty()) return;
		const nlohmann::json data{ {"provider", m_Config.provider}, {"client_id", m_Config.clientId}, {"refresh_token", refreshToken} };
		{
			std::ofstream out(m_Config.tokenFile, std::ios::trunc);
			out << data.dump();
		}
		std::error_code ec;
		std::filesystem::permissions(m_Config.tokenFile, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, ec);
	}

	std::string TokenManager::LoadStoredRefreshToken() {
		std::ifstream in(m_Config.tokenFile);
		if (!in) return "";
		const auto data = nlohmann::json::parse(in, nullptr, false);
		if (data.is_discarded() || !data.is_object()) return "";
		// A token issued to a different app registration is useless; ignore it
		if (data.value("client_id", "") != m_Config.clientId || data.value("provider", "") != m_Config.provider) return "";
		return data.value("refresh_token", "");
	}
}
