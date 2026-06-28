#pragma once

#include "IHTTPMiddleware.h"
#include "eHTTPStatusCode.h"

/**
 * Require Authentication Middleware
 * 
 * Verifies that the request has been authenticated.
 * Must be placed AFTER AuthMiddleware in the chain.
 * 
 * Fails with 401 Unauthorized if not authenticated.
 * Optionally checks for minimum GM level.
 */
class RequireAuthMiddleware : public IHTTPMiddleware {
public:
	/**
	 * Create a require auth middleware
	 * 
	 * @param minGmLevel Minimum GM level required (0 = any authenticated user)
	 */
	explicit RequireAuthMiddleware(uint8_t minGmLevel = 0) 
		: minGmLevel(minGmLevel) {}
	
	bool Process(HTTPContext& context, HTTPReply& reply) override;
	
	std::string GetName() const override { 
		return "RequireAuthMiddleware(minGM=" + std::to_string(minGmLevel) + ")"; 
	}
	
private:
	uint8_t minGmLevel{};
};
