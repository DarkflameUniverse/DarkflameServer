#include "AuthMiddleware.h"
#include "HTTPContext.h"
#include "eHTTPStatusCode.h"
#include <algorithm>
#include "Logger.h"

// Forward declare DashboardAuthService::VerifyToken
// This will be implemented in the dashboard server
namespace DashboardAuthService {
	bool VerifyToken(const std::string& token, std::string& username, uint8_t& gmLevel);
}

bool AuthMiddleware::Process(HTTPContext& context, HTTPReply& reply) {
	// Try to extract token from query string first
	std::string token = ExtractTokenFromQueryString(context.queryString);
	
	// If not found in query string, try cookies
	if (token.empty()) {
		const std::string& cookieHeader = context.GetHeader("Cookie");
		if (!cookieHeader.empty()) {
			token = ExtractTokenFromCookies(cookieHeader);
		}
	}
	
	// If not found in query or cookies, try Authorization header (API token)
	if (token.empty()) {
		const std::string& authHeader = context.GetHeader("Authorization");
		if (!authHeader.empty()) {
			token = ExtractTokenFromAuthHeader(authHeader);
		}
	}
	
	// If token found, verify it
	if (!token.empty()) {
		std::string username{};
		uint8_t gmLevel = 0;
		
		if (DashboardAuthService::VerifyToken(token, username, gmLevel)) {
			context.isAuthenticated = true;
			context.authenticatedUser = username;
			context.gmLevel = gmLevel;
			LOG_DEBUG("Authenticated user %s (GM level %d)", username.c_str(), gmLevel);
			return true; // Continue to next middleware
		} else {
			LOG_DEBUG("Failed to verify token from %s", context.clientIP.c_str());
		}
	}
	
	// No valid token found, but we don't fail here
	// Routes can check context.isAuthenticated if they require auth
	return true;
}

std::string AuthMiddleware::ExtractTokenFromQueryString(const std::string& queryString) {
	if (queryString.empty()) return "";
	
	const std::string tokenPrefix = "token=";
	const size_t tokenPos = queryString.find(tokenPrefix);
	
	if (tokenPos == std::string::npos) {
		return "";
	}
	
	const size_t valueStart = tokenPos + tokenPrefix.length();
	const size_t valueEnd = queryString.find("&", valueStart);
	
	if (valueEnd == std::string::npos) {
		return queryString.substr(valueStart);
	}
	
	return queryString.substr(valueStart, valueEnd - valueStart);
}

std::string AuthMiddleware::ExtractTokenFromCookies(const std::string& cookieHeader) {
	if (cookieHeader.empty()) return "";
	
	const std::string searchStr = "dashboardToken=";
	const size_t pos = cookieHeader.find(searchStr);
	
	if (pos == std::string::npos) {
		return "";
	}
	
	const size_t valueStart = pos + searchStr.length();
	const size_t valueEnd = cookieHeader.find(";", valueStart);
	
	std::string value;
	if (valueEnd == std::string::npos) {
		value = cookieHeader.substr(valueStart);
	} else {
		value = cookieHeader.substr(valueStart, valueEnd - valueStart);
	}
	
	// URL decode the value
	std::string decoded{};
	for (size_t i = 0; i < value.length(); ++i) {
		if (value[i] == '%' && i + 2 < value.length()) {
			const std::string hex = value.substr(i + 1, 2);
			char* endptr = nullptr;
			const int charCode = static_cast<int>(std::strtol(hex.c_str(), &endptr, 16));
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

std::string AuthMiddleware::ExtractTokenFromAuthHeader(const std::string& authHeader) {
	if (authHeader.empty()) return "";
	
	// Check for "Bearer <token>" format
	const std::string bearerPrefix = "Bearer ";
	if (authHeader.find(bearerPrefix) == 0) {
		return authHeader.substr(bearerPrefix.length());
	}
	
	// Also check for "Token <token>" format (API tokens)
	const std::string tokenPrefix = "Token ";
	if (authHeader.find(tokenPrefix) == 0) {
		return authHeader.substr(tokenPrefix.length());
	}
	
	// Return as-is if no prefix (raw token)
	return authHeader;
}
