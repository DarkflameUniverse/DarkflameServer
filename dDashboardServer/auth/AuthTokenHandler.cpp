#include "AuthTokenHandler.h"
#include "DashboardAuthService.h"
#include "Game.h"
#include "Logger.h"
#include "HTTPContext.h"
#include "Web.h"

std::string AuthTokenHandler::ExtractCookie(const std::string& cookieHeader, const std::string& name) {
	// Match whole cookie names only so "xdashboardToken" does not satisfy "dashboardToken"
	size_t pos = 0;
	while (pos < cookieHeader.size()) {
		while (pos < cookieHeader.size() && (cookieHeader[pos] == ' ' || cookieHeader[pos] == ';')) pos++;
		const size_t end = std::min(cookieHeader.find(';', pos), cookieHeader.size());
		const std::string_view pair(cookieHeader.data() + pos, end - pos);
		const size_t eq = pair.find('=');
		if (eq != std::string_view::npos && pair.substr(0, eq) == name) {
			const auto value = pair.substr(eq + 1);
			std::string decoded;
			decoded.reserve(value.size());
			for (size_t i = 0; i < value.size(); ++i) {
				if (value[i] == '%' && i + 2 < value.size()) {
					char* endptr = nullptr;
					const std::string hex(value.substr(i + 1, 2));
					const auto code = std::strtol(hex.c_str(), &endptr, 16);
					if (endptr == hex.c_str() + 2) {
						decoded += static_cast<char>(code);
						i += 2;
						continue;
					}
				}
				decoded += value[i];
			}
			return decoded;
		}
		pos = end + 1;
	}
	return "";
}

std::string AuthTokenHandler::ExtractTokenFromAuthHeader(const std::string& authHeader) {
	if (authHeader.starts_with("Bearer ")) return authHeader.substr(7);
	if (authHeader.starts_with("Token ")) return authHeader.substr(6);
	return "";
}

std::string AuthTokenHandler::ExtractToken(const std::string& cookieHeader, const std::string& authHeader, eTokenSource& source) {
	auto token = ExtractTokenFromAuthHeader(authHeader);
	if (!token.empty()) {
		source = eTokenSource::HEADER;
		return token;
	}

	token = ExtractCookie(cookieHeader, COOKIE_NAME);
	source = token.empty() ? eTokenSource::NONE : eTokenSource::COOKIE;
	return token;
}

AuthTokenHandler::TokenValidationResult AuthTokenHandler::ValidateToken(const std::string& token) {
	TokenValidationResult result;

	if (token.empty()) {
		result.errorMessage = "No token provided";
		return result;
	}

	if (!DashboardAuthService::VerifyToken(token, result.username, result.gmLevel, result.accountId)) {
		result.errorMessage = "Invalid or expired token";
		return result;
	}

	result.isValid = true;
	return result;
}

bool AuthTokenHandler::ProcessHTTPContext(HTTPContext& context, HTTPReply& reply) {
	eTokenSource source = eTokenSource::NONE;
	const auto token = ExtractToken(context.GetHeader("Cookie"), context.GetHeader("Authorization"), source);
	if (token.empty()) return true;

	const auto result = ValidateToken(token);
	if (!result.isValid) {
		LOG_DEBUG("Authentication token validation failed: %s", result.errorMessage.c_str());
		return true; // Let routes decide if auth is required
	}

	context.isAuthenticated = true;
	context.authenticatedUser = result.username;
	context.accountId = result.accountId;
	context.gmLevel = result.gmLevel;
	context.userData["auth_source"] = source == eTokenSource::COOKIE ? "cookie" : "header";
	if (DashboardAuthService::NeedsTwoFactorSetup(result.accountId, result.gmLevel)) context.userData["needs_2fa"] = "1";
	return true;
}

std::string AuthTokenHandler::BuildSessionCookie(const std::string& token, bool rememberMe, bool secure) {
	std::string cookie = std::string("Set-Cookie: ") + COOKIE_NAME + "=" + token + "; Path=/; HttpOnly; SameSite=Strict";
	if (rememberMe) cookie += "; Max-Age=" + std::to_string(30 * 24 * 60 * 60);
	if (secure) cookie += "; Secure";
	return cookie;
}

std::string AuthTokenHandler::BuildClearSessionCookie(bool secure) {
	std::string cookie = std::string("Set-Cookie: ") + COOKIE_NAME + "=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0";
	if (secure) cookie += "; Secure";
	return cookie;
}
