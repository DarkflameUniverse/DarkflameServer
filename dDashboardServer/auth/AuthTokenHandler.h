#pragma once

#include <string>
#include <cstdint>

/**
 * Centralized authentication token handler
 * Consolidates token extraction from multiple sources and validation
 * Used by both HTTP API routes and WebSocket connections
 */
class AuthTokenHandler {
public:
	/**
	 * Result of token extraction and validation
	 */
	struct TokenValidationResult {
		bool isValid{false};
		std::string username{};
		uint8_t gmLevel{0};
		std::string errorMessage{};
	};

	/**
	 * Extract token from query string
	 * Expected format: "?token=eyJhbGc..." or "?token=xyz&other=abc"
	 * @param queryString The query string from the request
	 * @return The token value, or empty string if not found
	 */
	static std::string ExtractTokenFromQueryString(const std::string& queryString);

	/**
	 * Extract token from Cookie header
	 * Looks for "dashboardToken=<value>" in the cookie string
	 * @param cookieHeader The Cookie header value
	 * @return The token value, or empty string if not found
	 */
	static std::string ExtractTokenFromCookieHeader(const std::string& cookieHeader);

	/**
	 * Extract token from Authorization header
	 * Supports "Bearer <token>", "Token <token>", or raw token
	 * @param authHeader The Authorization header value
	 * @return The token value, or empty string if not found
	 */
	static std::string ExtractTokenFromAuthHeader(const std::string& authHeader);

	/**
	 * Extract token from any available source
	 * Tries in priority order: query string, cookie, auth header
	 * @param queryString The query string
	 * @param cookieHeader The Cookie header
	 * @param authHeader The Authorization header
	 * @return The first token found, or empty string
	 */
	static std::string ExtractToken(
		const std::string& queryString,
		const std::string& cookieHeader,
		const std::string& authHeader
	);

	/**
	 * Validate a token and extract user information
	 * Checks JWT signature, expiration, and user permissions
	 * @param token The JWT token
	 * @return TokenValidationResult with validity status and user info
	 */
	static TokenValidationResult ValidateToken(const std::string& token);

	/**
	 * Convenience method: Extract and validate token in one call
	 * @param queryString Query string from request
	 * @param cookieHeader Cookie header from request
	 * @param authHeader Authorization header from request
	 * @return TokenValidationResult with validity status and user info
	 */
	static TokenValidationResult ExtractAndValidateToken(
		const std::string& queryString,
		const std::string& cookieHeader,
		const std::string& authHeader
	);

	/**
	 * Process authentication for HTTP middleware use
	 * Extracts and validates token from request, sets HTTPContext properties
	 * @param context HTTP request context (modified to include auth info)
	 * @param reply HTTP reply (not modified unless validation fails silently)
	 * @return true to continue middleware chain, false to stop
	 */
	static bool ProcessHTTPContext(class HTTPContext& context, class HTTPReply& reply);
};
