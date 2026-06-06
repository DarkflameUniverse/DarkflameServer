#pragma once

#include "IHTTPMiddleware.h"
#include <vector>

/**
 * Authentication Middleware
 * 
 * Verifies JWT tokens from:
 * - Query parameter: ?token=...
 * - Cookie: dashboardToken=...
 * - Authorization header: Bearer <token> or Token <token>
 * 
 * Populates HTTPContext with authentication information if valid.
 * Does NOT fail on missing auth - that's left to specific routes.
 */
class AuthMiddleware : public IHTTPMiddleware {
public:
	AuthMiddleware() = default;
	
	bool Process(HTTPContext& context, HTTPReply& reply) override;
	
	std::string GetName() const override { return "AuthMiddleware"; }
	
private:
	/**
	 * Extract token from query string
	 * Expected format: "?token=eyJhbGc..." or "&token=eyJhbGc..."
	 */
	static std::string ExtractTokenFromQueryString(const std::string& queryString);
	
	/**
	 * Extract token from Cookie header
	 * Looks for "dashboardToken=..." cookie
	 */
	static std::string ExtractTokenFromCookies(const std::string& cookieHeader);
	
	/**
	 * Extract token from Authorization header
	 * Supports: "Bearer <token>", "Token <token>", or raw token
	 */
	static std::string ExtractTokenFromAuthHeader(const std::string& authHeader);
};
