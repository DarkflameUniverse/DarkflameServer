#pragma once

#include <string>
#include <cstdint>

/**
 * Centralized authentication token handler
 * Consolidates token extraction and validation for HTTP routes and WebSocket connections
 */
class AuthTokenHandler {
public:
	enum class eTokenSource : uint8_t {
		NONE,
		HEADER, // Authorization header, used by API clients
		COOKIE  // HttpOnly session cookie, used by the browser dashboard
	};

	struct TokenValidationResult {
		bool isValid{false};
		std::string username{};
		uint32_t accountId{0};
		uint8_t gmLevel{0};
		std::string errorMessage{};
	};

	static constexpr const char* COOKIE_NAME = "dashboardToken";

	/**
	 * Extract a named cookie value from a Cookie header
	 * @return The URL-decoded value, or an empty string if not present
	 */
	static std::string ExtractCookie(const std::string& cookieHeader, const std::string& name);

	/**
	 * Extract token from Authorization header
	 * Supports "Bearer <token>" and "Token <token>"
	 */
	static std::string ExtractTokenFromAuthHeader(const std::string& authHeader);

	/**
	 * Extract token from the Authorization header, falling back to the session cookie.
	 * Tokens are never read from the query string since URLs end up in logs and browser history.
	 */
	static std::string ExtractToken(const std::string& cookieHeader, const std::string& authHeader, eTokenSource& source);

	/**
	 * Validate a token against its signature and the current state of the account in the database
	 */
	static TokenValidationResult ValidateToken(const std::string& token);

	/**
	 * Process authentication for HTTP middleware use
	 * Populates the HTTPContext auth fields if a valid token is present; never rejects on its own
	 */
	static bool ProcessHTTPContext(class HTTPContext& context, class HTTPReply& reply);

	/**
	 * Build Set-Cookie header values for the session cookie
	 */
	static std::string BuildSessionCookie(const std::string& token, bool rememberMe, bool secure);
	static std::string BuildClearSessionCookie(bool secure);
};
