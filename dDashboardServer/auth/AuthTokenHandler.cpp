#include "AuthTokenHandler.h"
#include "DashboardAuthService.h"
#include "Game.h"
#include "Logger.h"
#include "HTTPContext.h"
#include "Web.h"

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

std::string AuthTokenHandler::ExtractTokenFromQueryString(const std::string& queryString) {
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

std::string AuthTokenHandler::ExtractTokenFromCookieHeader(const std::string& cookieHeader) {
	if (cookieHeader.empty()) {
		return "";
	}

	// Extract dashboardToken cookie value
	return ExtractCookieValue(cookieHeader, "dashboardToken");
}

std::string AuthTokenHandler::ExtractTokenFromAuthHeader(const std::string& authHeader) {
	if (authHeader.empty()) {
		return "";
	}

	// Check for "Bearer <token>" format
	if (authHeader.length() >= 7 && authHeader.substr(0, 7) == "Bearer ") {
		return authHeader.substr(7);
	}

	// Check for "Token <token>" format
	if (authHeader.length() >= 6 && authHeader.substr(0, 6) == "Token ") {
		return authHeader.substr(6);
	}

	// If no prefix, assume raw token
	return authHeader;
}

std::string AuthTokenHandler::ExtractToken(
	const std::string& queryString,
	const std::string& cookieHeader,
	const std::string& authHeader
) {
	// Try in priority order: query string, cookie, auth header
	std::string token = ExtractTokenFromQueryString(queryString);
	
	if (!token.empty()) {
		return token;
	}
	
	token = ExtractTokenFromCookieHeader(cookieHeader);
	
	if (!token.empty()) {
		return token;
	}
	
	token = ExtractTokenFromAuthHeader(authHeader);
	
	return token;
}

AuthTokenHandler::TokenValidationResult AuthTokenHandler::ValidateToken(const std::string& token) {
	TokenValidationResult result;

	if (token.empty()) {
		result.isValid = false;
		result.errorMessage = "No token provided";
		return result;
	}

	// Verify JWT token
	std::string username;
	uint8_t gmLevel = 0;
	
	if (!DashboardAuthService::VerifyToken(token, username, gmLevel)) {
		result.isValid = false;
		result.errorMessage = "Invalid or expired token";
		LOG_DEBUG("Token validation failed");
		return result;
	}

	result.isValid = true;
	result.username = username;
	result.gmLevel = gmLevel;
	LOG_DEBUG("Token validated successfully for user: %s (GM Level: %d)", username.c_str(), gmLevel);
	return result;
}

AuthTokenHandler::TokenValidationResult AuthTokenHandler::ExtractAndValidateToken(
	const std::string& queryString,
	const std::string& cookieHeader,
	const std::string& authHeader
) {
	TokenValidationResult result;

	// Extract token from any source
	std::string token = ExtractToken(queryString, cookieHeader, authHeader);

	if (token.empty()) {
		result.isValid = false;
		result.errorMessage = "No authentication token found";
		return result;
	}

	// Validate the token
	return ValidateToken(token);
}

bool AuthTokenHandler::ProcessHTTPContext(HTTPContext& context, HTTPReply& reply) {
	// Extract and validate token from all available sources
	const std::string& queryString = context.queryString;
	const std::string& cookieHeader = context.GetHeader("Cookie");
	const std::string& authHeader = context.GetHeader("Authorization");
	
	auto result = ExtractAndValidateToken(queryString, cookieHeader, authHeader);
	
	if (result.isValid) {
		context.isAuthenticated = true;
		context.authenticatedUser = result.username;
		context.gmLevel = result.gmLevel;
		LOG_DEBUG("User %s authenticated via API token (GM level %d)", result.username.c_str(), result.gmLevel);
		return true;
	} else {
		LOG_DEBUG("Authentication token validation failed: %s", result.errorMessage.c_str());
		return true;  // Continue - let routes decide if auth is required
	}
}
