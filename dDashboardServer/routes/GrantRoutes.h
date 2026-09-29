#pragma once

/**
 * Permission grants (PermissionGrants.h): dashboard permissions and in-game commands given to, or taken from, one
 * account or character. Needs grants_manage; nobody grants or takes away what they don't hold themselves, and only on
 * accounts the rank rules let them manage. Every change is audited.
 */
namespace GrantRoutes {
	void RegisterRoutes();
}
