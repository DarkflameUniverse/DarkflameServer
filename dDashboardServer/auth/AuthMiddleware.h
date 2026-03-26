#ifndef __AUTHMIDDLEWARE_H__
#define __AUTHMIDDLEWARE_H__

#include <string>
#include <memory>
#include "IHTTPMiddleware.h"

/**
 * AuthMiddleware: Extracts and verifies authentication tokens
 * 
 * Token extraction sources (in priority order):
 * 1. Query parameter: ?token=eyJhbGc...
 * 2. Cookie: dashboardToken=...
 * 3. Authorization header: Bearer <token> or Token <token>
 * 
 * Sets HTTPContext.isAuthenticated, HTTPContext.authenticatedUser,
 * and HTTPContext.gmLevel if token is valid.
 */
class AuthMiddleware final : public IHTTPMiddleware {
public:
	AuthMiddleware() = default;
	~AuthMiddleware() override = default;

	bool Process(HTTPContext& context, HTTPReply& reply) override;
	std::string GetName() const override { return "AuthMiddleware"; }
};

#endif // !__AUTHMIDDLEWARE_H__
