#include "JWTUtils.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "json.hpp"
#include <ctime>
#include <cstring>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>

namespace {
	std::string g_Secret{};

	// Simple base64 encoding
	std::string Base64Encode(const std::string& input) {
		static const char* base64_chars = 
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string ret;
		int i = 0;
		unsigned char char_array_3[3];
		unsigned char char_array_4[4];

		for (size_t n = 0; n < input.length(); n++) {
			char_array_3[i++] = input[n];
			if (i == 3) {
				char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
				char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
				char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
				char_array_4[3] = char_array_3[2] & 0x3f;
				for (i = 0; i < 4; i++) ret += base64_chars[char_array_4[i]];
				i = 0;
			}
		}

		if (i) {
			for (int j = i; j < 3; j++) char_array_3[j] = '\0';
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			for (int j = 0; j <= i; j++) ret += base64_chars[char_array_4[j]];
			while (i++ < 3) ret += '=';
		}

		return ret;
	}

	// Simple base64 decoding
	std::string Base64Decode(const std::string& encoded_string) {
		static const std::string base64_chars = 
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		int in_len = encoded_string.size();
		int i = 0, j = 0, in_ = 0;
		unsigned char char_array_4[4], char_array_3[3];
		std::string ret;

		while (in_len-- && (encoded_string[in_] != '=') && 
			   (isalnum(encoded_string[in_]) || encoded_string[in_] == '+' || encoded_string[in_] == '/')) {
			char_array_4[i++] = encoded_string[in_]; in_++;
			if (i == 4) {
				for (i = 0; i < 4; i++) char_array_4[i] = base64_chars.find(char_array_4[i]);
				char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
				char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
				char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
				for (i = 0; i < 3; i++) ret += char_array_3[i];
				i = 0;
			}
		}

		if (i) {
			for (j = i; j < 4; j++) char_array_4[j] = 0;
			for (j = 0; j < 4; j++) char_array_4[j] = base64_chars.find(char_array_4[j]);
			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			for (j = 0; j < i - 1; j++) ret += char_array_3[j];
		}

		return ret;
	}

	// HMAC-SHA256
	std::string HmacSha256(const std::string& key, const std::string& message) {
		unsigned char* digest = HMAC(EVP_sha256(), 
			reinterpret_cast<const unsigned char*>(key.c_str()), key.length(),
			reinterpret_cast<const unsigned char*>(message.c_str()), message.length(),
			nullptr, nullptr);
		
		std::string result(reinterpret_cast<char*>(digest), SHA256_DIGEST_LENGTH);
		return result;
	}

	// Create signature for JWT
	std::string CreateSignature(const std::string& header, const std::string& payload, const std::string& secret) {
		std::string message = header + "." + payload;
		std::string signature = HmacSha256(secret, message);
		return Base64Encode(signature);
	}

	// Verify JWT signature
	bool VerifySignature(const std::string& header, const std::string& payload, const std::string& signature, const std::string& secret) {
		std::string expected = CreateSignature(header, payload, secret);
		return signature.size() == expected.size() && CRYPTO_memcmp(signature.data(), expected.data(), expected.size()) == 0;
	}
}

namespace JWTUtils {
	std::string BuildToken(const std::string& username, uint8_t gmLevel, int64_t lifetimeSeconds, bool rememberMe, uint32_t accountId);

	void SetSecretKey(const std::string& secret) {
		if (secret.size() < 32) {
			LOG("Refusing JWT secret shorter than 32 bytes");
			return;
		}
		g_Secret = secret;
	}

	bool HasSecretKey() {
		return !g_Secret.empty();
	}

	std::string GenerateSecret() {
		unsigned char bytes[48];
		if (RAND_bytes(bytes, sizeof(bytes)) != 1) return "";
		std::string hex;
		hex.reserve(sizeof(bytes) * 2);
		constexpr const char* digits = "0123456789abcdef";
		for (const auto b : bytes) {
			hex += digits[b >> 4];
			hex += digits[b & 0xf];
		}
		return hex;
	}

	std::string GenerateToken(const std::string& username, uint8_t gmLevel, bool rememberMe) {
		return GenerateToken(username, gmLevel, rememberMe ? 30 * 24 * 60 * 60 : 24 * 60 * 60, rememberMe);
	}

	std::string GenerateToken(const std::string& username, uint8_t gmLevel, int64_t lifetimeSeconds, bool rememberMe) {
		return BuildToken(username, gmLevel, lifetimeSeconds, rememberMe, 0);
	}

	std::string GenerateSessionToken(uint32_t accountId, const std::string& username, uint8_t gmLevel, bool rememberMe, int64_t lifetimeSeconds) {
		if (lifetimeSeconds <= 0) lifetimeSeconds = rememberMe ? 30 * 24 * 60 * 60 : 24 * 60 * 60;
		return BuildToken(username, gmLevel, lifetimeSeconds, rememberMe, accountId);
	}

	std::string BuildToken(const std::string& username, uint8_t gmLevel, int64_t lifetimeSeconds, bool rememberMe, uint32_t accountId) {
		if (g_Secret.empty()) return "";
		// Header
		std::string header = R"({"alg":"HS256","typ":"JWT"})";
		std::string encodedHeader = Base64Encode(header);

		// Payload
		int64_t now = std::time(nullptr);
		int64_t expiresAt = now + lifetimeSeconds;

		// Built with the JSON library so a username can never inject extra claims
		auto claims = nlohmann::json{
			{"username", username},
			{"gmLevel", gmLevel},
			{"rememberMe", rememberMe},
			{"iat", now},
			{"exp", expiresAt}
		};
		if (accountId != 0) claims["aid"] = accountId;
		const std::string payload = claims.dump();
		std::string encodedPayload = Base64Encode(payload);

		// Signature
		std::string signature = CreateSignature(encodedHeader, encodedPayload, g_Secret);

		return encodedHeader + "." + encodedPayload + "." + signature;
	}

	bool ValidateToken(const std::string& token, JWTPayload& payload) {
		if (g_Secret.empty()) return false;

		// Split token into parts
		size_t firstDot = token.find('.');
		size_t secondDot = token.find('.', firstDot + 1);

		if (firstDot == std::string::npos || secondDot == std::string::npos) {
			LOG_DEBUG("Invalid JWT format");
			return false;
		}

		std::string header = token.substr(0, firstDot);
		std::string encodedPayload = token.substr(firstDot + 1, secondDot - firstDot - 1);
		std::string signature = token.substr(secondDot + 1);

		// Verify signature
		if (!VerifySignature(header, encodedPayload, signature, g_Secret)) {
			LOG_DEBUG("Invalid JWT signature");
			return false;
		}

		// Decode and parse payload
		std::string decodedPayload = Base64Decode(encodedPayload);
		try {
			auto json = nlohmann::json::parse(decodedPayload);
			
			payload.username = json.value("username", "");
			payload.gmLevel = json.value("gmLevel", 0);
			payload.rememberMe = json.value("rememberMe", false);
			payload.issuedAt = json.value("iat", 0);
			payload.expiresAt = json.value("exp", 0);
			payload.accountId = json.value("aid", 0u);

			if (payload.username.empty()) {
				LOG_DEBUG("JWT missing username");
				return false;
			}

			// Check expiration
			if (IsTokenExpired(payload.expiresAt)) {
				LOG_DEBUG("JWT token expired");
				return false;
			}

			return true;
		} catch (const std::exception& ex) {
			LOG_DEBUG("Error parsing JWT payload: %s", ex.what());
			return false;
		}
	}

	bool IsTokenExpired(int64_t expiresAt) {
		return std::time(nullptr) > expiresAt;
	}
}
