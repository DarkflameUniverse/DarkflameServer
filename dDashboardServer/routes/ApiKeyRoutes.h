#pragma once

#include <cstdint>
#include <string>

struct HTTPContext;

/**
 * Dashboard API keys: each person makes, rotates and revokes their own keys on their account page (with a signed-in
 * browser session, never with a key). A key's scope is chosen from the permissions its maker has; staff with
 * api_keys_manage can see and revoke other accounts' keys, following the rank rules.
 */
namespace ApiKeyRoutes {
	void RegisterRoutes();

	/**
	 * Make a key with all of its maker's permissions (what POST /api/auth/token hands out, as the old API tokens did).
	 * @return The key, shown once
	 */
	std::string CreateFullKey(const HTTPContext& context, const std::string& name, int64_t days);
}
