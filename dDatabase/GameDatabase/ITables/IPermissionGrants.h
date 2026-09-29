#ifndef __IPERMISSIONGRANTS__H__
#define __IPERMISSIONGRANTS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * Permissions and slash commands granted to (or denied from) one account or one character, on top of what its GM level
 * allows (PermissionGrants.h). Rows are never deleted: removing a grant sets revokedAt, so the rows are also the history.
 */
class IPermissionGrants {
public:
	struct Grant {
		uint64_t id{};
		std::string targetType;    // "account" or "character"
		int64_t targetId{};        // the account's ID, or the character's charinfo ID
		std::string kind;          // "permission", "command", "permission_group" or "command_group"
		std::string name;          // the permission key, command name, permission category or GM level
		bool deny{};
		int64_t expiresAt{};       // 0: never
		std::string note;
		int64_t grantedAt{};
		uint32_t grantedById{};    // the account that granted it
		std::string grantedBy;
		int64_t revokedAt{};       // 0: not removed
		std::string revokedBy;
	};

	virtual uint64_t InsertPermissionGrant(const Grant& grant) = 0;
	virtual std::optional<Grant> GetPermissionGrant(uint64_t id) = 0;
	// Every grant of one target, removed and expired ones too, newest first
	virtual std::vector<Grant> GetPermissionGrants(const std::string& targetType, int64_t targetId) = 0;
	// The grants in force at `now` (not removed, not expired) of an account and, when characterId isn't 0, one character
	virtual std::vector<Grant> GetActivePermissionGrants(uint32_t accountId, int64_t characterId, int64_t now) = 0;
	// Every target's grants, newest first: only the ones in force at `now`, or all of them (the history)
	virtual std::vector<Grant> GetRecentPermissionGrants(bool activeOnly, int64_t now, uint32_t limit) = 0;
	// Whether it was in force until now (false: no such grant, or already removed)
	virtual bool RevokePermissionGrant(uint64_t id, const std::string& revokedBy, int64_t time) = 0;
};

#endif  //!__IPERMISSIONGRANTS__H__
