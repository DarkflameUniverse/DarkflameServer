#pragma once

#include <string>
#include <ctime>
#include "json_fwd.hpp"

/**
 * JWT Token utilities for dashboard authentication
 * Provides secure token generation, validation, and parsing
 */
namespace JWTUtils {
	/**
	 * JWT payload structure
	 */
	struct JWTPayload {
		std::string username{};
		uint8_t gmLevel{0};
		bool rememberMe{false};
		int64_t issuedAt{0};
		int64_t expiresAt{0};
	};

	/**
	 * Generate a new JWT token
	 * @param username The username to encode in the token
	 * @param gmLevel The GM level of the user
	 * @param rememberMe If true, extends token expiration to 30 days; otherwise 24 hours
	 * @return Signed JWT token string
	 */
	std::string GenerateToken(const std::string& username, uint8_t gmLevel, bool rememberMe = false);

	/**
	 * Validate and decode a JWT token
	 * @param token The JWT token to validate
	 * @param payload Output parameter for the decoded payload
	 * @return true if token is valid and not expired, false otherwise
	 */
	bool ValidateToken(const std::string& token, JWTPayload& payload);

	/**
	 * Check if a token is expired
	 * @param expiresAt Expiration timestamp
	 * @return true if token is expired
	 */
	bool IsTokenExpired(int64_t expiresAt);

	/**
	 * Set the JWT secret key (must be called once at startup)
	 * @param secret The secret key for signing tokens
	 */
	void SetSecretKey(const std::string& secret);
}
