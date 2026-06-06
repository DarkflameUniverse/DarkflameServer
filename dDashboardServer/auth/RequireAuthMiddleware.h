#ifndef __REQUIREAUTHMIDDLEWARE_H__
#define __REQUIREAUTHMIDDLEWARE_H__

#include <memory>
#include <cstdint>
#include "IHTTPMiddleware.h"

/**
 * RequireAuthMiddleware: Enforces authentication on protected routes
 * 
 * Returns 401 Unauthorized if user is not authenticated
 * Returns 403 Forbidden if user's GM level is below minimum required
 */
class RequireAuthMiddleware final : public IHTTPMiddleware {
public:
	/**
	 * @param minGmLevel Minimum GM level required to access this route
	 *                   0 = any authenticated user, higher numbers = GM-only
	 */
	explicit RequireAuthMiddleware(uint8_t minGmLevel = 0);
	~RequireAuthMiddleware() override = default;

	bool Process(HTTPContext& context, HTTPReply& reply) override;
	std::string GetName() const override { return "RequireAuthMiddleware"; }

private:
	uint8_t minGmLevel;
};

#endif // !__REQUIREAUTHMIDDLEWARE_H__
