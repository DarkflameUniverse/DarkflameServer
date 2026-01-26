#include "AuthMiddleware.h"
#include "DashboardAuthService.h"
#include "Game.h"
#include "Logger.h"
#include <string>
#include <cctype>

// Helper to extract cookie value from header
static std::string ExtractCookieValue(const std::string& cookieHeader, const std::string& cookieName) {
	std::string searchStr = cookieName + "=";
	size_t pos = cookieHeader.find(searchStr);
	
	if (pos == std::string::npos) {
		return "";
	}
	
	size_t valueStart = pos + searchStr.length();
	size_t valueEnd = cookieHeader.find(";", valueStart);
	
	if (valueEnd == std::string::npos) {
		valueEnd = cookieHeader.length();
	}
	
	std::string value = cookieHeader.substr(valueStart, valueEnd - valueStart);
	
	// URL decode the value
	std::string decoded;
	for (size_t i = 0; i < value.length(); ++i) {
		if (value[i] == '%' && i + 2 < value.length()) {
			std::string hex = value.substr(i + 1, 2);
			char* endptr;
			int charCode = static_cast<int>(std::strtol(hex.c_str(), &endptr, 16));
			if (endptr - hex.c_str() == 2) {
				decoded += static_cast<char>(charCode);
				i += 2;
				continue;
			}
		}
		decoded += value[i];
	}
	
	return decoded;
}

std::string AuthMiddleware::ExtractTokenFromQueryString(const std::string& queryString) {
	if (queryString.empty()) {
		return "";
	}

	// Parse query string to find token parameter
	// Expected format: "?token=eyJhbGc..."
	std::string tokenPrefix = "token=";
	size_t tokenPos = queryString.find(tokenPrefix);
	
	if (tokenPos == std::string::npos) {
		return "";
	}

	// Extract token value (from "token=" to next "&" or end of string)
	size_t valueStart = tokenPos + tokenPrefix.length();
	size_t valueEnd = queryString.find("&", valueStart);
	
	if (valueEnd == std::string::npos) {
		valueEnd = queryString.length();
	}

	return queryString.substr(valueStart, valueEnd - valueStart);
}

std::string AuthMiddleware::ExtractTokenFromCookies(const std::string& cookieHeader) {
	if (cookieHeader.empty()) {
		return "";
	}

	// Extract dashboardToken cookie value
	return ExtractCookieValue(cookieHeader, "dashboardToken");
}

std::string AuthMiddleware::ExtractTokenFromAuthHeader(const std::string& authHeader) {
	if (authHeader.empty()) {
		return "";
	}

	// Check for "Bearer <token>" format
	if (authHeader.substr(0, 7) == "Bearer ") {
		return authHeader.substr(7);
	}

	// Check for "Token <token>" format
	if (authHeader.substr(0, 6) == "Token ") {
		return authHeader.substr(6);
	}

	// If no prefix, assume raw token
	return authHeader;
}

bool AuthMiddleware::Process(HTTPContext& context, HTTPReply& reply) {
	// Try to extract token from various sources (in priority order)
	std::string token = ExtractTokenFromQueryString(context.queryString);
	
	if (token.empty()) {
		const std::string& cookieHeader = context.GetHeader("Cookie");
		token = ExtractTokenFromCookies(cookieHeader);
	}
	
	if (token.empty()) {
		const std::string& authHeader = context.GetHeader("Authorization");
		token = ExtractTokenFromAuthHeader(authHeader);
	}

	// If we found a token, try to verify it
	if (!token.empty()) {
		std::string username;
		uint8_t gmLevel{};
		
		if (DashboardAuthService::VerifyToken(token, username, gmLevel)) {
			context.isAuthenticated = true;
			context.authenticatedUser = username;
			context.gmLevel = gmLevel;
			LOG_DEBUG("User %s authenticated via API token (GM level %d)", username.c_str(), gmLevel);
			return true;
		} else {
			LOG_DEBUG("Invalid authentication token provided");
			return true;  // Continue - let routes decide if auth is required
		}
	}

	// No token found - continue without authentication
	// Routes can use RequireAuthMiddleware to enforce authentication
	return true;
}
