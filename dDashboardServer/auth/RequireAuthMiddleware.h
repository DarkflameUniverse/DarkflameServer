#ifndef __REQUIREAUTHMIDDLEWARE_H__
#define __REQUIREAUTHMIDDLEWARE_H__

#include <memory>
#include <cstdint>
#include <functional>
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

	// The required level is looked up on every request (a permission that can be changed while running)
	explicit RequireAuthMiddleware(std::function<uint8_t()> requiredLevel);
	~RequireAuthMiddleware() override = default;

	bool Process(HTTPContext& context, HTTPReply& reply) override;

	// Whether a GM level may use the API (requests signed in with a token in the Authorization header rather than
	// the browser's cookie). Set by the dashboard from its api_access permission; unset allows everyone.
	static void SetApiAccessCheck(std::function<bool(uint8_t gmLevel)> check);

	// Renders the page a signed-in account gets when it may not open a page (not /api/); unset replies with JSON
	static void SetForbiddenPage(std::function<void(const HTTPContext& context, HTTPReply& reply)> render);
	std::string GetName() const override { return "RequireAuthMiddleware"; }

private:
	std::function<uint8_t()> requiredLevel;
};

#endif // !__REQUIREAUTHMIDDLEWARE_H__
